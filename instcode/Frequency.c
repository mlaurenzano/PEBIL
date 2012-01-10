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
#include <cpufreq.h>
#include <sched.h>
#include <InstrumentationCommon.h>

// MUST DEFINE THESE CORRECTLY!!!
#define MAX_CPU_IN_SYSTEM 8
#define MAX_FREQ 2133000

#define UNUSED_FREQ_VALUE 0xdeadbeef

uint32_t* site = NULL;
uint32_t numberOfLoops = 0;
uint64_t* loopHashCodes = NULL;
uint32_t* frequencyMap = NULL;
uint32_t* loopStatus = NULL;
uint32_t currentFreq = 0;
uint32_t lastFreq = 0;
uint64_t* entryCalled = NULL;
uint64_t* exitCalled = NULL;

double t1, t2;

//#define DEBUG
#ifdef DEBUG
#define PRINT_DEBUG(...) PRINT_INSTR(__VA_ARGS__)
#define DEBUG(...) __VA_ARGS__
#else
#define PRINT_DEBUG(...) 
#define DEBUG(...)
#endif

// called at loop entry
void pfreq_throttle_lpentry(){
    PRINT_DEBUG(stdout, "enter site %d", *site);
    DEBUG(ptimer(&t1));
    entryCalled[*site]++;

    // return if mpi_init has not yet been called
    if (!isMpiValid()){
        return;
    }

    // return if no frequency scheme is set for this loop
    int f = frequencyMap[*site];
    if (f == UNUSED_FREQ_VALUE){
        return;
    }

    if (loopStatus[*site] != 0){
        PRINT_INSTR(stderr, "loop entry scaling call w/o exit for site %d", *site);
    }
    loopStatus[*site] = 1;

    // set frequency
    int cpu = getTaskId();
    pfreq_throttle_set(cpu, f);
}

// called at loop exit
void pfreq_throttle_lpexit(){
    DEBUG(ptimer(&t2));
    PRINT_DEBUG(stdout, "exit site %d %f", *site, t2-t1);
    exitCalled[*site]++;

    if (!isMpiValid()){
        return;
    }

    // return if no frequency scheme is set for this loop
    int f = frequencyMap[*site];
    if (f == UNUSED_FREQ_VALUE){
        return;
    }

    if (loopStatus[*site] != 1){
        PRINT_INSTR(stderr, "loop exit scaling call w/o entry for site %d", *site);
    }
    loopStatus[*site] = 0;

    // set frequency
    int cpu = getTaskId();
    pfreq_throttle_set(cpu, lastFreq);
}

int findSiteIndex(uint64_t hash){
    int i;
    for (i = 0; i < numberOfLoops; i++){
        if (loopHashCodes[i] == hash){
            return i;
        }
    }
    return -1;
}

void clearFrequencyMap(){
    int i;
    if (frequencyMap){
        for (i = 0; i < numberOfLoops; i++){
            frequencyMap[i] = UNUSED_FREQ_VALUE;
        }
    }
}

void initialize_frequency_map(){
    int i, j;
    frequencyMap = malloc(sizeof(uint32_t) * numberOfLoops);
    clearFrequencyMap();

    char * fPath = getenv("PFREQ_FREQUENCY_MAP");
    if (fPath == NULL){
        PRINT_INSTR(stderr, "environment variable PFREQ_FREQUENCY_MAP not set. proceeding without dvfs");
        return;
    }

    FILE* freqFile = fopen(fPath, "r");
    if (freqFile == NULL){
        PRINT_INSTR(stderr, "PFREQ_FREQUENCY_MAP file %s cannot be opened. proceeding without dvfs", fPath);
        return;
    }

    i = 0;
    char line[__MAX_STRING_SIZE];
    PRINT_INSTR(stdout, "Setting frequency map for dvfs from env variable PFREQ_FREQUENCY_MAP (%s)", fPath);
    while (fgets(line, __MAX_STRING_SIZE, freqFile) != NULL){
        i++;
        uint64_t hash;
        uint32_t rank;
        uint32_t freq;
        int res = sscanf(line, "%lld %d %d", &hash, &rank, &freq);
        if (res != 3){
            PRINT_INSTR(stderr, "line %d of %s cannot be understood as a frequency map. proceeding without dvfs", i, fPath);
            clearFrequencyMap();
            return;
        }

        //PRINT_INSTR(stdout, "line %d: %lld %d %d", i, hash, rank, freq);
        if (rank == getTaskId()){
            j = findSiteIndex(hash);
            if (j < 0){
                PRINT_INSTR(stderr, "not an instrumented loop (line %d of %s): %lld. ignoring", i, fPath, hash);
            } else {
                frequencyMap[j] = freq;
            }
        }
    }
}

// called at program start
void pfreq_throttle_init(uint32_t* s, uint32_t* numLoops, uint64_t* loopHashes){
    int i, j;

    site = s;
    numberOfLoops = *numLoops;
    loopHashCodes = loopHashes;

    entryCalled = malloc(sizeof(uint64_t) * numberOfLoops);
    bzero(entryCalled, sizeof(uint64_t) * numberOfLoops);

    exitCalled = malloc(sizeof(uint64_t) * numberOfLoops);
    bzero(exitCalled, sizeof(uint64_t) * numberOfLoops);

    loopStatus = malloc(sizeof(uint32_t) * numberOfLoops);
    bzero(loopStatus, sizeof(uint32_t) * numberOfLoops);

    currentFreq = MAX_FREQ;
}

// called at program finish
void pfreq_throttle_fini(){
    int i;
    int e = 0;
    for (i = 0; i < numberOfLoops; i++){
        if (entryCalled[i] != exitCalled[i]){
            PRINT_INSTR(stderr, "site %d: entry called %lld times but exit %lld", i, entryCalled[i], exitCalled[i]);
            e++;
        }
    }

    free(entryCalled);
    free(exitCalled);
    free(loopStatus);
    free(frequencyMap);

    if (e){
        exit(1);
    }
}

// called at mpi_init
void tool_mpi_init(){
    pfreq_affinity_get();
    pfreq_affinity_set(getTaskId());

    initialize_frequency_map();
    DEBUG(
        int i;
        for (i = 0; i < numberOfLoops; i++){
            if (frequencyMap[i] != UNUSED_FREQ_VALUE){
                PRINT_DEBUG(stdout, "frequency plan found for loop %lld: %d", loopHashCodes[i], frequencyMap[i]);
            }
        }
          )
}

inline int32_t pfreq_throttle_set(uint32_t cpu, uint32_t freq){
    PRINT_DEBUG(stdout, "throttling task %d to frequency %dKHz", cpu, freq);
    lastFreq = currentFreq;
    currentFreq = freq;
    return internal_set_currentfreq((unsigned int)cpu, (int)freq);
}

unsigned long pfreq_throttle_get(){
    return currentFreq;
}

inline int32_t internal_set_currentfreq(unsigned int cpu, int freq){
    return cpufreq_set_frequency((unsigned int)cpu, (int)freq);
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
    PRINT_INSTR(stdout, "Setting affinity to cpu%u for caller task pid %d (retcode %d)", cpu, getpid(), retCode);
    
    return retCode;
}
