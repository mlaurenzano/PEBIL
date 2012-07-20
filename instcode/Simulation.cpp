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
#include <unistd.h>
#include <strings.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <string.h>
#include <assert.h>

// TODO: add dfp support
// TODO: test per-insn
// TODO: test multithread + multiimage runs
#include <DFPattern.h>
#include <InstrumentationCommon.hpp>
#include <Simulation.hpp>

static uint32_t MinimumHighAssociativity = 256;

static uint32_t CountMemoryHandlers = 0;
#define CountCacheStructures (CountMemoryHandlers - 1)
#define RangeHandlerIndex (CountMemoryHandlers - 1)

static MemoryStreamHandler** MemoryHandlers = NULL;
static SamplingMethod* Sampler = NULL;
static DataManager<SimulationStats*>* AllData = NULL;

static set<uint64_t>* NonmaxKeys = NULL;

#define synchronize(__locker) __locker->Lock(); for (bool syncbool = true; syncbool == true; __locker->UnLock(), syncbool=false) 

void PrintReference(uint32_t id, BufferEntry* ref){
    inform 
        << "Thread " << hex << pthread_self()
        << TAB << " buffer slot " << dec << id
        << TAB << hex << ref->address
        << TAB << dec << ref->memseq
        << TAB << hex << ref->imageid
        << TAB << hex << ref->threadid
        << ENDL;
    cout.flush();
}

void PrintBlockData(uint32_t id, SimulationStats* s){
    inform
        << "Id" << dec << id
        << TAB << CounterTypeNames[s->Types[id]]
        << TAB << "Count " << dec << s->Counters[id]
        << TAB << "Memops " << dec << s->MemopsPerBlock[id]
        << TAB << s->Files[id] << ":" << s->Lines[id]
        << TAB << s->Functions[id]
        << TAB << hex << s->Hashes[id]
        << TAB << hex << s->Addresses[id]
        << ENDL;
}

static double* __dtimer;
#define CountDebugTimers (CountMemoryHandlers + 3)

extern "C" {
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn){
        InitializeDynamicInstrumentation(count, dyn);

        assert(AllData);

        synchronize(AllData){
            NonmaxKeys = new set<uint64_t>();

            for (uint32_t i = 0; i < CountDynamicInst; i++){
                DynamicInst* d = GetDynamicInstPoint(i);
                uint64_t k = d->Key;
                if (GET_TYPE(k) == PointType_bufferfill){
                    if (NonmaxKeys->count(k) == 0){
                        NonmaxKeys->insert(k);
                    }
                }

                if (d->IsEnabled == false){
                    SetDynamicPointStatus(d, false);
                }
            }
            debug(PrintDynamicPoints());

            if (Sampler->SampleOn == 0){
                inform << "Disabling all simulation related instrumentation because METASIM_SAMPLE_ON is set to 0" << ENDL;
                set<uint64_t> AllSimPoints;
                for (set<uint64_t>::iterator it = NonmaxKeys->begin(); it != NonmaxKeys->end(); it++){
                    AllSimPoints.insert(GENERATE_KEY(GET_BLOCKID((*it)), PointType_buffercheck));
                    AllSimPoints.insert(GENERATE_KEY(GET_BLOCKID((*it)), PointType_bufferinc));
                    AllSimPoints.insert(GENERATE_KEY(GET_BLOCKID((*it)), PointType_bufferfill));
                }
                SetDynamicPoints(&AllSimPoints, false);
                NonmaxKeys->clear();
            }
        }
    }

    void* tool_mpi_init(){
        return NULL;
    }

    void* tool_thread_init(thread_key_t tid){
        if (AllData){
            AllData->AddThread(tid);
            InitializeSuspendHandler();
        } else {
            ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
        }
        return NULL;
    }

    void* tool_image_init(void* s, image_key_t* key, ThreadData* td){
        SimulationStats* stats = (SimulationStats*)s;

        ReadSettings();

        __dtimer = new double[CountDebugTimers];
        for (uint32_t i = 0; i < CountDebugTimers; i++){
            __dtimer[i] = 0.0;
        }

        assert(stats->Stats == NULL);
        stats->Stats = new StreamStats*[CountMemoryHandlers];
        for (uint32_t i = 0; i < CountCacheStructures; i++){
            CacheStructureHandler* c = (CacheStructureHandler*)MemoryHandlers[i];
            stats->Stats[i] = new CacheStats(c->levelCount, c->sysId, stats->InstructionCount);
        }
        stats->Stats[RangeHandlerIndex] = new RangeStats(stats->InstructionCount);

        assert(stats->Initialized == true);
        if (AllData == NULL){
            AllData = new DataManager<SimulationStats*>(GenerateCacheStats, DeleteCacheStats, ReferenceCacheStats);
        }

        AllData->AddImage(stats, td, *key);
        stats->imageid = *key;
        stats->threadid = pthread_self();

        AllData->SetTimer(*key, 0);
        return NULL;
    }

    bool TryProcessBuffer(uint32_t HandlerIdx, MemoryStreamHandler* m, uint32_t numElements, SimulationStats* stats, bool force, image_key_t iid, thread_key_t tid){
        AllData->SetTimer(iid, tid+3);

        // wait for the lock if force == true, otherwise just see if it is unlocked
        if (force){
            m->Lock();
        } else {
            if (m->TryLock() == false){
                return false;
            }
        }
        // we have the lock now!

        uint32_t numProcessed = 0;

        for (uint32_t j = 0; j < numElements; j++){
            register BufferEntry* reference = BUFFER_ENTRY(stats, j + 1);
                        
            if (reference->imageid == 0){
                continue;
            }

            // TODO: this is super slow. need to speed it up somehow?
            register StreamStats* s = AllData->GetData(reference->imageid, reference->threadid)->Stats[HandlerIdx];
            debug(inform << "Stats at " << hex << (uint64_t)s << ENDL);
            debug(PrintReference(j + 1, reference));

            m->Process((void*)s, reference);
            numProcessed++;
        }

        AllData->SetTimer(iid, tid+4);
        __dtimer[HandlerIdx] += (AllData->GetTimer(iid, tid+4) - AllData->GetTimer(iid, tid+3));

        m->UnLock();
        return true;
    }

    void* process_thread_buffer(image_key_t iid, thread_key_t tid){

#define DONE_WITH_BUFFER(...)                   \
        BUFFER_CURRENT(stats) = 0;                                      \
        bzero(BUFFER_ENTRY(stats, 1), sizeof(BufferEntry) * BUFFER_CAPACITY(stats)); \
        return NULL;

        assert(iid);
        if (AllData == NULL){
            ErrorExit("data manager does not exist. no images were initialized", MetasimError_NoImage);
            return NULL;
        }

        // Buffer is shared between all images
        debug(inform << "Getting data for image " << hex << iid << " thread " << tid << ENDL);
        SimulationStats* stats = (SimulationStats*)AllData->GetData(iid, tid);
        if (stats == NULL){
            ErrorExit("Cannot retreive image data using key " << dec << iid, MetasimError_NoImage);
            return NULL;
        }

        register uint64_t numElements = BUFFER_CURRENT(stats);
        uint64_t capacity = BUFFER_CAPACITY(stats);

        debug(inform 
            << "Thread " << dec << AllData->GetThreadSequence(tid)
            << TAB << "Counter " << dec << numElements
            << TAB << "Capacity " << dec << capacity
            << TAB << "Total " << dec << Sampler->AccessCount
              << ENDL);


        bool isSampling;
        synchronize(AllData){
            AllData->SetTimer(iid, tid);
            isSampling = Sampler->CurrentlySampling();
            if (NonmaxKeys->empty()){
                AllData->UnLock();
                DONE_WITH_BUFFER();
            }
            AllData->SetTimer(iid, tid+1);
            __dtimer[CountMemoryHandlers] += (AllData->GetTimer(iid, tid+1) - AllData->GetTimer(iid, tid));
        }

        if (isSampling){
            BufferEntry* buffer = &(stats->Buffer[1]);

            int Attempts[CountMemoryHandlers];
            for (uint32_t i = 0; i < CountMemoryHandlers; i++){
                Attempts[i] = 0;
            }

#define PROCESS_DONE (-1)
#define PROCESS_UNFORCED_ATTEMPTS (8)
            bool Processing = true;
            while (Processing){
                Processing = false;
                for (uint32_t i = 0; i < CountMemoryHandlers; i++){
                    if (Attempts[i] == PROCESS_DONE){
                        continue;
                    }

                    register MemoryStreamHandler* m = MemoryHandlers[i];
                    bool force = (Attempts[i] >= PROCESS_UNFORCED_ATTEMPTS);

                    bool res = TryProcessBuffer(i, m, numElements, stats, force, iid, tid);
                    if (res){
                        Attempts[i] = PROCESS_DONE;
                    } else {
                        Attempts[i]++;
                        Processing = true;
                    }

                    if (force){
                        assert(Attempts[i] == PROCESS_DONE);
                    }
                }
            }

            for (uint32_t i = 0; i < CountMemoryHandlers; i++){
                assert(Attempts[i] == PROCESS_DONE);
            }
        }

        AllData->SetTimer(iid, tid+6);
        synchronize(AllData){
            if (isSampling){
                set<uint64_t> MemsRemoved;
                for (uint32_t j = 0; j < numElements; j++){
                    BufferEntry* reference = BUFFER_ENTRY(stats, j + 1);
                    debug(inform << "Memseq " << dec << reference->memseq << " has " << stats->Stats[0]->GetAccessCount(reference->memseq) << ENDL);
                    uint32_t bbid = stats->BlockIds[reference->memseq];

                    // if max block count is reached, disable all buffer-related points related to this block
                    uint32_t idx = bbid;
                    uint32_t midx = bbid;
                    if (stats->Types[bbid] == CounterType_instruction){
                        idx = stats->Counters[bbid];
                    }
                    if (stats->PerInstruction){
                        midx = stats->MemopIds[bbid];
                    }

                    debug(inform << "Slot " << dec << j
                          << TAB << "Thread " << dec << AllData->GetThreadSequence(pthread_self())
                          << TAB << "BLock " << bbid
                          << TAB << "Counter " << stats->Counters[bbid]
                          << TAB << "Real " << stats->Counters[idx]
                          << ENDL);

                    if (Sampler->ExceedsAccessLimit(stats->Counters[idx])){

                        uint64_t k1 = GENERATE_KEY(midx, PointType_buffercheck);
                        uint64_t k2 = GENERATE_KEY(midx, PointType_bufferinc);
                        uint64_t k3 = GENERATE_KEY(midx, PointType_bufferfill);

                        if (NonmaxKeys->count(k3) > 0){

                            PrintBlockData(idx, stats);
                            PrintBlockData(bbid, stats);
                            inform << "Slot " << dec << j
                                   << TAB << "BLock " << bbid
                                   << TAB << "Counter " << stats->Counters[bbid]
                                   << TAB << "Real " << stats->Counters[idx]
                                   << ENDL;

                            if (MemsRemoved.count(k1) == 0){
                                MemsRemoved.insert(k1);
                            }
                            assert(MemsRemoved.count(k1) == 1);

                            if (MemsRemoved.count(k2) == 0){
                                MemsRemoved.insert(k2);
                            }
                            assert(MemsRemoved.count(k2) == 1);

                            if (MemsRemoved.count(k3) == 0){
                                MemsRemoved.insert(k3);
                            }
                            assert(MemsRemoved.count(k3) == 1);

                            NonmaxKeys->erase(k3);
                            assert(NonmaxKeys->count(k3) == 0);
                        }
                    }
                }

                if (MemsRemoved.size()){
                    assert(MemsRemoved.size() % 3 == 0);
                    inform << "REMOVING " << dec << (MemsRemoved.size() / 3) << " blocks" << ENDL;
                    SuspendAllThreads(AllData->CountThreads(), AllData->allthreads.begin(), AllData->allthreads.end());
                    SetDynamicPoints(&MemsRemoved, false);
                    ResumeAllThreads();
                }

                if (Sampler->SwitchesMode(numElements)){
                    SuspendAllThreads(AllData->CountThreads(), AllData->allthreads.begin(), AllData->allthreads.end());
                    SetDynamicPoints(NonmaxKeys, false);
                    ResumeAllThreads();
                }

            } else {
                if (Sampler->SwitchesMode(numElements)){
                    SuspendAllThreads(AllData->CountThreads(), AllData->allthreads.begin(), AllData->allthreads.end());
                    SetDynamicPoints(NonmaxKeys, true);
                    ResumeAllThreads();
                }
            }

            Sampler->IncrementAccessCount(numElements);

            AllData->SetTimer(iid, tid+7);
            __dtimer[CountMemoryHandlers+2] += (AllData->GetTimer(iid, tid+7) - AllData->GetTimer(iid, tid+6));
        }

        DONE_WITH_BUFFER();
    }

    void* process_buffer(image_key_t* key){
        image_key_t iid = *key;
        process_thread_buffer(iid, pthread_self());
    }

    void* tool_image_fini(image_key_t* key){
        image_key_t iid = *key;

        for (uint32_t i = 0; i < CountDebugTimers; i++){
            inform << "Debug timer " << dec << i << TAB << __dtimer[i] << ENDL;
        }

        AllData->SetTimer(iid, 1);

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
        SimulationStats* stats = (SimulationStats*)AllData->GetData(iid, pthread_self());
        if (stats == NULL){
            ErrorExit("Cannot retreive image data using key " << dec << (*key), MetasimError_NoImage);
            return NULL;
        }

        // only print stats when the executable exits
        if (!stats->Master){
            return NULL;
        }

        // clear all threads' buffers
        for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
            process_thread_buffer(iid, (*it));
        }


        // dump cache simulation results
        ofstream MemFile;
        const char* fileName = SimulationFileName(stats);
        TryOpen(MemFile, fileName);

        inform << "Printing cache simulation results to " << fileName << ENDL;

        uint64_t sampledCount = 0;
        uint64_t totalMemop = 0;
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
            for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                SimulationStats* s = (SimulationStats*)AllData->GetData(iid, (*it));
                RangeStats* r = (RangeStats*)s->Stats[RangeHandlerIndex];
                assert(r);

                for (uint32_t i = 0; i < r->Capacity; i++){
                    sampledCount += r->Counts[i];
                }

                for (uint32_t i = 0; i < s->BlockCount; i++){
                    uint32_t idx;
                    if (s->Types[i] == CounterType_basicblock){
                        idx = i;
                    } else if (s->Types[i] == CounterType_instruction){
                        idx = s->Counters[i];
                    }
                    //inform << dec << i << TAB << s->Counters[idx] << TAB << s->MemopsPerBlock[idx] << TAB << CounterTypeNames[s->Types[i]] << ENDL;
                    totalMemop += (s->Counters[idx] * s->MemopsPerBlock[idx]);
                }
                //inform << "Total memop: " << dec << totalMemop << ENDL;
            }
        }

        MemFile
            << "# appname       = " << stats->Application << ENDL
            << "# extension     = " << stats->Extension << ENDL
            << "# rank          = " << dec << GetTaskId() << ENDL
            << "# ntasks        = " << dec << GetNTasks() << ENDL
            << "# buffer        = " << BUFFER_CAPACITY(stats) << ENDL
            << "# total         = " << dec << totalMemop << ENDL
            << "# sampled       = " << dec << Sampler->AccessCount << ENDL
            << "# processed     = " << dec << sampledCount << ENDL
            << "# samplemax     = " << Sampler->AccessLimit << ENDL
            << "# sampleon      = " << Sampler->SampleOn << ENDL
            << "# sampleoff     = " << Sampler->SampleOff << ENDL
            << "# numcache      = " << CountCacheStructures << ENDL
            << "# perinsn       = " << (stats->PerInstruction? "yes" : "no") << ENDL
            << "# countimage    = " << dec << AllData->CountImages() << ENDL
            << "# countthread   = " << dec << AllData->CountThreads() << ENDL
            << "# masterthread  = " << hex << AllData->GetThreadSequence(pthread_self()) << ENDL
            << ENDL;

        MemFile
            << "# IMG"
            << TAB << "ImageHash"
            << TAB << "ImageSequence"
            << TAB << "ImageType"
            << TAB << "Name"
            << ENDL;
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
            SimulationStats* s = (SimulationStats*)AllData->GetData((*iit), pthread_self());
            MemFile 
                << "IMG"
                << TAB << hex << (*iit)
                << TAB << dec << AllData->GetImageSequence((*iit))
                << TAB << (s->Master ? "Executable" : "SharedLib")
                << TAB << s->Application
                << ENDL;
        }
        MemFile << ENDL;

        for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
            bool first = true;
            for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                SimulationStats* s = AllData->GetData(iid, (*it));
                assert(s);

                CacheStats* c = (CacheStats*)s->Stats[sys];
                assert(c->Capacity == s->InstructionCount);

                if (first){
                    MemFile << "# sysid" << dec << c->SysId << ENDL;
                    first = false;
                }

                MemFile << "#" << TAB << dec << AllData->GetThreadSequence((*it)) << " ";
                for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                    uint64_t h = c->GetHits(lvl);
                    uint64_t m = c->GetMisses(lvl);
                    uint64_t t = h + m;
                    MemFile << "l" << dec << lvl << "[" << h << "," << t << "(" << CacheStats::GetHitRate(h, m) << ")] ";
                }
                MemFile << ENDL;
            }
        }
        MemFile << ENDL;

        MemFile 
            << "# " << "BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageSequence" << TAB << "Threadid"
            << TAB << "BlockCounter" << TAB << "InstructionSimulated"
            << ENDL;
        MemFile
            << "# " << TAB << "SysId" << TAB << "Level" << TAB << "HitCount" << TAB << "MissCount" << ENDL;

        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
            for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                SimulationStats* st = AllData->GetData((*iit), (*it));
                assert(st);

                // compile per-instruction stats into blocks
                CacheStats** aggstats = new CacheStats*[CountCacheStructures];
                for (uint32_t sys = 0; sys < CountCacheStructures; sys++){

                    CacheStats* s = (CacheStats*)st->Stats[sys];
                    assert(s);
                    CacheStats* c = new CacheStats(s->LevelCount, s->SysId, st->BlockCount);
                    aggstats[sys] = c;

                    for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                        for (uint32_t memid = 0; memid < st->InstructionCount; memid++){
                            uint32_t bbid;
                            if (stats->PerInstruction){
                                bbid = memid;
                            } else {
                                bbid = st->BlockIds[memid];
                            }
                            c->Hit(bbid, lvl, s->GetHits(memid, lvl));
                            c->Miss(bbid, lvl, s->GetMisses(memid, lvl));
                        }
                    }
                }

                CacheStats* root = aggstats[0];
                for (uint32_t bbid = 0; bbid < root->Capacity; bbid++){

                    // dont print blocks which weren't touched
                    if (root->GetAccessCount(bbid) == 0){
                        continue;
                    }

                    // this isn't necessarily true since this tool can suspend threads at any point,
                    // potentially shutting off instrumention in a block while a thread is midway through
                    if (AllData->CountThreads() == 1){
                        assert(root->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] == 0);
                    }

                    uint32_t idx;
                    if (stats->Types[bbid] == CounterType_basicblock){
                        idx = bbid;
                    } else if (stats->Types[bbid] == CounterType_instruction){
                        idx = stats->Counters[bbid];
                    }

                    MemFile << "BLK" 
                            << TAB << dec << bbid
                            << TAB << hex << stats->Hashes[bbid]
                            << TAB << dec << AllData->GetImageSequence((*iit))
                            << TAB << dec << AllData->GetThreadSequence(st->threadid)
                            << TAB << dec << st->Counters[idx]
                            << TAB << dec << root->GetAccessCount(bbid)
                            << ENDL;

                    for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
                        CacheStats* c = aggstats[sys];
                        assert(root->GetAccessCount(bbid) == c->GetHits(bbid, 0) + c->GetMisses(bbid, 0));
                        for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){

                            MemFile
                              << TAB << dec << c->SysId
                              << TAB << dec << (lvl+1)
                              << TAB << dec << c->GetHits(bbid, lvl)
                              << TAB << dec << c->GetMisses(bbid, lvl)
                              << ENDL;
                        }
                    }
                }

                for (uint32_t i = 0; i < CountCacheStructures; i++){
                    delete aggstats[i];
                }
                delete[] aggstats;
            }
        }

        MemFile.close();

        // if single-thread and single-image, also print in old format
        if (AllData->CountThreads() == 1 && AllData->CountImages() == 1){
            const char* fileName = LegacySimulationFileName(stats);
            TryOpen(MemFile, fileName);

            inform << "Printing cache simulation results to " << fileName << ENDL;

            MemFile
                << "# appname       = " << stats->Application << ENDL
                << "# extension     = " << stats->Extension << ENDL
                << "# rank          = " << dec << GetTaskId() << ENDL
                << "# buffer        = " << BUFFER_CAPACITY(stats) << ENDL
                << "# total         = " << dec << totalMemop << ENDL
                << "# sampled       = " << dec << Sampler->AccessCount << ENDL
                << "# processed     = " << dec << sampledCount << ENDL
                << "# samplemax     = " << Sampler->AccessLimit << ENDL
                << "# sampleon      = " << Sampler->SampleOn << ENDL
                << "# sampleoff     = " << Sampler->SampleOff << ENDL
                << "# numcache      = " << CountCacheStructures << ENDL
                << "# perinsn       = " << (stats->PerInstruction? "yes" : "no") << ENDL
                << "#" << ENDL;

            for (uint32_t sys = 0; sys < CountCacheStructures; sys++){

                CacheStats* c = (CacheStats*)stats->Stats[sys];
                assert(c->Capacity == stats->InstructionCount);

                MemFile << "# sysid" << dec << c->SysId << TAB;

                for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                    uint64_t h = c->GetHits(lvl);
                    uint64_t m = c->GetMisses(lvl);
                    uint64_t t = h + m;
                    MemFile << "l" << dec << lvl << "[" << h << "," << t << "(" << CacheStats::GetHitRate(h, m) << ")] ";
                }
                MemFile << ENDL;
            }
            MemFile << ENDL;

            MemFile << "#block <seqid> <blockcount> <blocksimulated> <insnsimulated>" << ENDL
                    << "#       sys <sysid> lvl <cachelvl> <hitcount> <miscount> <hitpercent>" << ENDL
                    << ENDL;

            for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                stats = AllData->GetData(iid, (*it));
                assert(stats);
                PrintSimulationStats(MemFile, stats, (*it), false);
            }

            MemFile.close();

        }


        // dump address range (dfp) file
        for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
            SimulationStats* s = (SimulationStats*)AllData->GetData(iid, (*it));
            RangeStats* r = (RangeStats*)s->Stats[RangeHandlerIndex];
            assert(r);
            assert(s->InstructionCount == r->Capacity);

            /*
            for (uint32_t i = 0; i < s->InstructionCount; i++){
                if (r->Counts[i]){
                    inform 
                        << "Instruction " << dec << i
                        << TAB << "Thread " << dec << AllData->GetThreadSequence((*it))
                        << TAB << "Count " << dec << r->Counts[i]
                        << TAB << "[" << hex << r->GetMinimum(i) << "," << hex << r->GetMaximum(i) << "]"
                        << ENDL;
                }
            }
            */
        }

        double t = (AllData->GetTimer(*key, 1) - AllData->GetTimer(*key, 0));
        inform << "CXXX Total Execution time for instrumented application: " << t << ENDL;
        double m = (double)(CountCacheStructures * Sampler->AccessCount);
        inform << "CXXX Memops simulated (excludes unsampled memops) per second: " << (m/t) << ENDL;

        if (NonmaxKeys){
            delete NonmaxKeys;
        }
    }

};

void PrintSimulationStats(ofstream& f, SimulationStats* stats, thread_key_t tid, bool perThread){
    debug(
    for (uint32_t bbid = 0; bbid < stats->BlockCount; bbid++){
        if (stats->Counters[bbid] == 0){
            continue;
        }

        inform
            << "Block " << dec << bbid
            << TAB << "@" << hex << &(stats->Counters[bbid])
            << TAB << dec << stats->Counters[bbid]
            << ENDL;
    })

    // compile per-instruction stats into blocks
    CacheStats** aggstats = new CacheStats*[CountCacheStructures];
    for (uint32_t sys = 0; sys < CountCacheStructures; sys++){

        CacheStats* s = (CacheStats*)stats->Stats[sys];
        assert(s);
        CacheStats* c = new CacheStats(s->LevelCount, s->SysId, stats->BlockCount);
        aggstats[sys] = c;

        for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){

            for (uint32_t memid = 0; memid < stats->InstructionCount; memid++){
                uint32_t bbid = stats->BlockIds[memid];
                c->Hit(bbid, lvl, s->GetHits(memid, lvl));
                c->Miss(bbid, lvl, s->GetMisses(memid, lvl));
            }
        }

    }

    CacheStats* root = aggstats[0];
    for (uint32_t bbid = 0; bbid < root->Capacity; bbid++){

        if (root->GetAccessCount(bbid) == 0){
            continue;
        }

        assert(root->GetAccessCount(bbid) % stats->MemopsPerBlock[bbid] == 0);
        uint64_t bsampled = root->GetAccessCount(bbid) / stats->MemopsPerBlock[bbid];

        f << "block" 
          << TAB << dec << bbid;
        if (perThread){
            f << TAB << dec << AllData->GetThreadSequence(tid);
        }
        f << TAB << dec << stats->Counters[bbid]
          << TAB << dec << bsampled
          << TAB << dec << root->GetAccessCount(bbid)
          << ENDL;

        for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
            CacheStats* c = aggstats[sys];
            for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){

                f << TAB << "sys"
                  << TAB << dec << c->SysId
                  << TAB << "lvl"
                  << TAB << dec << (lvl+1)
                  << TAB << dec << c->GetHits(bbid, lvl)
                  << TAB << dec << c->GetMisses(bbid, lvl)
                  << TAB << c->GetHitRate(bbid, lvl)
                  << ENDL;
            }
        }
    }

    for (uint32_t i = 0; i < CountCacheStructures; i++){
        delete aggstats[i];
    }
    delete[] aggstats;
}

const char* SimulationFileName(SimulationStats* stats){
    string oFile;

    oFile.append(stats->Application);
    oFile.append(".r");
    AppendRankString(oFile);
    oFile.append(".t");
    AppendTasksString(oFile);
    oFile.append(".");
    oFile.append(stats->Extension);

    return oFile.c_str();
}

const char* RangeFileName(SimulationStats* stats){
    string oFile;

    oFile.append(stats->Application);
    oFile.append(".r");
    AppendRankString(oFile);
    oFile.append(".t");
    AppendTasksString(oFile);
    oFile.append(".");
    oFile.append("dfp");

    return oFile.c_str();
}

const char* LegacySimulationFileName(SimulationStats* stats){
    string oFile;

    oFile.append(stats->Application);
    if (stats->Phase > 0){
        assert(stats->Phase == 1 && "phase number must be 1");
        oFile.append(".phase.1");
    }
    oFile.append(".meta_");
    AppendLegacyRankString(oFile);
    oFile.append(".");
    oFile.append(stats->Extension);

    return oFile.c_str();
}

const char* LegacyRangeFileName(SimulationStats* stats){
    string oFile;

    oFile.append(stats->Application);
    if (stats->Phase > 0){
        assert(stats->Phase == 1 && "phase number must be 1");
        oFile.append(".phase.1");
    }
    oFile.append(".meta_");
    AppendLegacyRankString(oFile);
    oFile.append(".");
    oFile.append("dfp");

    return oFile.c_str();
}

uint32_t RandomInt(uint32_t max){
    return rand() % max;
}

inline uint32_t Low32(uint64_t f){
    return (uint32_t)f & 0xffffffff;
}

inline uint32_t High32(uint64_t f){
    return (uint32_t)((f & 0xffffffff00000000) >> 32);
}

char ToLowerCase(char c){
    if (c < 'a'){
        c += ('a' - 'A');
    }
    return c;
}

bool IsEmptyComment(string str){
    if (str == ""){
        return true;
    }
    if (str.compare(0, 1, "#") == 0){
        return true;
    }
    return false;
}

string GetCacheDescriptionFile(){
    char* e = getenv("METASIM_CACHE_DESCRIPTIONS");
    string knobvalue;

    if (e != NULL){
        knobvalue = (string)e;
    }

    if (e == NULL || knobvalue.compare(0, 1, "$") == 0){
        string str;
        const char* freeenv = getenv(METASIM_ENV);
        if (freeenv == NULL){
            ErrorExit("default cache descriptions file requires that " METASIM_ENV " be set", MetasimError_Env);
        }
        str.append(freeenv);
        str.append("/" DEFAULT_CACHE_FILE);

        return str;
    }
    return knobvalue;
}

RangeStats::RangeStats(uint32_t capacity){
    Capacity = capacity;
    Counts = new uint64_t[Capacity];
    bzero(Counts, sizeof(uint64_t) * Capacity);
    Ranges = new AddressRange*[Capacity];
    for (uint32_t i = 0; i < Capacity; i++){
        Ranges[i] = new AddressRange();
        Ranges[i]->Minimum = MAX_64BIT_VALUE;
        Ranges[i]->Maximum = 0;
    }
}

RangeStats::~RangeStats(){
    if (Ranges){
        delete[] Ranges;
    }
    if (Counts){
        delete[] Counts;
    }
}

bool RangeStats::HasMemId(uint32_t memid){
    return (memid < Capacity);
}

uint64_t RangeStats::GetMinimum(uint32_t memid){
    assert(HasMemId(memid));
    return Ranges[memid]->Minimum;
}

uint64_t RangeStats::GetMaximum(uint32_t memid){
    assert(HasMemId(memid));
    return Ranges[memid]->Maximum;
}

void RangeStats::Update(uint32_t memid, uint64_t addr){
    AddressRange* r = Ranges[memid];
    if (addr < r->Minimum){
        r->Minimum = addr;
    }
    if (addr > r->Maximum){
        r->Maximum = addr;
    }
    Counts[memid]++;
}

AddressRangeHandler::AddressRangeHandler(){
}
AddressRangeHandler::~AddressRangeHandler(){
}
void AddressRangeHandler::Print(){
    inform << "AddressRangeHandler" << ENDL;
}
void AddressRangeHandler::Process(void* stats, BufferEntry* access){
    uint32_t memid = (uint32_t)access->memseq;
    uint64_t addr = access->address;
    RangeStats* rs = (RangeStats*)stats;

    rs->Update(memid, addr);
}
bool AddressRangeHandler::Verify(){
    return true;
}

CacheStats::CacheStats(uint32_t lvl, uint32_t sysid, uint32_t capacity){
    LevelCount = lvl;
    SysId = sysid;
    Capacity = capacity;

    Stats = new LevelStats*[Capacity];
    for (uint32_t i = 0; i < Capacity; i++){
        NewMem(i);
    }
}

CacheStats::~CacheStats(){
    if (Stats){
        for (uint32_t i = 0; i < Capacity; i++){
            if (Stats[i]){
                delete Stats[i];
            }
        }
        delete[] Stats;
    }
}

float CacheStats::GetHitRate(LevelStats* stats){
    return GetHitRate(stats->hitCount, stats->missCount);
}

float CacheStats::GetHitRate(uint64_t hits, uint64_t misses){
    if (hits + misses == 0){
        return 0.0;
    }
    return ((float)hits) / ((float)hits + (float)misses);
}

void CacheStats::ExtendCapacity(uint32_t newSize){
    assert(0 && "Should not be updating the size of this dynamically");
    LevelStats** nn = new LevelStats*[newSize];

    memset(nn, 0, sizeof(LevelStats*) * newSize);
    memcpy(nn, Stats, sizeof(LevelStats*) * Capacity);

    delete[] Stats;
    Stats = nn;
}

void CacheStats::NewMem(uint32_t memid){
    assert(memid < Capacity);

    LevelStats* mem = new LevelStats[LevelCount];
    memset(mem, 0, sizeof(LevelStats) * LevelCount);
    Stats[memid] = mem;
}

void CacheStats::Hit(uint32_t memid, uint32_t lvl){
    Hit(memid, lvl, 1);
}

void CacheStats::Miss(uint32_t memid, uint32_t lvl){
    Miss(memid, lvl, 1);
}

void CacheStats::Hit(uint32_t memid, uint32_t lvl, uint32_t cnt){
    Stats[memid][lvl].hitCount += cnt;
}

void CacheStats::Miss(uint32_t memid, uint32_t lvl, uint32_t cnt){
    Stats[memid][lvl].missCount += cnt;
}

uint64_t CacheStats::GetHits(uint32_t memid, uint32_t lvl){
    return Stats[memid][lvl].hitCount;
}

uint64_t CacheStats::GetHits(uint32_t lvl){
    uint64_t hits = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        hits += Stats[i][lvl].hitCount;
    }
    return hits;
}

uint64_t CacheStats::GetMisses(uint32_t memid, uint32_t lvl){
    return Stats[memid][lvl].missCount;
}

uint64_t CacheStats::GetMisses(uint32_t lvl){
    uint64_t hits = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        hits += Stats[i][lvl].missCount;
    }
    return hits;
}

bool CacheStats::HasMemId(uint32_t memid){
    if (memid >= Capacity){
        return false;
    }
    if (Stats[memid] == NULL){
        return false;
    }
    return true;
}

LevelStats* CacheStats::GetLevelStats(uint32_t memid, uint32_t lvl){
    return &(Stats[memid][lvl]);
}

uint64_t CacheStats::GetAccessCount(uint32_t memid){
    LevelStats* l1 = GetLevelStats(memid, 0);
    if (l1){
        return (l1->hitCount + l1->missCount);
    }
    return 0;
}

float CacheStats::GetHitRate(uint32_t memid, uint32_t lvl){
    return GetHitRate(GetLevelStats(memid, lvl));
}

float CacheStats::GetCumulativeHitRate(uint32_t memid, uint32_t lvl){
    uint64_t tcount = GetAccessCount(memid);
    if (tcount == 0){
        return 0.0;
    }
        
    uint64_t hits = 0;
    for (uint32_t i = 0; i < lvl; i++){
        hits += GetLevelStats(memid, i)->hitCount;
    }
    return ((float)hits / (float)GetAccessCount(memid));
}

bool ParsePositiveInt32(string token, uint32_t* value){
    return ParseInt32(token, value, 1);
}
                
// returns true on success... allows things to continue on failure if desired
bool ParseInt32(string token, uint32_t* value, uint32_t min){
    int32_t val;
    uint32_t mult = 1;
    bool ErrorFree = true;
   
    istringstream stream(token);
    if (stream >> val){

        if (!stream.eof()){
            char c;
            stream.get(c);

            c = ToLowerCase(c);
            if (c == 'k'){
                mult = KILO;
            } else if (c == 'm'){
                mult = MEGA;
            } else if (c == 'g'){
                mult = GIGA;
            } else {
                ErrorFree = false;
            }

            if (!stream.eof()){
                stream.get(c);

                c = ToLowerCase(c);
                if (c != 'b'){
                    ErrorFree = false;
                }
            }
        }
    }

    if (val < min){
        ErrorFree = false;
    }

    (*value) = (val * mult);
    return ErrorFree;
}

// returns true on success... allows things to continue on failure if desired
bool ParsePositiveInt32Hex(string token, uint32_t* value){
    int32_t val;
    bool ErrorFree = true;
   
    istringstream stream(token);

    char c1, c2;
    stream.get(c1);
    if (!stream.eof()){
        stream.get(c2);
    }

    if (c1 != '0' || c2 != 'x'){
        stream.putback(c1);
        stream.putback(c2);        
    }

    stringstream ss;
    ss << hex << token;
    if (ss >> val){
    }

    if (val <= 0){
        ErrorFree = false;
    }

    (*value) = val;
    return ErrorFree;
}

uint64_t ReferenceCacheStats(void* args){
    SimulationStats* stats = (SimulationStats*)args;
    return (uint64_t)stats;
}

void DeleteCacheStats(void* args){
    SimulationStats* stats = (SimulationStats*)args;
    if (!stats->Initialized){
        // TODO: delete buffer only for thread-initialized structures?

        delete[] stats->Counters;

        for (uint32_t i = 0; i < CountMemoryHandlers; i++){
            delete stats->Stats[i];
        }
        delete[] stats->Stats;
    }
}

bool ReadEnvUint32(string name, uint32_t* var){
    char* e = getenv(name.c_str());
    if (e == NULL){
        return false;
        inform << "unable to find " << name << " in environment" << ENDL;
    }
    string s = (string)e;
    if (!ParseInt32(s, var, 0)){
        return false;
        inform << "unable to find " << name << " in environment" << ENDL;
    }

    return true;
}

SamplingMethod::SamplingMethod(uint32_t limit, uint32_t on, uint32_t off){
    AccessLimit = limit;
    SampleOn = on;
    SampleOff = off;

    AccessCount = 0;
    mlock = PTHREAD_MUTEX_INITIALIZER;
}

SamplingMethod::~SamplingMethod(){
}

void SamplingMethod::Print(){
    inform << "SamplingMethod:" << TAB << "AccessLimit " << AccessLimit << " SampleOn " << SampleOn << " SampleOff " << SampleOff << ENDL;
}

void SamplingMethod::IncrementAccessCount(uint64_t count){
    pthread_mutex_lock(&mlock);
    AccessCount += count;
    pthread_mutex_unlock(&mlock);
}

bool SamplingMethod::SwitchesMode(uint64_t count){
    return (CurrentlySampling(0) != CurrentlySampling(count));
}

bool SamplingMethod::CurrentlySampling(){
    return CurrentlySampling(0);
}

bool SamplingMethod::CurrentlySampling(uint64_t count){
    uint32_t PeriodLength = SampleOn + SampleOff;
    bool res = false;
    if (SampleOn == 0){
        return res;
    }

    pthread_mutex_lock(&mlock);
    if (PeriodLength == 0){
        res = true;
    }
    if ((AccessCount + count) % PeriodLength < SampleOn){
        res = true;
    }
    pthread_mutex_unlock(&mlock);
    return res;
}

bool SamplingMethod::ExceedsAccessLimit(uint64_t count){
    pthread_mutex_lock(&mlock);
    bool res = false;
    if (AccessLimit > 0 && count > AccessLimit){
        res = true;
    }
    pthread_mutex_unlock(&mlock);
    return res;
}

CacheLevel::CacheLevel(){
}

void CacheLevel::Init(CacheLevel_Constructor_Interface){
    level = lvl;
    size = sizeInBytes;
    associativity = assoc;
    linesize = lineSz;
    replpolicy = pol;
    //assert(associativity > 0);

    countsets = size / (linesize * associativity);

    linesizeBits = 0;
    while (lineSz > 0){
        linesizeBits++;
        lineSz = (lineSz >> 1);
    }
    linesizeBits--;

    contents = new uint64_t*[countsets];
    for (uint32_t i = 0; i < countsets; i++){
        contents[i] = new uint64_t[associativity];
        memset(contents[i], 0, sizeof(uint64_t) * associativity);
    }

    recentlyUsed = NULL;
    if (replpolicy == ReplacementPolicy_nmru){
        recentlyUsed = new uint32_t[countsets];
        memset(recentlyUsed, 0, sizeof(uint32_t) * countsets);
    }
}

void HighlyAssociativeCacheLevel::Init(CacheLevel_Constructor_Interface)
{
    assert(associativity >= MinimumHighAssociativity);
    fastcontents = new unordered_map<uint64_t, uint32_t>*[countsets];
    for (uint32_t i = 0; i < countsets; i++){
        fastcontents[i] = new unordered_map<uint64_t, uint32_t>();
        fastcontents[i]->clear();
    }
}

HighlyAssociativeCacheLevel::~HighlyAssociativeCacheLevel(){
    if (fastcontents){
        for (uint32_t i = 0; i < countsets; i++){
            if (fastcontents[i]){
                delete fastcontents[i];
            }
        }
        delete[] fastcontents;
    }
}

CacheLevel::~CacheLevel(){
    if (contents){
        for (uint32_t i = 0; i < countsets; i++){
            if (contents[i]){
                delete[] contents[i];
            }
        }
        delete[] contents;
    }
    if (recentlyUsed){
        delete[] recentlyUsed;
    }
}

CacheLevelType CacheLevel::GetType(){
    return type;
}

uint32_t CacheLevel::GetLevel(){
    return level;
}

uint32_t CacheLevel::GetSetCount(){
    return countsets;
}

uint64_t CacheLevel::CountColdMisses(){
    return (countsets * associativity);
}

void CacheLevel::Print(uint32_t sysid){
    inform << TAB << dec << sysid
           << TAB << dec << level
           << TAB << dec << size
           << TAB << dec << associativity
           << TAB << dec << linesize
           << TAB << ReplacementPolicyNames[replpolicy]
           << TAB << TypeString()
           << ENDL;
}

uint64_t CacheLevel::GetStorage(uint64_t addr){
    return (addr >> linesizeBits);
}

uint32_t CacheLevel::GetSet(uint64_t addr){
    return (addr % countsets);
}

uint32_t CacheLevel::LineToReplace(uint32_t setid){
    if (replpolicy == ReplacementPolicy_nmru){
        return (recentlyUsed[setid] + 1) % associativity;
    } else if (replpolicy == ReplacementPolicy_random){
        return RandomInt(associativity);
    } else if (replpolicy == ReplacementPolicy_direct){
        return 0;
    } else {
        assert(0);
    }
    return 0;
}

uint64_t HighlyAssociativeCacheLevel::Replace(uint64_t addr, uint32_t setid, uint32_t lineid){
    uint64_t prev = contents[setid][lineid];
    contents[setid][lineid] = addr;

    unordered_map<uint64_t, uint32_t>* fastset = fastcontents[setid];
    if (fastset->count(prev) > 0){
        //assert((*fastset)[prev] == lineid);
        fastset->erase(prev);
    }
    (*fastset)[addr] = lineid;

    MarkUsed(setid, lineid);
    return prev;
}

uint64_t CacheLevel::Replace(uint64_t addr, uint32_t setid, uint32_t lineid){
    uint64_t prev = contents[setid][lineid];
    contents[setid][lineid] = addr;
    MarkUsed(setid, lineid);
    return prev;
}

inline void CacheLevel::MarkUsed(uint32_t setid, uint32_t lineid){
    if (USES_MARKERS(replpolicy)){
        debug(inform << "level " << dec << level << " USING set " << dec << setid << " line " << lineid << ENDL << flush);
        recentlyUsed[setid] = lineid;
    }
}

bool HighlyAssociativeCacheLevel::Search(uint64_t addr, uint32_t* set, uint32_t* lineInSet){
    uint32_t setId = GetSet(addr);
    debug(inform << TAB << TAB << "stored " << hex << addr << " set " << dec << setId << endl << flush);
    if (set){
        (*set) = setId;
    }

    unordered_map<uint64_t, uint32_t>* fastset = fastcontents[setId];
    if (fastset->count(addr) > 0){
        if (lineInSet){
            (*lineInSet) = (*fastset)[addr];
        }
        return true;
    }

    return false;
}

bool CacheLevel::Search(uint64_t addr, uint32_t* set, uint32_t* lineInSet){
    uint32_t setId = GetSet(addr);
    debug(inform << TAB << TAB << "stored " << hex << addr << " set " << dec << setId << endl << flush);
    if (set){
        (*set) = setId;
    }

    uint64_t* thisset = contents[setId];
    for (uint32_t i = 0; i < associativity; i++){
        //debug(inform << TAB << TAB << TAB << "checking assoc=" << dec << i << " contains " << hex << thisset[i] << endl << flush);
        if (thisset[i] == addr){
            if (lineInSet){
                (*lineInSet) = i;
            }
            return true;
        }
    }

    return false;
}

// TODO: not implemented
bool CacheLevel::MultipleLines(uint64_t addr, uint32_t width){
    return false;
}

uint32_t CacheLevel::Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info){
    uint32_t set = 0, lineInSet = 0;
    uint64_t store = GetStorage(addr);
    debug(inform << TAB << "level " << dec << level << endl << flush);

    // hit
    if (Search(store, &set, &lineInSet)){
        stats->Stats[memid][level].hitCount++;
        debug(inform << TAB << TAB << "hit" << endl << flush);
        MarkUsed(set, lineInSet);

        return INVALID_CACHE_LEVEL;
    }
    debug(inform << TAB << TAB << "missed" << endl << flush);

    // miss
    stats->Stats[memid][level].missCount++;
    Replace(store, set, LineToReplace(set));
    return level + 1;
}

uint32_t ExclusiveCacheLevel::Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info){
    uint32_t set = 0;
    uint32_t lineInSet = 0;

    uint64_t store = GetStorage(addr);
    debug(inform << TAB << "level " << dec << level << " store " << hex << store << endl << flush);

    // handle victimizing
    EvictionInfo* e = (EvictionInfo*)info; 
    if (e->level != INVALID_CACHE_LEVEL){

        set = GetSet(e->addr);
        lineInSet = LineToReplace(set);

        // use the location of the replaced line if the eviction happens to go to the same set
        if (level == e->level){
            if (e->setid == set){
                lineInSet = e->lineid;
            }
        }

        debug(inform << TAB << "up-evicting " << hex << e->addr << " into level " << dec << level << " set " << dec << set << " line " << dec << lineInSet << ENDL << flush);
        e->addr = Replace(e->addr, set, lineInSet);

        if (level == e->level){
            return INVALID_CACHE_LEVEL;
        } else {
            return level + 1;
        }
    }

    // hit
    if (Search(store, &set, &lineInSet)){
        stats->Stats[memid][level].hitCount++;
        MarkUsed(set, lineInSet);

        debug(inform << TAB << TAB << "hit" << endl << flush);
        e->level = level;
        e->addr = store;
        e->setid = set;
        e->lineid = lineInSet;

        if (level == FirstExclusive){
            return INVALID_CACHE_LEVEL;
        }
        return FirstExclusive;
    }

    debug(inform << TAB << TAB << "missed" << endl << flush);

    // miss
    stats->Stats[memid][level].missCount++;

    if (level == LastExclusive){
        e->level = LastExclusive + 1;
        e->addr = store;
        debug(inform << "airball!" << ENDL << flush);

        return FirstExclusive;
    }
    return level + 1;
}

MemoryStreamHandler::MemoryStreamHandler(){
    mlock = PTHREAD_MUTEX_INITIALIZER;
}
MemoryStreamHandler::~MemoryStreamHandler(){
}

bool MemoryStreamHandler::TryLock(){
    return (pthread_mutex_trylock(&mlock) == 0);
}

bool MemoryStreamHandler::Lock(){
    return (pthread_mutex_lock(&mlock) == 0);
}

bool MemoryStreamHandler::UnLock(){
    return (pthread_mutex_unlock(&mlock) == 0);
}

CacheStructureHandler::CacheStructureHandler(){
}

void CacheStructureHandler::Print(){
    inform << "CacheStructureHandler: "
           << "SysId " << dec << sysId
           << TAB << "Levels " << dec << levelCount
           << ENDL;

    for (uint32_t i = 0; i < levelCount; i++){
        levels[i]->Print(sysId);
    }

}

bool CacheStructureHandler::Verify(){
    bool passes = true;
    if (levelCount < 1 || levelCount > 3){
        warn << "Sysid " << dec << sysId
             << " has " << dec << levelCount << " levels."
             << ENDL << flush;
        passes = false;
    }

    ExclusiveCacheLevel* firstvc = NULL;
    for (uint32_t i = 0; i < levelCount; i++){
        if (levels[i]->GetType() == CacheLevelType_Exclusive){
            firstvc = dynamic_cast<ExclusiveCacheLevel*>(levels[i]);
            break;
        }
    }

    if (firstvc){
        for (uint32_t i = firstvc->GetLevel(); i <= firstvc->LastExclusive; i++){
            if (levels[i]->GetType() != CacheLevelType_Exclusive){
                warn << "Sysid " << dec << sysId
                     << " level " << dec << i
                     << " should be exclusive."
                     << ENDL << flush;
                passes = false;
            }
            if (levels[i]->GetSetCount() != firstvc->GetSetCount()){
                warn << "Sysid " << dec << sysId
                     << " has exclusive cache levels with different set counts."
                     << ENDL << flush;
                //passes = false;
            }
        }
    }
    return passes;
}

bool CacheStructureHandler::Init(string desc){
    description = desc;

    stringstream tokenizer(description);
    string token;
    uint32_t cacheValues[3];
    ReplacementPolicy repl;

    sysId = 0;
    levelCount = 0;

    uint32_t whichTok = 0;
    uint32_t firstExcl = INVALID_CACHE_LEVEL;
    for ( ; tokenizer >> token; whichTok++){

        // comment reached on line
        if (token.compare(0, 1, "#") == 0){
            break;
        }

        // 2 special tokens appear first
        if (whichTok == 0){
            if (!ParsePositiveInt32(token, &sysId)){
                return false;
            }
            continue;
        }
        if (whichTok == 1){
            if (!ParsePositiveInt32(token, &levelCount)){
                return false;
            }
            levels = new CacheLevel*[levelCount];
            continue;
        }

        int32_t idx = (whichTok - 2) % 4;
        // the first 3 numbers for a cache value
        if (idx < 3){
            if (!ParsePositiveInt32(token, &cacheValues[idx])){
                return false;
            }

            // the last token for a cache (replacement policy)
        } else {

            // parse replacement policy
            if (token.compare(0, 3, "lru") == 0){
                repl = ReplacementPolicy_nmru;
            } else if (token.compare(0, 4, "rand") == 0){
                repl = ReplacementPolicy_random;
            } else if (token.compare(0, 6, "trulru") == 0){
                warn << "True lru is not implemented... using nmru" << ENDL << flush;
                repl = ReplacementPolicy_nmru;
            } else if (token.compare(0, 3, "dir") == 0){
                repl = ReplacementPolicy_direct;
            } else {
                return false;
            }

            int32_t levelId = (whichTok - 2) / 4;

            // look for victim cache
            if (token.compare(token.size() - 3, token.size(), "_vc") == 0){
                if (firstExcl == INVALID_CACHE_LEVEL){
                    firstExcl = levelId;
                }
            } else {
                if (firstExcl != INVALID_CACHE_LEVEL){
                    warn << "nonsensible structure found in sysid " << sysId << "; using a victim cache for level " << levelId << ENDL << flush;
                }
            }

            // create cache
            uint32_t sizeInBytes = cacheValues[0];
            uint32_t assoc = cacheValues[1];
            uint32_t lineSize = cacheValues[2];
            if (assoc >= MinimumHighAssociativity){
                if (firstExcl != INVALID_CACHE_LEVEL){
                    HighlyAssociativeExclusiveCacheLevel* l = new HighlyAssociativeExclusiveCacheLevel();
                    l->Init(levelId, sizeInBytes, assoc, lineSize, repl, firstExcl, levelCount - 1);
                    levels[levelId] = (CacheLevel*)l;
                } else {
                    HighlyAssociativeInclusiveCacheLevel* l = new HighlyAssociativeInclusiveCacheLevel();
                    l->Init(levelId, sizeInBytes, assoc, lineSize, repl);
                    levels[levelId] = (CacheLevel*)l;
                }
            } else {
                if (firstExcl != INVALID_CACHE_LEVEL){
                    ExclusiveCacheLevel* l = new ExclusiveCacheLevel();
                    l->Init(levelId, sizeInBytes, assoc, lineSize, repl, firstExcl, levelCount - 1);
                    levels[levelId] = l;
                } else {
                    InclusiveCacheLevel* l = new InclusiveCacheLevel();
                    l->Init(levelId, sizeInBytes, assoc, lineSize, repl);
                    levels[levelId] = l;
                }
            }
        }
    }

    if (whichTok != levelCount * 4 + 2){
        return false;
    }

    return Verify();
}

CacheStructureHandler::~CacheStructureHandler(){
    if (levels){
        for (uint32_t i = 0; i < levelCount; i++){
            if (levels[i]){
                delete levels[i];
            }
        }
        delete[] levels;
    }
}

void CacheStructureHandler::Process(void* stats, BufferEntry* access){
    uint32_t next = 0;
    uint64_t victim = access->address;

    EvictionInfo evictInfo;
    evictInfo.level = INVALID_CACHE_LEVEL;
    debug(inform << "Processing sysid " << dec << sysId << " memory id " << dec << access->memseq << " addr " << hex << access->address << endl << flush);
    while (next < levelCount){
        debug(inform << TAB << "next=" << dec << next << ENDL << flush);
        next = levels[next]->Process((CacheStats*)stats, access->memseq, victim, (void*)(&evictInfo));
    }
}

void* GenerateCacheStats(void* args, uint32_t typ, image_key_t iid, thread_key_t tid){
    SimulationStats* stats = (SimulationStats*)args;

    // allocate Counters contiguously with SimulationStats. Since the address of SimulationStats is the
    // address of the thread data, this allows us to avoid an extra memory ref on Counter updates
    SimulationStats* s = (SimulationStats*)malloc(sizeof(SimulationStats) + (sizeof(uint64_t) * stats->BlockCount));
    assert(s);

    memcpy(s, stats, sizeof(SimulationStats));

    s->threadid = tid;
    s->imageid = iid;
    s->Initialized = false;

    // each thread gets its own buffer, all images for a thread share a buffer
    if (typ == AllData->ThreadType){
        s->Buffer = new BufferEntry[BUFFER_CAPACITY(stats) + 1];
        memcpy(s->Buffer, stats->Buffer, sizeof(BufferEntry) * (BUFFER_CAPACITY(stats) + 1));
        BUFFER_CURRENT(s) = 0;           
    }

    // each thread/image gets its own counters and stats
    uint64_t tmp64 = (uint64_t)(s) + (uint64_t)(sizeof(SimulationStats));
    s->Counters = (uint64_t*)(tmp64);

    // keep all CounterType_instruction in place
    memcpy(s->Counters, stats->Counters, sizeof(uint64_t) * s->BlockCount);
    for (uint32_t i = 0; i < s->BlockCount; i++){
        if (s->Types[i] != CounterType_instruction){
            s->Counters[i] = 0;
        }
    }


    s->Stats = new StreamStats*[CountMemoryHandlers];
    s->Stats[RangeHandlerIndex] = new RangeStats(s->InstructionCount);
    for (uint32_t i = 0; i < CountCacheStructures; i++){
        CacheStructureHandler* c = (CacheStructureHandler*)MemoryHandlers[i];
        s->Stats[i] = new CacheStats(c->levelCount, c->sysId, s->InstructionCount);
    }

    return (void*)s;
}

void ReadSettings(){

    // read caches to simulate
    string cachedf = GetCacheDescriptionFile();
    ifstream CacheFile(cachedf);
    if (CacheFile.fail()){
        ErrorExit("cannot open cache descriptions file: " << cachedf, MetasimError_FileOp);
    }
    
    string line;
    vector<CacheStructureHandler*> caches;
    while (getline(CacheFile, line)){
        if (IsEmptyComment(line)){
            continue;
        }
        CacheStructureHandler* c = new CacheStructureHandler();
        if (!c->Init(line)){
            ErrorExit("cannot parse cache description line: " << line, MetasimError_StringParse);
        }
        caches.push_back(c);
        c->Print();
    }

    CountMemoryHandlers = caches.size() + 1;
    assert(CountMemoryHandlers > 0 && "No cache structures found for simulation");
    assert(CountMemoryHandlers - 1 == RangeHandlerIndex);
    MemoryHandlers = new MemoryStreamHandler*[CountMemoryHandlers];
    for (uint32_t i = 0; i < CountCacheStructures; i++){
        MemoryHandlers[i] = caches[i];
    }
    MemoryHandlers[RangeHandlerIndex] = new AddressRangeHandler();

    uint32_t SampleMax;
    uint32_t SampleOn;
    uint32_t SampleOff;

    if (!ReadEnvUint32("METASIM_SAMPLE_MAX", &SampleMax)){
        SampleMax = DEFAULT_SAMPLE_MAX;
    }
    if (!ReadEnvUint32("METASIM_SAMPLE_OFF", &SampleOff)){
        SampleOff = DEFAULT_SAMPLE_OFF;
    }
    if (!ReadEnvUint32("METASIM_SAMPLE_ON", &SampleOn)){
        SampleOn = DEFAULT_SAMPLE_ON;
    }
    
    Sampler = new SamplingMethod(SampleMax, SampleOn, SampleOff);
    Sampler->Print();
}
