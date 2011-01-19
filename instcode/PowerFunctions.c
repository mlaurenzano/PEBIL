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

#ifdef HAVE_CPUFREQ

#define  _GNU_SOURCE
#include <sched.h>
#include <errno.h>
#include <cpufreq.h>
#include <signal.h>
#include <InstrumentationCommon.h>

#define HWCOUNT_COLLECT
#ifdef HWCOUNT_COLLECT
#include <papi.h>
#endif

//#define INSTRUMENT
//#define ALWAYS_THROTTLE_LOW
#define THROTTLE_LOOP
#define POWER_MEASURE
#ifdef POWER_MEASURE
//#define POWER_MEASURE_LOOP
#endif

#define MAX_CPU_IN_SYSTEM 8
#define PIN_LOGGER_TO_CORE 0

int32_t* throttleLevels;
int32_t lowFreqIdx = 0;
#ifndef LOW_FREQ_IDX
#define LOW_FREQ_IDX 0
#endif

uint64_t* siteIndex;
uint32_t numberOfSites = 0;

uint32_t totalFreqs = 0;
char* rawPowerName = "power_readings.txt";
FILE* rawPowerLog = NULL;
FILE* nicePowerLog = NULL;
pid_t loggerProcess = 0;
int logRunning = 0;
#define check_and_admit_log(__b) \
    assert(logRunning == __b); \
    logRunning = 1 - __b;

unsigned long* availableFreqs = NULL;
int32_t currentFreq = -1;
double startTime = 0.0;
double loopStart = 0.0;
uint64_t frequencyChanges = 0;
uint64_t callCounters[1024];
double callTimers[1024];
int32_t currentLoop = 0;
#define CURRENT_TIMER (read_process_clock())
//#define TIMER(__tmr) __tmr = CURRENT_TIMER

#define TIMER(__t)        { struct timeval __tmp; gettimeofday(&__tmp,NULL); \
        __t = ((double)1.0 * __tmp.tv_sec)  + ((double)1.0e-6 * __tmp.tv_usec); }

#ifdef HWCOUNT_COLLECT
int papiEventSet;
//#define PAPI_NATIVE_EVT3 "OFFCORE_REQUESTS"
//#define PAPI_NATIVE_EVT0 "RESOURCE_STALLS:ANY"
//#define PAPI_NATIVE_EVT1 "RESOURCE_STALLS:LOAD"
//#define PAPI_NATIVE_EVT2 "RESOURCE_STALLS:STORE"
#define PAPI_DEFINED_EVT0 PAPI_L2_DCM
#define PAPI_DEFINED_EVT1 PAPI_RES_STL
#define PAPI_DEFINED_EVT2 PAPI_TOT_INS
#define PAPI_DEFINED_EVT3 PAPI_L2_ICM
uint64_t values[1024][4];

void pfreq_hwcount_init(int num_events, int* event_defs){
    int i;
    papiEventSet = PAPI_NULL;
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT){
        PRINT_INSTR(stderr, "Papi init error");
        exit(-1);
    } else {
        PRINT_INSTR(stdout, "Papi init success");
    }
    PAPI_create_eventset(&papiEventSet);
    int native;
    /*
    if (PAPI_event_name_to_code(PAPI_NATIVE_EVT0, &native) != PAPI_OK ||
        PAPI_add_event(papiEventSet, native) != PAPI_OK){
        PRINT_INSTR(stderr, "Problem adding papi evt %#s", PAPI_NATIVE_EVT0);
        exit(-1);
    }
    */
    for (i = 0; i < num_events; i++){
        if (PAPI_add_event(papiEventSet, event_defs[i]) != PAPI_OK){
            PRINT_INSTR(stderr, "Problem adding papi evt %#x", event_defs[i]);
            exit(-1);
        }
    }
}

void pfreq_hwcount_fini(){
    
}

void pfreq_hwcount_start(){
    PAPI_start(papiEventSet);
}

void pfreq_hwcount_stop(int numevents, uint64_t* event_counts){
    assert(numevents == 4);
    PAPI_stop(papiEventSet, event_counts);
    //    PRINT_INSTR(stdout, "Papi Counter[%d]: %lld\t%lld\t%lld\t%lld", currentLoop, values[currentLoop][0], values[currentLoop][1], values[currentLoop][2], values[currentLoop][3]);
}
#endif

void pfreq_invoke_power_log(){
    check_and_admit_log(0);
    loggerProcess = fork();
    if (loggerProcess == 0){
        assert(pfreq_affinity_get() == getTaskId());
        assert(PIN_LOGGER_TO_CORE < MAX_CPU_IN_SYSTEM);
        pfreq_affinity_set(PIN_LOGGER_TO_CORE);

        // this doesn't return, we need to kill it manually when we are done logging
        FILE* log = fopen(rawPowerName, "w");
        wattsup_logger(log);
        assert(0);
    } else {
    }

    PRINT_INSTR(stdout, "Raw power measurements will be written to %s", rawPowerName);
}

double pfreq_log_power(uint32_t* samples, double* last){
    int full = 0;
    int decimal = 0;
    double watts = 0.0;
    double tmp;

    assert(rawPowerLog);
    *samples = 0;
    while (fscanf(rawPowerLog, "%d.%d", &full, &decimal) != EOF){
        tmp = (double)((double)full) + (double)((double)decimal / 10.0);
        watts += tmp;
        *last = tmp;
        (*samples)++;
        /*
        if (getTaskId() == 0){
            PRINT_INSTR(stdout, "value from power log %d.%d", full, decimal);
        }
        */
    }

    return watts;
}

void pfreq_kill_power_log(double t){
    check_and_admit_log(1);
    kill(loggerProcess, SIGQUIT);

    int samples;
    double watts;
    double last;

    rewind(rawPowerLog);
    watts = pfreq_log_power(&samples, &last);
    close(rawPowerLog);

    PRINT_INSTR(stdout, "LOGGING KILLED: Overall power readings from %s -- %d samples; average %f watts; %f seconds; %f joules", rawPowerName, samples, (double)(watts/samples), t, (double)((watts/samples)*t));

    close(nicePowerLog);

    return watts / samples;
}

// these are the main controllers for the tool for now
// executes before a loop entry
int32_t pfreq_throttle_low(){
    currentLoop = *siteIndex;

    int32_t ret = 0;
#ifdef THROTTLE_LOOP
    //    PRINT_INSTR(stdout, "doing throttle for site %lld", *siteIndex);
    //    ret = internal_set_currentfreq(pfreq_affinity_get(), lowFreqIdx);
    ret = internal_set_currentfreq(pfreq_affinity_get(), throttleLevels[*siteIndex]);
#else
    unsigned long freqIs = cpufreq_get(pfreq_affinity_get());
    //    PRINT_INSTR(stdout, "Not throttling loop; current frequency is %lldKHz", freqIs);
#endif
#ifdef POWER_MEASURE_LOOP
    // fast forwards the log to the current location
    uint32_t samples;
    double last;
    double watts = pfreq_log_power(&samples, &last);
#endif
    TIMER(loopStart);

    return ret;
}
 
// executes after a loop exit
int32_t pfreq_throttle_high(){
    double loopEnd; 
    TIMER(loopEnd);
    int32_t ret = 0;
    callCounters[currentLoop]++;
    callTimers[currentLoop] += (loopEnd - loopStart);
    //    PRINT_INSTR(stdout, "unthrottling site %lld", *siteIndex);
#ifdef THROTTLE_LOOP
#ifdef ALWAYS_THROTTLE_LOW
    ret = internal_set_currentfreq(pfreq_affinity_get(), lowFreqIdx);
#else
    ret = internal_set_currentfreq(pfreq_affinity_get(), totalFreqs - 1);
#endif //ALWAYS_THROTTLE_LOW
#endif //THROTTLE_LOOP
#ifdef POWER_MEASURE_LOOP
    uint32_t samples;
    double last;
    double watts = pfreq_log_power(&samples, &last);
    PRINT_INSTR(stdout, "Loop execution report (site %lld) -- cxxx runtime %f, %d samples, averaged %f watts", *siteIndex, loopEnd - loopStart, samples, (double)last);
    fprintf(nicePowerLog, "%f\t%f\t%d\n", loopEnd - loopStart, (double)(watts/samples), samples);
    fflush(nicePowerLog);
#else
    //    PRINT_INSTR(stdout, "Loop execution report (site %lld) -- cxxx runtime %f", *siteIndex, loopEnd - loopStart);
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
    PRINT_INSTR(stdout, "Setting affitinity to cpu%u for caller task pid %d (retcode %d)", cpu, getpid(), retCode);

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

int32_t pfreq_throttle_init(uint64_t* site, uint32_t* levels, uint32_t* numSites){
    int32_t i;


    totalFreqs = find_available_cpufreq();
    currentFreq = find_current_cpufreq(pfreq_affinity_get());

#ifdef INSTRUMENT
    siteIndex = site;
    throttleLevels = levels;
    numberOfSites = *numSites;
#else
    tool_mpi_init();
#endif

    /*
    char *p;
    char* end;
    p = getenv("PFREQ_LOW_IDX");
    if (p != NULL){
        lowFreqIdx = strtol(p, &end, 10);
        PRINT_INSTR(stdout, "Obtaining frequency from environment variable PFREQ_LOW_IDX (%lld)", availableFreqs[lowFreqIdx]);
    } else {
        lowFreqIdx = LOW_FREQ_IDX;
        PRINT_INSTR(stdout, "Obtaining frequency from compilation (%lld)", availableFreqs[lowFreqIdx]);
    }
    */

    if (getTaskId() == 0){
        for (i = 0; i < numberOfSites; i++){
            PRINT_INSTR(stdout, "call site %d throttles freq to %lld KHz", i, availableFreqs[throttleLevels[i]]);
        }
    }

    TIMER(startTime);
    return 0;
}

void pfreq_throttle_fini_(){
    pfreq_throttle_fini();
}

void pfreq_throttle_fini(){
    uint32_t i;
    int callCount;
    double callTime;
    double endTime, totalTime;
    TIMER(endTime);
    totalTime = endTime - startTime;
    PRINT_INSTR(stdout, "ending run with freq %luKHz", availableFreqs[currentFreq]);

#ifdef POWER_MEASURE
    if (getTaskId() == 0){
        pfreq_kill_power_log(totalTime);
    }
#endif //POWER_MEASURE

    PRINT_INSTR(stdout, "Overall runtime CXXX %f -- made %lld frequency changes", totalTime, frequencyChanges);
    for (i = 0; i < 1024; i++){
        if (callCounters[i]){
            PRINT_INSTR(stdout, "Per-call counters %d %lld %f", i, callCounters[i], callTimers[i]);
        }
    }
    PRINT_INSTR(stdout, "Overall runtime CXXX %f", totalTime);
}

void tool_mpi_init(){
    pfreq_affinity_get();
    pfreq_affinity_set(getTaskId());
    //    pfreq_affinity_set(5);

    PRINT_INSTR(stdout, "starting freq %luKHz; throttle low freq %luKHz", availableFreqs[currentFreq], availableFreqs[lowFreqIdx]);

#ifdef POWER_MEASURE
    char name[1024];
    sprintf(name, "pfreq_power_measure_%04d.txt", getTaskId());
    nicePowerLog = fopen(name, "w");
    fprintf(nicePowerLog, "time_sec\tavg_watts\tnum_samples\n");
    
    //    PRINT_INSTR(stdout, "Nice power measurements will be written to %s", name);

    if (getTaskId() == 0){
        pfreq_invoke_power_log();
    }
    sleep(1);

    rawPowerLog = fopen(rawPowerName, "r");
    assert(rawPowerLog);
#endif

}

inline int32_t pfreq_throttle_set(uint32_t cpu, unsigned long freqInKiloHz){
    frequencyChanges++;
    return cpufreq_set_frequency((unsigned int)cpu, freqInKiloHz);
    int32_t currCpu = pfreq_affinity_get();

    assert(cpu == (uint32_t)currCpu);
    int32_t ret = cpufreq_set_frequency((unsigned int)cpu, freqInKiloHz);

    PRINT_INSTR(stdout, "Throttling for caller task pid %d and cpu %u to %luKHz (retcode %d)", getpid(), cpu, freqInKiloHz, ret);
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
    //    PRINT_INSTR(stdout, "throttling task %d to frequency %lldKHz", cpu, freqInKiloHz);
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

uint64_t loopCount = 0;
double st, t1, t2;
#define TIMER_PERIOD 3.0
#define FREQ_FLIP 100

void loop_hook_init_(){
    loop_hook_init();
}

void loop_hook_init(){
    pfreq_throttle_init(0, 0, 0);
    PRINT_INSTR(stdout, "Frequency swap amt: %lld", FREQ_FLIP);
    TIMER(st);
    t1 = st;
}

void loop_hook_throttle_(){
    loop_hook_throttle();
}

void loop_hook_throttle(){
    int s;
    double t2;
    double last;
    if (loopCount % 1000000000 == 0){
        if (getTaskId() == 0){
            unsigned long freqIs = cpufreq_get(pfreq_affinity_get());
            TIMER(t2);
            PRINT_INSTR(stdout, "Checkin Loop exec %lld; current frequency is %lldKHz, timer %f", loopCount, freqIs, t2-t1);
            pfreq_log_power(&s, &last);
        }
    }
    if (loopCount % FREQ_FLIP == 0){
        if (getTaskId() == 0){
            TIMER(t2);
            //            PRINT_INSTR(stdout, "Printing log at iteration %lld for frequency %lldKHz, timer %f ...", loopCount, availableFreqs[currentFreq], t2-t1);
            pfreq_log_power(&s, &last);
        }
        if (currentFreq < totalFreqs - 1){
            pfreq_throttle_max();
        } else {
            pfreq_throttle_min();
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    /*
    if (t2-t1 >= TIMER_PERIOD){
        if (getTaskId() == 0){
            PRINT_INSTR(stdout, "loop execution %lld - timer %f", loopCount, t2-st);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        TIMER(t1);
        pfreq_log_power(&s);
    }
    */
    loopCount++;
}

#endif //HAVE_CPUFREQ
