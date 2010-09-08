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

#define NUM_PRINT 10

int64_t ticksPerSecond;

struct funcInfo* funcInfos = NULL;
int32_t numberOfFunctions = 0;
char** functionNames = NULL;

struct funcInfo
{
    uint64_t   count;
    uint64_t   timer_start;
    uint64_t   timer_total;
    uint64_t   idx;
};

void printFunctionInfo(int i){
#ifdef EXCLUDE_TIMER
    PRINT_INSTR(stdout, "%s: %lld executions", functionNames[funcInfos[i].idx], funcInfos[i].count);    
#else
    PRINT_INSTR(stdout, "%s: %lld executions, %.6f seconds", functionNames[funcInfos[i].idx], funcInfos[i].count, ((double)((double)funcInfos[i].timer_total/(double)ticksPerSecond)));
#endif
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

int32_t program_entry(int32_t* numFunctions, char** funcNames){
    int i;
    numberOfFunctions = *numFunctions;
    functionNames = funcNames;

    ticksPerSecond = CLOCK_RATE_HZ;
    PRINT_INSTR(stdout, "%lld ticks per second", ticksPerSecond);

    if (!funcInfos){
        PRINT_DEBUG("allocating %d funcInfos", numberOfFunctions);

        funcInfos = malloc(sizeof(struct funcInfo) * numberOfFunctions);
        bzero(funcInfos, sizeof(struct funcInfo) * numberOfFunctions);
    }

    for (i = 0; i < numberOfFunctions; i++){
        funcInfos[i].idx = i;
    }

    assert(funcInfos);
}

int32_t program_exit(){
    PRINT_INSTR(stdout, "Printing the %d most time-consuming function calls", NUM_PRINT);
    int32_t i, j;

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

int32_t function_entry(int64_t* functionIndex){
    //    PRINT_INSTR(stdout, "%s entr", functionNames[*functionIndex]);

    int64_t currentRecord = *functionIndex;
#ifdef EXCLUDE_TIMER
    funcInfos[currentRecord].timer_start = 0;
#else
    funcInfos[currentRecord].timer_start = readtsc();
#endif
}

int32_t function_exit(int64_t* functionIndex){
    uint64_t tstop;
#ifdef EXCLUDE_TIMER
    tstop = 1;
#else
    tstop = readtsc();
#endif
    int64_t currentRecord = *functionIndex;
    
    int64_t tadd = tstop - funcInfos[currentRecord].timer_start;
    //    PRINT_INSTR(stdout, "%s exit -- add %lld", functionNames[*functionIndex], tadd);
    funcInfos[currentRecord].count++;
    funcInfos[currentRecord].timer_total += tadd;
}
