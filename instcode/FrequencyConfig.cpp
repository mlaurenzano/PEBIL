
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
    retval->ipcMap = new float[retval->loopCount];

    memset(retval->frequencyMap, 0, sizeof(uint32_t) * retval->loopCount);
    memset(retval->ipcMap, 0, sizeof(float) * retval->loopCount);
    return retval;
}

void DeleteFrequencyConfig(FrequencyConfig* fconfig){
    delete fconfig->frequencyMap;
    delete fconfig->ipcMap;
}

uint64_t ReferenceFrequencyConfig(FrequencyConfig* fconfig){
    return (uint64_t)fconfig;
}

extern "C"
{

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

    struct throttler_msg {
        unsigned int cpu;
        unsigned long freq;
    };

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
              throttler_msg msg;
              msg.cpu = fconfig->cpu; msg.freq = fconfig->frequencyMap[loopIndex];
              send(fconfig->throttler, &msg, sizeof(msg), 0);
            }
            else
              fprintf(stderr,"Frequency for loop %u equal to current frequency\n", loopIndex);
          }
          else {
            char msrfile[16];
            sprintf(msrfile, "/dev/cpu/%d/msr", fconfig->cpu);
            int fd = open(msrfile, O_RDONLY);
            lseek(fd,0x19a,SEEK_SET);
            read(fd, &cur, sizeof cur);
            close(fd);
            if(((cur&0x10) && cur!=fconfig->frequencyMap[loopIndex]) || ((cur&0x10)==0 && fconfig->frequencyMap[loopIndex]!=32)) {
              fprintf(stderr,"Changing modulation entering loop %u to %u\n", loopIndex, fconfig->frequencyMap[loopIndex]&0x1F);
              throttler_msg msg;
              msg.cpu = fconfig->cpu; msg.freq = fconfig->frequencyMap[loopIndex]&0x1F;
              send(fconfig->throttler, &msg, sizeof(msg), 0);
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
          float ipc = get_ipc();
          if(fconfig->frequencyMap[loopIndex]>32) {
            ipc *= fconfig->frequencyMap[loopIndex];
            float mips = fconfig->ipcMap[loopIndex]*fconfig->maxFreq;
            fprintf(stderr,"Comparing measured ips %.2f with expected ips %.2f: diff=%.2f%%\n", ipc, mips, (mips-ipc)/mips*100);
            if(ipc<mips*0.95) {
                fprintf(stderr,"Disabled throttling for loop %d\n", loopIndex);
                fconfig->frequencyMap[loopIndex] = 0;
            }
            else
                fprintf(stderr,"Slowdown below 5%% threshold compared to native performance\n");
          }
          else if(fconfig->frequencyMap[loopIndex]&0x1F) {
            float mipc = fconfig->ipcMap[loopIndex];
            fprintf(stderr,"Comparing measured ipc %.2f with expected ipc %.2f: diff=%.2f%%\n", ipc, mipc, (mipc-ipc)/mipc*100);
            if(ipc<mipc*0.95) {
                fprintf(stderr,"Disabled modulation for loop %d\n", loopIndex);
                fconfig->frequencyMap[loopIndex] = 0;
            }
            else
                fprintf(stderr,"Slowdown below 5%% threshold compared to native performance\n");
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
              fconfig->ipcMap[lid] = ipc;
            }
            ++lid;
          }while(ret==2 && lid<fconfig->loopCount);
          fclose(freq_file);
        }
        filename = getenv("PMAC_THROTTLER_SOCKET") ? getenv("PMAC_THROTTLER_SOCKET") : "/var/run/throttler.socket";
        fconfig->throttler = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        struct sockaddr_un sockaddr;
        sockaddr.sun_family = AF_UNIX; strcpy(sockaddr.sun_path,filename);
        connect(fconfig->throttler,(const struct sockaddr *)&sockaddr, sizeof sockaddr);
        unsigned int mycpu = GetTaskId()%sysconf(_SC_NPROCESSORS_ONLN);
        fconfig->cpu = mycpu;
        cpu_set_t mask;
        CPU_ZERO(&mask); CPU_SET(mycpu,&mask);
        sched_setaffinity(0,sizeof(mask),&mask);
        unsigned long min;
        cpufreq_get_hardware_limits(mycpu, &min, &fconfig->maxFreq);
        return NULL;
    }

    // Nothing to do here
    void* tool_image_fini(image_key_t* key) {
        return NULL;
    }
};
