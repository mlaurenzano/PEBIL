
/*
 * PAPI Function Instrumentation
 *
 * file per rank
 * function: total
 *   - per thread time
 * Timer
 */

#include <InstrumentationCommon.hpp>
#include <PAPIFunc.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <papi.h>

#include <iostream>


DataManager<FunctionPAPI*>* AllData = NULL;

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
FunctionPAPI* GenerateFunctionPAPI(FunctionPAPI* counters, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    FunctionPAPI* retval;
    retval = new FunctionPAPI();

    retval->master = counters->master && typ == AllData->ImageType;
    retval->application = counters->application;
    retval->extension = counters->extension;
    retval->functionCount = counters->functionCount;
    retval->functionNames = counters->functionNames;
    retval->functionTimerAccum = new uint64_t[retval->functionCount];
    retval->functionTimerLast = new uint64_t[retval->functionCount];
    retval->inFunction = new uint32_t[retval->functionCount];
		retval->tmpValues = new values_t[retval->functionCount];
		retval->accumValues = new values_t[retval->functionCount];
		retval->num = 0;

    memset(retval->functionTimerAccum, 0, sizeof(*retval->functionTimerAccum) * retval->functionCount);
    memset(retval->functionTimerLast, 0, sizeof(*retval->functionTimerLast) * retval->functionCount);
    memset(retval->inFunction, 0, sizeof(*retval->inFunction) * retval->functionCount);

    return retval;
}

void DeleteFunctionPAPI(FunctionPAPI* counters){
    delete counters->functionTimerAccum;
    delete counters->functionTimerLast;
    delete counters->inFunction;
}

uint64_t ReferenceFunctionPAPI(FunctionPAPI* counters){
    return (uint64_t)counters;
}

extern "C"
{

    // start timer
    int32_t function_entry(uint32_t funcIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        FunctionPAPI* counters = AllData->GetData(*key, pthread_self());
        assert(counters != NULL);
        assert(counters->functionTimerLast != NULL);
        if(counters->inFunction[funcIndex] == 0){
            counters->functionTimerLast[funcIndex] = read_timestamp_counter();
						PAPI_start_counters(counters->events, counters->num);				
        }
        ++counters->inFunction[funcIndex];


        return 0;
    }

    // end timer
    int32_t function_exit(uint32_t funcIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();
        FunctionPAPI* counters = AllData->GetData(*key, pthread_self());

        uint32_t recDepth = counters->inFunction[funcIndex];
        --recDepth;
        if(recDepth == 0) {
						PAPI_stop_counters(counters->tmpValues[funcIndex], counters->num);
            uint64_t last = counters->functionTimerLast[funcIndex];
            uint64_t now = read_timestamp_counter();
            counters->functionTimerAccum[funcIndex] += now - last;
            counters->functionTimerLast[funcIndex] = now;
						for(int i = 0; i < counters->num; i++)
						{
							counters->accumValues[funcIndex][i] += counters->tmpValues[funcIndex][i];
						}
        } else if(recDepth < 0) {
            recDepth = 0;
        }
        counters->inFunction[funcIndex] = recDepth;


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

        FunctionPAPI* counters = (FunctionPAPI*)args;

        // Remove this instrumentation
        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);

        // If this is the first image, set up a data manager
        if (AllData == NULL){
            AllData = new DataManager<FunctionPAPI*>(GenerateFunctionPAPI, DeleteFunctionPAPI, ReferenceFunctionPAPI);
        }

        // Add this image
        AllData->AddImage(counters, td, *key);

				counters = AllData->GetData(*key, pthread_self());
	
				if(PAPI_num_counters() < PAPI_OK)
				{
					fprintf(stderr, "PAPI initialization failed");
					return NULL;
				}
		
				while(counters->num < MAX_HWC)
				{
					char hwc_var[32];
					sprintf(hwc_var, "HWC%d", counters->num);
					char* hwc_name = getenv(hwc_var);
					if(hwc_name)
					{	
						PAPI_event_name_to_code(hwc_name, counters->events + counters->num);
						++counters->num;
					}
					else	
						break;
				}

        return NULL;
    }

    // 
    void* tool_image_fini(image_key_t* key)
		{
    	image_key_t iid = *key;

      if (AllData == NULL){
      	ErrorExit("data manager does not exist. no images were intialized", MetasimError_NoImage);
        return NULL;
      }

      FunctionPAPI* counters = AllData->GetData(iid, pthread_self());
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

			fprintf(outFile, "\n\# Counters: ");
			char EventName[512];
			int i, j, retval;
			char** units;
			units = new char*[MAX_HWC];
			PAPI_event_info_t evinfo;

			for(i = 0; i < counters-> num; i++)
			{
				units[i] = new char[PAPI_MIN_STR_LEN];
				PAPI_event_code_to_name(*(counters->events + i), EventName);
				retval = PAPI_get_event_info(*(counters->events + i), &evinfo);
				if (retval != PAPI_OK)
				{
					cerr << "\n\t Error getting event info\n";
					exit(-1);
				}
				strncpy(units[i], evinfo.units, PAPI_MIN_STR_LEN);
				fprintf(outFile, "\t %s", EventName);
			}
			fprintf(outFile, "\n\n");

      // for each image
      //   for each function
      //     for each thread
      //       print time
      for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit)
			{
      	FunctionPAPI* imageData = AllData->GetData(*iit, pthread_self());

        char** functionNames = imageData->functionNames;
        uint64_t functionCount = imageData->functionCount;

        for (uint64_t funcIndex = 0; funcIndex < functionCount; ++funcIndex)
				{
          char* fname = functionNames[funcIndex];
          //fprintf(outFile, "\n%s:\t", fname);
          for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); ++tit)
					{
            FunctionPAPI* counters = AllData->GetData(*iit, *tit);
            //fprintf(outFile, "\tThread: 0x%llx\tTime: %f\t", *tit, (double)(counters->functionTimerAccum[funcIndex]) / CLOCK_RATE_HZ);
            int hwc;
						double scaledValue;
						for(hwc = 0; hwc < counters->num; ++hwc)
						{
							PAPI_event_code_to_name(*(counters->events + hwc), EventName);
							retval = PAPI_get_event_info(*(counters->events + hwc), &evinfo);
							if(retval != PAPI_OK) {
								cerr << "\n\t Error getting event info\n";
								exit(-1);
							}

							if(strstr(units[hwc],"nJ"))
							{
								scaledValue=(double)(counters->accumValues[funcIndex][hwc]/(1.0e9));
								fprintf(outFile, "@@ %s\t%s", counters->application, fname);
								fprintf(outFile, "\t %s", EventName);
								fprintf(outFile, "\t%.4f \n", scaledValue);
	
								// calculate the watts
								double watts=scaledValue/((double)(counters->functionTimerAccum[funcIndex])/CLOCK_RATE_HZ);
								fprintf(outFile, "@@ %s\t%s", counters->application, fname);
								fprintf(outFile, "\t %s_watts", EventName);
								fprintf(outFile, "\t%.4f \n", watts);
							}
							else
							{
								fprintf(outFile, "@@ %s\t%s", counters->application, fname);
								fprintf(outFile, "\t %s", EventName);
								fprintf(outFile, "\t%lld \n", counters->accumValues[funcIndex][hwc]);
							}
						}
						fprintf(outFile, "%s", fname);
						fprintf(outFile, "\tTime: %f\n", *tit, (double)(counters->functionTimerAccum[funcIndex])/CLOCK_RATE_HZ);
						fprintf(outFile, "\n");
          }
        }
      }
      
			fflush(outFile);
      fclose(outFile);
      return NULL;
    }
};

