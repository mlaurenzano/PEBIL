
/*
 * Time spent in each function
 *
 * file per rank
 * function: total
 *   - per thread time
 * Timer
 */

#include <InstrumentationCommon.hpp>
#include <LoopTimer.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>


DataManager<LoopTimers*>* AllData = NULL;

// Clark
#define CLOCK_RATE_HZ 2600079000

// Xeon Phi Max Rate
//#define CLOCK_RATE_HZ 1333332000
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
LoopTimers* GenerateLoopTimers(LoopTimers* timers, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    LoopTimers* retval;
    retval = new LoopTimers();

    retval->master = timers->master && typ == AllData->ImageType;
    retval->application = timers->application;
    retval->extension = timers->extension;
    retval->loopCount = timers->loopCount;
    retval->loopHashes = timers->loopHashes;
    retval->loopTimerAccum = new uint64_t[retval->loopCount];
    retval->loopTimerLast = new uint64_t[retval->loopCount];

    memset(retval->loopTimerAccum, 0, sizeof(uint64_t) * retval->loopCount);
    memset(retval->loopTimerLast, 0, sizeof(uint64_t) * retval->loopCount);
    return retval;
}

void DeleteLoopTimers(LoopTimers* timers){
    delete timers->loopTimerAccum;
    delete timers->loopTimerLast;
}

uint64_t ReferenceLoopTimers(LoopTimers* timers){
    return (uint64_t)timers;
}

extern "C"
{

    // start timer
    int32_t loop_entry(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        LoopTimers* timers = AllData->GetData(*key, pthread_self());
        assert(timers != NULL);
        assert(timers->loopTimerLast != NULL);
        
        timers->loopTimerLast[loopIndex] = read_timestamp_counter();
        return 0;
    }

    // end timer
    int32_t loop_exit(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        LoopTimers* timers = AllData->GetData(*key, pthread_self());
        uint64_t last = timers->loopTimerLast[loopIndex];
        uint64_t now = read_timestamp_counter();
        timers->loopTimerAccum[loopIndex] += now - last;
        //timers->loopTimerLast = now;

        return 0;
    }

    // initialize dynamic instrumentation
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn,bool* isThreadedModeFlag) {
        InitializeDynamicInstrumentation(count, dyn,isThreadedModeFlag);
        return NULL;
    }

    // Just after MPI_Init is called
    void* tool_mpi_init() {
        return NULL;
    }

    // Entry function for threads
    void* tool_thread_init(thread_key_t tid) {
        if (AllData){
            if(isThreadedMode())
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
        LoopTimers* timers = (LoopTimers*)args;

        // Remove this instrumentation
        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);

        // If this is the first image, set up a data manager
        if (AllData == NULL){
            init_signal_handlers();
            AllData = new DataManager<LoopTimers*>(GenerateLoopTimers, DeleteLoopTimers, ReferenceLoopTimers);
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

        LoopTimers* timers = AllData->GetData(iid, pthread_self());
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
        //   for each loop
        //     for each thread
        //       print time
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit) {
            LoopTimers* imageData = AllData->GetData(*iit, pthread_self());

            uint64_t imgHash = *iit;
            uint64_t* loopHashes = imageData->loopHashes;
            uint64_t loopCount = imageData->loopCount;

            for (uint64_t loopIndex = 0; loopIndex < loopCount; ++loopIndex){
                uint64_t loopHash = loopHashes[loopIndex];
                fprintf(outFile, "0x%llx:0x%llx:\n", imgHash, loopHash);
                for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); ++tit) {
                    LoopTimers* timers = AllData->GetData(*iit, *tit);
                    fprintf(outFile, "\tThread: 0x%llx\tTime: %f\n", *tit, (double)(timers->loopTimerAccum[loopIndex]) / CLOCK_RATE_HZ);
                }
            }
        }
        fflush(outFile);
        fclose(outFile);

        return NULL;
    }
};

