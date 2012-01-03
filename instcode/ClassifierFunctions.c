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

#define BINS_NUMBER (27)
#define INSTRUCTIONS_THRESHOLD (1000000)

static FILE* traceFile = NULL;
static uint64_t* bins = NULL;
static uint64_t* instructionCount = NULL;
static int last = 0;

void bins_entry_function(uint64_t* bufferStore, uint64_t* instructionsCountStore, char* fileName)
{
    ptimer(&pebiltimers[0]);

    int rank = 0;
#ifdef MPI_INIT_REQUIRED
    if (!isMpiValid()){
        PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print files", getpid());
        return -1;
    }
    else
        rank = getTaskId();
#endif

    char* offset = strrchr(fileName,'.');
    sprintf(offset-4,"%04d", rank); *offset = '.';
    traceFile = fopen(fileName, "w");
    if (!traceFile){
        fprintf(stderr, "Cannot open trace file %s, exiting...\n", fileName);
        fflush(stderr);
        exit(-1);
    }

    offset[-5] = '\0';
    fprintf(traceFile, "# appname   = %s\n", fileName);
    fprintf(traceFile, "# extension = %s\n", offset+1);
    fprintf(traceFile, "# rank      = %d\n", rank);

    bins = bufferStore;
    instructionCount = instructionsCountStore;
    bzero(bins, sizeof(uint64_t)*BINS_NUMBER);
}

void bins_classify_function()
{
    int b;
    for(b = 0; b < BINS_NUMBER; ++b){
        fprintf(traceFile, " %d", bins[b]);
    }
    fprintf(traceFile, "\n");
    bzero(bins, sizeof(uint64_t)*BINS_NUMBER);
    *instructionCount = 0;
}

void bins_exit_function()
{
    if(*instructionCount)
        bins_classify_function();

    PRINT_INSTR(stdout, "*** Instrumentation Summary ****");

    fflush(traceFile);
    fclose(traceFile);

    ptimer(&pebiltimers[1]);
    PRINT_INSTR(stdout, "cxxx Total Execution time: %f", pebiltimers[1] - pebiltimers[0]);
}
