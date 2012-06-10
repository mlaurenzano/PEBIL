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

#include <InstrumentationCommon.hpp>
#define _GNU_SOURCE
#include <dlfcn.h>

int taskid;
#ifdef HAVE_MPI
#define __taskid taskid
#define __ntasks ntasks
int __ntasks = 1;
#else //HAVE_MPI
#define __taskid getpid()
#define __ntasks 1
#endif //HAVE_MPI

uint32_t mpiValid = 0;
uint32_t isMpiValid() { return mpiValid; }
void setMpiValid(int a) { mpiValid = a; }

inline uint64_t read_timestamp_counter(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}

inline double read_process_clock(){
    struct timespec myclock;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &myclock);
    uint64_t nsec = (NANOS_PER_SECOND * myclock.tv_sec) + myclock.tv_nsec;
    return (double)(nsec) / (double)(NANOS_PER_SECOND);
}

void ptimer(double *tmr) {
    struct timeval timestr;
    void *tzp=0;

    gettimeofday(&timestr, (struct timezone*)tzp);
    *tmr=(double)timestr.tv_sec + 1.0E-06*(double)timestr.tv_usec;
}

// List of data entries keyed by image_ids
struct data_list {
    uint64_t image_id;
    void * data;
    struct data_list * next;
};

// List of thread entries keyed by thread_ids
struct thread_list {
    pthread_t thread_id;
    struct data_list * datas;
    struct thread_list * next;
};

extern "C" {
    // called when pthread_create is called for programs instrumented without the --threaded flag
    int pthread_create_pebil_nothread(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg){
        PRINT_INSTR(stderr, "Application was not instrumented by PEBIL without thread support but pthread_create has been called.");
        PRINT_INSTR(stderr, "Results should not be considered reliable.");
        fflush(stderr);
        return pthread_create(thread, attr, start_routine, arg);
    }

    // called when pthread_create is called for programs instrumented with the --threaded flag
    int __give_pebil_name(pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg){
        tool_thread_args* x = (tool_thread_args*)malloc(sizeof(tool_thread_args));
        x->start_function = start_routine;
        x->function_args = arg;
        int ret = pthread_create(thread, attr, tool_thread_init, x);
        return ret;
    }
};

#ifdef HAVE_MPI
// C init wrapper
#ifdef USES_PSINSTRACER
int __give_pebil_name(MPI_Init)(int* argc, char*** argv){
    int retval = 0;
#else
int __wrapper_name(MPI_Init)(int* argc, char*** argv){
    int retval = PMPI_Init(argc, argv);
#endif // USES_PSINSTRACER

    PMPI_Comm_rank(MPI_COMM_WORLD, &__taskid);
    PMPI_Comm_size(MPI_COMM_WORLD, &__ntasks);

    mpiValid = 1;

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d on host %u in MPI_Init wrapper\n", getpid(), __taskid, __ntasks, gethostid());
    tool_mpi_init();

    return retval;
}

// fortran init wrapper
extern void pmpi_init_(int* ierr);
extern void pmpi_comm_rank_(int* comm, int* rank, int* ierr);
extern void pmpi_comm_size_(int* comm, int* rank, int* ierr);

#ifdef USES_PSINSTRACER
void__give_pebil_name(mpi_init_)(int* ierr){
#else
void __wrapper_name(mpi_init_)(int* ierr){
    pmpi_init_(ierr);
#endif // USES_PSINSTRACER

    PMPI_Comm_rank(MPI_COMM_WORLD, &__taskid);
    PMPI_Comm_size(MPI_COMM_WORLD, &__ntasks);

    mpiValid = 1;

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d on host %u in mpi_init_ wrapper\n", getpid(), __taskid, __ntasks, gethostid());
    tool_mpi_init();
}

#endif // HAVE_MPI

int getTaskId(){
    return __taskid;
}
int getNTasks(){
    return __ntasks;
}



