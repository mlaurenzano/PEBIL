/*
 * PAPI Loop Instrumentation
 * The instrumentation reads the env var HWC0, HWC1, ... up to HWC31, in the order,
 * but stops at the first that is not defined (no gaps allowed).
 * The env variables should be set to PAPI present events, e.g. PAPI_TOT_CYC.
 * Then, for each loop instrumented, the value of the counters specified is accumulated
 * and reported on at the end of execution. The values are printed in a meta_%.lpiinst file.
 * There is no check that events are compatible, please use the papi_event_chooser to verify
 * events compatibility.
 *
 * Usage example:
 * pebil --tool LoopIntercept --app bench --inp outer.loops --lnc libpapiinst.so,libpapi.so
 * export HWC0=PAPI_TOT_INS
 * export HWC1=PAPI_TOT_CYC
 * ./bench.lpiinst
 * grep Thread bench.meta_0.lpiinst | awk '{print $3/$4}' # compute IPC of loops
 */

#include <InstrumentationCommon.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <papi.h>
#include <iostream>

#include <PAPIInst.hpp>

//#define DEBUG_PAPI_THREADING

DataManager<PAPIInst*>* AllData = NULL;
static int PAPI_events[MAX_HWC];
static int PAPI_event_count = 0;
static long long* PAPI_tmp_stopper = NULL;

PAPIInst* GeneratePAPIInst(PAPIInst* counters, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    PAPIInst* retval;
    retval = new PAPIInst();
    retval->master = counters->master && typ == AllData->ImageType;
    retval->application = counters->application;
    retval->extension = counters->extension;
    retval->loopCount = counters->loopCount;
    retval->loopHashes = counters->loopHashes;
    retval->values = new values_t[retval->loopCount];
    return retval;
}

void DeletePAPIInst(PAPIInst* counters){
    delete counters;
}

uint64_t ReferencePAPIInst(PAPIInst* counters){
    return (uint64_t)counters;
}

extern "C"
{
    int32_t loop_entry(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        PAPIInst* counters = AllData->GetData(*key, pthread_self());
        assert(counters != NULL);
       
#ifdef DEBUG_PAPI_THREADING
        fprintf(stdout, "Thread %lx starting %d counters for loop %ld\n", pthread_self(), PAPI_event_count, counters->loopHashes[loopIndex]);
        fflush(stdout);
#endif

        // this will fail every time but the first
        int ret = PAPI_start_counters(PAPI_events, PAPI_event_count);
#ifdef DEBUG_PAPI_THREADING
        if (ret != PAPI_OK){
            warn << "PAPI_start_counters failed: " << PAPI_strerror(ret) << ENDL;
        }
#endif

        return 0;
    }

    int32_t loop_exit(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        PAPIInst* counters = AllData->GetData(*key, pthread_self());

#ifdef DEBUG_PAPI_THREADING
        fprintf(stdout, "Thread %lx stopping counters for loop %ld\n", pthread_self(), counters->loopHashes[loopIndex]);
        fflush(stdout);
#endif

        // this will fail only if it is hit before the first call to PAPI_start_counters
        int ret = PAPI_accum_counters(counters->values[loopIndex], PAPI_event_count);
        if (ret != PAPI_OK){
            warn << "PAPI_accum_counters failed: " << PAPI_strerror(ret) << ENDL;
        }


        // this call will fail 100% of the time
        ret = PAPI_stop_counters(NULL, 0);

#ifdef DEBUG_PAPI_THREADING
        //ret = PAPI_stop_counters(PAPI_tmp_stopper, PAPI_event_count);
        if (ret != PAPI_OK){
            warn << "PAPI_stop_counters failed: " << PAPI_strerror(ret) << ENDL;
        }
#endif

        return 0;
    }

    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn) {
        InitializeDynamicInstrumentation(count, dyn);
        return NULL;
    }

    void* tool_mpi_init() {
        return NULL;
    }

    void* tool_thread_init(thread_key_t tid) {
        if (AllData){
            AllData->AddThread(tid);
            PAPI_register_thread();
        } else {
            ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
        }
        return NULL;
    }

    void* tool_thread_fini(thread_key_t tid) {
        return NULL;
    }

    void* tool_image_init(void* args, image_key_t* key, ThreadData* td) {

        PAPIInst* counters = (PAPIInst*)args;

        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);

        if (AllData == NULL){
            AllData = new DataManager<PAPIInst*>(GeneratePAPIInst, DeletePAPIInst, ReferencePAPIInst);
        }

        AllData->AddImage(counters, td, *key);

        counters = AllData->GetData(*key, pthread_self());

        int ret = PAPI_library_init(PAPI_VER_CURRENT);
        if (ret != PAPI_VER_CURRENT && ret > 0){
            ErrorExit("PAPI_library_init failed: " << PAPI_strerror(ret), MetaSimError_ExternalLib);
            return NULL;
        }

        ret = PAPI_thread_init(pthread_self);
        if (ret != PAPI_OK){
            ErrorExit("PAPI_thread_init failed: " << PAPI_strerror(ret), MetaSimError_ExternalLib);
            return NULL;
        }
	
        ret = PAPI_num_counters();
        if (ret <= 0){
            ErrorExit("PAPI_num_counters gave an answer we don't like: " << PAPI_strerror(ret), MetaSimError_ExternalLib);
            return NULL;
        }

        while(PAPI_event_count < MAX_HWC){
            char hwc_var[32];
            sprintf(hwc_var, "HWC%d", PAPI_event_count);
            char* hwc_name = getenv(hwc_var);
            if(hwc_name) {
                PAPI_event_name_to_code(hwc_name, PAPI_events+PAPI_event_count);
                ++PAPI_event_count;
            } else {
                break;
            }
        }

        PAPI_tmp_stopper = new long long[PAPI_event_count];

        return NULL;
    }

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
            printf("Image %lx is not master, skipping\n", iid);
            return NULL;
        }

        if (PAPI_tmp_stopper){
            delete[] PAPI_tmp_stopper;
            PAPI_tmp_stopper = NULL;
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
                    for(hwc = 0; hwc < PAPI_event_count; ++hwc)
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
