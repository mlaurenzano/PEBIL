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

extern int __give_pebil_name(pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
extern int pthread_create_pebil_nothread(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);

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

#define DataMap unordered_map
template <class T = void*> class DataManager {
private:
    DataMap <pthread_key_t, DataMap<pthread_t, T> > datamap;
    void* (*datagen)(void*);
    set<pthread_t> allthreads;
    set<pthread_key_t> allimages;
public:

    DataManager(void* (*d)(void*)){
        datagen = d;
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
    void AddThread(T data){
    }

    pthread_key_t AddImage(T data){
        pthread_key_t iid = GenerateImageKey();
        pthread_t tid = pthread_self();

        assert(allimages.count(iid) == 0);

        // insert data for this thread
        allthreads.insert(tid);
        allimages.insert(iid);
        datamap[iid] = DataMap<pthread_t, T>();
        datamap[iid][tid] = data;

        // create/insert data every other thread
        for (set<pthread_t>::iterator it = allthreads.begin(); it != allthreads.end(); it++){
            if ((*it) != tid){
                datamap[iid][(*it)] = (T)datagen((void*)data);
            }
        }
        return iid;
    }

    T GetData(pthread_t tid, pthread_key_t iid){
        if (datamap.count(iid) != 0 && datamap[iid].count(tid) != 0){
            return datamap[iid][tid];
        }
        return NULL;
    }
};

#endif // __INSTRUMENTATION_COMMON_H__

