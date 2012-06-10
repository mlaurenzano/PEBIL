/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __INSTRUMENTATION_COMMON_H__
#define __INSTRUMENTATION_COMMON_H__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <unordered_map>
#include <map>
#include <set>

using namespace std;

#define __MAX_STRING_SIZE 1024

#define CLOCK_RATE_HZ 2800000000
#define NANOS_PER_SECOND 1000000000
//#define EXCLUDE_TIMER

extern uint64_t read_timestamp_counter();
extern double read_process_clock();
extern void ptimer(double* tmr);

#define MAX_TIMERS 1024
static double pebiltimers[MAX_TIMERS];

typedef struct
{
    int64_t pt_vaddr;
    int64_t pt_target;
    int64_t pt_flags;
    int32_t pt_size;
    int32_t pt_blockid;
    unsigned char pt_content[16];
    unsigned char pt_disable[16];
} instpoint_info;


#define __give_pebil_name(__fname) \
    __fname ## _pebil_wrapper

#ifdef PRELOAD_WRAPPERS
#define __wrapper_name(__fname) \
    __fname
#else // PRELOAD_WRAPPERS
#define __wrapper_name(__fname) \
    __give_pebil_name(__fname)
#endif // PRELOAD_WRAPPERS

extern "C" {
    extern int __give_pebil_name(pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
    extern int pthread_create_pebil_nothread(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
};

typedef struct
{
    void* (*start_function)(void*);
    void* function_args;
} tool_thread_args;
extern void pebil_image_init(pthread_key_t* image_id);
extern void pebil_set_data(pthread_key_t image_id, void* data);
extern void* pebil_get_data(pthread_t thread_id, pthread_key_t image_id);
extern void* pebil_get_data_self(pthread_key_t image_id);

extern void* tool_thread_init(void* args);
extern void* tool_mpi_init();

#ifdef HAVE_MPI
#define __taskmarker "-[t%d]- "
#include <mpi.h>

// C init wrapper
#ifdef USES_PSINSTRACER
extern int __give_pebil_name(MPI_Init)(int* argc, char*** argv);
#else
extern int __wrapper_name(MPI_Init)(int* argc, char*** argv);
#endif // USES_PSINSTRACER

#ifdef USES_PSINSTRACER
extern void __give_pebil_name(mpi_init_)(int* ierr);
#else
extern void __wrapper_name(mpi_init_)(int* ierr);
#endif

#else // HAVE_MPI
#define __taskmarker "-[p%d]- "
#endif // HAVE_MPI

extern int getTaskId();
extern int getNTasks();

#define PRINT_INSTR(__file, ...) fprintf(__file, __taskmarker, getTaskId()); \
    fprintf(__file, __VA_ARGS__); \
    fprintf(__file, "\n"); \
    fflush(__file);
#define PRINT_DEBUG(...) 
//#define PRINT_DEBUG(...) PRINT_INSTR(__VA_ARGS__)

#ifdef STATS_PER_INSTRUCTION
#define USES_STATS_PER_INSTRUCTION "yes"
#else
#define USES_STATS_PER_INSTRUCTION "no"
#endif

typedef struct {
    uint64_t id;
    uint64_t data;
} ThreadData;
#define ThreadHashShift (12)
#define ThreadHashAnd   (0xffff)

#define DataMap unordered_map
template <class T = void*> class DataManager {
private:
    DataMap <pthread_key_t, DataMap<pthread_t, T> > datamap;
    void* (*datagen)(void*, uint32_t, pthread_key_t, pthread_t);
    void (*datadel)(void*);
    uint64_t (*dataref)(void*);

    DataMap <pthread_key_t, DataMap<uint32_t, double> > timers;

    // stores data in a ThreadData[] which can be more easily accessed by tools.
    DataMap <pthread_key_t, ThreadData*> threaddata;

    uint32_t HashThread(pthread_t tid){
        return (tid >> ThreadHashShift) & ThreadHashAnd;
    }

    uint64_t SetThreadData(pthread_key_t iid, pthread_t tid){
        uint32_t h = HashThread(tid);

        assert(threaddata.count(iid) == 1);
        assert(datamap.count(iid) == 1);
        assert(datamap[iid].count(tid) == 1);

        ThreadData* td = threaddata[iid];

        uint32_t actual = h;
        while (td[actual].id != 0){
            actual = (actual + 1) % (ThreadHashAnd + 1);
        }
        td[actual].id = tid;
        T d = datamap[iid][tid];
        td[actual].data = (uint64_t)dataref((void*)d);

        PRINT_INSTR(stdout, "Setting up thread %#lx data at %#lx", td[actual].id, td[actual].data); 

        // fail if there was a collision. it makes writing tools much easier so we see how well this works for now
        assert(actual == h);
    }
    void RemoveThreadData(pthread_key_t iid, pthread_t tid){
        uint32_t h = HashThread(tid);

        assert(threaddata.count(iid) == 1);
        assert(datamap.count(iid) == 1);
        assert(datamap[iid].count(tid) == 1);

        ThreadData* td = threaddata[iid];

        uint32_t actual = h;
        while (td[actual].id != tid){
            actual = (actual + 1) % (ThreadHashAnd + 1);
        }
        td[actual].id = 0;
        td[actual].data = 0;
    }

public:

    set<pthread_t> allthreads;
    set<pthread_key_t> allimages;

    static const uint32_t ThreadType = 0;
    static const uint32_t ImageType = 1;

    DataManager(void* (*g)(void*, uint32_t, pthread_key_t, pthread_t), void (*d)(void*), uint64_t (*r)(void*)){
        datagen = g;
        datadel = d;
        dataref = r;
    }

    ~DataManager(){
    }

    pthread_key_t GenerateImageKey(){
        pthread_key_t ret;
        pthread_key_create(&ret, NULL);
        return ret;
    }

    void DeleteImageKey(pthread_key_t k){
        pthread_key_delete(k);
    }

    // these can only be called correctly by the current thread
    pthread_t GenerateThreadKey(){
        return pthread_self();
    }
    void AddThread(){
        pthread_t tid = pthread_self();
        assert(allthreads.count(tid) == 0);

        for (set<pthread_key_t>::iterator iit = allimages.begin(); iit != allimages.end(); iit++){
            assert(datamap[(*iit)].size() > 0);
            assert(datamap[(*iit)].count(tid) == 0);
            
            for (set<pthread_t>::iterator tit = allthreads.begin(); tit != allthreads.end(); tit++){
	        datamap[(*iit)][tid] = (T)datagen((void*)datamap[(*iit)][(*tit)], ThreadType, (*iit), tid);
                assert(datamap[(*iit)][tid]);
                break;
            }

            assert(threaddata.count(*iit) == 1);
            SetThreadData((*iit), tid);
        }
        allthreads.insert(tid);
    }

    void RemoveData(pthread_key_t iid, pthread_t tid){
        assert(datamap.count(iid) == 1);
        assert(datamap[iid].count(tid) == 1);

        T data = datamap[iid][tid];
        datadel(data);
        datamap[iid].erase(tid);
    }

    void RemoveThread(){
        assert(false);
        pthread_t tid = pthread_self();
        assert(allthreads.count(tid) == 1);

        for (set<pthread_key_t>::iterator iit = allimages.begin(); iit != allimages.end(); iit++){
            assert(datamap[(*iit)].size() > 0);
            assert(datamap[(*iit)].count(tid) == 1);
            RemoveData((*iit), tid);
            RemoveThreadData((*iit), tid);
        }
        allthreads.erase(tid);
    }

    void SetTimer(pthread_key_t iid, uint32_t idx){
        double t;
        ptimer(&t);

        if (timers.count(iid) == 0){
            timers[iid] = DataMap<uint32_t, double>();
        }
        timers[iid][idx] = t;
    }

    double GetTimer(pthread_key_t iid, uint32_t idx){
        return timers[iid][idx];
    }

    pthread_key_t AddImage(T data, ThreadData* t){
        pthread_key_t iid = GenerateImageKey();
        pthread_t tid = pthread_self();

        assert(allimages.count(iid) == 0);

        // insert data for this thread
        allthreads.insert(tid);
        allimages.insert(iid);
        datamap[iid] = DataMap<pthread_t, T>();
        datamap[iid][tid] = data;

        threaddata[iid] = t;
        SetThreadData(iid, tid);

        // create/insert data every other thread
        for (set<pthread_t>::iterator it = allthreads.begin(); it != allthreads.end(); it++){
            if ((*it) != tid){
                datamap[iid][(*it)] = (T)datagen((void*)data, ImageType, iid, (*it));
            }
        }
        return iid;
    }

    void RemoveImage(pthread_key_t iid){
        assert(allimages.count(iid) == 1);
        assert(datamap.count(iid) == 1);

        for (set<pthread_t>::iterator it = allthreads.begin(); it != allthreads.end(); it++){
            assert(datamap[iid].count((*it)) == 1);
            RemoveData(iid, (*it));
        }
        allimages.erase(iid);
        threaddata.erase(iid);
    }

    T GetData(pthread_key_t iid, pthread_t tid){
        assert(datamap.count(iid) == 1);
        assert(datamap[iid].count(tid) == 1);
        return datamap[iid][tid];
    }

    uint32_t CountThreads(){
        return allthreads.size();
    }
    uint32_t CountImages(){
        return allimages.size();
    }
};

#endif // __INSTRUMENTATION_COMMON_H__

