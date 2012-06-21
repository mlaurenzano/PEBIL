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
#include <CacheSimulationCommon.hpp>
#include <CounterFunctions.hpp>

#define PRINT_MINIMUM 1

static DataManager<CounterArray*>* alldata = NULL;

void print_loop_array(FILE* stream, CounterArray* ctrs, pthread_t tid){
    if (ctrs == NULL){
        return;
    }

    for (uint32_t i = 0; i < ctrs->Size; i++){
        uint32_t idx;
        if (ctrs->Types[i] == CounterType_basicblock){
            continue;
        } else if (ctrs->Types[i] == CounterType_instruction){
            continue;
        } else if (ctrs->Types[i] == CounterType_loop){
            idx = i;
        } else {
            assert(false && "unsupported counter type");
        }
        if (ctrs->Counters[idx] >= PRINT_MINIMUM){
            fprintf(stream, "%ld\t", ctrs->Hashes[i]);
            fprintf(stream, "%lu\t#", ctrs->Counters[idx]);
            fprintf(stream, "%s:", ctrs->Files[i]);
            fprintf(stream, "%d\t", ctrs->Lines[i]);
            fprintf(stream, "%s\t", ctrs->Functions[i]);
            fprintf(stream, "%ld\t", ctrs->Hashes[i]);
            fprintf(stream, "%#lx\t", ctrs->Addresses[i]);
            fprintf(stream, "%lx\n", tid);
        }
    }
    fflush(stream);
}

void print_counter_array(FILE* stream, CounterArray* ctrs, pthread_t tid){
    if (ctrs == NULL){
        return;
    }

    for (uint32_t i = 0; i < ctrs->Size; i++){
        uint32_t idx;
        if (ctrs->Types[i] == CounterType_basicblock){
            idx = i;
        } else if (ctrs->Types[i] == CounterType_instruction){
            idx = ctrs->Counters[i];
        } else if (ctrs->Types[i] == CounterType_loop){
            continue;
        } else {
            assert(false && "unsupported counter type");
        }
        if (ctrs->Counters[idx] >= PRINT_MINIMUM){
            fprintf(stream, "%d\t", i);
            fprintf(stream, "%lu\t#", ctrs->Counters[idx]);
            fprintf(stream, "%s:", ctrs->Files[i]);
            fprintf(stream, "%d\t", ctrs->Lines[i]);
            fprintf(stream, "%#lx\t", ctrs->Addresses[i]);
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

    // keep all instruction CounterTypes in place
    memcpy(c->Counters, ctrs->Counters, sizeof(uint64_t) * c->Size);
    for (uint32_t i = 0; i < c->Size; i++){
        if (c->Types[i] != CounterType_instruction){
            c->Counters[i] = 0;
        }
    }

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

void* tool_thread_init(pthread_t tid){
    if (alldata){
        alldata->AddThread(tid);
    } else {
        PRINT_INSTR(stderr, "Calling PEBIL thread initialization library for thread %lx but no images have been initialized.", tid);
    }
    return NULL;
}

extern "C"
{
    void* tool_mpi_init(){
        return NULL;
    }

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

        // PRINT BLOCK/INSTRUCTION COUNTERS
        char outFileName[__MAX_STRING_SIZE];
        sprintf(outFileName, "%s.meta_%04d.%s", ctrs->Application, getTaskId(), ctrs->Extension);
        FILE* outFile = fopen(outFileName, "w");
        if (!outFile){
            fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
            fflush(stderr);
            exit(-1);
        }
        
        PRINT_INSTR(stdout, "*** Instrumentation Summary ****");
        uint32_t countBlocks = 0;
        uint32_t countLoops = 0;
        for (uint32_t i = 0; i < ctrs->Size; i++){
            if (ctrs->Types[i] == CounterType_basicblock){
                countBlocks++;
            } else if (ctrs->Types[i] == CounterType_loop){
                countLoops++;
            }
        }

        PRINT_INSTR(stdout, "%ld blocks; printing those with at least %d executions to file %s", countBlocks, PRINT_MINIMUM, outFileName);
        
        fprintf(outFile, "# appname   = %s\n", ctrs->Application);
        fprintf(outFile, "# extension = %s\n", ctrs->Extension);
        fprintf(outFile, "# phase     = %d\n", 0);
        fprintf(outFile, "# rank      = %d\n", getTaskId());
        fprintf(outFile, "# perinsn   = %s\n", ctrs->PerInstruction? "yes" : "no");
        fprintf(outFile, "# imageid   = %ld\n", *key);
        fprintf(outFile, "# cntimage  = %d\n", alldata->CountImages());
        fprintf(outFile, "# mainthread= %lx\n", pthread_self());
        fprintf(outFile, "# cntthread = %d\n", alldata->CountThreads());
        
        fprintf(outFile, "#id\tcount\t#file:line\taddr\tfunc\thash\tthreadid\n");
        fflush(outFile);

        // this wastes tons of space in the meta.jbbinst file, need to think of a better output format.
        for (set<pthread_t>::iterator it = alldata->allthreads.begin(); it != alldata->allthreads.end(); it++){
            ctrs = alldata->GetData(*key, (*it));
            assert(ctrs);
            print_counter_array(outFile, ctrs, (*it));
        }
        fflush(outFile);
        fclose(outFile);

        // print loop counters
        sprintf(outFileName, "%s.meta_%04d.%s", ctrs->Application, getTaskId(), "loopcnt");
        outFile = fopen(outFileName, "w");
        if (!outFile){
            fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
            fflush(stderr);
            exit(-1);
        }
        
        PRINT_INSTR(stdout, "%ld loops; printing those with at least %d executions to file %s", countLoops, PRINT_MINIMUM, outFileName);

        fprintf(outFile, "# appname   = %s\n", ctrs->Application);
        fprintf(outFile, "# extension = %s\n", ctrs->Extension);
        fprintf(outFile, "# phase     = %d\n", 0);
        fprintf(outFile, "# rank      = %d\n", getTaskId());
        fprintf(outFile, "# perinsn   = %s\n", ctrs->PerInstruction? "yes" : "no");
        fprintf(outFile, "# imageid   = %ld\n", *key);
        fprintf(outFile, "# cntimage  = %d\n", alldata->CountImages());
        fprintf(outFile, "# mainthread= %lx\n", pthread_self());
        fprintf(outFile, "# cntthread = %d\n", alldata->CountThreads());
        
        fprintf(outFile, "#hash\tcount\t#file:line\tfunc\thash\taddr\tthreadid\n");
        fflush(outFile);
        for (set<pthread_t>::iterator it = alldata->allthreads.begin(); it != alldata->allthreads.end(); it++){
            ctrs = alldata->GetData(*key, (*it));
            assert(ctrs);
            print_loop_array(outFile, ctrs, (*it));
        }
        fflush(outFile);
        fclose(outFile);

        PRINT_INSTR(stdout, "cxxx Total Execution time for image: %f", alldata->GetTimer(*key, 1) - alldata->GetTimer(*key, 0));
        alldata->RemoveImage(*key);
        return NULL;
    }
};

