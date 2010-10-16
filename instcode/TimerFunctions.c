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

#include <InstrumentationCommon.h>

#define RECORDS_PER_FUNCTION 8
#define STACK_BACKTRACE_SIZE  8
#define NUM_PRINT 10

int64_t ticksPerSecond;

struct funcInfo* backtraceInfo = NULL;
int32_t numberOfBacktraces = 0;
int32_t numberOfFunctions = 0;
char** functionNames = NULL;
int32_t* stackError = NULL;
#define CURRENT_STACK_BACKTRACE (&funcStack[stackIdx - STACK_MIN_ALLOWED])
#define HASH_STACK(__idx) hashFunction(CURRENT_STACK_BACKTRACE, STACK_BACKTRACE_SIZE)
#define INDEX_RECORD(__val) (__val & hashmask(numberOfBacktraces))

struct funcInfo
{
    uint64_t   count;
    uint64_t   timer_start;
    uint64_t   timer_total;
    uint64_t   hash;
    int32_t    backtrace[STACK_BACKTRACE_SIZE]; // backtrace[STACK_BACKTRACE_SIZE - 1] holds the function index
};
#define FUNCTION_INDEX (STACK_BACKTRACE_SIZE - 1)

// we have a local copy of the call stack
#define MAX_STACK_IDX 0x100000
#define STACK_MIN_ALLOWED (STACK_BACKTRACE_SIZE - 1)
int32_t* funcStack;
int32_t  stackIdx = STACK_MIN_ALLOWED;

#define hashmask(__n) (__n - 1)
#define hashrotate(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#define hashmix(a, b, c) \
    { \
    a -= c;  a ^= hashrotate(c, 4);  c += b; \
    b -= a;  b ^= hashrotate(a, 6);  a += c; \
    c -= b;  c ^= hashrotate(b, 8);  b += a; \
    a -= c;  a ^= hashrotate(c,16);  c += b; \
    b -= a;  b ^= hashrotate(a,19);  a += c; \
    c -= b;  c ^= hashrotate(b, 4);  b += a; \
    }
#define hashfinal(a,b,c) \
    { \
    c ^= b; c -= hashrotate(b,14); \
    a ^= c; a -= hashrotate(c,11); \
    b ^= a; b -= hashrotate(a,25); \
    c ^= b; c -= hashrotate(b,16); \
    a ^= c; a -= hashrotate(c,4);  \
    b ^= a; b -= hashrotate(a,14); \
    c ^= b; c -= hashrotate(b,24); \
    }
/*
--------------------------------------------------------------------
 This works on all machines.  To be useful, it requires
 -- that the key be an array of uint32_t's, and
 -- that the length be the number of uint32_t's in the key

 The function hashword() is identical to hashlittle() on little-endian
 machines, and identical to hashbig() on big-endian machines,
 except that the length has to be measured in uint32_ts rather than in
 bytes.  hashlittle() is more complicated than hashword() only because
 hashlittle() has to dance around fitting the key bytes into registers.
--------------------------------------------------------------------
*/
const uint32_t initval = 0; // this needs to not change if we want repeatable hash lookups
uint32_t hashFunction(const uint32_t *k, size_t length) 
{
    uint32_t a,b,c;
    /* Set up the internal state */
    a = b = c = 0xdeadbeef + (((uint32_t)length)<<2) + initval;
    /*------------------------------------------------- handle most of the key */
    while (length > 3){
        a += k[0];
        b += k[1];
        c += k[2];
        hashmix(a,b,c);
        length -= 3;
        k += 3;
    }
    /*------------------------------------------- handle the last 3 uint32_t's */
    switch(length)                     /* all the case statements fall through */
    { 
        case 3 : c+=k[2];
        case 2 : b+=k[1];
        case 1 : a+=k[0];
            hashfinal(a,b,c);
        case 0:     /* case 0: nothing left to add */
            break;
    }
    /*------------------------------------------------------ report the result */
    return c;
}

uint32_t getRecordIndex(){
    uint64_t hashCode = HASH_STACK(stackIdx);
    uint32_t idx = INDEX_RECORD(hashCode);
    while (backtraceInfo[idx].hash && hashCode != backtraceInfo[idx].hash){
        idx++;
        idx = INDEX_RECORD(idx);
    }
    return idx;
}

int32_t funcStack_push(int32_t n){
    assert(n < MAX_STACK_IDX);
    funcStack[++stackIdx] = n;
}

int32_t funcStack_pop(){
    assert(stackIdx >= STACK_MIN_ALLOWED);
    return funcStack[stackIdx--];
}

int32_t funcStack_peep(int32_t b){
    if (stackIdx - b < 0){
        return -1;
    }
    return funcStack[stackIdx-b];
}

void funcStack_print(){
    int i;
    for (i = STACK_MIN_ALLOWED; i <= stackIdx; i++){
        fprintf(stdout, "%d ", funcStack[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

void printFunctionInfo(int i){
    int j;
#ifdef EXCLUDE_TIMER
    PRINT_INSTR(stdout, "%s (%lld): %lld executions", functionNames[backtraceInfo[i].backtrace[FUNCTION_INDEX]], INDEX_RECORD(backtraceInfo[i].hash), backtraceInfo[i].count);    
#else
    PRINT_INSTR(stdout, "%s (%lld): %lld executions, %.6f seconds", functionNames[backtraceInfo[i].backtrace[FUNCTION_INDEX]], INDEX_RECORD(backtraceInfo[i].hash), backtraceInfo[i].count, ((double)((double)backtraceInfo[i].timer_total/(double)ticksPerSecond)));
#endif
    for (j = FUNCTION_INDEX - 1; j >= 0; j--){
        if (backtraceInfo[i].backtrace[j] >= 0){
            if (stackError[backtraceInfo[i].backtrace[j]]){
                PRINT_INSTR(stdout, "\t^%d\t-e-\t%s", FUNCTION_INDEX - j, functionNames[backtraceInfo[i].backtrace[j]]);
            } else {
                PRINT_INSTR(stdout, "\t^%d\t\t%s", FUNCTION_INDEX - j, functionNames[backtraceInfo[i].backtrace[j]]);
            }
        }
    }
}

int compareFuncInfos(const void* f1, const void* f2){
    struct funcInfo* func1 = (struct funcInfo*)f1;
    struct funcInfo* func2 = (struct funcInfo*)f2;

    if (func1->timer_total > func2->timer_total){
        return -1;
    } else if (func1->timer_total < func2->timer_total){
        return 1;
    }
    return 0;
}

uint32_t nextPowerOfTwo(int32_t n){
    int32_t p = 1;
    while (n - p > 0){
        p = p << 1;
    }
    return p;
}

int32_t program_entry(int32_t* numFunctions, char** funcNames){
    int i;
    numberOfFunctions = *numFunctions;
    functionNames = funcNames;

    ticksPerSecond = CLOCK_RATE_HZ;
    PRINT_DEBUG("%lld ticks per second", ticksPerSecond);

    assert(!backtraceInfo);

    numberOfBacktraces = nextPowerOfTwo(numberOfFunctions * RECORDS_PER_FUNCTION);
    PRINT_INSTR(stdout, "%d -> %d backtraces", numberOfFunctions * RECORDS_PER_FUNCTION, nextPowerOfTwo(numberOfFunctions * RECORDS_PER_FUNCTION));

    backtraceInfo = (struct funcInfo*)malloc(sizeof(struct funcInfo) * numberOfBacktraces);
    bzero(backtraceInfo, sizeof(struct funcInfo) * numberOfBacktraces);

    funcStack = (int32_t*)malloc(sizeof(int32_t) * MAX_STACK_IDX);
    bzero(funcStack, sizeof(int32_t) * MAX_STACK_IDX);
    for (i = 0; i < STACK_BACKTRACE_SIZE; i++){
        funcStack[i] = -1;
    }

    stackError = (int32_t*)malloc(sizeof(int32_t) * numberOfFunctions);
    bzero(stackError, sizeof(int32_t) * numberOfFunctions);

    assert(backtraceInfo);
}

int32_t program_exit(){
    PRINT_INSTR(stdout, "===========================================================================");
    PRINT_INSTR(stdout, "Printing the %d most time-consuming call paths + up to %d stack frames", NUM_PRINT, STACK_BACKTRACE_SIZE);
    PRINT_INSTR(stdout, "===========================================================================");
    int32_t i, j;

    uint32_t collisions = 0;
    for (i = 0; i < numberOfBacktraces - 1; i++){
        if (backtraceInfo[i].hash && backtraceInfo[i].hash == backtraceInfo[i+1].hash){
            collisions++;
        }
    }

    qsort((void*)backtraceInfo, numberOfBacktraces, sizeof(struct funcInfo), compareFuncInfos);

    int32_t numUsed = 0;
    for (i = 0; i < numberOfBacktraces; i++){
        if (backtraceInfo[i].count){
            if (numUsed < NUM_PRINT){
                printFunctionInfo(i);
            }
            numUsed++;
        }
    }

    for (i = 0; i < numberOfFunctions; i++){
        if (stackError[i]){
            PRINT_INSTR(stdout, "Function %s timer failed (exit not taken) -- execution count %d", functionNames[i], stackError[i]);
        }
    }

    PRINT_INSTR(stdout, "Hash Table usage = %d of %d entries (%.3f), collisions: %d (%.3f)", numUsed, numberOfBacktraces, ((double)((double)numUsed/((double)(numberOfBacktraces)))), collisions, (double)(((double)collisions)/((double)numUsed)));

}

int32_t function_entry(int64_t* functionIndex){
    return 0;
    int i, j;

    funcStack_push(*functionIndex);
    int32_t currentRecord = getRecordIndex();

    // initialize this backtrace's record
    if (!backtraceInfo[currentRecord].hash){
        backtraceInfo[currentRecord].hash = HASH_STACK(stackIdx);

        PRINT_DEBUG("hash[%d](idx %d) = %#llx", currentRecord, *functionIndex, backtraceInfo[currentRecord].hash);
        memcpy(&backtraceInfo[currentRecord].backtrace[0], CURRENT_STACK_BACKTRACE, STACK_BACKTRACE_SIZE * sizeof(int32_t));
    }

#ifdef EXCLUDE_TIMER
    backtraceInfo[currentRecord].timer_start = 0;
#else
    backtraceInfo[currentRecord].timer_start = read_timestamp_counter();
#endif
}

int32_t function_exit(int64_t* functionIndex){
    return 0;
    int64_t tstop;
#ifdef EXCLUDE_TIMER
    tstop = 1;
#else
    tstop = read_timestamp_counter();
#endif

    int32_t currentRecord = getRecordIndex();

    int32_t popped = funcStack_pop();
    if (popped != *functionIndex){
        while (popped != *functionIndex){
            stackError[popped]++;
            popped = funcStack_pop();
        }
        funcStack_push(popped);
        currentRecord = getRecordIndex();
        popped = funcStack_pop();
    }
    int64_t tadd = tstop - backtraceInfo[currentRecord].timer_start;
    backtraceInfo[currentRecord].count++;
    backtraceInfo[currentRecord].timer_total += tadd;
}
