/*
 * PAPI Loop Instrumentation
 *
 */

#include <InstrumentationCommon.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <papi.h>
#include <iostream>

#include <PAPIInst.hpp>

DataManager<PAPIInst*>* AllData = NULL;

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
PAPIInst* GeneratePAPIInst(PAPIInst* counters, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    PAPIInst* retval;
    retval = new PAPIInst();
    retval->master = counters->master && typ == AllData->ImageType;
    retval->application = counters->application;
    retval->extension = counters->extension;
    retval->loopCount = counters->loopCount;
    retval->loopHashes = counters->loopHashes;
    retval->values = new values_t[retval->loopCount];
    retval->num = 0;
    return retval;
}

void DeletePAPIInst(PAPIInst* counters){
}

uint64_t ReferencePAPIInst(PAPIInst* counters){
    return (uint64_t)counters;
}

extern "C"
{

    // start timer
    int32_t loop_entry(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        PAPIInst* counters = AllData->GetData(*key, pthread_self());
        assert(counters != NULL);
       
        PAPI_start_counters(counters->events, counters->num);

        return 0;
    }

    // end timer
    int32_t loop_exit(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        PAPIInst* counters = AllData->GetData(*key, pthread_self());

        PAPI_accum_counters(counters->values[loopIndex], counters->num);
        PAPI_stop_counters(NULL, 0);

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

        PAPIInst* counters = (PAPIInst*)args;

        // Remove this instrumentation
        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);

        // If this is the first image, set up a data manager
        if (AllData == NULL){
            AllData = new DataManager<PAPIInst*>(GeneratePAPIInst, DeletePAPIInst, ReferencePAPIInst);
        }

        // Add this image
        AllData->AddImage(counters, td, *key);

        counters = AllData->GetData(*key, pthread_self());
	
        if(PAPI_num_counters() < PAPI_OK) {
          fprintf(stderr,"PAPI initialization failed");
          return NULL;
        }

        while(counters->num<MAX_HWC) {
          char hwc_var[32];
          sprintf(hwc_var,"HWC%d",counters->num);
          char* hwc_name = getenv(hwc_var);
          if(hwc_name) {
            PAPI_event_name_to_code(hwc_name,counters->events+counters->num);
            ++counters->num;
          } else
            break;
        }

        return NULL;
    }

    // Nothing to do here
    void* tool_image_fini(image_key_t* key) {
        image_key_t iid = *key;

        if (AllData == NULL){
            ErrorExit("data manager does not exist. no images were intialized", MetasimError_NoImage);
            return NULL;
        }

        PAPIInst* counters = AllData->GetData(iid, pthread_self());
        if (counters == NULL){
            ErrorExit("Cannot retrieve image data using key " << dec << (*key), MetasimError_NoImage);
            return NULL;
        }

        if (!counters->master){
            printf("Image is not master, skipping\n");
            return NULL;
        }

        char outFileName[1024];
        sprintf(outFileName, "%s.meta_%0d.%s", counters->application, GetTaskId(), counters->extension);
        FILE* outFile = fopen(outFileName, "w");
        if (!outFile){
            cerr << "error: cannot open output file %s" << outFileName << ENDL;
            exit(-1);
        }

        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit) {
            PAPIInst* imageData = AllData->GetData(*iit, pthread_self());

            uint64_t* loopHashes = imageData->loopHashes;
            uint64_t loopCount = imageData->loopCount;

            for (uint64_t loopIndex = 0; loopIndex < loopCount; ++loopIndex){
                uint64_t loopHash = loopHashes[loopIndex];
                fprintf(outFile, "0x%llx:\n", loopHash);
                for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); ++tit) {
                    counters = AllData->GetData(*iit, *tit);
                    fprintf(outFile, "\tThread: 0x%llx");
                    int hwc;
                    for(hwc = 0; hwc < counters->num; ++hwc)
                      fprintf(outFile,"\t%lld",counters->values[loopIndex][hwc]);
                    fprintf(outFile, "\n");
                }
            }
        }
        fflush(outFile);
        fclose(outFile);
        return NULL;
    }
};
