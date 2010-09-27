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

#define __MAX_STRING_SIZE 1024
#define CLOCK_RATE_HZ 2270000000

//#define EXCLUDE_TIMER

inline unsigned long long readtsc(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}

inline uint64_t read_process_clock(){
    struct timespec myclock;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &myclock);
    return (1000000*myclock.tv_sec) + myclock.tv_nsec;
}

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


int taskid;
#ifdef HAVE_MPI
#define __taskid taskid
#define __ntasks ntasks
#define __taskmarker "-[t%d]- "

#include <mpi.h>

int __ntasks = 1;

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

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d on host %u in MPI_Init wrapper\n", getpid(), __taskid, __ntasks, gethostid());

    return retval;
}

// fortran init wrapper
extern void pmpi_init_(int* ierr);
extern void pmpi_comm_rank_(int* comm, int* rank, int* ierr);
extern void pmpi_comm_size_(int* comm, int* rank, int* ierr);

#ifndef USES_PSINSTRACER
void __wrapper_name(mpi_init_)(int* ierr){
    pmpi_init_(&ierr);

    int myerr;
    MPI_Comm world = MPI_COMM_WORLD;
    pmpi_comm_rank_(&world, &__taskid, &myerr);
    pmpi_comm_size_(&world, &__ntasks, &myerr);

    fprintf(stdout, "-[p%d]- remapping to taskid %d/%d on host %u in mpi_init_ wrapper\n", getpid(), __taskid, __ntasks, gethostid());
}
#endif // USES_PSINSTRACER

#else // HAVE_MPI
#define __taskid getpid()
#define __ntasks 1
#define __taskmarker "-[p%d]- "
#endif // HAVE_MPI

#define PRINT_INSTR(__file, ...) fprintf(__file, __taskmarker, __taskid);  \
    fprintf(__file, __VA_ARGS__); \
    fprintf(__file, "\n"); \
    fflush(__file);
#define PRINT_DEBUG(...) 
//#define PRINT_DEBUG(...) PRINT_INSTR(__VA_ARGS__)

#endif // __INSTRUMENTATION_COMMON_H__

