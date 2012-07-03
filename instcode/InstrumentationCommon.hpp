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

#ifndef _InstrumentationCommon_hpp_
#define _InstrumentationCommon_hpp_
#define _GNU_SOURCE
#include <dlfcn.h>

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
#include <string>
#include <iostream>
#include <fstream>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

using namespace std;


// thread id support
typedef struct {
    uint64_t id;
    uint64_t data;
} ThreadData;
#define ThreadHashShift (12)
#define ThreadHashAnd   (0xffff)


// handling of different initialization/finalization events
// analysis libraries define these differently
extern "C" {
    extern void* tool_mpi_init();
    extern void* tool_thread_init(pthread_t args);
    extern void* tool_image_init(void* s, uint64_t* key, ThreadData* td);
    extern void* tool_image_fini(uint64_t* key);
};


// information about instrumentation points
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


// some function re-naming support
#define __give_pebil_name(__fname) \
    __fname ## _pebil_wrapper

#ifdef PRELOAD_WRAPPERS
#define __wrapper_name(__fname) \
    __fname
#else // PRELOAD_WRAPPERS
#define __wrapper_name(__fname) \
    __give_pebil_name(__fname)
#endif // PRELOAD_WRAPPERS



// handle rank/process identification with/without MPI
static int taskid;
#ifdef HAVE_MPI
#define __taskid taskid
#define __ntasks ntasks
static int __ntasks = 1;
#else //HAVE_MPI
#define __taskid getpid()
#define __ntasks 1
#endif //HAVE_MPI

static int GetTaskId(){
    return __taskid;
}
static int GetNTasks(){
    return __ntasks;
}


// a timer
static void ptimer(double *tmr) {
    struct timeval timestr;
    void *tzp=0;

    gettimeofday(&timestr, (struct timezone*)tzp);
    *tmr=(double)timestr.tv_sec + 1.0E-06*(double)timestr.tv_usec;
}

// thread handling

extern "C" {
    static int __give_pebil_name(clone)(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
        va_list ap;
        va_start(ap, arg);
        pid_t* ptid = va_arg(ap, pid_t*);
        struct user_desc* tls = va_arg(ap, struct user_desc*);
        pid_t* ctid = va_arg(ap, pid_t*);
        va_end(ap);
        /*
        printf("Entry function: 0x%llx\n", fn);
        printf("Stack location: 0x%llx\n", child_stack);
        printf("Flags: %d\n", flags);
        printf("Function args: 0x%llx\n", arg);
        printf("ptid address: 0x%llx\n", ptid);
        printf("tls address: 0x%llx\n", tls);
        printf("ctid address: 0x%llx\n", ctid);
        */    
        static int (*clone_ptr)(int (*fn)(void*), void* child_stack, int flags, void* arg, pid_t *ptid, struct user_desc *tls, pid_t *ctid)
            = (int (*)(int (*fn)(void*), void* child_stack, int flags, void* arg, pid_t *ptid, struct user_desc *tls, pid_t *ctid))dlsym(RTLD_NEXT, "clone");

        tool_thread_init((uint64_t)tls);

        return clone_ptr(fn, child_stack, flags, arg, ptid, tls, ctid);
    }

    int __clone(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
        va_list ap;
        va_start(ap, arg);
        pid_t* ptid = va_arg(ap, pid_t*);
        struct user_desc* tls = va_arg(ap, struct user_desc*);
        pid_t* ctid = va_arg(ap, pid_t*);
        va_end(ap);
        return __give_pebil_name(clone)(fn, child_stack, flags, arg, ptid, tls, ctid);
    }

    int clone(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
        va_list ap;
        va_start(ap, arg);
        pid_t* ptid = va_arg(ap, pid_t*);
        struct user_desc* tls = va_arg(ap, struct user_desc*);
        pid_t* ctid = va_arg(ap, pid_t*);
        va_end(ap);
        return __give_pebil_name(clone)(fn, child_stack, flags, arg, ptid, tls, ctid);
    }

    int __clone2(int (*fn)(void*), void* child_stack, int flags, void* arg, ...){
        va_list ap;
        va_start(ap, arg);
        pid_t* ptid = va_arg(ap, pid_t*);
        struct user_desc* tls = va_arg(ap, struct user_desc*);
        pid_t* ctid = va_arg(ap, pid_t*);
        va_end(ap);
        return __give_pebil_name(clone)(fn, child_stack, flags, arg, ptid, tls, ctid);
    }
};


// support for output/warnings/errors
#define METASIM_ID "Metasim"
#define METASIM_VERSION "3.0.0"
#define METASIM_ENV "PEBIL_ROOT"

#define TAB "\t"
#define ENDL "\n"
#define DISPLAY_ERROR cerr << "[" << METASIM_ID << "-r" << GetTaskId() << "] " << "Error: "
#define warn cerr << "[" << METASIM_ID << "-r" << GetTaskId() << "] " << "Warning: "
#define ErrorExit(__msg, __errno) DISPLAY_ERROR << __msg << endl << flush; exit(__errno);
#define inform cout << "[" << METASIM_ID << "-r" << GetTaskId() << "] "

enum MetasimErrors {
    MetasimError_None = 0,
    MetasimError_MemoryAlloc,
    MetasimError_NoThread,
    MetasimError_TooManyInsnReads,
    MetasimError_StringParse,
    MetasimError_FileOp,
    MetasimError_Env,
    MetasimError_NoImage,
    MetasimError_Total,
};

static void TryOpen(ofstream& f, const char* name){
    f.open(name);
    f.setf(ios::showbase);
    if (f.fail()){
        ErrorExit("cannot open output file: " << name, MetasimError_FileOp);
    }
}


// some help geting task/process information into strings
static void AppendPidString(string& str){
    char buf[6];
    sprintf(buf, "%05d", getpid());
    buf[5] = '\0';

    str.append(buf);
}

static void AppendRankString(string& str){
    char buf[9];
    sprintf(buf, "%08d", GetTaskId());
    buf[8] = '\0';

    str.append(buf);
}


static void AppendTasksString(string& str){
    char buf[9];
    sprintf(buf, "%08d", GetNTasks());
    buf[8] = '\0';

    str.append(buf);
}



// data management support
#define DataMap unordered_map
template <class T = void*> class DataManager {
private:

    pthread_mutex_t mutex;

    DataMap <pthread_key_t, DataMap<pthread_t, T> > datamap;
    void* (*datagen)(void*, uint32_t, pthread_key_t, pthread_t);
    void (*datadel)(void*);
    uint64_t (*dataref)(void*);

    DataMap <pthread_key_t, DataMap<uint32_t, double> > timers;

    uint32_t currentthreadseq;
    DataMap <pthread_t, uint32_t> threadseq;

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

        inform
            << "Image " << hex << (uint64_t)iid
            << " setting up thread " << td[actual].id
            << " data at " << (uint64_t)td
            << "-> " << td[actual].data
            << endl;

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

        mutex = PTHREAD_MUTEX_INITIALIZER;
        currentthreadseq = 0;
        threadseq[GenerateThreadKey()] = currentthreadseq++;
    }

    ~DataManager(){
    }

    void TakeMutex(){
        pthread_mutex_unlock(&mutex);
    }

    void ReleaseMutex(){
        pthread_mutex_unlock(&mutex);
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

    uint32_t GetThreadSequence(pthread_t tid){
        assert(threadseq.count(tid) == 1);
        return threadseq[tid];
    }

    void AddThread(pthread_t tid){
        assert(allthreads.count(tid) == 0);
        assert(threadseq.count(tid) == 0);

        threadseq[tid] = currentthreadseq++;

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
    void AddThread(){
        AddThread(pthread_self());
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
        assert(timers.count(iid) == 1);
        assert(timers[iid].count(idx) == 1);
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


// support for MPI wrapping
static bool MpiValid = false;
static bool IsMpiValid() { return MpiValid; }

extern "C" {
#ifdef HAVE_MPI
// C init wrapper
#ifdef USES_PSINSTRACER
static int __give_pebil_name(MPI_Init)(int* argc, char*** argv){
    int retval = 0;
#else
static int __wrapper_name(MPI_Init)(int* argc, char*** argv){
    int retval = PMPI_Init(argc, argv);
#endif // USES_PSINSTRACER

    PMPI_Comm_rank(MPI_COMM_WORLD, &__taskid);
    PMPI_Comm_size(MPI_COMM_WORLD, &__ntasks);

    MpiValid = true;

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d on host %u in MPI_Init wrapper\n", getpid(), __taskid, __ntasks, gethostid());
    tool_mpi_init();

    return retval;
}

extern void pmpi_init_(int*);

#ifdef USES_PSINSTRACER
static void__give_pebil_name(mpi_init_)(int* ierr){
#else
static void __wrapper_name(mpi_init_)(int* ierr){
    pmpi_init_(ierr);
#endif // USES_PSINSTRACER

    PMPI_Comm_rank(MPI_COMM_WORLD, &__taskid);
    PMPI_Comm_size(MPI_COMM_WORLD, &__ntasks);

    MpiValid = true;

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d on host %u in mpi_init_ wrapper\n", getpid(), __taskid, __ntasks, gethostid());
    tool_mpi_init();
}
};
#endif // HAVE_MPI

#endif //_InstrumentationCommon_hpp_

