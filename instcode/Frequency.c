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
#include <InstrumentationCommon.h>

#define UNUSED_PASSTHRU_VALUE 0xdeadbeef

// MUST DEFINE THIS!!!
#define MAX_CPU_IN_SYSTEM 4

uint32_t* site = NULL;
uint32_t numberOfLoops = 0;
uint32_t numberOfRanks = 0;
uint32_t* frequencies = NULL;
#define freq_idx(__l, __r) ((numberOfRanks * __l) + (__r))

uint32_t currentFreq = 0;
uint32_t lastFreq = 0;
uint64_t* entryCalled = NULL;
uint64_t* exitCalled = NULL;

// called at loop entry
void pfreq_throttle_lpentry(){
    entryCalled[*site]++;

    // return if mpi_init has not yet been called
    if (!isMpiValid()){
        return;
    }

                              
    uint32_t cpu = getTaskId();
    uint32_t f = frequencies[freq_idx(*site, cpu)];

    // return if no value was set up for this rank
    if (f == UNUSED_PASSTHRU_VALUE){
        return;
    }

    pfreq_throttle_set(cpu, f);
}

// called at loop exit
void pfreq_throttle_lpexit(){
    exitCalled[*site]++;

    if (!isMpiValid()){
        return;
    }

    uint32_t cpu = getTaskId();
    uint32_t f = frequencies[freq_idx(*site, cpu)];

    if (f == UNUSED_PASSTHRU_VALUE){
        return;
    }

    // this is quite unsophisticated, and will do the wrong thing if you want frequency swaps stack on each other
    pfreq_throttle_set(cpu, lastFreq);
}

// called at program start
void pfreq_throttle_init(uint32_t* siteIndex, uint32_t* numLoops, uint32_t* numRanks, uint32_t* freqArray){
    int i, j;

    site = siteIndex;
    numberOfLoops = *numLoops;
    numberOfRanks = *numRanks;
    frequencies = freqArray;

    entryCalled = malloc(sizeof(uint64_t) * numberOfLoops);
    exitCalled = malloc(sizeof(uint64_t) * numberOfLoops);

    /*
    for (i = 0; i < numberOfLoops; i++){
        for (j = 0; j < numberOfRanks; j++){
            if (frequencies[freq_idx(i,j)] != freq_idx(i,j)){
                PRINT_INSTR(stdout, "freq[%d][%d] == %d", i, j, frequencies[freq_idx(i,j)]);
                exit(1);
            }
        }
    }
    */
    /*
    PRINT_INSTR(stdout, "initializing throttle lib with %d loops and %d ranks", numberOfLoops, numberOfRanks);
    for (i = 0; i < numberOfLoops; i++){
        for (j = 0; j < numberOfRanks; j++){
            if (frequencies[freq_idx(i, j)]){
                PRINT_INSTR(stdout, "\tf[%d][%d] = %d", i, j, frequencies[freq_idx(i, j)]);
            }
        }
    }
    */
}

// called at program finish
void pfreq_throttle_fini(){
    int i;
    for (i = 0; i < numberOfLoops; i++){
        if (entryCalled[i] != exitCalled[i]){
            PRINT_INSTR(stderr, "site %d: entry called %lld times but exit %lld", i, entryCalled[i], exitCalled[i]);
            exit(1);
        }
    }
    free(entryCalled);
    free(exitCalled);
}

// called at mpi_init
void tool_mpi_init(){
    pfreq_affinity_get();
    pfreq_affinity_set(getTaskId());
}

inline int32_t pfreq_throttle_set(uint32_t cpu, uint32_t freq){
    //PRINT_INSTR(stdout, "throttling task %d to frequency %dKHz", cpu, freq);
    lastFreq = currentFreq;
    currentFreq = freq;
    return internal_set_currentfreq((unsigned int)cpu, (int)freq);
}

unsigned long pfreq_throttle_get(){
    return currentFreq;
}

inline int32_t internal_set_currentfreq(unsigned int cpu, int freq){
    //return cpufreq_set_frequency((unsigned int)cpu, (int)freq);
}

int32_t pfreq_affinity_get(){
    int32_t retCode = 0;
    cpu_set_t cpuset;
    int32_t i;
    
    if (sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset) < 0) {
        retCode = -1;
    } else {
        for(i = 0; i < MAX_CPU_IN_SYSTEM; i++){
            if (CPU_ISSET(i, &cpuset)){
                retCode = i;
                break;
            }
        }
    }
    return retCode;
}

int32_t pfreq_affinity_set(uint32_t cpu){
    int32_t retCode = 0;
    cpu_set_t cpuset;
    
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    
    if(sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset) < 0) {
        retCode = -1;
    }
    PRINT_INSTR(stdout, "Setting affitinity to cpu%u for caller task pid %d (retcode %d)", cpu, getpid(), retCode);
    
    return retCode;
}
