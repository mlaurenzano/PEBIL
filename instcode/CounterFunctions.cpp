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
#include <map>

#include <InstrumentationCommon.hpp>
#include <CounterFunctions.hpp>

#define PRINT_MINIMUM 1

static DataManager<CounterArray*>* alldata = NULL;

void print_counter_array(FILE* stream, CounterArray* ctrs){
    if (ctrs == NULL){
        return;
    }

    for (uint32_t i = 0; i < ctrs->Size; i++){
        if (ctrs->Counters[i] >= PRINT_MINIMUM){
            fprintf(stream, "%ld\t", i);
            fprintf(stream, "%lu\t#", ctrs->Counters[i]);
            fprintf(stream, "%s:", ctrs->Files[i]);
            fprintf(stream, "%d\t", ctrs->Lines[i]);
            fprintf(stream, "%s\t", ctrs->Functions[i]);
            fprintf(stream, "%ld\n", ctrs->Hashes[i]);
        }
    }
    fflush(stream);
}

void* generate_counter_array(void* args){
    CounterArray* ctrs = (CounterArray*)args;

    CounterArray* c = (CounterArray*)malloc(sizeof(CounterArray));
    memcpy(c, ctrs, sizeof(CounterArray));
    c->Counters = (uint64_t*)malloc(sizeof(uint64_t) * c->Size);

    return (void*)c;
}

void* tool_thread_init(void* threadargs){
    tool_thread_args* x = (tool_thread_args*)threadargs;
    PRINT_INSTR(stdout, "Hooked pthread_create for thread id %p", (void*)pthread_self());
    x->start_function(x->function_args);
    free(threadargs);
    return NULL;
}

extern "C"
{
    void* tool_image_init(CounterArray* ctrs, uint64_t* key){
        assert(ctrs->Initialized == true);

        if (alldata == NULL){
            alldata = new DataManager<CounterArray*>(generate_counter_array);
        }

        *key = alldata->AddImage(ctrs);

        ptimer(&pebiltimers[0]);
        return NULL;
    }

    void* tool_image_fini(uint64_t* key){
        uint64_t i;
        ptimer(&pebiltimers[1]);

#ifdef MPI_INIT_REQUIRED
        if (!isMpiValid()){
            PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print jbbinst files", getpid());
            return NULL;
        }
#endif
        if (alldata == NULL){
            PRINT_INSTR(stderr, "data manager does not exist. no images were initialized");
            return NULL;
        }
        CounterArray* ctrs = (CounterArray*)alldata->GetData(pthread_self(), *key);
        if (ctrs == NULL){
            PRINT_INSTR(stderr, "Cannot retreive image data using key %ld", *key);
            return NULL;
        }
        
        char outFileName[__MAX_STRING_SIZE];
        sprintf(outFileName, "%s.meta_%04d.%s", ctrs->Application, getTaskId(), ctrs->Extension);
        FILE* outFile = fopen(outFileName, "w");
        if (!outFile){
            fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
            fflush(stderr);
            exit(-1);
        }
        
        fprintf(outFile, "# appname   = %s\n", ctrs->Application);
        fprintf(outFile, "# extension = %s\n", ctrs->Extension);
        fprintf(outFile, "# phase     = %d\n", 0);
        fprintf(outFile, "# rank      = %d\n", getTaskId());
        fprintf(outFile, "# perinsn   = %s\n", USES_STATS_PER_INSTRUCTION);
        
        fprintf(outFile, "#id\tcount\t#file:line\tfunc\thash\n");
        fflush(outFile);

        PRINT_INSTR(stdout, "*** Instrumentation Summary ****");
        PRINT_INSTR(stdout, "%ld blocks; printing those with at least %d executions to file %s", ctrs->Size, PRINT_MINIMUM, outFileName);
        
        print_counter_array(outFile, ctrs);
        fflush(outFile);
        fclose(outFile);
        
        PRINT_INSTR(stdout, "cxxx Total Execution time: %f", pebiltimers[1] - pebiltimers[0]);
        return NULL;
    }
};

