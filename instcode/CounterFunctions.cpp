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

#include <InstrumentationCommon.hpp>
#include <Simulation.hpp>
#include <CounterFunctions.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>
#include <signal.h>

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

/*
 * Given a template ctrs, create  a new one for this thread
 * if typ == image, then this will be the master thread and it gets the template as its own counter array
 */
CounterArray* GenerateCounterArray(CounterArray* ctrs, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage){
    CounterArray* c = ctrs;
    c->threadid = tid;
    c->imageid = iid;

    if (typ == AllData->ImageType){
        return c;
    }

    assert(typ == AllData->ThreadType);

    c = (CounterArray*)malloc(sizeof(CounterArray));
    assert(c);
    memcpy(c, ctrs, sizeof(CounterArray));

    c->Counters = (uint64_t*)malloc(sizeof(uint64_t) * c->Size);

    c->Initialized = false;

    // keep all CounterType_instruction in place
    memcpy(c->Counters, ctrs->Counters, sizeof(uint64_t) * c->Size);
    for (uint32_t i = 0; i < c->Size; i++){
        if (c->Types[i] != CounterType_instruction){
            c->Counters[i] = 0;
        }
    }

    return c;
}

uint64_t RefCounterArray(CounterArray* ctrs){
    return (uint64_t)ctrs->Counters;
}

void DeleteCounterArray(CounterArray* ctrs){
    if (!ctrs->Initialized){
        free(ctrs->Counters);
        free(ctrs);
    }
}

void* tool_thread_init(thread_key_t tid){
    //inform << "Entering tool_thread_init" << ENDL;
    SAVE_STREAM_FLAGS(cout);
    if (AllData){
        AllData->AddThread(tid);
    } else {
        ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
    }
    RESTORE_STREAM_FLAGS(cout);
    //inform << "Leaving tool_thread_init" << ENDL;
    return NULL;
}

void* tool_thread_fini(thread_key_t tid){
    return NULL;
}

extern "C"
{
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn){
        //inform << "Entered tool_dynamic_init" << ENDL;
        SAVE_STREAM_FLAGS(cout);
        InitializeDynamicInstrumentation(count, dyn);

        RESTORE_STREAM_FLAGS(cout);
        //inform << "Leaving tool_dynamic_init" << ENDL;
        return NULL;
    }

    void* tool_mpi_init(){
        return NULL;
    }

    /*
     * tool_image_init
     * For executables, this function is called by the initialization routine
     * For shared libraries, this function is called at the start of every function
     *
     * AllData should be initialized iff it doesn't exist
     * Initialization points for this image should be disabled
     *
     * s is the initial statically allocated counter struct
     * *key is the image hashcode
     * td is the thread hash table
     */
    static pthread_mutex_t image_init_mutex = PTHREAD_MUTEX_INITIALIZER;
    void* tool_image_init(void* s, uint64_t* key, ThreadData* td){
        //inform << "Entered tool_image_init" << ENDL;
        SAVE_STREAM_FLAGS(cout);

        CounterArray* ctrs = (CounterArray*)s;
        assert(ctrs->Initialized == true);


        pthread_mutex_lock(&image_init_mutex);
        // on first visit create data manager - once per address space
        if (AllData == NULL){
            init_signal_handlers();
            AllData = new DataManager<CounterArray*>(GenerateCounterArray, DeleteCounterArray, RefCounterArray);
        }

        assert(AllData);
        // Initialize this image if we need to
        if (AllData->allimages.count(*key) == 0){
            // Remove initialization points -- once per image
            set<uint64_t> inits;
            inits.insert(*key);
            inform << "Removing init points for image " << hex << (*key) << ENDL;
            SetDynamicPoints(inits, false);

            // Add data for this image -- once per image
            AllData->AddImage(ctrs, td, *key);
            ctrs->threadid = AllData->GenerateThreadKey();
            ctrs->imageid = *key;

            AllData->SetTimer(*key, 0);

        }
        assert(AllData->allimages.count(*key) == 1);

        pthread_mutex_unlock(&image_init_mutex);

        RESTORE_STREAM_FLAGS(cout);
        //inform << "Leaving tool_image_init" << ENDL;
        return NULL;
    }

    /**
     * tool_image_fini
     *
     * This should be called exactly once
     * The caller won't necessarily be the master thread
     * That thread may not even exist anymore
     *
     */
    void  image_fini_master();
    void* tool_image_fini(uint64_t* key){
        //inform << "Entering tool_image_fini" << ENDL;
        AllData->SetTimer(*key, 1);
        SAVE_STREAM_FLAGS(cout);

#ifdef MPI_INIT_REQUIRED
        if (!IsMpiValid()){
            warn << "Process " << dec << getpid() << " did not execute MPI_Init, will not print execution count files" << ENDL;
            RESTORE_STREAM_FLAGS(cout);
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

        static bool finalized;
        assert(!finalized);
        finalized = true;

        string bfile;

        const char* prefix = getenv(ENV_OUTPUT_PREFIX);
        if(prefix != NULL) {
            bfile.append(prefix);
            bfile.append("/");
        }
        bfile.append(ctrs->Application);
        bfile.append(".r");
        AppendRankString(bfile);
        bfile.append(".t");
        AppendTasksString(bfile);
        bfile.append(".");
    
        bfile.append(ctrs->Extension);

        ofstream BlockFile;
        const char* b = bfile.c_str();
        TryOpen(BlockFile, b);
    
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
    
        inform << dec << blockCount << " blocks and " << loopCount << " loops. print those with counts of at least " << dec << PRINT_MINIMUM << " to " << bfile << ENDL;
    
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
        BlockFile
            << "# loopcount       = " << dec << loopCount << ENDL
            << ENDL;
            
        // print image summaries
        BlockFile
            << "# IMG"
            << TAB << "ImageHash"
            << TAB << "ImageSequence"
            << TAB << "ImageType"
            << TAB << "Name"
            << TAB << "BlockCount"
            << TAB << "LoopCount"
            << ENDL;
    
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
                << "IMG"
                << TAB << hex << (*iit)
                << TAB << dec << AllData->GetImageSequence((*iit))
                << TAB << (c->Master ? "Executable" : "SharedLib") // FIXME Master is not necessarily the executable
                << TAB << c->Application
                << TAB << dec << blockCount
                << TAB << dec << loopCount
                << ENDL;
        }
    
        // print information per-block/loop
        BlockFile 
            << ENDL
            << "# BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageSequence" << TAB << "AllCounter" << TAB << "# File:Line" << TAB << "Function" << TAB << "Address" << ENDL
            << "#" << TAB << "ThreadId" << TAB << "ThreadCounter" << ENDL 
            << ENDL;
    
        BlockFile
            << "# LPP" << TAB << "Hashcode" << TAB << "ImageSequence" << TAB << "AllCounter" << TAB << "# File:Line" << TAB << "Function" << TAB << "Address" << ENDL
            << "#" << TAB << "ThreadId" << TAB << "ThreadCounter" << ENDL 
            << ENDL;

        // For each image
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
            uint32_t imgseq = AllData->GetImageSequence(*iit);

            // For each counter
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

                // Sum Counts from each thread
                uint32_t counter = 0;
                for(DataManager<CounterArray*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it) {
                    CounterArray* tc = it->second;
                    counter += tc->Counters[idx];
                }

                if (counter >= PRINT_MINIMUM){
                    if (c->Types[i] == CounterType_loop){
                        BlockFile
                            << "LPP"
                            << TAB << hex << c->Hashes[i]
                            << TAB << dec << imgseq
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
                            << TAB << dec << imgseq
                            << TAB << dec << counter
                            << TAB << "# " << c->Files[i] << ":" << dec << c->Lines[i]
                            << TAB << c->Functions[i]
                            << TAB << hex << c->Addresses[i]
                            << ENDL;
                    }

                    for(DataManager<CounterArray*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it) {
                        thread_key_t tid = it->first;
                        CounterArray* tc = it->second;
                        if( tc->Counters[idx] >= PRINT_MINIMUM) {
                            BlockFile
                                << TAB << dec << AllData->GetThreadSequence(tid)
                                << TAB << dec << tc->Counters[idx]
                                << ENDL;
                        }
                    }
                }
            }
    
        }

        inform << "cxxx Total Execution time for " << ctrs->Extension << "-instrumented image " << ctrs->Application << ": " << (AllData->GetTimer(*key, 1) - AllData->GetTimer(*key, 0)) << " seconds" << ENDL;

        RESTORE_STREAM_FLAGS(cout);
        //inform << "Leaving tool_image_fini" << ENDL;
        return NULL;
    }
};
