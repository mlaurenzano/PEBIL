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

#define  __USE_GNU
#include <cpufreq.h>
#include <sched.h>
#include <InstrumentationCommon.h>

#define MAX_CPU_IN_SYSTEM 4

uint32_t totalFreqs = 0;
/* can get these out of /sys/devices/system/cpu/cpu1/cpufreq/scaling_available_frequencies
*/
unsigned long* availableFreqs = NULL;
int32_t currentFreq = -1;

//#define DEBUG_FREQ_THROTTLE                                                                                                                                          
#ifdef DEBUG_FREQ_THROTTLE
#define logfile stderr
#define PRINT_FREQI(...) fprintf(logfile, __VA_ARGS__);
#else
#define PRINT_FREQI(...) PRINT_INSTR(stdout, __VA_ARGS__)
#endif //DEBUG_FREQ_THROTTLE                                                                                                                                           
int32_t pfreq_affinity_get(){
    int32_t retCode = 0;
    cpu_set_t cpuset;

    if (sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset) < 0) {
        retCode = -1;
    } else {
        for(uint32_t i = 0; i < MAX_CPU_IN_SYSTEM; i++){
            if (CPU_ISSET(i, &cpuset)){
                retCode = i;
                break;
            }
        }
    }
    //PRINT_FREQI("Getting affitinity for caller task pid %d (retcode %d)",getpid(),retCode);                                                                      
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
    PRINT_FREQI("Setting affitinity to cpu%u for caller task pid %d (retcode %d)", cpu,getpid(), retCode);

    return retCode;
}

int ulong_compare(const void *a, const void *b) 
{ 
    const unsigned long *ia = (const unsigned long *)a;
    const unsigned long *ib = (const unsigned long *)b;
    return *ia  - *ib; 
} 

uint32_t find_current_cpufreq(uint32_t cpu){
    int32_t i;
    unsigned long cpufreq = cpufreq_get(cpu);
    for(i = 0; i < totalFreqs; i++){
        if (availableFreqs[i] == cpufreq){
            return i;
        }
    }
    PRINT_INSTR(stderr, "Cannot match cpu frequency %llu from library with available frequencies", cpufreq);
    exit(-1);
    return -1;
}

uint32_t find_available_cpufreq(){
    int32_t i;
    FILE* freqFile = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies", "r");
    if (!freqFile){
        PRINT_INSTR(stderr, "Cannot open file to read available frequencies, dying...");
    }
    uint32_t freqCount = 0;
    unsigned long freq;
    while (fscanf(freqFile, "%lu", &freq) != EOF){
        freqCount++;
    }
    rewind(freqFile);
    availableFreqs = (unsigned long*)malloc(sizeof(unsigned long) * freqCount);
    freqCount = 0;
    while (fscanf(freqFile, "%llu", &freq) != EOF){
        availableFreqs[freqCount++] = freq;
    }
    close(freqFile);

    qsort(availableFreqs, freqCount, sizeof(unsigned long), ulong_compare);

    fprintf(stdout, "%d available frequencies: ", freqCount);
    for (i = 0; i < freqCount; i++){
        fprintf(stdout, "%llu ", availableFreqs[i]);
    }
    fprintf(stdout, "\n");

    return freqCount;
}

int32_t pfreq_throttle_init(){
    totalFreqs = find_available_cpufreq();
    currentFreq = find_current_cpufreq(pfreq_affinity_get());

    pfreq_affinity_get();
    return pfreq_affinity_set(pfreq_affinity_get());
}

void pfreq_throttle_fini(){
}

void tool_mpi_init(){
    pfreq_affinity_get();
    pfreq_affinity_set(pfreq_affinity_get());
}

int32_t pfreq_throttle_set(uint32_t cpu, unsigned long freqInKiloHz){
    int32_t currCpu = pfreq_affinity_get();
    assert(cpu == (uint32_t)currCpu);
    int32_t ret = cpufreq_set_frequency((unsigned int)cpu, freqInKiloHz);
    PRINT_FREQI("Throttling for caller task pid %d and cpu %u to %luKHz (retcode %d)", getpid(), cpu, freqInKiloHz, ret);
    return ret;
}

unsigned long pfreq_throttle_get(){
    return availableFreqs[currentFreq];
}

inline int32_t internal_set_currentfreq(uint32_t cpu, uint32_t freqIndex){
    unsigned long freqInKiloHz = availableFreqs[freqIndex];
    currentFreq = freqIndex;
    return pfreq_throttle_set(cpu, (int)freqInKiloHz);
}

int32_t pfreq_throttle_max(){
    return internal_set_currentfreq(pfreq_affinity_get(), totalFreqs - 1);
}

int32_t pfreq_throttle_min(){
    return internal_set_currentfreq(pfreq_affinity_get(), 0);
}

int32_t pfreq_throttle_high(){
    return internal_set_currentfreq(pfreq_affinity_get(), totalFreqs - 1);
}

int32_t pfreq_throttle_low(){
    return internal_set_currentfreq(pfreq_affinity_get(), 0);
}

int32_t pfreq_throttle_down(){
    if (currentFreq > 0){
        return internal_set_currentfreq(pfreq_affinity_get(), currentFreq - 1);
    }
    return -1;
}

int32_t pfreq_throttle_up(){
    if (currentFreq < totalFreqs - 1){
        return internal_set_currentfreq(pfreq_affinity_get(), currentFreq + 1);
    }
    return -1;
}
