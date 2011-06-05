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

#define NUM_PRINT 50

int64_t ticksPerSecond;

struct funcInfo* funcInfos = NULL;
int32_t numberOfFunctions = 0;
char** functionNames = NULL;
uint32_t* functionIndexAddr;

#define STACK_SIZE (1024*1024)
struct stackRecord {
    uint64_t timer_start;
    uint32_t function_index;
};
struct stackRecord* indexStack;
int32_t stackPos = -1;

void pushIndex(uint32_t idx, uint64_t tmr){
    stackPos++;
    indexStack[stackPos].function_index = idx;
    indexStack[stackPos].timer_start = tmr;    
}

uint64_t popIndex(uint32_t idx, uint64_t tmr){
    while (indexStack[stackPos].function_index != idx){
        //        PRINT_INSTR(stdout, "stack[%d] = %d", stackPos, indexStack[stackPos].function_index);
        stackPos--;
    }
    //    PRINT_INSTR(stdout, "using stack pos %d", stackPos);
    stackPos--;
    return (tmr - indexStack[stackPos + 1].timer_start);
}

struct funcInfo {
    uint64_t   count;
    uint64_t   timer_total;
    uint32_t   name_index;
};

void printFunctionInfo(int i){
#ifdef EXCLUDE_TIMER
    PRINT_INSTR(stdout, "\t%s: %lld executions", functionNames[funcInfos[i].name_index], funcInfos[i].count);    
#else
    PRINT_INSTR(stdout, "\t%s: %lld executions, %.6f seconds", functionNames[funcInfos[i].name_index], funcInfos[i].count, ((double)((double)funcInfos[i].timer_total/(double)ticksPerSecond)));
#endif
}

void tool_mpi_init() {}

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

int32_t program_entry(int32_t* numFunctions, char** funcNames, uint32_t* idxAddr){
    int i;
    numberOfFunctions = *numFunctions;
    functionNames = funcNames;
    functionIndexAddr = idxAddr;
    
    ticksPerSecond = CLOCK_RATE_HZ;
    //    PRINT_INSTR(stdout, "%lld ticks per second", ticksPerSecond);

    if (!funcInfos){
        PRINT_DEBUG("allocating %d funcInfos", numberOfFunctions);

        funcInfos = malloc(sizeof(struct funcInfo) * numberOfFunctions);
        bzero(funcInfos, sizeof(struct funcInfo) * numberOfFunctions);
    }

    for (i = 0; i < numberOfFunctions; i++){
        //        PRINT_INSTR(stdout, "Function[%d] = %s", i, functionNames[i]);
        funcInfos[i].name_index = i;
    }

    assert(funcInfos);

    if (!indexStack){
        indexStack = malloc(sizeof(uint32_t) * STACK_SIZE);
    }
}

int32_t program_exit(){
    PRINT_INSTR(stdout, "Printing the %d most time-consuming function calls", NUM_PRINT);
    int32_t i;

    qsort((void*)funcInfos, numberOfFunctions, sizeof(struct funcInfo), compareFuncInfos);

    for (i = 0; i < numberOfFunctions; i++){
        if (funcInfos[i].count){
            if (i < NUM_PRINT){
                printFunctionInfo(i);
            }
        }
    }

    free(funcInfos);
}

int32_t function_entry(){
#ifdef EXCLUDE_TIMER
    funcInfos[*functionIndexAddr].count++;
#else
    pushIndex(*functionIndexAddr, read_timestamp_counter());
#endif
    //    PRINT_INSTR(stdout, "%s entry %d", functionNames[*functionIndexAddr], *functionIndexAddr);
}

int32_t function_exit(){
    register uint32_t currentRecord = *functionIndexAddr;
#ifndef EXCLUDE_TIMER
    //    PRINT_INSTR(stdout, "%s exit %d", functionNames[currentRecord], currentRecord);
    uint64_t tadd = popIndex(currentRecord, read_timestamp_counter());
    funcInfos[currentRecord].count++;
    funcInfos[currentRecord].timer_total += tadd;
#endif
    //    printFunctionInfo(currentRecord);
}
