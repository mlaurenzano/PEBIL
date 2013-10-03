
/*
 * Time spent in each function
 *
 * file per rank
 * function: total
 *   - per thread time
 * Timer
 */

#include <InstrumentationCommon.hpp>
#include <FrequencyConfig.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>

DataManager<FrequencyConfig*>* AllData = NULL;

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
FrequencyConfig* GenerateFrequencyConfig(FrequencyConfig* fconfig, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    FrequencyConfig* retval;
    retval = new FrequencyConfig();

    retval->master = fconfig->master && typ == AllData->ImageType;
    retval->application = fconfig->application;
    retval->extension = fconfig->extension;
    retval->loopCount = fconfig->loopCount;
    retval->loopHashes = fconfig->loopHashes;
    retval->frequencyMap = new uint32_t[retval->loopCount];

    memset(retval->frequencyMap, 0, sizeof(uint32_t) * retval->loopCount);
    return retval;
}

void DeleteFrequencyConfig(FrequencyConfig* fconfig){
    delete fconfig->frequencyMap;
}

uint64_t ReferenceFrequencyConfig(FrequencyConfig* fconfig){
    return (uint64_t)fconfig;
}

extern "C"
{

    // start timer
    int32_t loop_entry(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        FrequencyConfig* fconfig = AllData->GetData(*key, pthread_self());
        assert(fconfig != NULL);
       
        // FIXME do throttling 
        uint32_t f = fconfig->frequencyMap[loopIndex];

        return 0;
    }

    // end timer
    int32_t loop_exit(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        FrequencyConfig* fconfig = AllData->GetData(*key, pthread_self());

        // FIXME do nothing or reset frequency

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

        FrequencyConfig* fconfig = (FrequencyConfig*)args;

        // Remove this instrumentation
        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);

        // If this is the first image, set up a data manager
        if (AllData == NULL){
            AllData = new DataManager<FrequencyConfig*>(GenerateFrequencyConfig, DeleteFrequencyConfig, ReferenceFrequencyConfig);
        }

        // Add this image
        AllData->AddImage(fconfig, td, *key);

        // FIXME read input file to configure frequencies for this image

        return NULL;
    }

    // Nothing to do here
    void* tool_image_fini(image_key_t* key) {
        return NULL;
    }
};

