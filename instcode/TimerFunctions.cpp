
/*
 * Time spent in each function
 *
 * file per rank
 * function: total
 *   - per thread time
 * Timer
 */

#include <InstrumentationCommon.hpp>
#include <TimerFunctions.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>


DataManager<FunctionTimers*>* AllData = NULL;

// Clark
#define CLOCK_RATE_HZ 2600079000

//#define CLOCK_RATE_HZ 2800000000
//#define CLOCK_RATE_HZ 3326000000
inline uint64_t read_timestamp_counter(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}

/*
 * When a new image is added, called once per existing thread
 * When a new thread is added, called once per loaded image
 *
 * timers: some pre-existing data
 * typ: ThreadTyp when called via AddThread
 *      ImageTyp when called via AddImage
 * iid: image the new data will be for
 * tid: thread the new data will be for
 * firstimage: key of first image created
 * 
 */
FunctionTimers* GenerateFunctionTimers(FunctionTimers* timers, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    FunctionTimers* retval;
    retval = new FunctionTimers();

    retval->master = timers->master && typ == AllData->ImageType;
    retval->application = timers->application;
    retval->extension = timers->extension;
    retval->functionCount = timers->functionCount;
    retval->functionNames = timers->functionNames;
    retval->functionTimerAccum = new uint64_t[retval->functionCount];
    retval->functionTimerLast = new uint64_t[retval->functionCount];
    retval->inFunction = new uint32_t[retval->functionCount];

    memset(retval->functionTimerAccum, 0, sizeof(*retval->functionTimerAccum) * retval->functionCount);
    memset(retval->functionTimerLast, 0, sizeof(*retval->functionTimerLast) * retval->functionCount);
    memset(retval->inFunction, 0, sizeof(*retval->inFunction) * retval->functionCount);

    return retval;
}

void DeleteFunctionTimers(FunctionTimers* timers){
    delete timers->functionTimerAccum;
    delete timers->functionTimerLast;
    delete timers->inFunction;
}

uint64_t ReferenceFunctionTimers(FunctionTimers* timers){
    return (uint64_t)timers;
}

extern "C"
{

    // start timer
    int32_t function_entry(uint32_t funcIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        FunctionTimers* timers = AllData->GetData(*key, pthread_self());
        assert(timers != NULL);
        assert(timers->functionTimerLast != NULL);
        if(timers->inFunction[funcIndex] == 0){
            timers->functionTimerLast[funcIndex] = read_timestamp_counter();
        }
        ++timers->inFunction[funcIndex];
        return 0;
    }

    // end timer
    int32_t function_exit(uint32_t funcIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();
        FunctionTimers* timers = AllData->GetData(*key, pthread_self());

        uint32_t recDepth = timers->inFunction[funcIndex];
        --recDepth;
        if(recDepth == 0) {
            uint64_t last = timers->functionTimerLast[funcIndex];
            uint64_t now = read_timestamp_counter();
            timers->functionTimerAccum[funcIndex] += now - last;
            timers->functionTimerLast[funcIndex] = now;
        } else if(recDepth < 0) {
            recDepth = 0;
        }
        timers->inFunction[funcIndex] = recDepth;

        return 0;
    }

    // initialize dynamic instrumentation
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn) {
        InitializeDynamicInstrumentation(count, dyn);
        return NULL;
    }

    // Just after MPI_Init is called
    void* tool_mpi_init() {
        return NULL;
    }

    // Entry function for threads
    void* tool_thread_init(thread_key_t tid) {
        if (AllData){
            AllData->AddThread(tid);
        } else {
        ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
        }
        return NULL;
    }

    // Optionally? called on thread join/exit?
    void* tool_thread_fini(thread_key_t tid) {
        return NULL;
    }

    // Called when new image is loaded
    void* tool_image_init(void* args, image_key_t* key, ThreadData* td) {

        FunctionTimers* timers = (FunctionTimers*)args;

        // Remove this instrumentation
        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);

        // If this is the first image, set up a data manager
        if (AllData == NULL){
            AllData = new DataManager<FunctionTimers*>(GenerateFunctionTimers, DeleteFunctionTimers, ReferenceFunctionTimers);
        }

        // Add this image
        AllData->AddImage(timers, td, *key);
        return NULL;
    }

    // 
    void* tool_image_fini(image_key_t* key) {

        image_key_t iid = *key;

        if (AllData == NULL){
            ErrorExit("data manager does not exist. no images were intialized", MetasimError_NoImage);
            return NULL;
        }

        FunctionTimers* timers = AllData->GetData(iid, pthread_self());
        if (timers == NULL){
            ErrorExit("Cannot retrieve image data using key " << dec << (*key), MetasimError_NoImage);
            return NULL;
        }

        if (!timers->master){
            printf("Image is not master, skipping\n");
            return NULL;
        }

        char outFileName[1024];
        sprintf(outFileName, "%s.meta_%0d.%s", timers->application, GetTaskId(), timers->extension);
        FILE* outFile = fopen(outFileName, "w");
        if (!outFile){
            cerr << "error: cannot open output file %s" << outFileName << ENDL;
            exit(-1);
        }


        // for each image
        //   for each function
        //     for each thread
        //       print time
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit) {
            FunctionTimers* imageData = AllData->GetData(*iit, pthread_self());

            char** functionNames = imageData->functionNames;
            uint64_t functionCount = imageData->functionCount;

            for (uint64_t funcIndex = 0; funcIndex < functionCount; ++funcIndex){
                char* fname = functionNames[funcIndex];
                fprintf(outFile, "\n%s:\t", fname);
                for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); ++tit) {
                    FunctionTimers* timers = AllData->GetData(*iit, *tit);
                    fprintf(outFile, "\tThread: 0x%llx\tTime: %f\t", *tit, (double)(timers->functionTimerAccum[funcIndex]) / CLOCK_RATE_HZ);
                }
            }
        }
        fflush(outFile);
        fclose(outFile);
        return NULL;
    }
};

