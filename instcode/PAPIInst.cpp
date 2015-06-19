/*
 * PAPI Loop Instrumentation
 * The instrumentation reads the env var HWC0, HWC1, ... up to HWC31, in the order,
 * but stops at the first that is not defined (no gaps allowed).
 * The env variables should be set to PAPI present events, e.g. PAPI_TOT_CYC.
 * Then, for each loop instrumented, the value of the counters specified is accumulated
 * and reported on at the end of execution. The valses are printed in a meta_%.lpiinst file.
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
#include <sys/socket.h>
#include <sys/un.h>
#include <papi.h>
#include <iostream>

#include <PAPIInst.hpp>

// Clark
#define CLOCK_RATE_HZ 2600079000

// Xeon Phi Max Rate
//#define CLOCK_RATE_HZ 1333332000
inline uint64_t read_timestamp_counter(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}

DataManager<PAPIInst*>* AllData = NULL;

PAPIInst* GeneratePAPIInst(PAPIInst* counters, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage) {

    PAPIInst* retval;
    retval = new PAPIInst();
    retval->master = counters->master && typ == AllData->ImageType;
    retval->application = counters->application;
    retval->extension = counters->extension;
    retval->loopCount = counters->loopCount;
    retval->loopHashes = counters->loopHashes;
    retval->tmpValues= new values_t[retval->loopCount];
    retval->accumValues = new values_t[retval->loopCount];
    retval->num = 0;

    /* loop timer additions */
    retval->loopTimerAccum = new uint64_t[retval->loopCount];
    retval->loopTimerLast = new uint64_t[retval->loopCount];
    memset(retval->loopTimerAccum, 0, sizeof(uint64_t) * retval->loopCount);
    memset(retval->loopTimerLast, 0, sizeof(uint64_t) * retval->loopCount);
    /* loop timer additions done */
    

    return retval;
}

void DeletePAPIInst(PAPIInst* counters){
  /* loop timer additions */
  delete counters->loopTimerAccum;
  delete counters->loopTimerLast;
  /* loop timer additions done */

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

	/* loop timer additions */
	assert(counters->loopTimerLast != NULL);
        counters->loopTimerLast[loopIndex] = read_timestamp_counter();
	/* loop timer additions done */

        PAPI_start_counters(counters->events, counters->num);

        return 0;
    }

    int32_t loop_exit(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        PAPIInst* counters = AllData->GetData(*key, pthread_self());

        PAPI_stop_counters(counters->tmpValues[loopIndex], counters->num);

	/* loop timer additions */
	uint64_t now = read_timestamp_counter();
	uint64_t last = counters->loopTimerLast[loopIndex];
        counters->loopTimerAccum[loopIndex] += now - last;
	/* loop timer additions done */

	for(int i=0;i<counters->num;i++)
		counters->accumValues[loopIndex][i]+=counters->tmpValues[loopIndex][i];
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

		void* tool_image_fini(image_key_t* key)
		{
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

      fprintf(outFile,"\n\# Counters: ");
			char EventName[512];
			int i,j,retval;
      char** units;
			// AT: changed the counters->num to MAX_HWC
			units=new char*[MAX_HWC];
			//units=new char*[counters->num];
			PAPI_event_info_t evinfo;
	
		  for(i=0;i<counters->num;i++)
			{
			  units[i]=new char[PAPI_MIN_STR_LEN];
			  PAPI_event_code_to_name(*(counters->events+i),EventName);
			  retval = PAPI_get_event_info(*(counters->events+i),&evinfo);
			  if (retval != PAPI_OK) {
			    cerr<<"\n\t Error getting event info\n";
			    exit(-1);
			  }
			  strncpy(units[i],evinfo.units,PAPI_MIN_STR_LEN);
			  fprintf(outFile,"\t %s",EventName);
			}
			fprintf(outFile,"\n\n");
				
 //       for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit) {
 //           PAPIInst* imageData = AllData->GetData(*iit, pthread_self());

 //           uint64_t* loopHashes = imageData->loopHashes;
 //           uint64_t loopCount = imageData->loopCount;

 //           for (uint64_t loopIndex = 0; loopIndex < loopCount; ++loopIndex){
 //               uint64_t loopHash = loopHashes[loopIndex];
 //               fprintf(outFile, "0x%llx\n", loopHash);
 //               for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); ++tit) {
 //                   counters = AllData->GetData(*iit, *tit);
 //                   fprintf(outFile, "\tThread: 0x%llx");
 // 	    //fprintf(outFile, "\tThread: 0x");
 //                   int hwc;
 // 	    double scaledValue;
 //                   for(hwc = 0; hwc < counters->num; ++hwc) {
 // 	    
 // 	      
 // 		if(strstr(units[hwc],"nJ"))
 //                     	{
 // 		  scaledValue=(double)( counters->accumValues[loopIndex][hwc] /(1.0e9) );
 // 		  fprintf(outFile,"\t%.4f",scaledValue);
 // 		}
 // 		else {
 // 		  
 // 		  fprintf(outFile,"\t%lld",counters->accumValues[loopIndex][hwc]);
 // 		} 
 // 	
 // 	    }
 //                   fprintf(outFile, "\n");
 //               }
 //           }
 //       }

			for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); ++iit) 
			{
		  	PAPIInst* imageData = AllData->GetData(*iit, pthread_self());
		
		    uint64_t* loopHashes = imageData->loopHashes;
		    uint64_t loopCount = imageData->loopCount;
		
		    for (uint64_t loopIndex = 0; loopIndex < loopCount; ++loopIndex)
				{
		      uint64_t loopHash = loopHashes[loopIndex];
		 //   fprintf(outFile, "0x%llx\n", loopHash);
		      for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); ++tit)
					{
		        counters = AllData->GetData(*iit, *tit);
				    
		   			//fprintf(outFile, "\tThread: 0x%llx\n");
				    //fprintf(outFile, "\tThread: 0x");
		        int hwc;
				    double scaledValue;
		        for(hwc = 0; hwc < counters->num; ++hwc)
						{
				      PAPI_event_code_to_name(*(counters->events+hwc),EventName);
				      retval = PAPI_get_event_info(*(counters->events+hwc),&evinfo);
				      if (retval != PAPI_OK)
							{
								cerr<<"\n\t Error getting event info\n";
								exit(-1);
				      }
				      
							if(strstr(units[hwc],"nJ"))
		          {
					 			scaledValue=(double)( counters->accumValues[loopIndex][hwc] /(1.0e9) );
					  		fprintf(outFile, "@@ %s\t0x%llx", counters->application, loopHash);
					  		fprintf(outFile,"\t %s",EventName);
					  		fprintf(outFile,"\t%.4f \n",scaledValue);
		
					  		// calculate the watts
					  		double watts=scaledValue/((double)(counters->loopTimerAccum[loopIndex]) / CLOCK_RATE_HZ);
					  		fprintf(outFile, "@@ %s\t0x%llx", counters->application, loopHash);
					  		fprintf(outFile,"\t %s_watts",EventName);
					  		fprintf(outFile,"\t%.4f \n",watts);
					  		//fprintf(outFile,"\t%.4f",scaledValue);
							}
							else
							{
					  		fprintf(outFile, "@@ %s\t0x%llx", counters->application, loopHash);
					  		fprintf(outFile,"\t %s",EventName);
					  		//printf("loopIndex: %d hwc: %d \n", loopIndex, hwc);
					  		fprintf(outFile,"\t%lld \n",counters->accumValues[loopIndex][hwc]);
							} 
				    }
				    fprintf(outFile, "0x%llx", loopHash);
				    fprintf(outFile, "\tTime: %f\n", *tit, (double)(counters->loopTimerAccum[loopIndex]) / CLOCK_RATE_HZ);
		        fprintf(outFile, "\n");
      		}
    		}
  		}

      fflush(outFile);
      fclose(outFile);
      return NULL;
    }
};
