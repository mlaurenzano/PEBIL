/* This program is free software: you can redistribute it and/or modify
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
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <InstrumentationCommon.h>
#include <IOWrapper.h>

char* appName;
int32_t* currentSiteIndex;
char** fileNames;
int32_t* lineNumbers;

// to handle trace buffering
TraceBuffer_t traceBuffer;
char message[__MAX_STRING_SIZE];

// to handle timers
int64_t timerstart;
int64_t timerstop;
#define TIMER_START (timerstart = readtsc())
#define TIMER_STOP  (timerstop  = readtsc())
#define TIMER_VALUE (timerstop - timerstart)
#define TIMER_EXECUTE(__stmts) TIMER_START; __stmts TIMER_STOP; 
#define PRINT_TIMER(__file) PRINT_INSTR(__file, "timer value (in cycles): %lld", TIMER_VALUE)


uint32_t dumpBuffer(){
    if (traceBuffer.outFile == NULL){
        char fname[__MAX_STRING_SIZE];
        sprintf(fname, "pebiliotrace.%d.log", __taskid);
        traceBuffer.outFile = fopen(fname, "w");
    }

    uint32_t oerr = fwrite(traceBuffer.storage, sizeof(char), traceBuffer.freeIdx, traceBuffer.outFile);
    assert(oerr == traceBuffer.freeIdx);
    traceBuffer.freeIdx = 0;

    assert(traceBuffer.freeIdx == 0);
    return traceBuffer.freeIdx;
}

uint32_t storeToBuffer(char* msg, uint32_t sizeInBytes){
    // if traceBuffer is full dump it
    if (sizeInBytes > traceBuffer.size - traceBuffer.freeIdx){
        dumpBuffer(traceBuffer);
    }
    assert(sizeInBytes < traceBuffer.size - traceBuffer.freeIdx);

    // store the msg to the buffer
    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), msg, sizeInBytes);
    traceBuffer.freeIdx += sizeInBytes;

    return traceBuffer.freeIdx;
}

// do any initialization here
// NOTE: on static-linked binaries, calling any functions from here will cause some problems
int32_t initwrapper(int32_t* indexLoc, char** fNames, int32_t* lNum){
    // at each call site we will put the index of the originating point in this location
    currentSiteIndex = indexLoc;

    fileNames = fNames;
    lineNumbers = lNum;

    taskid = getpid();

    traceBuffer.freeIdx = 0;
    traceBuffer.size = IO_BUFFER_SIZE;
    traceBuffer.outFile = NULL;
}

// do any cleanup here
int32_t finishwrapper(){
    PRINT_INSTR(stdout, "Finishing IO Trace for task %d, dumping buffer", __taskid);
    dumpBuffer(&traceBuffer);
    fclose(traceBuffer.outFile);
}

#include <CLIBWrappers.c>
#include <POSXWrappers.c>

#ifdef HAVE_MPI
//#include <MPIOWrappers.c>
#endif // HAVE_MPI    

#ifdef HAVE_HDF5
#include <HDF5Wrappers.c>
#endif // HAVE_HDF5
