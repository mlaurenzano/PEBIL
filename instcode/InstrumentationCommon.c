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

#include <InstrumentationCommon.h>

int taskid;
#ifdef HAVE_MPI
#define __taskid taskid
#define __ntasks ntasks
int __ntasks = 1;
#else //HAVE_MPI
#define __taskid getpid()
#define __ntasks 1
#endif //HAVE_MPI

#ifdef MPI_INIT_REQUIRED
uint32_t taskValid = 0;
uint32_t isTaskValid() { return taskValid; }
#endif

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

    gettimeofday(&timestr, tzp);
    *tmr=(double)timestr.tv_sec + 1.0E-06*(double)timestr.tv_usec;
}


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

#ifdef MPI_INIT_REQUIRED
    taskValid = 1;
#endif
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

#ifdef MPI_INIT_REQUIRED
    taskValid = 1;
#endif
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
