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

#ifdef PRELOAD_WRAPPERS
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <InstrumentationCommon.h>
#include <IOWrappers.h>

char* appName;
int32_t* currentSiteIndex;
char** fileNames;
int32_t* lineNumbers;

// to handle trace buffering
TraceBuffer_t traceBuffer = { NULL, 65536, 0, 0 };
uint32_t callDepth = 0;
io_event entry;

// to handle timers
int64_t timerstart;
int64_t timerstop;
#define TIMER_START (timerstart = readtsc())
#define TIMER_STOP  (timerstop  = readtsc())
#define TIMER_VALUE (timerstop - timerstart)
#define TIMER_EXECUTE(__stmts) callDepth++; TIMER_START; __stmts TIMER_STOP; callDepth--;
#define PRINT_TIMER(__file) PRINT_INSTR(__file, "timer value (in cycles): %lld", TIMER_VALUE)

// the dump function makes IO calls. so we must protect from an infinite recursion by not entering
// any buffer function if one is already stacked
uint32_t iowrapperDepth = 0;

uint32_t dumpBuffer(){
    if (iowrapperDepth){
        return 0;
    }
    iowrapperDepth++;
    if (traceBuffer.outFile == NULL){
        char fname[__MAX_STRING_SIZE];
        sprintf(fname, "pebiliotrace.%d.log", __taskid);
        traceBuffer.outFile = fopen(fname, "w");
    }

    uint32_t oerr = fwrite(traceBuffer.storage, sizeof(char), traceBuffer.freeIdx, traceBuffer.outFile);
    assert(oerr == traceBuffer.freeIdx);
    traceBuffer.freeIdx = 0;

    assert(traceBuffer.freeIdx == 0);
    iowrapperDepth--;
    return traceBuffer.freeIdx;
}

uint32_t storeToBuffer(io_event* event){
    if (iowrapperDepth){
        return 0;
    }
    iowrapperDepth++;
    // if traceBuffer is full dump it
    if (sizeof(io_event) >= traceBuffer.size - traceBuffer.freeIdx){
        dumpBuffer(traceBuffer);
    }
    assert(sizeof(io_event) < traceBuffer.size - traceBuffer.freeIdx);

    char message[MAX_MESSAGE_SIZE];
    bzero(&message, MAX_MESSAGE_SIZE);
    sprintf(message, "class=%s, o_class=%hhd, h_class=%hhd, mode=%hhd, e_type=%s, h_id=%hd, flags=%d, source=%lld, size=%lld, offset=%lld\n\0",
            IOEventClassNames[event->class], event->offset_class, event->handle_class, event->mode, 
            IOEventNames[event->event_type], event->handle_id, event->flags, event->source, event->size, event->offset);
    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), message, strlen(message));
    // store the msg to the buffer
    //    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), event, sizeof(io_event));
    traceBuffer.freeIdx += strlen(message);//sizeof(io_event);

    iowrapperDepth--;
#ifdef PRELOAD_WRAPPERS
    dumpBuffer(traceBuffer);
#endif
    return traceBuffer.freeIdx;
}

int32_t checkFileName(char* filename){
    return 0;
}

// do any initialization here
// NOTE: on static-linked binaries, calling any functions from here will cause some problems
int32_t initwrapper(int32_t* indexLoc, char** fNames, int32_t* lNum){
    // at each call site we will put the index of the originating point in this location
    currentSiteIndex = indexLoc;

    fileNames = fNames;
    lineNumbers = lNum;
}

// do any cleanup here
int32_t finishwrapper(){
    PRINT_INSTR(stdout, "Finishing IO Trace for task %d, dumping buffer", __taskid);
    dumpBuffer(&traceBuffer);
    fclose(traceBuffer.outFile);
}

#include <IOEvents.c>
