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

#define __MAX_STRING_SIZE 1024

#define CLOCK_RATE_HZ 2800000000
#define NANOS_PER_SECOND 1000000000
//#define EXCLUDE_TIMER

extern uint64_t read_timestamp_counter();
extern double read_process_clock();
extern void ptimer();

#define MAX_TIMERS 1024
double pebiltimers[MAX_TIMERS];

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

typedef struct
{
    void* (*start_function)(void*);
    void* function_args;
} tool_thread_args;
extern int __give_pebil_name(pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
extern int pthread_create_pebil_nothread(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
extern void * tool_thread_init(tool_thread_args * threadargs);

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

#endif // __INSTRUMENTATION_COMMON_H__

