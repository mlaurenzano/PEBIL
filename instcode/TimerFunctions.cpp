
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
    retval->master = firstimage == iid && typ == AllData->ImageType;
    retval->functionCount = timers->functionCount;
    retval->functionNames = timers->functionNames;
    retval->functionTimerAccum = new double[retval->functionCount];
    retval->functionTimerLast = new double[retval->functionCount];

    memset(retval->functionTimerAccum, 0, sizeof(double) * retval->functionCount);
    memset(retval->functionTimerLast, 0, sizeof(double) * retval->functionCount);
    return retval;
}

void DeleteFunctionTimers(FunctionTimers* timers){
    delete timers->functionTimerAccum;
    delete timers->functionTimerLast;
}

uint64_t ReferenceFunctionTimers(FunctionTimers* timers){
    return (uint64_t)timers;
}

extern "C"
{

    // start timer
    int32_t function_entry(image_key_t* key, uint32_t funcIndex) {
        inform << "function_entry" << ENDL;
        thread_key_t tid = pthread_self();

        FunctionTimers* timers = AllData->GetData(*key, pthread_self());
        timers->functionTimerLast[funcIndex] = read_timestamp_counter();
        return 0;
    }

    // end timer
    int32_t function_exit(image_key_t* key, uint32_t funcIndex) {
        inform << "function_exit" << ENDL;
        thread_key_t tid = pthread_self();

        FunctionTimers* timers = AllData->GetData(*key, pthread_self());
        double last = timers->functionTimerLast[funcIndex];
        double now = read_timestamp_counter();
        timers->functionTimerAccum = now - last;
        //timers->functionTimerLast = now;

        return 0;
    }

    // initialize dynamic instrumentation
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn) {
        inform << "tool_dynamic_init" << ENDL;
        InitializeDynamicInstrumentation(count, dyn);
        return NULL;
    }

    // Just after MPI_Init is called
    void* tool_mpi_init() {
        inform << "tool_mpi_init" << ENDL;
        return NULL;
    }

    // Entry function for threads
    void* tool_thread_init(thread_key_t tid) {
        inform << "tool_thread_init" << ENDL;
        return NULL;
    }

    // Optionally? called on thread join/exit?
    void* tool_thread_fini(thread_key_t tid) {
        inform << "tool_thread_fini" << ENDL;
        return NULL;
    }

    // Called when new image is loaded
    void* tool_image_init(void* args, image_key_t* key, ThreadData* td) {
        inform << "tool_image_init " << *key << ENDL;

        FunctionTimers* timers = (FunctionTimers*)args;
        inform << "There are " << timers->functionCount << " functions" << ENDL;

        // Remove this instrumentation
        set<uint64_t> inits;
        inits.insert(*key);
        inform << "Removing init points for image " << hex << (*key) << ENDL;
        SetDynamicPoints(inits, false);

        // If this is the first image, set up a data manager
        if (AllData == NULL){
            AllData = new DataManager<FunctionTimers*>(GenerateFunctionTimers, DeleteFunctionTimers, ReferenceFunctionTimers);
        }

        // Add this image
        AllData->AddImage(timers, td, *key);
        inform << "image added" << ENDL;
        return NULL;
    }

    // 
    void* tool_image_fini(image_key_t* key) {
        inform << "tool_image_fini " << *key << ENDL;

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
            return NULL;
        }

        inform << "Printing counters here!" << ENDL;
        FunctionTimers* masterData = AllData->GetData(*key, pthread_self());

        // for each image
        //   for each function
        //     for each thread
        //       print time
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit) {
            
            char** functionNames = masterData->functionNames;
            uint64_t functionCount = masterData->functionCount;
            for (uint64_t funcIndex = 0; funcIndex < functionCount; ++funcIndex){
                char* fname = functionNames[funcIndex];
               // FIXME 
            }
        }
        return NULL;
    }
};

