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

#define NO_FINEGRAIN_LOOP
#define THROTTLE_LOOP
#define POWER_MEASURE
#ifdef POWER_MEASURE
#define POWER_MEASURE_LOOP
#endif

#define MAX_CPU_IN_SYSTEM 8
#define PIN_LOGGER_TO_CORE 0

#ifndef LOW_FREQ_IDX
#define LOW_FREQ_IDX 4
#endif

uint64_t* siteIndex;

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
#define CURRENT_TIMER (read_process_clock())
//#define TIMER(__tmr) __tmr = CURRENT_TIMER

#define TIMER(__t)        { struct timeval __tmp; gettimeofday(&__tmp,NULL); \
        __t = ((double)1.0 * __tmp.tv_sec)  + ((double)1.0e-6 * __tmp.tv_usec); }


void pfreq_invoke_power_log(){
    check_and_admit_log(0);
    loggerProcess = fork();
    if (loggerProcess == 0){
        assert(pfreq_affinity_get() == 0);
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

double pfreq_log_power(uint32_t* samples){
    int full = 0;
    int decimal = 0;
    double watts = 0.0;

    assert(rawPowerLog);
    *samples = 0;
    while (fscanf(rawPowerLog, "%d.%d", &full, &decimal) != EOF){
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

    rewind(rawPowerLog);
    watts = pfreq_log_power(&samples);
    close(rawPowerLog);

    PRINT_INSTR(stdout, "LOGGING KILLED: Overall power readings from %s -- %d samples; average %f watts", rawPowerName, samples, (double)(watts/samples));

    close(nicePowerLog);

    return watts / samples;
}

// these are the main controllers for the tool for now
// executes before a loop entry
int32_t pfreq_throttle_low(){
#ifdef NO_FINEGRAIN_LOOP
    //    return internal_set_currentfreq(pfreq_affinity_get(), LOW_FREQ_IDX);
    return 0;
#endif
    int32_t ret = 0;
#ifdef THROTTLE_LOOP
    ret = internal_set_currentfreq(pfreq_affinity_get(), LOW_FREQ_IDX);
#endif
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
#ifdef NO_FINEGRAIN_LOOP
    //    pfreq_throttle_max();
    return 0;
#endif
    double loopEnd; 
    TIMER(loopEnd);
    int32_t ret = 0;
#ifdef THROTTLE_LOOP
    ret = internal_set_currentfreq(pfreq_affinity_get(), totalFreqs - 1);
#endif
#ifdef POWER_MEASURE_LOOP
    uint32_t samples;
    double watts = pfreq_log_power(&samples);
    PRINT_INSTR(stdout, "Loop execution report -- cxxx runtime %f, %d samples, averaged %f watts", loopEnd - loopStart, samples, (double)(watts/samples));
    fprintf(nicePowerLog, "%f\t%f\t%d\n", loopEnd - loopStart, (double)(watts/samples), samples);
    fflush(nicePowerLog);
#else
    //    PRINT_INSTR(stdout, "Loop execution report -- cxxx runtime %f", loopEnd - loopStart);
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

int32_t pfreq_throttle_init(uint64_t* site){
    totalFreqs = find_available_cpufreq();
    currentFreq = find_current_cpufreq(pfreq_affinity_get());

    siteIndex = site;

    TIMER(startTime);
    return 0;
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
    PRINT_INSTR(stdout, "Overall runtime CXXX %f -- made %lld frequency changes", totalTime, frequencyChanges);
    for (uint32_t i = 0; i < 1024; i++){
        if (callCounters[i]){
            PRINT_INSTR(stdout, "Per-call counters[%d]: %lld %f", i, callCounters[i], callTimers[i]);
        }
    }
}

void tool_mpi_init(){
    pfreq_affinity_get();
    pfreq_affinity_set(getTaskId());

    PRINT_INSTR(stdout, "starting run with freq %luKHz", availableFreqs[currentFreq]);
    PRINT_INSTR(stdout, "throttle low is freq %luKHz", availableFreqs[LOW_FREQ_IDX]);

#ifdef POWER_MEASURE
    char name[1024];
    sprintf(name, "MultiMAPS_power_measure_%04d.txt", getTaskId());
    nicePowerLog = fopen(name, "w");
    fprintf(nicePowerLog, "time_sec\tavg_watts\tnum_samples\n");
    
    PRINT_INSTR(stdout, "Nice power measurements will be written to %s", name);

    if (getTaskId() == 0){
        pfreq_invoke_power_log();
    }

    sleep(2);
    rawPowerLog = fopen(rawPowerName, "r");
    assert(rawPowerLog);
#endif

}

inline int32_t pfreq_throttle_set(uint32_t cpu, unsigned long freqInKiloHz){
    return cpufreq_set_frequency((unsigned int)cpu, freqInKiloHz);
    int32_t currCpu = pfreq_affinity_get();

    assert(cpu == (uint32_t)currCpu);
    int32_t ret = cpufreq_set_frequency((unsigned int)cpu, freqInKiloHz);

    //    PRINT_INSTR(stdout, "Throttling for caller task pid %d and cpu %u to %luKHz (retcode %d)", getpid(), cpu, freqInKiloHz, ret);
    frequencyChanges++;
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

#define LOW_FREQ 1170000
#define MAX_FREQ 2300000

#define REQ_THRESHOLD 3

int __give_pebil_name(MPI_Barrier)(MPI_Comm comm){
    return PMPI_Barrier(comm);
}

void __give_pebil_name(mpi_barrier)(MPI_Fint comm, int* ierr){
    cpufreq_set_frequency((unsigned int)getTaskId(), LOW_FREQ);
    pmpi_barrier(comm, ierr);
    cpufreq_set_frequency((unsigned int)getTaskId(), MAX_FREQ);
}

int __give_pebil_name(MPI_Wait)(MPI_Request* request, MPI_Status* status){
    int ret = PMPI_Wait(request, status);
    return ret;
}
void __give_pebil_name(mpi_wait)(MPI_Request* request, MPI_Status* status, int* ierr){
    pmpi_wait(request, status, ierr);
}

int __give_pebil_name(MPI_Waitall)(int count, MPI_Request requests[], MPI_Status statuses[]){
    pfreq_throttle_down();
    int ret = PMPI_Waitall(count, requests, statuses);
    pfreq_throttle_up();
    return ret;
}
void __give_pebil_name(mpi_waitall)(int* count, MPI_Fint* requests, MPI_Fint* statuses, int* ierr){
    double t1, t2;
    int doThrottle = 0;
    /*
    int flag = 0;
    int err;
    MPI_Status status;
    int countWait = 0;

    for (uint32_t i = 0; i < *count; i++){
        MPI_Request req = PMPI_Request_f2c(requests[i]);
        MPI_Request_get_status(req, &flag, &status);
        if (flag){
            countWait++;
        }
    }
    if (getTaskId() < 4){
        doThrottle = 1;
    }
    */
    
    if (*siteIndex == 5 && getTaskId() > 3){
        doThrottle = 1;
    }
    if (doThrottle){
        cpufreq_set_frequency((unsigned int)getTaskId(), LOW_FREQ);
    }
    callCounters[*siteIndex]++;
    TIMER(t1);
    pmpi_waitall(count, requests, statuses, ierr);
    TIMER(t2);
    callTimers[*siteIndex] += (t2-t1);

    if (doThrottle){
        cpufreq_set_frequency((unsigned int)getTaskId(), MAX_FREQ);
    }
    /*
    callCounters[countWait]++;
    //    PRINT_INSTR(stdout, "waitall timer %f, not-done %d", t2-t1, countWait);
    */
}

int __give_pebil_name(MPI_Startall)(int count, MPI_Request requests[]){
    int ret = PMPI_Startall(count, requests);
    return ret;
}
int __give_pebil_name(mpi_startall)(int* count, MPI_Request* requests, int* ierr){
    pmpi_startall(count, requests, ierr);
}

#endif //HAVE_CPUFREQ
