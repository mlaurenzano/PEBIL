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

#include <InstrumentationCommon.h>

#define PRINT_MINIMUM 1

int32_t numberOfBasicBlocks;
int32_t* lineNumbers;
char** fileNames;
char** functionNames;
int64_t* hashValues;

int32_t numberOfLoops;
int32_t* loopLineNumbers;
char** loopFileNames;
char** loopFunctionNames;
int64_t* loopHashValues;

void tool_mpi_init(){}

int32_t functioncounter(int32_t* numFunctions, int32_t* functionCounts, char** functionNames){
    int32_t i;

    PRINT_INSTR(stdout, "*** Instrumentation Summary ****");
    PRINT_INSTR(stdout, "raw args: %x %x %x", numFunctions, functionCounts, functionNames);
    PRINT_INSTR(stdout, "There are %d functions in the code:", *numFunctions);
    PRINT_INSTR(stdout, "Printing functions with at least %d executions", PRINT_MINIMUM);

    for (i = 0; i < *numFunctions; i++){
        if (functionCounts[i] >= PRINT_MINIMUM){
            PRINT_INSTR(stdout, "\tFunction(%d) %.80s executed %d times", i, functionNames[i], functionCounts[i]);
        }
    }

    return i;
}

int32_t initcounter(int32_t* numBlocks, int32_t* lineNums, char** fileNms, char** functionNms, int64_t* hashVals){
    numberOfBasicBlocks = *numBlocks;
    lineNumbers = lineNums;
    fileNames = fileNms;
    functionNames = functionNms;
    hashValues = hashVals;

    ptimer(&pebiltimers[0]);
}

int32_t initloop(int32_t* numLoops, int32_t* lineNums, char** fileNms, char** functionNms, int64_t* hashVals){
    numberOfLoops = *numLoops;
    loopLineNumbers = lineNums;
    loopFileNames = fileNms;
    loopFunctionNames = functionNms;
    loopHashValues = hashVals;
}

int32_t blockcounter(uint64_t* blockCounts, char* appName, char* instExt){
    int32_t i;

    ptimer(&pebiltimers[1]);

#ifdef MPI_INIT_REQUIRED
    if (!isTaskValid()){
        PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print files", getpid());
        return -1;
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
        if (blockCounts[i] >= PRINT_MINIMUM){
            fprintf(outFile, "%#d\t", i);
            fprintf(outFile, "%llu\t#", blockCounts[i]);
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

int32_t loopcounter(uint64_t* loopCounters, char* appName, char* instExt){
    int32_t i;

#ifdef MPI_INIT_REQUIRED
    if (!isTaskValid()){
        PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print files", getpid());
        return -1;
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

    fprintf(outFile, "#id\tcount\t#file:line\tfunc\thash\n");
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

