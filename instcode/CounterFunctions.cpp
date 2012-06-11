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

void print_counter_array(FILE* stream, CounterArray* ctrs, pthread_t tid){
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
            fprintf(stream, "%ld\t", ctrs->Hashes[i]);
            fprintf(stream, "%lx\n", tid);
        }
    }
    fflush(stream);
}

void* generate_counter_array(void* args, uint32_t typ, pthread_key_t iid, pthread_t tid){
    CounterArray* ctrs = (CounterArray*)args;

    CounterArray* c = (CounterArray*)malloc(sizeof(CounterArray));
    assert(c);
    memcpy(c, ctrs, sizeof(CounterArray));
    c->threadid = tid;
    c->imageid = iid;
    c->Initialized = false;
    c->Counters = (uint64_t*)malloc(sizeof(uint64_t) * c->Size);
    bzero(c->Counters, sizeof(uint64_t) * c->Size);

    return (void*)c;
}

uint64_t ref_counter_array(void* args){
    CounterArray* ctrs = (CounterArray*)args;
    return (uint64_t)ctrs->Counters;
}

void delete_counter_array(void* args){
    CounterArray* ctrs = (CounterArray*)args;
    if (!ctrs->Initialized){
        free(ctrs->Counters);
        free(ctrs);
    }
}

void* tool_thread_init2(pthread_t tid){
    assert(alldata != NULL && "tool_image_init wasn't already called!?!");
    alldata->AddThread(tid);
    return NULL;
}

void* tool_thread_init(void* threadargs){
    tool_thread_args* x = (tool_thread_args*)threadargs;
    
    assert(alldata != NULL && "tool_image_init wasn't already called!?!");
    alldata->AddThread();

    x->start_function(x->function_args);
    free(threadargs);
    return NULL;
}

extern "C"
{
    void* tool_image_init(CounterArray* ctrs, uint64_t* key, ThreadData* td){
        assert(ctrs->Initialized == true);

        // on first visit create data manager
        if (alldata == NULL){
            alldata = new DataManager<CounterArray*>(generate_counter_array, delete_counter_array, ref_counter_array);
        }

        *key = alldata->AddImage(ctrs, td);
        ctrs->imageid = *key;
        ctrs->threadid = pthread_self();

        alldata->SetTimer(*key, 0);
        return NULL;
    }

    void* tool_image_fini(uint64_t* key){
        alldata->SetTimer(*key, 1);

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
        CounterArray* ctrs = (CounterArray*)alldata->GetData(*key, pthread_self());
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
        
        PRINT_INSTR(stdout, "*** Instrumentation Summary ****");
        PRINT_INSTR(stdout, "%ld blocks; printing those with at least %d executions to file %s", ctrs->Size, PRINT_MINIMUM, outFileName);
        
        fprintf(outFile, "# appname   = %s\n", ctrs->Application);
        fprintf(outFile, "# extension = %s\n", ctrs->Extension);
        fprintf(outFile, "# phase     = %d\n", 0);
        fprintf(outFile, "# rank      = %d\n", getTaskId());
        fprintf(outFile, "# perinsn   = %s\n", USES_STATS_PER_INSTRUCTION);
        fprintf(outFile, "# imageid   = %ld\n", *key);
        fprintf(outFile, "# cntimage  = %d\n", alldata->CountImages());
        fprintf(outFile, "# mainthread= %lx\n", pthread_self());
        fprintf(outFile, "# cntthread = %d\n", alldata->CountThreads());
        
        fprintf(outFile, "#id\tcount\t#file:line\tfunc\thash\tthreadid\n");
        fflush(outFile);

        // this wastes tons of space in the meta.jbbinst file, need to think of a better output format.
        for (set<pthread_t>::iterator it = alldata->allthreads.begin(); it != alldata->allthreads.end(); it++){
            ctrs = alldata->GetData(*key, (*it));
            assert(ctrs);
            print_counter_array(outFile, ctrs, (*it));
        }
        fflush(outFile);
        fclose(outFile);
        
        PRINT_INSTR(stdout, "cxxx Total Execution time for image: %f", alldata->GetTimer(*key, 1) - alldata->GetTimer(*key, 0));
        alldata->RemoveImage(*key);
        return NULL;
    }
};

