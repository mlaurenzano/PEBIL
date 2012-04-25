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

#define  _GNU_SOURCE

#include <sched.h>
#include <errno.h>

#include <InstrumentationCommon.h>

/* Public functions */
void inst_lptimer_lpentry();
void inst_lptimer_lpexit();
void inst_lptimer_init(uint32_t* s, uint32_t* numLoops, uint64_t* loopHashes, void* instpoints, int32_t* numpoints);
void inst_lptimer_fini();
void tool_mpi_init();

// Pointer to the current loop
static uint32_t* site = NULL;
static uint32_t numberOfLoops = 0;
static uint64_t* loopHashCodes = NULL;
static uint32_t* loopStatus = NULL;  // keeps track of whether we are in loop

static uint64_t * loopTimes;
static uint64_t * loopTimers;

// number of times we enter or exit loops
static uint64_t* entryCalled = NULL;
static uint64_t* exitCalled = NULL;

static long nprocessors;

//#define DEBUG
#ifdef DEBUG
#define PRINT_DEBUG(...) PRINT_INSTR(__VA_ARGS__)
#define DEBUG(...) __VA_ARGS__
#else
#define PRINT_DEBUG(...) 
#define DEBUG(...)
#endif

#define ENABLE_INSTRUMENTATION_KILL
static void* instrumentationPoints;
static int32_t numberOfInstrumentationPoints;
static int32_t numberKilled;

#ifdef ENABLE_INSTRUMENTATION_KILL
static void disableInstrumentationPointsInBlock(int32_t blockId){
    instpoint_info* ip;
    int i;

    PRINT_INSTR(stdout, "disabling points for site %d (loop hash %#llx)", blockId, loopHashCodes[blockId]);

    int32_t killedPoints = 0;
    for (i = 0; i < numberOfInstrumentationPoints; i++){
        ip = (instpoint_info*)(instrumentationPoints + (i * sizeof(instpoint_info)));
        if (ip->pt_blockid == blockId){
            int32_t size = ip->pt_size;
            int64_t vaddr = ip->pt_vaddr;
            char* program_point = (char*)vaddr;

            PRINT_INSTR(stdout, "\tkilling instrumentation for point %d in block %d at %#llx", i, blockId, vaddr);

            memcpy(ip->pt_content, program_point, size);
            memcpy(program_point, ip->pt_disable, size);
            killedPoints++;
        }
    }

    numberKilled += killedPoints;
    PRINT_INSTR(stdout, "Killing instrumentation points for block %d (%d points of %d total) -- %d killed so far", blockId, killedPoints, numberOfInstrumentationPoints, numberKilled);
}
#endif //ENABLE_INSTRUMENTATION_KILL

// pin process to core
static int32_t pinto(uint32_t cpu){
    int32_t retCode = 0;
    cpu_set_t cpuset;
    
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    if(sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset) < 0) {
        retCode = -1;
    }
    PRINT_INSTR(stdout, "Setting affinity to cpu%u for caller task pid %d (retcode %d)", cpu, getpid(), retCode);
    
    return retCode;
}

// called at loop entry
void inst_lptimer_lpentry(){
    loopTimers[*site] = read_timestamp_counter();
    entryCalled[*site]++;
}

// called at loop exit
void inst_lptimer_lpexit(){
    double t2 = read_timestamp_counter();
    loopTimes[*site] += t2 - loopTimers[*site];
    exitCalled[*site]++;
}


// called at program start
void inst_lptimer_init(uint32_t* s, uint32_t* numLoops, uint64_t* loopHashes, void* instpoints, int32_t* numpoints){
    int i, j;


    site = s;
    numberOfLoops = *numLoops;
    loopHashCodes = loopHashes;

    instrumentationPoints = instpoints;
    numberOfInstrumentationPoints = *numpoints;
    numberKilled = 0;

    entryCalled = malloc(sizeof(uint64_t) * numberOfLoops);
    bzero(entryCalled, sizeof(uint64_t) * numberOfLoops);

    exitCalled = malloc(sizeof(uint64_t) * numberOfLoops);
    bzero(exitCalled, sizeof(uint64_t) * numberOfLoops);

    loopStatus = malloc(sizeof(uint32_t) * numberOfLoops);
    bzero(loopStatus, sizeof(uint32_t) * numberOfLoops);

    loopTimes = malloc(sizeof(uint64_t) * numberOfLoops);
    bzero(loopTimes, sizeof(uint64_t) * numberOfLoops);

    loopTimers = malloc(sizeof(uint64_t) * numberOfLoops);
    bzero(loopTimers, sizeof(uint64_t) * numberOfLoops);
}

static void write_lptimes(FILE * outfp) {
    int i;
    for( i = 0; i < numberOfLoops; ++i ) {
        fprintf(outfp, "%llu\t%llu\n", loopHashCodes[i], loopTimes[i]);
    }
}

// called at program finish
void inst_lptimer_fini(){
    int i;
    int e = 0;

    // verify that loop entry counts == exit counts
    for (i = 0; i < numberOfLoops; i++){
        if (entryCalled[i] != exitCalled[i]){
            PRINT_INSTR(stderr, "site %d: entry called %lld times but exit %lld", i, entryCalled[i], exitCalled[i]);
            e++;
        }
    }


    char filename[1024];
    sprintf(filename, "meta_%04d.lptimerinst", getTaskId());
    FILE * outfp = fopen(filename, "w");
    write_lptimes(outfp);
    fclose(outfp);

    free(entryCalled);
    free(exitCalled);
    free(loopStatus);
    free(loopTimes);
    free(loopTimers);


    if (e){
        exit(1);
    }
}

// called just after mpi_init
void tool_mpi_init(){
    nprocessors = sysconf(_SC_NPROCESSORS_ONLN);
    pinto(getTaskId() % nprocessors);
}


