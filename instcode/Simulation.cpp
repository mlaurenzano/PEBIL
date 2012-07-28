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

// TODO: test per-insn
#include <InstrumentationCommon.hpp>
#include <Simulation.hpp>

// Can tinker with this at runtime using the environment variable
// METASIM_LIMIT_HIGH_ASSOC if desired.
static uint32_t MinimumHighAssociativity = 256;

// These control reuse distance calculations. Activate this feature by setting 
// METASIM_REUSE_WINDOW to something other than 0. The design of the reuse 
// distance tool is such that tracking a large window size isn't a lot more 
// expensive than a small size. Also this will be rendered relatively useless
// unless the sampling period (METASIM_SAMPLE_ON) is somewhat large relative 
// to METASIM_REUSE_WINDOW.
static uint32_t ReuseWindow = 0;
static const uint64_t ReuseCleanupMin = 10000000;
static const double ReusePrintScale = 1.5;
static const uint32_t ReuseIndivPrint = 32;

// global data
static uint32_t CountMemoryHandlers = 0;
static uint32_t CountCacheStructures = 0;
static uint32_t RangeHandlerIndex = 0;
static uint32_t ReuseHandlerIndex = 0;

static SamplingMethod* Sampler = NULL;
static DataManager<SimulationStats*>* AllData = NULL;
static FastData<SimulationStats*, BufferEntry*>* FastStats = NULL;
static set<uint64_t>* NonmaxKeys = NULL;

// should not be used directly. kept here to be cloned by anyone who needs it
static MemoryStreamHandler** MemoryHandlers = NULL;


#define synchronize(__locker) __locker->Lock(); for (bool __s = true; __s == true; __locker->UnLock(), __s = false) 

bool IsPower2(int32_t x)
{
    return ((x > 0) && ((x & (x - 1)) == 0));
}

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

void GetBufferIds(BufferEntry* b, image_key_t* i){
    *i = b->imageid;
}

extern "C" {
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn){
        InitializeDynamicInstrumentation(count, dyn);

        assert(AllData);

        synchronize(AllData){

            if (NonmaxKeys == NULL){
                NonmaxKeys = new set<uint64_t>();
            }

            set<uint64_t> keys;
            GetAllDynamicKeys(keys);
            for (set<uint64_t>::iterator it = keys.begin(); it != keys.end(); it++){
                uint64_t k = (*it);
                if (GET_TYPE(k) == PointType_bufferfill){
                    NonmaxKeys->insert(k);
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
                SetDynamicPoints(AllSimPoints, false);
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

            assert(FastStats);
            FastStats->AddThread(tid);
        } else {
            ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
        }
        return NULL;
    }

    void* tool_image_init(void* s, image_key_t* key, ThreadData* td){
        SimulationStats* stats = (SimulationStats*)s;

        set<uint64_t> inits;
        inits.insert(*key);
        SetDynamicPoints(inits, false);        

        assert(stats->Initialized == true);

        if (AllData == NULL){
            ReadSettings();
            AllData = new DataManager<SimulationStats*>(GenerateCacheStats, DeleteCacheStats, ReferenceCacheStats);
        }
        assert(AllData);
        AllData->AddImage(stats, td, *key);

        if (FastStats == NULL){
            FastStats = new FastData<SimulationStats*, BufferEntry*>(GetBufferIds, AllData, BUFFER_CAPACITY(stats));
        }
        assert(FastStats);
        FastStats->AddImage();

        stats->threadid = AllData->GenerateThreadKey();
        stats->imageid = *key;

        AllData->SetTimer(*key, 0);
        return NULL;
    }

    void ProcessBuffer(uint32_t HandlerIdx, MemoryStreamHandler* m, uint32_t numElements, image_key_t iid, thread_key_t tid){
        uint32_t threadSeq = AllData->GetThreadSequence(tid);
        uint32_t numProcessed = 0;

        SimulationStats** faststats = FastStats->GetBufferStats(tid);
        uint32_t bufcur = 0;
        for (bufcur = 0; bufcur < numElements; bufcur++){
            debug(assert(faststats[bufcur]));
            debug(assert(faststats[bufcur]->Stats));

            SimulationStats* stats = faststats[bufcur];
            StreamStats* ss = stats->Stats[HandlerIdx];
            BufferEntry* reference = BUFFER_ENTRY(stats, bufcur + 1);

            if (reference->imageid == 0){
                debug(assert(AllData->CountThreads() > 1));
                continue;
            }

            m->Process((void*)ss, reference);
            numProcessed++;
        }
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


        uint64_t numElements = BUFFER_CURRENT(stats);
        uint64_t capacity = BUFFER_CAPACITY(stats);
        uint32_t threadSeq = AllData->GetThreadSequence(tid);

        debug(inform 
            << "Thread " << hex << tid
            << TAB << "Image " << hex << iid
            << TAB << "Counter " << dec << numElements
            << TAB << "Capacity " << dec << capacity
            << TAB << "Total " << dec << Sampler->AccessCount
              << ENDL);


        bool isSampling;
        synchronize(AllData){
            isSampling = Sampler->CurrentlySampling();
            if (NonmaxKeys->empty()){
                AllData->UnLock();
                DONE_WITH_BUFFER();
            }
        }

        if (isSampling){
            BufferEntry* buffer = &(stats->Buffer[1]);

            //inform << "Processing buffer for thread " << hex << tid << " image " << iid << ENDL;
            FastStats->Refresh(buffer, numElements, tid);
            for (uint32_t i = 0; i < CountMemoryHandlers; i++){
                MemoryStreamHandler* m = stats->Handlers[i];
                ProcessBuffer(i, m, numElements, iid, tid);
            }
        } 

        synchronize(AllData){
            if (isSampling){
                set<uint64_t> MemsRemoved;
                SimulationStats** faststats = FastStats->GetBufferStats(tid);
                for (uint32_t j = 0; j < numElements; j++){
                    SimulationStats* s = faststats[j];
                    BufferEntry* reference = BUFFER_ENTRY(s, j + 1);
                    debug(inform << "Memseq " << dec << reference->memseq << " has " << s->Stats[0]->GetAccessCount(reference->memseq) << ENDL);
                    uint32_t bbid = s->BlockIds[reference->memseq];

                    // if max block count is reached, disable all buffer-related points related to this block
                    uint32_t idx = bbid;
                    uint32_t midx = bbid;
                    if (s->Types[bbid] == CounterType_instruction){
                        idx = s->Counters[bbid];
                    }
                    if (s->PerInstruction){
                        midx = s->MemopIds[bbid];
                    }

                    debug(inform << "Slot " << dec << j
                          << TAB << "Thread " << dec << AllData->GetThreadSequence(pthread_self())
                          << TAB << "BLock " << bbid
                          << TAB << "Counter " << s->Counters[bbid]
                          << TAB << "Real " << s->Counters[idx]
                          << ENDL);

                    if (Sampler->ExceedsAccessLimit(s->Counters[idx])){

                        uint64_t k1 = GENERATE_KEY(midx, PointType_buffercheck);
                        uint64_t k2 = GENERATE_KEY(midx, PointType_bufferinc);
                        uint64_t k3 = GENERATE_KEY(midx, PointType_bufferfill);

                        if (NonmaxKeys->count(k3) > 0){

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
                    debug(inform << "REMOVING " << dec << (MemsRemoved.size() / 3) << " blocks" << ENDL);
                    SuspendAllThreads(AllData->CountThreads(), AllData->allthreads.begin(), AllData->allthreads.end());
                    SetDynamicPoints(MemsRemoved, false);
                    ResumeAllThreads();
                }

                if (Sampler->SwitchesMode(numElements)){
                    SuspendAllThreads(AllData->CountThreads(), AllData->allthreads.begin(), AllData->allthreads.end());
                    SetDynamicPoints(*NonmaxKeys, false);
                    ResumeAllThreads();
                }

            } else {
                if (Sampler->SwitchesMode(numElements)){
                    SuspendAllThreads(AllData->CountThreads(), AllData->allthreads.begin(), AllData->allthreads.end());
                    SetDynamicPoints(*NonmaxKeys, true);
                    ResumeAllThreads();
                }

                // reuse distance handler needs to know that we passed over some addresses
                if (ReuseWindow){
                    ReuseDistanceHandler* r = (ReuseDistanceHandler*)stats->Handlers[ReuseHandlerIndex];
                    r->AddSequence(numElements);
                }
            }

            Sampler->IncrementAccessCount(numElements);
        }

        DONE_WITH_BUFFER();
    }

    void* process_buffer(image_key_t* key){
        image_key_t iid = *key;
        process_thread_buffer(iid, pthread_self());
    }

    void* tool_image_fini(image_key_t* key){
        image_key_t iid = *key;

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
        inform << "Printing cache simulation results to " << fileName << ENDL;
        TryOpen(MemFile, fileName);


        if (ReuseWindow){
            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                    SimulationStats* s = (SimulationStats*)AllData->GetData((*iit), (*it));
                    ReuseStats* r = (ReuseStats*)s->Stats[ReuseHandlerIndex];
                    assert(r);

                    vector<int32_t> dkeys;
                    for (unordered_map<int32_t, uint64_t>::iterator dit = r->DistanceCounts.begin(); dit != r->DistanceCounts.end(); dit++){
                        int32_t dist = (*dit).first;
                        if (dist != INVALID_REUSE_DISTANCE){
                            dkeys.push_back(dist);
                        }
                    }
                    sort(dkeys.begin(), dkeys.end());

                    inform << "Reuse distance bins for " << hex << s->Application << " Thread " << AllData->GetThreadSequence((*it)) << ENDL;

                    uint64_t currenttot = 0;
                    int32_t prevbin = 0;
                    for (vector<int32_t>::iterator dit = dkeys.begin(); dit != dkeys.end(); dit++){
                        int32_t dist = (*dit);
                        uint64_t count = r->DistanceCounts[dist];

                        currenttot += count;

                        if (dist <= ReuseIndivPrint || (int32_t)((double)prevbin * ReusePrintScale) <= dist || IsPower2(dist) || dist == dkeys.back() || prevbin == 0){
                            inform << TAB << dec << "(" << prevbin << "," << dist << "]" << TAB << currenttot << ENDL;

                            currenttot = 0;
                            prevbin = dist;
                        }
                    }

                    dkeys.clear();
                    inform << TAB << dec << ReuseWindow << "+" << TAB << r->DistanceCounts[INVALID_REUSE_DISTANCE] << ENDL;
                }
            }
        }

        uint64_t sampledCount = 0;
        uint64_t totalMemop = 0;
        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
            for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                SimulationStats* s = (SimulationStats*)AllData->GetData((*iit), (*it));
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
                    totalMemop += (s->Counters[idx] * s->MemopsPerBlock[idx]);
                }
                //inform << "Total memop: " << dec << totalMemop << TAB << "after " << hex << (*iit) << TAB << (*it) << ENDL;
            }
        }

        MemFile
            << "# appname       = " << stats->Application << ENDL
            << "# extension     = " << stats->Extension << ENDL
            << "# rank          = " << dec << GetTaskId() << ENDL
            << "# ntasks        = " << dec << GetNTasks() << ENDL
            << "# buffer        = " << BUFFER_CAPACITY(stats) << ENDL
            << "# total         = " << dec << totalMemop << ENDL
            << "# processed     = " << dec << sampledCount << " (" << ((double)sampledCount / (double)totalMemop * 100.0) << "% of total)" << ENDL
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
            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                bool first = true;
                for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
                    SimulationStats* s = AllData->GetData((*iit), (*it));
                    assert(s);

                    CacheStats* c = (CacheStats*)s->Stats[sys];
                    assert(c->Capacity == s->InstructionCount);

                    if (first){
                        MemFile << "# sysid" << dec << c->SysId << " in image " << hex << (*iit) << ENDL;
                        first = false;
                    }

                    MemFile << "#" << TAB << dec << AllData->GetThreadSequence((*it)) << " ";
                    for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                        uint64_t h = c->GetHits(lvl);
                        uint64_t m = c->GetMisses(lvl);
                        uint64_t t = h + m;
                        MemFile << "l" << dec << lvl << "[" << h << "/" << t << "(" << CacheStats::GetHitRate(h, m) << ")] ";
                    }
                    MemFile << ENDL;
                }
            }
            MemFile << ENDL;
        }

        MemFile 
            << "# " << "BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageSequence" << TAB << "ThreadId"
            << TAB << "BlockCounter" << TAB << "InstructionSimulated" << TAB << "MinAddress" << TAB << "MaxAddress" << TAB << "AddrRange"
            << ENDL;
        MemFile
            << "# " << TAB << "SysId" << TAB << "Level" << TAB << "HitCount" << TAB << "MissCount" << ENDL;

        for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
            for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){

                SimulationStats* st = AllData->GetData((*iit), (*it));
                assert(st);

                // compile per-instruction stats into blocks
                RangeStats* aggrange = new RangeStats(st->InstructionCount);
                for (uint32_t memid = 0; memid < st->InstructionCount; memid++){
                    uint32_t bbid;
                    RangeStats* r = (RangeStats*)st->Stats[RangeHandlerIndex];
                    if (st->PerInstruction){
                        bbid = memid;
                    } else {
                        bbid = st->BlockIds[memid];
                    }
                    aggrange->Update(bbid, r->GetMinimum(bbid), 0);
                    aggrange->Update(bbid, r->GetMaximum(bbid), r->GetAccessCount(bbid));
                }

                CacheStats** aggstats = new CacheStats*[CountCacheStructures];
                for (uint32_t sys = 0; sys < CountCacheStructures; sys++){

                    CacheStats* s = (CacheStats*)st->Stats[sys];
                    assert(s);
                    CacheStats* c = new CacheStats(s->LevelCount, s->SysId, st->BlockCount);
                    aggstats[sys] = c;

                    for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                        for (uint32_t memid = 0; memid < st->InstructionCount; memid++){
                            uint32_t bbid;
                            if (st->PerInstruction){
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
                        if (root->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] != 0){
                            inform << "bbid " << dec << bbid << " image " << hex << (*iit) << " accesses " << dec << root->GetAccessCount(bbid) << " memops " << st->MemopsPerBlock[bbid] << ENDL;
                        }
                        assert(root->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] == 0);
                    }

                    uint32_t idx;
                    if (st->Types[bbid] == CounterType_basicblock){
                        idx = bbid;
                    } else if (st->Types[bbid] == CounterType_instruction){
                        idx = st->Counters[bbid];
                    }

                    MemFile << "BLK" 
                            << TAB << dec << bbid
                            << TAB << hex << st->Hashes[bbid]
                            << TAB << dec << AllData->GetImageSequence((*iit))
                            << TAB << dec << AllData->GetThreadSequence(st->threadid)
                            << TAB << dec << st->Counters[idx]
                            << TAB << dec << root->GetAccessCount(bbid)
                            << TAB << hex << aggrange->GetMinimum(bbid)
                            << TAB << hex << aggrange->GetMaximum(bbid)
                            << TAB << hex << (aggrange->GetMaximum(bbid) - aggrange->GetMinimum(bbid))
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

#ifdef LEGACY_METASIM_SUPPORT
        // if single-thread and single-image, also print in old format
        if (AllData->CountThreads() == 1 && AllData->CountImages() == 1){
            const char* fileName = LegacySimulationFileName(stats);
            inform << LegacyToken << "printing cache simulation results to " << fileName << ENDL;
            TryOpen(MemFile, fileName);

            if (stats->PerInstruction){
                warn << "You are using per-instruction mode. " << LegacyToken << " files in this mode are unreliable" << ENDL;
            }

            MemFile
                << "# appname       = " << stats->Application << ENDL
                << "# extension     = " << stats->Extension << ENDL
                << "# rank          = " << dec << GetTaskId() << ENDL
                << "# buffer        = " << BUFFER_CAPACITY(stats) << ENDL
                << "# total         = " << dec << totalMemop << ENDL
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
                    MemFile << "l" << dec << lvl << "[" << t << "," << h << "(" << CacheStats::GetHitRate(h, m) << ")] ";
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


            // dump address range (dfp) file
            fileName = LegacyRangeFileName(stats);
            inform << LegacyToken << "printing address range results to " << fileName << ENDL;

            TryOpen(MemFile, fileName);

            MemFile
                << "# appname       = " << stats->Application << ENDL
                << "# extension     = " << "dfp" << ENDL
                << "# rank          = " << dec << GetTaskId() << ENDL
                << "# buffer        = " << BUFFER_CAPACITY(stats) << ENDL
                << "# total         = " << dec << totalMemop << ENDL
                << "# processed     = " << dec << sampledCount << ENDL
                << "# samplemax     = " << Sampler->AccessLimit << ENDL
                << "# sampleon      = " << Sampler->SampleOn << ENDL
                << "# sampleoff     = " << Sampler->SampleOff << ENDL
                << "# perinsn       = " << (stats->PerInstruction? "yes" : "no") << ENDL
                << "#" << ENDL;

            MemFile
                << "#block" << TAB << "<seqid>" << TAB << "<idiom>" << TAB << "<rangecnt>" << ENDL
                << "#range" << TAB << "<ranid>" << TAB << "<minaddress>" << TAB "<maxaddress>" << ENDL
                << ENDL;


            RangeStats* r = (RangeStats*)stats->Stats[RangeHandlerIndex];
            assert(r);
            assert(stats->InstructionCount == r->Capacity);

            uint32_t currentBlock = 0;
            bool doneCurrent = false;
            for (uint32_t i = 0; i < stats->InstructionCount; i++){
                if (r->Counts[i]){
                    if (stats->BlockIds[i] != currentBlock){
                        currentBlock = stats->BlockIds[i];
                        doneCurrent = false;
                    }
                    if (!doneCurrent){
                        MemFile
                            << "block" << TAB << dec << currentBlock << TAB << "dfTypePattern_Gather" << TAB << stats->MemopsPerBlock[i] << ENDL;
                    }

                    MemFile
                        << TAB << "range"
                        << TAB << hex << r->GetMinimum(i)
                        << TAB << hex << r->GetMaximum(i)
                        << ENDL;
                }
            }

            MemFile.close();
        }
#endif // LEGACY_MEATSIM_SUPPORT


        double t = (AllData->GetTimer(*key, 1) - AllData->GetTimer(*key, 0));
        inform << "CXXX Total Execution time for instrumented application: " << t << ENDL;
        double m = (double)(CountCacheStructures * Sampler->AccessCount);
        inform << "CXXX Memops simulated (includes only sampled memops in cache structures) per second: " << (m/t) << ENDL;

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

#ifdef LEGACY_METASIM_SUPPORT
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
#endif

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

ReuseDistanceHandler::ReuseDistanceHandler(uint32_t clean){
    cleanup = clean;
    if (cleanup < ReuseCleanupMin){
        cleanup = ReuseCleanupMin;
    }
    lastcleanup = 0;
    sequence = 0;
}

ReuseDistanceHandler::ReuseDistanceHandler(ReuseDistanceHandler& h){
    cleanup = h.cleanup;
    if (cleanup < ReuseCleanupMin){
        cleanup = ReuseCleanupMin;
    }
    lastcleanup = 0;
    sequence = 0;
}

void ReuseDistanceHandler::Print(ofstream& f){
    f << "Reuse Distance Handler" << ENDL;
}

void ReuseDistanceHandler::Clean(){
    if (sequence - lastcleanup < cleanup){
        return;
    }

    set<uint64_t> erase;
    for (unordered_map<uint64_t, uint64_t>::iterator it = window.begin(); it != window.end(); it++){
        uint64_t addr = (*it).first;
        uint64_t seq = (*it).second;

        if (sequence - seq >= cleanup){
            erase.insert(addr);
        }
    }

    //inform << "cleaning up reuse: " << dec << erase.size() << TAB <<sequence << TAB << lastcleanup << TAB << cleanup << ENDL;
    for (set<uint64_t>::iterator it = erase.begin(); it != erase.end(); it++){
        window.erase((*it));
    }

    lastcleanup = sequence;
}

void ReuseDistanceHandler::Process(void* stats, BufferEntry* access){
    uint64_t addr = access->address;
    ReuseStats* r = (ReuseStats*)stats;

    Clean();

    if (window.count(addr) == 0){
        r->Update(INVALID_REUSE_DISTANCE);
    } else {
        assert(window.count(addr) == 1);
        uint64_t d = sequence - window[addr];
        if (d >= ReuseWindow){
            r->Update(INVALID_REUSE_DISTANCE);            
        } else {
            r->Update(d);
        }
    }

    window[addr] = sequence++;
}

void ReuseStats::Update(uint64_t dist){
    assert(dist == INVALID_REUSE_DISTANCE || dist < ReuseWindow);
    if (DistanceCounts.count(dist) == 0){
        DistanceCounts[dist] = 0;
    }
    DistanceCounts[dist] = DistanceCounts[dist] + 1;
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
    Update(memid, addr, 1);
}

void RangeStats::Update(uint32_t memid, uint64_t addr, uint32_t count){
    AddressRange* r = Ranges[memid];
    if (addr < r->Minimum){
        r->Minimum = addr;
    }
    if (addr > r->Maximum){
        r->Maximum = addr;
    }
    Counts[memid] += count;
}

AddressRangeHandler::AddressRangeHandler(){
}
AddressRangeHandler::AddressRangeHandler(AddressRangeHandler& h){
    mlock = PTHREAD_MUTEX_INITIALIZER;
}
AddressRangeHandler::~AddressRangeHandler(){
}

void AddressRangeHandler::Print(ofstream& f){
    f << "AddressRangeHandler" << ENDL;
}

void AddressRangeHandler::Process(void* stats, BufferEntry* access){
    uint32_t memid = (uint32_t)access->memseq;
    uint64_t addr = access->address;
    RangeStats* rs = (RangeStats*)stats;

    rs->Update(memid, addr);
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

uint64_t ReferenceCacheStats(SimulationStats* stats){
    return (uint64_t)stats;
}

void DeleteCacheStats(SimulationStats* stats){
    if (!stats->Initialized){
        // TODO: delete buffer only for thread-initialized structures?

        delete[] stats->Counters;

        for (uint32_t i = 0; i < CountMemoryHandlers; i++){
            delete stats->Stats[i];
            delete stats->Handlers[i];
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
}

SamplingMethod::~SamplingMethod(){
}

void SamplingMethod::Print(){
    inform << "SamplingMethod:" << TAB << "AccessLimit " << AccessLimit << " SampleOn " << SampleOn << " SampleOff " << SampleOff << ENDL;
}

void SamplingMethod::IncrementAccessCount(uint64_t count){
    AccessCount += count;
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

    if (PeriodLength == 0){
        res = true;
    }
    if ((AccessCount + count) % PeriodLength < SampleOn){
        res = true;
    }
    return res;
}

bool SamplingMethod::ExceedsAccessLimit(uint64_t count){
    bool res = false;
    if (AccessLimit > 0 && count > AccessLimit){
        res = true;
    }
    return res;
}

CacheLevel::CacheLevel(){
}

void CacheLevel::Init(CacheLevel_Init_Interface){
    level = lvl;
    size = sizeInBytes;
    associativity = assoc;
    linesize = lineSz;
    replpolicy = pol;

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

void HighlyAssociativeCacheLevel::Init(CacheLevel_Init_Interface)
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

uint64_t CacheLevel::CountColdMisses(){
    return (countsets * associativity);
}

void CacheLevel::Print(ofstream& f, uint32_t sysid){
    f << TAB << dec << sysid
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

uint32_t CacheLevel::GetSet(uint64_t store){
    return (store % countsets);
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

uint64_t HighlyAssociativeCacheLevel::Replace(uint64_t store, uint32_t setid, uint32_t lineid){
    uint64_t prev = contents[setid][lineid];
    contents[setid][lineid] = store;

    unordered_map<uint64_t, uint32_t>* fastset = fastcontents[setid];
    if (fastset->count(prev) > 0){
        //assert((*fastset)[prev] == lineid);
        fastset->erase(prev);
    }
    (*fastset)[store] = lineid;

    MarkUsed(setid, lineid);
    return prev;
}

uint64_t CacheLevel::Replace(uint64_t store, uint32_t setid, uint32_t lineid){
    uint64_t prev = contents[setid][lineid];
    contents[setid][lineid] = store;
    MarkUsed(setid, lineid);
    return prev;
}

inline void CacheLevel::MarkUsed(uint32_t setid, uint32_t lineid){
    if (USES_MARKERS(replpolicy)){
        debug(inform << "level " << dec << level << " USING set " << dec << setid << " line " << lineid << ENDL << flush);
        recentlyUsed[setid] = lineid;
    }
}

bool HighlyAssociativeCacheLevel::Search(uint64_t store, uint32_t* set, uint32_t* lineInSet){
    uint32_t setId = GetSet(store);
    debug(inform << TAB << TAB << "stored " << hex << store << " set " << dec << setId << endl << flush);
    if (set){
        (*set) = setId;
    }

    unordered_map<uint64_t, uint32_t>* fastset = fastcontents[setId];
    if (fastset->count(store) > 0){
        if (lineInSet){
            (*lineInSet) = (*fastset)[store];
        }
        return true;
    }

    return false;
}

bool CacheLevel::Search(uint64_t store, uint32_t* set, uint32_t* lineInSet){
    uint32_t setId = GetSet(store);
    debug(inform << TAB << TAB << "stored " << hex << store << " set " << dec << setId << endl << flush);
    if (set){
        (*set) = setId;
    }

    uint64_t* thisset = contents[setId];
    for (uint32_t i = 0; i < associativity; i++){
        if (thisset[i] == store){
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

    debug(assert(stats));
    debug(assert(stats->Stats));
    debug(assert(stats->Stats[memid]));

    // hit
    if (Search(store, &set, &lineInSet)){
        stats->Stats[memid][level].hitCount++;
        MarkUsed(set, lineInSet);

        return INVALID_CACHE_LEVEL;
    }

    // miss
    stats->Stats[memid][level].missCount++;
    Replace(store, set, LineToReplace(set));
    return level + 1;
}

uint32_t ExclusiveCacheLevel::Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info){
    uint32_t set = 0;
    uint32_t lineInSet = 0;

    uint64_t store = GetStorage(addr);

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

        e->level = level;
        e->addr = store;
        e->setid = set;
        e->lineid = lineInSet;

        if (level == FirstExclusive){
            return INVALID_CACHE_LEVEL;
        }
        return FirstExclusive;
    }

    // miss
    stats->Stats[memid][level].missCount++;

    if (level == LastExclusive){
        e->level = LastExclusive + 1;
        e->addr = store;

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

CacheStructureHandler::CacheStructureHandler(CacheStructureHandler& h){
    sysId = h.sysId;
    levelCount = h.levelCount;
    description.assign(h.description);

#define LVLF(__i, __feature) (h.levels[__i])->Get ## __feature
#define Extract_Level_Args(__i) LVLF(__i, Level()), LVLF(__i, SizeInBytes()), LVLF(__i, Associativity()), LVLF(__i, LineSize()), LVLF(__i, ReplacementPolicy())
    levels = new CacheLevel*[levelCount];
    for (uint32_t i = 0; i < levelCount; i++){
        if (LVLF(i, Type()) == CacheLevelType_InclusiveLowassoc){
            InclusiveCacheLevel* l = new InclusiveCacheLevel();
            l->Init(Extract_Level_Args(i));
            levels[i] = l;
        } else if (LVLF(i, Type()) == CacheLevelType_InclusiveHighassoc){
            HighlyAssociativeInclusiveCacheLevel* l = new HighlyAssociativeInclusiveCacheLevel();
            l->Init(Extract_Level_Args(i));
            levels[i] = l;
        } else if (LVLF(i, Type()) == CacheLevelType_ExclusiveLowassoc){
            ExclusiveCacheLevel* l = new ExclusiveCacheLevel();
            ExclusiveCacheLevel* p = dynamic_cast<ExclusiveCacheLevel*>(h.levels[i]);
            assert(p->GetType() == CacheLevelType_ExclusiveLowassoc);
            l->Init(Extract_Level_Args(i), p->FirstExclusive, p->LastExclusive);
            levels[i] = l;
        } else if (LVLF(i, Type()) == CacheLevelType_ExclusiveHighassoc){
            HighlyAssociativeExclusiveCacheLevel* l = new HighlyAssociativeExclusiveCacheLevel();
            ExclusiveCacheLevel* p = dynamic_cast<ExclusiveCacheLevel*>(h.levels[i]);
            assert(p->GetType() == CacheLevelType_ExclusiveHighassoc);
            l->Init(Extract_Level_Args(i), p->FirstExclusive, p->LastExclusive);
            levels[i] = l;
        } else {
            assert(false);
        }
    }
}

void CacheStructureHandler::Print(ofstream& f){
    f << "CacheStructureHandler: "
           << "SysId " << dec << sysId
           << TAB << "Levels " << dec << levelCount
           << ENDL;

    for (uint32_t i = 0; i < levelCount; i++){
        levels[i]->Print(f, sysId);
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
        if (levels[i]->IsExclusive()){
            firstvc = dynamic_cast<ExclusiveCacheLevel*>(levels[i]);
            break;
        }
    }

    if (firstvc){
        for (uint32_t i = firstvc->GetLevel(); i <= firstvc->LastExclusive; i++){
            if (!levels[i]->IsExclusive()){
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
            if (!ParseInt32(token, &sysId, 0)){
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

            if (sizeInBytes < lineSize){
                return false;
            }

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
    while (next < levelCount){
        next = levels[next]->Process((CacheStats*)stats, access->memseq, victim, (void*)(&evictInfo));
    }
}

// called for every new image and thread
SimulationStats* GenerateCacheStats(SimulationStats* stats, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage){

    SimulationStats* s = stats;

    // allocate Counters contiguously with SimulationStats. Since the address of SimulationStats is the
    // address of the thread data, this allows us to avoid an extra memory ref on Counter updates
    if (typ == AllData->ThreadType){
        SimulationStats* s = stats;
        stats = (SimulationStats*)malloc(sizeof(SimulationStats) + (sizeof(uint64_t) * stats->BlockCount));
        assert(stats);
        memcpy(stats, s, sizeof(SimulationStats));
        stats->Initialized = false;
    }
    stats->threadid = tid;
    stats->imageid = iid;

    // every thread and image gets its own statistics
    stats->Stats = new StreamStats*[CountMemoryHandlers];
    bzero(stats->Stats, sizeof(StreamStats*) * CountMemoryHandlers);
    for (uint32_t i = 0; i < CountCacheStructures; i++){
        CacheStructureHandler* c = (CacheStructureHandler*)MemoryHandlers[i];
        stats->Stats[i] = new CacheStats(c->levelCount, c->sysId, stats->InstructionCount);
    }
    stats->Stats[RangeHandlerIndex] = new RangeStats(s->InstructionCount);
    if (ReuseWindow){
        stats->Stats[ReuseHandlerIndex] = new ReuseStats();
    }

    // all images within a thread share a set of memory handlers, but they don't exist for any image
    if (typ == AllData->ThreadType || (iid == firstimage)){
        stats->Handlers = new MemoryStreamHandler*[CountMemoryHandlers];
        for (uint32_t i = 0; i < CountCacheStructures; i++){
            CacheStructureHandler* p = (CacheStructureHandler*)MemoryHandlers[i];
            CacheStructureHandler* c = new CacheStructureHandler(*p);
            stats->Handlers[i] = c;
        }

        AddressRangeHandler* p = (AddressRangeHandler*)MemoryHandlers[RangeHandlerIndex];
        AddressRangeHandler* r = new AddressRangeHandler(*p);
        stats->Handlers[RangeHandlerIndex] = r;

        if (ReuseWindow){
            ReuseDistanceHandler* d = (ReuseDistanceHandler*)MemoryHandlers[ReuseHandlerIndex];
            ReuseDistanceHandler* h = new ReuseDistanceHandler(*d);
            stats->Handlers[ReuseHandlerIndex] = h;
        }
    } else {
        inform << "first image " << hex << firstimage << TAB << "iid " << iid << ENDL;
        SimulationStats * fs = AllData->GetData(firstimage, tid);
        stats->Handlers = fs->Handlers;
    }

    // each thread gets its own buffer
    if (typ == AllData->ThreadType){
        stats->Buffer = new BufferEntry[BUFFER_CAPACITY(stats) + 1];
        bzero(BUFFER_ENTRY(stats, 1), (BUFFER_CAPACITY(stats) + 1) * sizeof(BufferEntry));
        BUFFER_CAPACITY(stats) = BUFFER_CAPACITY(s);
        BUFFER_CURRENT(stats) = 0;
    } else if (iid != firstimage){
        SimulationStats* fs = AllData->GetData(firstimage, tid);
        stats->Buffer = fs->Buffer;
    }

    // each thread/image gets its own counters
    if (typ == AllData->ThreadType){
        uint64_t tmp64 = (uint64_t)(stats) + (uint64_t)(sizeof(SimulationStats));
        stats->Counters = (uint64_t*)(tmp64);

        // keep all CounterType_instruction in place
        memcpy(stats->Counters, s->Counters, sizeof(uint64_t) * s->BlockCount);
        for (uint32_t i = 0; i < stats->BlockCount; i++){
            if (stats->Types[i] != CounterType_instruction){
                stats->Counters[i] = 0;
            }
        }
    }

    return stats;
}

void ReadSettings(){

    uint32_t SaveHashMin = MinimumHighAssociativity;
    if (!ReadEnvUint32("METASIM_LIMIT_HIGH_ASSOC", &MinimumHighAssociativity)){
        MinimumHighAssociativity = SaveHashMin;
    }

    if (!ReadEnvUint32("METASIM_REUSE_WINDOW", &ReuseWindow)){
        ReuseWindow = 0;
    }

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
    }

    CountCacheStructures = caches.size();
    CountMemoryHandlers = CountCacheStructures;

    RangeHandlerIndex = CountMemoryHandlers;
    CountMemoryHandlers++;

    if (ReuseWindow){
        ReuseHandlerIndex = CountMemoryHandlers;
        CountMemoryHandlers++;
    }

    assert(CountCacheStructures > 0 && "No cache structures found for simulation");

    MemoryHandlers = new MemoryStreamHandler*[CountMemoryHandlers];
    for (uint32_t i = 0; i < CountCacheStructures; i++){
        MemoryHandlers[i] = caches[i];
    }
    MemoryHandlers[RangeHandlerIndex] = new AddressRangeHandler();
    if (ReuseWindow){
        MemoryHandlers[ReuseHandlerIndex] = new ReuseDistanceHandler(ReuseWindow);
    }

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
