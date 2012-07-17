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
#include <Simulation.hpp>
#include <CounterFunctions.hpp>

#define PRINT_MINIMUM 1

static DataManager<CounterArray*>* AllData = NULL;

void print_loop_array(FILE* stream, CounterArray* ctrs){
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
            fprintf(stream, "%#lx\n", ctrs->Addresses[i]);
        }
    }
    fflush(stream);
}

void print_counter_array(FILE* stream, CounterArray* ctrs){
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
            fprintf(stream, "%ld\n", ctrs->Hashes[i]);
        }
    }
    fflush(stream);
}

void* GenerateCounterArray(void* args, uint32_t typ, image_key_t iid, thread_key_t tid){
    CounterArray* ctrs = (CounterArray*)args;

    CounterArray* c = (CounterArray*)malloc(sizeof(CounterArray));
    assert(c);
    memcpy(c, ctrs, sizeof(CounterArray));
    c->threadid = tid;
    c->imageid = iid;
    c->Initialized = false;
    c->Master = false;
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

uint64_t RefCounterArray(void* args){
    CounterArray* ctrs = (CounterArray*)args;
    return (uint64_t)ctrs->Counters;
}

void DeleteCounterArray(void* args){
    CounterArray* ctrs = (CounterArray*)args;
    if (!ctrs->Initialized){
        free(ctrs->Counters);
        free(ctrs);
    }
}

void* tool_thread_init(thread_key_t tid){
    if (AllData){
        AllData->AddThread(tid);
    } else {
        ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
    }
    return NULL;
}

extern "C"
{
    void* tool_dynamic_init(){
        return NULL;
    }

    void* tool_mpi_init(){
        return NULL;
    }

    void* tool_image_init(void* s, uint64_t* key, ThreadData* td){
        CounterArray* ctrs = (CounterArray*)s;
        assert(ctrs->Initialized == true);

        // on first visit create data manager
        if (AllData == NULL){
            AllData = new DataManager<CounterArray*>(GenerateCounterArray, DeleteCounterArray, RefCounterArray);
        }

        *key = AllData->AddImage(ctrs, td, *key);
        ctrs->imageid = *key;
        ctrs->threadid = pthread_self();

        AllData->SetTimer(*key, 0);
        return NULL;
    }

    void* tool_image_fini(uint64_t* key){
        AllData->SetTimer(*key, 1);

#ifdef MPI_INIT_REQUIRED
        if (!IsMpiValid()){
            warn << "Process " << dec << getpid() << " did not execute MPI_Init, will not print execution count files" << ENDL;
            return NULL;
        }
#endif

        if (AllData == NULL){
            ErrorExit("data manager does not exist. no images were initialized", MetasimError_NoImage);
            return NULL;
        }
        CounterArray* ctrs = (CounterArray*)AllData->GetData(*key, pthread_self());
        if (ctrs == NULL){
            ErrorExit("Cannot retreive image data using key " << dec << (*key), MetasimError_NoImage);
            return NULL;
        }

        // print counters when the application exits (do nothing on other images)
        if (ctrs->Master){
            string bfile;
            bfile.append(ctrs->Application);
            bfile.append(".r");
            AppendRankString(bfile);
            bfile.append(".t");
            AppendTasksString(bfile);
            bfile.append(".");

            string lfile(bfile);
            bfile.append(ctrs->Extension);
            lfile.append("loopcnt");

            ofstream BlockFile;
            const char* b = bfile.c_str();
            TryOpen(BlockFile, b);

            ofstream LoopFile;
            const char* l = lfile.c_str();
            TryOpen(LoopFile, l);


            // tally up counter types
            uint32_t blockCount = 0;
            uint32_t loopCount = 0;                
            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                CounterArray* c = (CounterArray*)AllData->GetData((*iit), pthread_self());
                for (uint32_t i = 0; i < c->Size; i++){
                    if (c->Types[i] == CounterType_loop){
                        loopCount++;
                    } else if (c->Types[i] == CounterType_basicblock){
                        blockCount++;
                    } else if (c->Types[i] == CounterType_instruction){
                        blockCount++;
                    }
                }
            }

            inform << dec << blockCount << " blocks. print those with counts of at least " << dec << PRINT_MINIMUM << " to " << bfile << ENDL;
            inform << dec << loopCount << " loops. print those with counts of at least " << dec << PRINT_MINIMUM << " to " << lfile << ENDL;

            // print file headers
            BlockFile
                << "# appname         = " << ctrs->Application << ENDL
                << "# extension       = " << ctrs->Extension << ENDL
                << "# rank            = " << dec << GetTaskId() << ENDL
                << "# ntasks          = " << dec << GetNTasks() << ENDL
                << "# perinsn         = " << (ctrs->PerInstruction ? "yes" : "no") << ENDL
                << "# countimage      = " << dec << AllData->CountImages() << ENDL
                << "# countthread     = " << dec << AllData->CountThreads() << ENDL
                << "# masterthread    = " << dec << AllData->GetThreadSequence(pthread_self()) << ENDL;

            if (ctrs->PerInstruction){
                BlockFile << "# insncount       = " << dec << blockCount << ENDL;
            } else {
                BlockFile << "# blockcount      = " << dec << blockCount << ENDL;
            }
                
            LoopFile
                << "# appname         = " << ctrs->Application << ENDL
                << "# extension       = " << ctrs->Extension << ENDL
                << "# rank            = " << dec << GetTaskId() << ENDL
                << "# ntasks          = " << dec << GetNTasks() << ENDL
                << "# perinsn         = " << (ctrs->PerInstruction ? "yes" : "no") << ENDL
                << "# countimage      = " << dec << AllData->CountImages() << ENDL
                << "# countthread     = " << dec << AllData->CountThreads() << ENDL
                << "# masterthread    = " << dec << AllData->GetThreadSequence(pthread_self()) << ENDL
                << "# loopcount       = " << dec << loopCount << ENDL;

            // print image summaries
            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                CounterArray* c = (CounterArray*)AllData->GetData((*iit), pthread_self());

                blockCount = 0;
                loopCount = 0;
                for (uint32_t i = 0; i < c->Size; i++){
                    if (c->Types[i] == CounterType_loop){
                        loopCount++;
                    } else if (c->Types[i] == CounterType_basicblock){
                        blockCount++;
                    } else if (c->Types[i] == CounterType_instruction){
                        blockCount++;
                    }
                }

                BlockFile 
                    << "# imagesummary    = "
                    << "(id)" << hex << (*iit)
                    << TAB << "(name)" << c->Application;

                if (ctrs->PerInstruction){
                    BlockFile << TAB << "(insncount)" << dec << blockCount << ENDL;
                } else {
                    BlockFile << TAB << "(blockcount)" << dec << blockCount << ENDL;
                }

                LoopFile
                    << "# imagesummary    = "
                    << "(id)" << hex << (*iit)
                    << TAB << "(name)" << c->Application
                    << TAB << "(loopcount)" << dec << loopCount << ENDL;

            }

            // print information per-block/loop
            BlockFile 
                << ENDL
                << "#" << "BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageId" << TAB << "AllCounter" << TAB << "# File:Line" << TAB << "Function" << TAB << "Address" << ENDL
                << "#" << TAB << "ThreadId" << TAB << "ThreadCounter" << ENDL 
                << ENDL;

            LoopFile
                << ENDL
                << "#" << "LPP" << TAB << "Hashcode" << TAB << "ImageId" << TAB << "AllCounter" << TAB << "# File:Line" << TAB << "Function" << TAB << "Address" << ENDL
                << "#" << TAB << "ThreadId" << TAB << "ThreadCounter" << ENDL 
                << ENDL;

            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                CounterArray* c = (CounterArray*)AllData->GetData((*iit), pthread_self());
                for (uint32_t i = 0; i < c->Size; i++){
                    uint32_t idx;
                    if (c->Types[i] == CounterType_basicblock){
                        idx = i;
                    } else if (c->Types[i] == CounterType_instruction){
                        idx = c->Counters[i];
                    } else {
                        idx = i;
                    }

                    uint32_t counter = 0;
                    for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); tit++){
                        CounterArray* tc = (CounterArray*)AllData->GetData((*iit), (*tit));
                        counter += tc->Counters[idx];
                    }

                    if (counter >= PRINT_MINIMUM){
                        if (c->Types[i] == CounterType_loop){
                            LoopFile
                                << "LPP"
                                << TAB << hex << c->Hashes[i]
                                << TAB << hex << (*iit)
                                << TAB << dec << counter
                                << TAB << "# " << c->Files[i] << ":" << dec << c->Lines[i]
                                << TAB << c->Functions[i]
                                << TAB << hex << c->Addresses[i]
                                << ENDL;
                        } else {
                            BlockFile
                                << "BLK"
                                << TAB << dec << i
                                << TAB << hex << c->Hashes[i]
                                << TAB << hex << (*iit)
                                << TAB << dec << counter
                                << TAB << "# " << c->Files[i] << ":" << dec << c->Lines[i]
                                << TAB << c->Functions[i]
                                << TAB << hex << c->Addresses[i]
                                << ENDL;
                        }

                        for (set<thread_key_t>::iterator tit = AllData->allthreads.begin(); tit != AllData->allthreads.end(); tit++){
                            CounterArray* tc = (CounterArray*)AllData->GetData((*iit), (*tit));
                            if (tc->Counters[idx] >= PRINT_MINIMUM){
                                if (c->Types[i] == CounterType_loop){
                                    LoopFile
                                        << TAB << dec << AllData->GetThreadSequence((*tit))
                                        << TAB << dec << tc->Counters[idx]
                                        << ENDL;
                                } else {
                                    BlockFile
                                        << TAB << dec << AllData->GetThreadSequence((*tit))
                                        << TAB << dec << tc->Counters[idx]
                                        << ENDL;
                                }
                            }
                        }
                    }
                }
            }
        }

        // also print in old format if only 1 thread and 1 image
        if (AllData->CountThreads() == 1 && AllData->CountImages() == 1){

            char outFileName[1024];
            sprintf(outFileName, "%s.meta_%04d.%s", ctrs->Application, GetTaskId(), ctrs->Extension);
            FILE* outFile = fopen(outFileName, "w");
            if (!outFile){
                cerr << "error: cannot open output file %s" << outFileName << ENDL;
                exit(-1);
            }
        
            inform << "*** Instrumentation Summary ****" << ENDL;
            uint32_t countBlocks = 0;
            uint32_t countLoops = 0;
            for (uint32_t i = 0; i < ctrs->Size; i++){
                if (ctrs->Types[i] == CounterType_basicblock){
                    countBlocks++;
                } else if (ctrs->Types[i] == CounterType_loop){
                    countLoops++;
                }
            }

            inform << dec << countBlocks << " blocks; printing those with at least " << PRINT_MINIMUM << " executions to file " << outFileName << ENDL;
        
            fprintf(outFile, "# appname   = %s\n", ctrs->Application);
            fprintf(outFile, "# extension = %s\n", ctrs->Extension);
            fprintf(outFile, "# phase     = %d\n", 0);
            fprintf(outFile, "# rank      = %d\n", GetTaskId());
            fprintf(outFile, "# perinsn   = %s\n", ctrs->PerInstruction? "yes" : "no");
            fprintf(outFile, "# imageid   = %#lx\n", *key);
        
            fprintf(outFile, "#id\tcount\t#file:line\taddr\tfunc\thash\n");
            fflush(outFile);

            print_counter_array(outFile, ctrs);

            fflush(outFile);
            fclose(outFile);

            // print loop counters
            sprintf(outFileName, "%s.meta_%04d.%s", ctrs->Application, GetTaskId(), "loopcnt");
            outFile = fopen(outFileName, "w");
            if (!outFile){
                fprintf(stderr, "Cannot open output file %s, exiting...\n", outFileName);
                fflush(stderr);
                exit(-1);
            }
        
            inform << dec << countLoops << " loops; printing those with at least " << PRINT_MINIMUM << " executions to file " << outFileName << ENDL;

            fprintf(outFile, "# appname   = %s\n", ctrs->Application);
            fprintf(outFile, "# extension = %s\n", ctrs->Extension);
            fprintf(outFile, "# phase     = %d\n", 0);
            fprintf(outFile, "# rank      = %d\n", GetTaskId());
            fprintf(outFile, "# perinsn   = %s\n", ctrs->PerInstruction? "yes" : "no");
            fprintf(outFile, "# imageid   = %#lx\n", *key);
        
            fprintf(outFile, "#hash\tcount\t#file:line\tfunc\thash\taddr\n");
            fflush(outFile);

            print_loop_array(outFile, ctrs);

            fflush(outFile);
            fclose(outFile);
        }

        inform << "cxxx Total Execution time for image " << ctrs->Application << ": " << (AllData->GetTimer(*key, 1) - AllData->GetTimer(*key, 0)) << " seconds" << ENDL;
        return NULL;
    }
};

