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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>
#include <signal.h>

#include <InstrumentationCommon.h>

#define PRINT_MINIMUM 1

uint64_t* blockCounters = NULL;
int32_t numberOfBasicBlocks = 0;
int64_t* hashValues = NULL;

uint64_t* loopCounters = NULL;
int32_t numberOfLoops = 0;
int64_t* loopHashValues = NULL;

#define COUNTER_DUMP_SIGNAL
#define FAKE_MEASURE
//#define SIGNAL_ALL_RANKS
#ifdef COUNTER_DUMP_SIGNAL
#define COUNTER_DUMP_MAGIC (0x5ca1ab1e)

uint64_t entriesWritten;
uint64_t* counterDumpBuffer = NULL;
#define COUNTER_BUFFER_ENTRIES 524288
uint32_t bufferLoc = 0;
FILE* outp = NULL;
int* otherRanksPids;

typedef struct {
    uint32_t magic;
    uint32_t blocks;
    uint32_t loops;
    uint8_t reserved[20];
} CounterDumpHeader_t;

void clear_counter_state(){
    bzero(blockCounters, sizeof(uint64_t) * numberOfBasicBlocks);
    bzero(loopCounters, sizeof(uint64_t) * numberOfLoops);
}

void clear_counter_buffer(){
    uint32_t i = 0, j = 0;
    if (!counterDumpBuffer || !outp){
        bufferLoc = 0;
        return;
    }
    PRINT_INSTR(stdout, "clearing dump buffer - %lld so far", entriesWritten);
    while (i < bufferLoc){
        fwrite((void*)&counterDumpBuffer[i], sizeof(uint64_t), numberOfBasicBlocks, outp);
        fwrite((void*)&counterDumpBuffer[i + numberOfBasicBlocks], sizeof(uint64_t), numberOfLoops, outp);
        i += numberOfBasicBlocks + numberOfLoops;
    }
    assert(i == bufferLoc);
    entriesWritten += i;
    bufferLoc = 0;
}

void dump_counter_state(int signum){
    if (!blockCounters){
        return;
    }
    if (!loopCounters){
        return;
    }
    //        print_64b_buffer(blockCounters, numberOfBasicBlocks, stdout, 'f');
    //        print_64b_buffer(loopCounters, numberOfLoops, stdout, 'l');
    //fprintf(stdout, "\n");
    //PRINT_INSTR(stdout, "dumping %d counters + %d loops", numberOfBasicBlocks, numberOfLoops);

#ifdef SIGNAL_ALL_RANKS
    int i;
    for (i = 0; i < getNTasks(); i++){
        if (i != 0){
            PRINT_INSTR(stdout, "signalling to %d", otherRanksPids[i]);
            kill(otherRanksPids[i], signum);
        }
    }
#endif

    if (bufferLoc + numberOfBasicBlocks + numberOfLoops > COUNTER_BUFFER_ENTRIES){
        clear_counter_buffer();
    }
    memcpy(&counterDumpBuffer[bufferLoc], blockCounters, numberOfBasicBlocks * sizeof(uint64_t));
    bufferLoc += numberOfBasicBlocks;
    memcpy(&counterDumpBuffer[bufferLoc], loopCounters, numberOfLoops * sizeof(uint64_t));
    bufferLoc += numberOfLoops;
    //    clear_counter_state();
}

void define_user_sig_handlers(){
    if (signal (SIGUSR1, dump_counter_state) == SIG_IGN){
        signal (SIGUSR1, SIG_IGN);
    }
    PRINT_INSTR(stdout, "setup signal handler dump_counter_state");
}

#ifdef FAKE_MEASURE
int continue_measuring = 0;
pid_t other_pid = 0;
#define SLEEP_INTERVAL 10000

void kill_self(int signum){
    PRINT_INSTR(stdout, "gracefully killing signaller %d", getpid());
    exit(0);
}

void initialize_signaller(){
    continue_measuring = 1;
    other_pid = getpid();

    PRINT_INSTR(stdout, "setup signal handler dump_counter_state");
    PRINT_INSTR(stdout, "forking to signaller from %d", other_pid);
    pid_t pid;
    if((pid = fork()) > 0) {
        other_pid = pid;
        return; // parent returns
    }

#ifdef MPI_INIT_REQUIRED
    // invalidate this task
    setTaskValid(0);
#endif

    PRINT_INSTR(stdout, "starting signaler in pid %d -> %d", pid, other_pid);
    if (signal (SIGUSR2, kill_self) == SIG_IGN){
        signal (SIGUSR2, SIG_IGN);
    }

    while (1){
        usleep(SLEEP_INTERVAL);
        //PRINT_INSTR(stdout, "signal!");
        kill(other_pid, SIGUSR1);
    }
    PRINT_INSTR(stdout, "killed signaler in pid %d -> %d", pid, other_pid);
}

void finalize_signaller(){
    kill(other_pid, SIGUSR2);
}

#endif //FAKE_MEASURE

void tool_mpi_init(){
    counterDumpBuffer = malloc(COUNTER_BUFFER_ENTRIES * sizeof(uint64_t));
    bzero(counterDumpBuffer, COUNTER_BUFFER_ENTRIES * sizeof(uint64_t));
    bufferLoc = 0;
    if (getTaskId() == 0){
#ifdef FAKE_MEASURE
        initialize_signaller();
#else
        initialize_pmeasure(1);
#endif
    }
    clear_counter_state();
    entriesWritten = 0;

#ifdef SIGNAL_ALL_RANKS
    otherRanksPids = malloc(getNTasks() * sizeof(int));
    otherRanksPids[0] = getpid();
    MPI_Allgather(otherRanksPids, 1, MPI_INT, otherRanksPids, 1, MPI_INT, MPI_COMM_WORLD);
    for (int i = 0; i < getNTasks(); i++){
        PRINT_INSTR(stdout, "o[%d] = %d", i, otherRanksPids[i]);
    }
#endif

    char fname[__MAX_STRING_SIZE];
    sprintf(fname, "counter.dump.%04d", getTaskId());
    outp = fopen(fname, "w");
    CounterDumpHeader_t hdr;
    bzero(&hdr, sizeof(CounterDumpHeader_t));
    hdr.magic = COUNTER_DUMP_MAGIC;
    hdr.blocks = numberOfBasicBlocks;
    hdr.loops = numberOfLoops;
    PRINT_INSTR(stdout, "%x %d %d", hdr.magic, hdr.blocks, hdr.loops);

    fwrite((void*)&hdr, 1, sizeof(CounterDumpHeader_t), outp);

    ptimer(&pebiltimers[0]);
}

void print_64b_buffer(uint64_t* b, uint32_t l, FILE* o, char d){
    uint32_t j;
    for (j = 0; j < l; j++){
        fprintf(o, "%c%lld\t", d, b[j]);
    }
}
#else //COUNTER_DUMP_SIGNAL
void tool_mpi_init(){
}
#endif //COUNTER_DUMP_SIGNAL

int32_t initcounter(int32_t* numBlocks, uint64_t* blockCounts, int64_t* hashVals){
    numberOfBasicBlocks = *numBlocks;
    blockCounters = blockCounts;
    hashValues = hashVals;
}

int32_t initloop(int32_t* numLoops, uint64_t* loopCounts, int64_t* hashVals){
    numberOfLoops = *numLoops;
    loopCounters = loopCounts;
    loopHashValues = hashVals;

#ifdef COUNTER_DUMP_SIGNAL
    define_user_sig_handlers();
    //tool_mpi_init();
#endif

    ptimer(&pebiltimers[0]);
}

int32_t blockcounter(int32_t* lineNumbers, char** fileNames, char** functionNames, char* appName, char* instExt){
    int32_t i;

    ptimer(&pebiltimers[1]);

#ifdef MPI_INIT_REQUIRED
    if (!isTaskValid()){
        PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print jbbinst files", getpid());
        return 1;
    }
#endif
#ifdef COUNTER_DUMP_SIGNAL
    clear_counter_buffer();
    if (outp){
        fclose(outp);
    } else {
        PRINT_INSTR(stderr, "This statement shouldn't be reached, but it seems to happen under some conditions?");
    }
    if (getTaskId() == 0){
#ifdef FAKE_MEASURE
        finalize_signaller();
#else
        finalize_pmeasure();
#endif
    }
#endif

    PRINT_INSTR(stdout, "*** Instrumentation Summary ****");

    char* outFileName = (char*)malloc(sizeof(char) * __MAX_STRING_SIZE);
    sprintf(outFileName, "%s.meta_%04d.%s", appName, getTaskId(), instExt);
    PRINT_INSTR(stdout, "%d blocks; printing those with at least %d executions to file %s", numberOfBasicBlocks, PRINT_MINIMUM, outFileName);
    FILE* outFile = fopen(outFileName, "w");
    free(outFileName);
    if (!outFile){
        fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
        fflush(stderr);
        exit(-1);
    }

    fprintf(outFile, "# appname   = %s\n", appName);
    fprintf(outFile, "# extension = %s\n", instExt);
    fprintf(outFile, "# phase     = %d\n", 0);
    fprintf(outFile, "# rank      = %d\n", getTaskId());
    fprintf(outFile, "# perinsn   = %s\n", USES_STATS_PER_INSTRUCTION);

    fprintf(outFile, "#id\tcount\t#file:line\tfunc\thash\n");
    fflush(outFile);
    for (i = 0; i < numberOfBasicBlocks; i++){
        if (blockCounters[i] >= PRINT_MINIMUM){
            fprintf(outFile, "%#d\t", i);
            fprintf(outFile, "%llu\t#", blockCounters[i]);
            fprintf(outFile, "%s:", fileNames[i]);
            fprintf(outFile, "%d\t", lineNumbers[i]);
            fprintf(outFile, "%s\t", functionNames[i]);
            fprintf(outFile, "%#lld\n", hashValues[i]);
            fflush(outFile);
        }
    }
    fflush(outFile);
    fclose(outFile);

    PRINT_INSTR(stdout, "cxxx Total Execution time: %f", pebiltimers[1] - pebiltimers[0]);

    return i;
}

int32_t loopcounter(int32_t* loopLineNumbers, char** loopFileNames, char** loopFunctionNames, char* appName, char* instExt){
    int32_t i;

#ifdef MPI_INIT_REQUIRED
    if (!isTaskValid()){
        PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print loopcnt files", getpid());
        return 1;
    }
#endif

    char* outFileName = (char*)malloc(sizeof(char) * __MAX_STRING_SIZE);
    sprintf(outFileName, "%s.meta_%04d.%s", appName, getTaskId(), instExt);
    PRINT_INSTR(stdout, "%d loops; printing those with at least %d executions to file %s", numberOfLoops, PRINT_MINIMUM, outFileName);
    FILE* outFile = fopen(outFileName, "w");
    free(outFileName);
    if (!outFile){
        fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
        fflush(stderr);
        exit(-1);
    }

    fprintf(outFile, "# appname   = %s\n", appName);
    fprintf(outFile, "# extension = %s\n", instExt);
    fprintf(outFile, "# phase     = %d\n", 0);
    fprintf(outFile, "# rank      = %d\n", getTaskId());
    fprintf(outFile, "# perinsn   = %s\n", USES_STATS_PER_INSTRUCTION);

    fprintf(outFile, "#hash\tcount\t#file:line\tfunc\thash\n");
    fflush(outFile);

    for (i = 0; i < numberOfLoops; i++){
        if (loopCounters[i] >= PRINT_MINIMUM){
            fprintf(outFile, "%#lld\t", loopHashValues[i]);
            fprintf(outFile, "%llu\t#", loopCounters[i]);
            fprintf(outFile, "%s:", loopFileNames[i]);
            fprintf(outFile, "%d\t", loopLineNumbers[i]);
            fprintf(outFile, "%s\t", loopFunctionNames[i]);
            fprintf(outFile, "%#lld\n", loopHashValues[i]);
            fflush(outFile);
        }
    }
    fflush(outFile);
    fclose(outFile);

    return i;
}

