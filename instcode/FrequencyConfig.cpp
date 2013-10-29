
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/un.h>

#define _GNU_SOURCE
#include <sched.h>

#include <papi.h>
#define USE_PAPI 1
#include <cpufreq.h>

#include <iostream>

#define MAXMOD (32.0)
#define MAXIPC (8)

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
    retval->ipsMap = new float[retval->loopCount];

    memset(retval->frequencyMap, 0, sizeof(uint32_t) * retval->loopCount);
    memset(retval->ipsMap, 0, sizeof(float) * retval->loopCount);
    return retval;
}

void DeleteFrequencyConfig(FrequencyConfig* fconfig){
    delete fconfig->frequencyMap;
    delete fconfig->ipsMap;
}

uint64_t ReferenceFrequencyConfig(FrequencyConfig* fconfig){
    return (uint64_t)fconfig;
}

extern "C"
{

    static unsigned long get_mod(int cpu) {
        char msrfile[16];
        sprintf(msrfile, "/dev/cpu/%d/msr", cpu);
        int fd = open(msrfile, O_RDONLY);
        lseek(fd,0x19a,SEEK_SET);
        unsigned long cur;
        read(fd, &cur, sizeof cur);
        close(fd);
        return cur;
    }

    static unsigned long set_mod(int cpu, unsigned long mod) {
        char msrfile[16];
        sprintf(msrfile,"/dev/cpu/%d/msr",cpu);
        int fd = open(msrfile,O_WRONLY);
        lseek(fd,0x19a,SEEK_SET);
        write(fd, &mod, sizeof mod);
        close(fd);
    }

    static float get_ipc() {
    #ifdef USE_PAPI
      float rtime, ptime, ipc;
      long long ins;
      int retval;
    
      if((retval=PAPI_ipc(&rtime,&ptime,&ins,&ipc)) < PAPI_OK)
      { 
        printf("IPC error: %d\n", retval);
        exit(1);
      }
      return ipc;
    #else
      return 0;
    #endif
    }

    // start timer
    int32_t loop_entry(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        FrequencyConfig* fconfig = AllData->GetData(*key, pthread_self());
        assert(fconfig != NULL);

        unsigned long cur;

        if(fconfig->frequencyMap[loopIndex]) {
          if(fconfig->frequencyMap[loopIndex]>32) {
            cur = cpufreq_get_freq_kernel(fconfig->cpu);
            if(cur!=fconfig->frequencyMap[loopIndex]) {
              fprintf(stderr,"Changing frequency entering loop %u to %u\n", loopIndex, fconfig->frequencyMap[loopIndex]);
              cpufreq_set_frequency(fconfig->cpu, fconfig->frequencyMap[loopIndex]);
            }
            else
              fprintf(stderr,"Frequency for loop %u equal to current frequency\n", loopIndex);
          }
          else {
            cur = get_mod(fconfig->cpu);
            if(((cur&0x10) && cur!=fconfig->frequencyMap[loopIndex]) || ((cur&0x10)==0 && fconfig->frequencyMap[loopIndex]!=32)) {
              fprintf(stderr,"Changing modulation entering loop %u to %u\n", loopIndex, fconfig->frequencyMap[loopIndex]&0x1F);
              set_mod(fconfig->cpu,fconfig->frequencyMap[loopIndex]&0x1F);
            }
            else
              fprintf(stderr,"Modulation for loop %u equal to current modulation\n", loopIndex);
          }
          get_ipc();
        }

        return 0;
    }

    // end timer
    int32_t loop_exit(uint32_t loopIndex, image_key_t* key) {
        thread_key_t tid = pthread_self();

        FrequencyConfig* fconfig = AllData->GetData(*key, pthread_self());
        assert(fconfig != NULL);

        if(fconfig->frequencyMap[loopIndex]) {
          float ips = get_ipc();
          if(fconfig->frequencyMap[loopIndex]>MAXMOD) {
            ips *= fconfig->frequencyMap[loopIndex];
            float mips = fconfig->ipsMap[loopIndex];
            fprintf(stderr,"Comparing measured ips %.2f with expected ips %.2f: diff=%.2f%%\n", ips, mips, (mips-ips)/mips*100);
            if(ips<mips) {
                fprintf(stderr,"Disabled throttling for loop %d\n", loopIndex);
                fconfig->frequencyMap[loopIndex] = 0;
            }
            else
                fprintf(stderr,"No slowdown detected\n");
          }
          else {
            ips *= fconfig->frequencyMap[loopIndex];
            float mips = fconfig->ipsMap[loopIndex];
            fprintf(stderr,"Comparing measured ips %.2f with expected ips %.2f: diff=%.2f%%\n", ips, mips, (mips-ips)/mips*100);
            if(ips<mips) {
                fprintf(stderr,"Disabled modulation for loop %d\n", loopIndex);
                fconfig->frequencyMap[loopIndex] = 0;
            }
            else
                fprintf(stderr,"No slowdown detected\n");
          }
        }

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

        fconfig = AllData->GetData(*key, pthread_self());

        unsigned long min,max;
        unsigned int mycpu = GetTaskId()%sysconf(_SC_NPROCESSORS_ONLN);
        cpufreq_get_hardware_limits(mycpu, &min, &max);

        const char* filename = getenv("PMAC_FREQ_FILE") ? getenv("PMAC_FREQ_FILE") : "loops.freq";
        FILE* freq_file = fopen(filename,"r");
        if(freq_file==NULL)
          fprintf(stderr,"Warning: no frequency file found\n");
        else {
          unsigned int lid = 0;
          int ret;
          do {
            unsigned long freq; float ipc;
            ret = fscanf(freq_file, "%lu %f\n", &freq, &ipc);
            if(ret==2) {
              fconfig->frequencyMap[lid] = freq;
              if(freq>MAXMOD)
                  fconfig->ipsMap[lid] = ipc*max;
              else
                  fconfig->ipsMap[lid] = ipc*MAXMOD;
            }
            ++lid;
          }while(ret==2 && lid<fconfig->loopCount);
          fclose(freq_file);
        }
        fconfig->cpu = mycpu;
        cpu_set_t mask;
        CPU_ZERO(&mask); CPU_SET(mycpu,&mask);
        sched_setaffinity(0,sizeof(mask),&mask);

        return NULL;
    }

    // Nothing to do here
    void* tool_image_fini(image_key_t* key) {
        return NULL;
    }
};
