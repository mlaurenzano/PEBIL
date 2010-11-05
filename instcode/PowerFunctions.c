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
#include <cpufreq.h>
#include <signal.h>
#include <InstrumentationCommon.h>

//#define POWER_MEASURE
#ifdef POWER_MEASURE
#define POWER_MEASURE_LOOP
#endif

#define MAX_CPU_IN_SYSTEM 4

#ifndef LOW_FREQ_IDX
#define LOW_FREQ_IDX 0
#endif
uint32_t totalFreqs = 0;

char* powerReadings = "power_readings.txt";
FILE* powerLog = NULL;
pid_t loggerProcess = 0;
int logRunning = 0;
#define check_and_admit_log(__b) \
    assert(logRunning == __b); \
    logRunning = 1 - __b;
/* can get these out of /sys/devices/system/cpu/cpu1/cpufreq/scaling_available_frequencies
*/
unsigned long* availableFreqs = NULL;
int32_t currentFreq = -1;
double startTime = 0.0;
double loopStart = 0.0;
#define CURRENT_TIMER (read_process_clock())
//#define TIMER(__tmr) __tmr = CURRENT_TIMER

#define TIMER(__t)        { struct timeval __tmp; gettimeofday(&__tmp,NULL); \
        __t = ((double)1.0 * __tmp.tv_sec)  + ((double)1.0e-6 * __tmp.tv_usec); }


//#define DEBUG_FREQ_THROTTLE                                                                                                                                          
#ifdef DEBUG_FREQ_THROTTLE
#define logfile stderr
#define PRINT_FREQI(...) fprintf(logfile, __VA_ARGS__);
#else
#define PRINT_FREQI(...) PRINT_INSTR(stdout, __VA_ARGS__)
#endif //DEBUG_FREQ_THROTTLE                                                                                                                                          

void pfreq_invoke_power_log(){
    check_and_admit_log(0);
    loggerProcess = fork();
    if (loggerProcess == 0){
        assert(pfreq_affinity_get() == 0);
        pfreq_affinity_set(0);

        // this doesn't return, we need to kill it manually when we are done logging
        FILE* log = fopen(powerReadings, "w");
        wattsup_logger(log);
        assert(0);
    } else {
        sleep(2);
        powerLog = fopen(powerReadings, "r");
    }
    
    assert(powerLog);
    PRINT_INSTR(stdout, "Started writing power log to %s", powerReadings);
}

double pfreq_log_power(uint32_t* samples){
    int full = 0;
    int decimal = 0;
    double watts = 0.0;

    assert(powerLog);
    *samples = 0;
    while (fscanf(powerLog, "%d.%d", &full, &decimal) != EOF){
        watts += (double)((double)full) + (double)((double)decimal / 10.0);
        (*samples)++;
        //        PRINT_INSTR(stdout, "value from power log %d.%d", full, decimal);
    }
    return watts;
}

void pfreq_kill_power_log(){
    check_and_admit_log(1);
    kill(loggerProcess, SIGQUIT);

    int samples;
    double watts;

    rewind(powerLog);
    watts = pfreq_log_power(&samples);
    close(powerLog);

    PRINT_INSTR(stdout, "LOGGING KILLED: Power readings from %s -- %d samples; average %f watts", powerReadings, samples, (double)(watts/samples));

    return watts / samples;
}

// these are the main controllers for the tool for now
// executes before a loop entry
int32_t pfreq_throttle_low(){
    int32_t ret = internal_set_currentfreq(pfreq_affinity_get(), LOW_FREQ_IDX);
#ifdef POWER_MEASURE_LOOP
    // fast forwards the log to the current location
    uint32_t samples;
    double watts = pfreq_log_power(&samples);
#endif
    TIMER(loopStart);
    return ret;
}
 
// executes after a loop exit
int32_t pfreq_throttle_high(){
    double loopEnd; 
    TIMER(loopEnd);
    int32_t ret = internal_set_currentfreq(pfreq_affinity_get(), totalFreqs - 1);

#ifdef POWER_MEASURE_LOOP
    uint32_t samples;
    double watts = pfreq_log_power(&samples);
    PRINT_INSTR(stdout, "Loop execution report -- cxxx runtime %f, %d samples, averaged %f watts", loopEnd - loopStart, samples, (double)(watts/samples));
#else
    PRINT_INSTR(stdout, "Loop execution report -- cxxx runtime %f", loopEnd - loopStart);
#endif    
    return ret;
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
    PRINT_FREQI("Setting affitinity to cpu%u for caller task pid %d (retcode %d)", cpu, getpid(), retCode);

    return retCode;
}

int ulong_compare(const void *a, const void *b) 
{ 
    const unsigned long *ia = (const unsigned long *)a;
    const unsigned long *ib = (const unsigned long *)b;
    return *ia - *ib; 
} 

uint32_t find_current_cpufreq(uint32_t cpu){
    int32_t i;
    unsigned long cpufreq = cpufreq_get(cpu);
    for(i = 0; i < totalFreqs; i++){
        if (availableFreqs[i] == cpufreq){
            return i;
        }
    }
    PRINT_INSTR(stderr, "Cannot match cpu frequency %lu from library with available frequencies", cpufreq);
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
    while (fscanf(freqFile, "%lu", &freq) != EOF){
        availableFreqs[freqCount++] = freq;
    }
    close(freqFile);

    qsort(availableFreqs, freqCount, sizeof(unsigned long), ulong_compare);

    fprintf(stdout, "%d available frequencies: ", freqCount);
    for (i = 0; i < freqCount; i++){
        fprintf(stdout, "%lu ", availableFreqs[i]);
    }
    fprintf(stdout, "\n");

    return freqCount;
}

int32_t pfreq_throttle_init(){
    totalFreqs = find_available_cpufreq();
    currentFreq = find_current_cpufreq(pfreq_affinity_get());
    PRINT_INSTR(stdout, "starting run with freq %luKHz", availableFreqs[currentFreq]);

    pfreq_affinity_get();
    int32_t ret = pfreq_affinity_set(pfreq_affinity_get());

#ifdef POWER_MEASURE
    if (getTaskId() == 0){
        pfreq_invoke_power_log();
    }
#endif

    TIMER(startTime);
    return ret;
}

void pfreq_throttle_fini(){
    double endTime, totalTime;
    TIMER(endTime);
    totalTime = endTime - startTime;
    PRINT_INSTR(stdout, "ending run with freq %luKHz", availableFreqs[currentFreq]);

#ifdef POWER_MEASURE
    if (getTaskId() == 0){
        pfreq_kill_power_log();
    }
#endif //POWER_MEASURE
    PRINT_INSTR(stdout, "Overall runtime CXXX %f", totalTime);
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
    unsigned long freqIs = cpufreq_get(cpu);

    if (freqInKiloHz != freqIs){
        if (ret == -ENODEV){
            PRINT_INSTR(stderr, "Frequency not correctly set - do you have write permissions to sysfs?");
        } else {
            PRINT_INSTR(stderr, "Frequency not correctly set - target frequency %lu, actual %lu", freqInKiloHz, freqIs);
        }
        exit(-1);
    }
    assert(freqInKiloHz == freqIs);

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
