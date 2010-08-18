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
TraceBuffer_t traceBuffer = { NULL, __IO_BUFFER_SIZE, 0, 0 };
uint32_t callDepth = 0;
IOFileName_t filereg;
uint64_t eventIndex = 0;
uint32_t fileRegSeq = 0x400;

// the dump function makes IO calls. so we must protect from an infinite recursion by not entering
// any buffer function if one is already stacked
uint32_t iowrapperDepth = 0;
#define CALL_DEPTH_ENTER(__var) if (__var) { return 0; } __var++;
#define CALL_DEPTH_EXIT(__var) __var--;

// to handle timers
int64_t timerstart;
int64_t timerstop;
#define TIMER_START (timerstart = readtsc())
#define TIMER_STOP  (timerstop  = readtsc())
#define TIMER_VALUE (timerstop - timerstart)
#define TIMER_EXECUTE(__stmts) TIMER_START; __stmts TIMER_STOP; 
#define PRINT_TIMER(__file) PRINT_INSTR(__file, "timer value (in cycles): %lld", TIMER_VALUE)

extern void printInitInfo(FILE* file);

uint32_t dumpBuffer(){
    CALL_DEPTH_ENTER(iowrapperDepth);

    if (traceBuffer.outFile == NULL){
        char fname[__MAX_STRING_SIZE];
        sprintf(fname, "pebiliotrace.%d.log", __taskid);
        traceBuffer.outFile = fopen(fname, "w");

        printInitInfo(traceBuffer.outFile);
    }

    uint32_t oerr = fwrite(traceBuffer.storage, sizeof(char), traceBuffer.freeIdx, traceBuffer.outFile);
    assert(oerr == traceBuffer.freeIdx);
    traceBuffer.freeIdx = 0;

    assert(traceBuffer.freeIdx == 0);
    CALL_DEPTH_EXIT(iowrapperDepth);

    return traceBuffer.freeIdx;
}

uint32_t storeRecord(uint8_t type, uint32_t size){

    // RECORD_HEADER(type, size)
    char message[__MAX_MESSAGE_SIZE];
    bzero(&message, __MAX_MESSAGE_SIZE);
    sprintf(message, "type %hhd\tsize %d\n", type, size);
    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), message, strlen(message));
    traceBuffer.freeIdx += strlen(message);//sizeof(EventInfo_t);

#ifdef PRELOAD_WRAPPERS
    dumpBuffer(traceBuffer);
#endif

    return 0;
}

uint32_t storeFileName(char* name, uint32_t handle, uint8_t class, uint8_t type, uint8_t protect){
    if (protect) { CALL_DEPTH_ENTER(iowrapperDepth); }

    bzero(&filereg, sizeof(IOFileName_t));
    filereg.handle_class = class;
    filereg.access_type = type;
    filereg.numchars = strlen(name)+1;
    if (class == IOFileAccess_ONCE){
        filereg.handle = fileRegSeq++;
    } else {
        filereg.handle = handle;
    }
    filereg.event_id = eventIndex;

    storeRecord(IORecord_FileName, sizeof(IOFileName_t) + filereg.numchars);

    char message[__MAX_MESSAGE_SIZE];
    bzero(&message, __MAX_MESSAGE_SIZE);
    sprintf(message, "\tunqid %5lld: h_class %hhd\ta_type %hhd\tnumchars %d\thandle %d name %s\n",
            filereg.event_id, filereg.handle_class, filereg.access_type, filereg.numchars, filereg.handle, name);

    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), message, strlen(message));
    traceBuffer.freeIdx += strlen(message);//sizeof(EventInfo_t);
    if (protect) { CALL_DEPTH_EXIT(iowrapperDepth); }

#ifdef PRELOAD_WRAPPERS
    dumpBuffer(traceBuffer);
#endif

    return filereg.handle;
}

uint32_t storeEventInfo(EventInfo_t* event){
    CALL_DEPTH_ENTER(iowrapperDepth);

    // if traceBuffer is full dump it

    if (sizeof(EventInfo_t) >= traceBuffer.size - traceBuffer.freeIdx){
        dumpBuffer(traceBuffer);
    }
    assert(sizeof(EventInfo_t) < traceBuffer.size - traceBuffer.freeIdx);

    storeRecord(IORecord_EventInfo, sizeof(EventInfo_t));

    char message[__MAX_MESSAGE_SIZE];
    bzero(&message, __MAX_MESSAGE_SIZE);
    event->unqid = eventIndex;
    sprintf(message, "\tunqid %5lld: class=%s, o_class=%s, h_class=%hhd, mode=%hhd, e_type=%s, h_id=%hd, source=%lld, size=%lld, offset=%lld\n\0",
            event->unqid, IOEventClassNames[event->class], IOOffsetClassNames[event->offset_class], event->handle_class, event->mode, 
            IOEventNames[event->event_type], event->handle_id, event->source, event->size, event->offset);
    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), message, strlen(message));
    // store the msg to the buffer
    //    memcpy(&(traceBuffer.storage[traceBuffer.freeIdx]), event, sizeof(EventInfo_t));
    traceBuffer.freeIdx += strlen(message);//sizeof(EventInfo_t);

    eventIndex++;
    CALL_DEPTH_EXIT(iowrapperDepth);

#ifdef PRELOAD_WRAPPERS
    dumpBuffer(traceBuffer);
#endif
    return traceBuffer.freeIdx;
}

void printInitInfo(FILE* file){
    storeFileName("stdout", stdout->_fileno, IOEventClass_CLIB, IOFileAccess_SYS, 0);
    storeFileName("stderr", stderr->_fileno, IOEventClass_CLIB, IOFileAccess_SYS, 0);
    storeFileName("stdin", stdin->_fileno, IOEventClass_CLIB, IOFileAccess_SYS, 0);
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

#ifdef PRELOAD_WRAPPERS
int __libc_start_main(int *(main) (int, char * *, char * *), int argc, char * * ubp_av, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void (* stack_end)){
    PRINT_INSTR(stdout, "Program loaded using pebil IO tracing library, PRELOAD_WRAPPERS version");

    static int *(*__libc_start_main_ptr)(int *(main) (int, char**, char**), int argc, char** ubp_av, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void (*stack_end));
    __libc_start_main_ptr = dlsym(RTLD_NEXT, "__libc_start_main");
    int retval = __libc_start_main_ptr(main, argc, ubp_av, init, fini, rtld_fini, stack_end);

    PRINT_INSTR(stdout, "Exxit wrap program");

    return retval;
}
#endif // PRELOAD_WRAPPERS

#include <IOEvents.c>
