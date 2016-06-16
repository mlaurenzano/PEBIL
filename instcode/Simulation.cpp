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
#include <ReuseDistance.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <string.h>
#include <assert.h>

#ifdef HAVE_ADAMANT
extern "C"{
    #include <adm.h> 
}
#endif
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
static uint32_t ReuseBin = 0;
static const uint64_t ReuseCleanupMin = 10000000;
static const double ReusePrintScale = 1.5;
static const uint32_t ReuseIndivPrint = 32;
// These control spatial locality calculations. Activate this feature by setting 
// // METASIM_SPATIAL_WINDOW to something other than 0. 
static uint32_t SpatialWindow = 0;
static uint32_t SpatialBin = 0;
static uint32_t SpatialNMAX = 0;

//These control Cache Simulation. Activate this feature by setting 
// METASIM_CACHE_SIMULATION to something other than 0.
static uint32_t CacheSimulation=0;

//These control Address Range Calculation. Activate this feature by setting 
// METASIM_ADDRESS_RANGE to something other than 0.
static uint32_t AddressRangeEnable=0;
static uint32_t EitherAddressRangeOrSimulation=0;
static uint32_t LoadStoreLogging = 0;
static uint32_t DirtyCacheHandling = 0; 
static uint32_t AdamantLogging = 0;

// global data
static uint32_t CountMemoryHandlers = 0;
static uint32_t CountReuseHandlers = 0;
static uint32_t CountCacheStructures = 0;
static uint32_t RangeHandlerIndex = 0;
static int32_t ReuseHandlerIndex = 0;
static int32_t SpatialHandlerIndex = 0;

static SamplingMethod* Sampler = NULL;
static DataManager<SimulationStats*>* AllData = NULL;
static FastData<SimulationStats*, BufferEntry*>* FastStats = NULL;
static set<uint64_t>* NonmaxKeys = NULL;

// should not be used directly. kept here to be cloned by anyone who needs it
static MemoryStreamHandler** MemoryHandlers = NULL;

static ReuseDistance** ReuseDistanceHandlers = NULL;


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
    // Called at just before image initialization
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn,bool* isThreadedModeFlag){
        SAVE_STREAM_FLAGS(cout);
        InitializeDynamicInstrumentation(count, dyn,isThreadedModeFlag);
        RESTORE_STREAM_FLAGS(cout);
        return NULL;
    }

    void* tool_mpi_init(){
        return NULL;
    }

    void* tool_thread_init(thread_key_t tid){
        SAVE_STREAM_FLAGS(cout);
        if (AllData){
            if(isThreadedMode())
                AllData->AddThread(tid);
            InitializeSuspendHandler();

            assert(FastStats);
            if(isThreadedMode())
                FastStats->AddThread(tid);
        } else {
            ErrorExit("Calling PEBIL thread initialization library for thread " << hex << tid << " but no images have been initialized.", MetasimError_NoThread);
        }
        RESTORE_STREAM_FLAGS(cout);
        return NULL;
    }

    void* tool_thread_fini(thread_key_t tid){
        SAVE_STREAM_FLAGS(cout);
        inform << "Destroying thread " << hex << tid << ENDL;

        // utilizing finished threads is *buggy*
        /*
        synchronize(AllData){
            AllData->FinishThread(tid);
        }
        */
        RESTORE_STREAM_FLAGS(cout);
    }

    // initializes an image
    // The mutex assures that the image is initialized exactly once, especially in the
    // case that multiple threads exist before this image is initialized
    // It should be unnecessary if only a single thread exists because
    // this function kills initialization points
    static pthread_mutex_t image_init_mutex = PTHREAD_MUTEX_INITIALIZER;
    void* tool_image_init(void* s, image_key_t* key, ThreadData* td){
        SAVE_STREAM_FLAGS(cout);
        SimulationStats* stats = (SimulationStats*)s;

        assert(stats->Initialized == true);

        pthread_mutex_lock(&image_init_mutex);

        // initialize AllData once per address space
        if (AllData == NULL){
            init_signal_handlers();
            ReadSettings();
            AllData = new DataManager<SimulationStats*>(GenerateCacheStats, DeleteCacheStats, ReferenceCacheStats);
        }
        assert(AllData);

        // Once per image
        if(AllData->allimages.count(*key) == 0){
            // Initialize image with AllData
            AllData->AddImage(stats, td, *key);

            // Once per address space, initialize FastStats
            // This must be done after AllData has exactly one image, thread initialized
            if (FastStats == NULL){
		//inform<<"\t FastStats wasnt initialized so far!! "<<ENDL;
                FastStats = new FastData<SimulationStats*, BufferEntry*>(GetBufferIds, AllData, BUFFER_CAPACITY(stats));
            }
            assert(FastStats);

            FastStats->AddImage();
            stats->threadid = AllData->GenerateThreadKey();
            stats->imageid = *key;
    
            // Get all dynamic point keys and possibly disable them
            synchronize(AllData){
                if (NonmaxKeys == NULL){
                    NonmaxKeys = new set<uint64_t>();
                }
    
                set<uint64_t> keys;
                GetAllDynamicKeys(keys);
                for (set<uint64_t>::iterator it = keys.begin(); it != keys.end(); it++){
                    uint64_t k = (*it);
                    if (GET_TYPE(k) == PointType_bufferfill && AllData->allimages.count(k) == 0){
                        NonmaxKeys->insert(k);
                    }
                }
    
                if (Sampler->SampleOn == 0){
                    inform << "Disabling all simulation-related instrumentation because METASIM_SAMPLE_ON is set to 0" << ENDL;
                    set<uint64_t> AllSimPoints;
                    for (set<uint64_t>::iterator it = NonmaxKeys->begin(); it != NonmaxKeys->end(); it++){
                        AllSimPoints.insert(GENERATE_KEY(GET_BLOCKID((*it)), PointType_buffercheck));
                        AllSimPoints.insert(GENERATE_KEY(GET_BLOCKID((*it)), PointType_bufferinc));
                        AllSimPoints.insert(GENERATE_KEY(GET_BLOCKID((*it)), PointType_bufferfill));
                    }
                    SetDynamicPoints(AllSimPoints, false);
                    NonmaxKeys->clear();
                }
    
                AllData->SetTimer(*key, 0);
            }

            // Kill initialization points for this image
            set<uint64_t> inits;
            inits.insert(*key);
            debug(inform << "Removing init points for image " << hex << (*key) << ENDL);
            SetDynamicPoints(inits, false); 

        }

        pthread_mutex_unlock(&image_init_mutex);

        RESTORE_STREAM_FLAGS(cout);
        return NULL;
    }

/*
    void printBuffer(SimulationStats* stats){
        uint32_t numElements = BUFFER_CURRENT(stats);

        for(uint32_t bufcur = 0; bufcur < numElements; ++bufcur){
            BufferEntry* reference = BUFFER_ENTRY(stats, bufcur);
            inform << "Buffer entry " << bufcur << ":" <<
                      " Address " << (void*)reference->address <<
                      " Memseq " << reference->memseq <<
                      " Imageid " << reference->imageid <<
                      " Threadid " << reference->threadid << ENDL;

        }
    }
*/

  //  void ProcessBuffer(uint32_t HandlerIdx, MemoryStreamHandler* m, ReuseDistance* rd, ReuseDistance* sd, uint32_t numElements, image_key_t iid, thread_key_t tid)
    static void ProcessBuffer(uint32_t HandlerIdx, MemoryStreamHandler* m,uint32_t numElements, image_key_t iid, thread_key_t tid){
        uint32_t threadSeq = AllData->GetThreadSequence(tid);
        uint32_t numProcessed = 0;
        uint32_t* resCacheProcess = new uint32_t[numElements];

        SimulationStats** faststats = FastStats->GetBufferStats(tid);
        //assert(faststats[0]->Stats[HandlerIdx]->Verify());
        uint32_t bufcur = 0; uint32_t resCur = 0;
        for (bufcur = 0; bufcur < numElements; bufcur++){
            debug(assert(faststats[bufcur]));
            debug(assert(faststats[bufcur]->Stats));

            SimulationStats* stats = faststats[bufcur];
            StreamStats* ss = stats->Stats[HandlerIdx];

            BufferEntry* reference = BUFFER_ENTRY(stats, bufcur);

            if (reference->imageid == 0){
                debug(assert(AllData->CountThreads() > 1));
                continue;
            }
            // This happens when uninitialized threads make their way into instrumented code.
            // See the FIXME in DataManager::AddImage
            // skip processing the reference for now
            if (reference->threadid != tid){
                continue;
            }

            resCur=m->Process((void*)ss, reference);
            resCacheProcess[bufcur] = resCur;
            numProcessed++;
        }

        #ifdef HAVE_ADAMANT
            if(AdamantLogging){
                adm_event_t* tempAdmEvents = new adm_event_t[numElements];
                uint64_t source =0;
                for(bufcur = 0; bufcur < numElements; bufcur++){
                    debug(assert(faststats[bufcur]));
                    debug(assert(faststats[bufcur]->Stats));
                    SimulationStats* stats = faststats[bufcur];
                    StreamStats* ss = stats->Stats[HandlerIdx];
                    BufferEntry* reference = BUFFER_ENTRY(stats, bufcur);

                    if (reference->imageid == 0){
                        debug(assert(AllData->CountThreads() > 1));
                        continue;
                    }
                    // This happens when uninitialized threads make their way into instrumented code.  // See the FIXME in DataManager::AddImage  // skip processing the reference for now
                    if (reference->threadid != tid){
                        continue;
                    }
                    source =0;
                    if(reference->loadstoreflag)
                        source |= PERF_MEM_OP_LOAD;
                    else
                        source |= PERF_MEM_OP_STORE;

                    source |= PERF_MEM_LVL_HIT  << PERF_MEM_LVL_SHIFT;
                    if(resCacheProcess[bufcur]==0)  
                        source |= PERF_MEM_LVL_L1 << PERF_MEM_LVL_SHIFT;
                    else if(resCacheProcess[bufcur]==1)
                        source |= PERF_MEM_LVL_L2 << PERF_MEM_LVL_SHIFT;
                    else if(resCacheProcess[bufcur]==2){
                        source |= PERF_MEM_LVL_L3 << PERF_MEM_LVL_SHIFT;
                    }
                    else 
                        source |= PERF_MEM_LVL_LOC_RAM << PERF_MEM_LVL_SHIFT;

                    tempAdmEvents[bufcur].ip= reference->programAddress;
                    tempAdmEvents[bufcur].tid= reference->threadid;
                    tempAdmEvents[bufcur].source= source; 
                    tempAdmEvents[bufcur].address = reference->address;
                }
                 adm_events_v(tempAdmEvents, numElements);

                 delete[] tempAdmEvents;
            }
         #endif
        delete[] resCacheProcess;
        //assert(faststats[0]->Stats[HandlerIdx]->Verify());
    }

    static void ProcessReuseBuffer(ReuseDistance* rd,uint32_t numElements, image_key_t iid, thread_key_t tid){

        uint32_t threadSeq = AllData->GetThreadSequence(tid);
        uint32_t numProcessed = 0;

        SimulationStats** faststats = FastStats->GetBufferStats(tid);
        //assert(faststats[0]->Stats[HandlerIdx]->Verify());
        uint32_t bufcur = 0;
        for (bufcur = 0; bufcur < numElements; bufcur++){
            debug(assert(faststats[bufcur]));
            debug(assert(faststats[bufcur]->Stats));

            SimulationStats* stats = faststats[bufcur];
            //StreamStats* ss = stats->Stats[HandlerIdx];

            BufferEntry* reference = BUFFER_ENTRY(stats, bufcur);

            if (reference->imageid == 0){
                debug(assert(AllData->CountThreads() > 1));
                continue;
            }
            assert(reference->threadid == tid);

            ReuseEntry entry = ReuseEntry();
            entry.id = stats->Hashes[stats->BlockIds[reference->memseq]]; // This is to track by BBID to track by memseq change to entry.id=reference->memseq;
            entry.address=reference->address ;
            rd->Process(entry);
            numProcessed++;
        }
    // assert(faststats[0]->Stats[HandlerIdx]->Verify());
    }

    static void ProcessSpatialBuffer( ReuseDistance* sd,uint32_t numElements, image_key_t iid, thread_key_t tid){
        uint32_t threadSeq = AllData->GetThreadSequence(tid);
        uint32_t numProcessed = 0;

        SimulationStats** faststats = FastStats->GetBufferStats(tid);
        //assert(faststats[0]->Stats[HandlerIdx]->Verify());
        uint32_t bufcur = 0;
        for (bufcur = 0; bufcur < numElements; bufcur++){
            debug(assert(faststats[bufcur]));
            debug(assert(faststats[bufcur]->Stats));

            SimulationStats* stats = faststats[bufcur];
            //StreamStats* ss = stats->Stats[HandlerIdx];

            BufferEntry* reference = BUFFER_ENTRY(stats, bufcur);

            if (reference->imageid == 0){
                debug(assert(AllData->CountThreads() > 1));
                continue;
            }
            assert(reference->threadid == tid);

            ReuseEntry entry = ReuseEntry();
            entry.id = stats->Hashes[stats->BlockIds[reference->memseq]]; // This is to track by BBID to track by memseq change to entry.id=reference->memseq;
            entry.address=reference->address ;
            sd->Process(entry);
            numProcessed++;
        }
    // assert(faststats[0]->Stats[HandlerIdx]->Verify());
    }


    static void* process_thread_buffer(image_key_t iid, thread_key_t tid){

#define DONE_WITH_BUFFER(...)                   \
        BUFFER_CURRENT(stats) = 0;                                      \
        return NULL;
        //bzero(BUFFER_ENTRY(stats, 0), sizeof(BufferEntry) * BUFFER_CAPACITY(stats)); \
        //return NULL;

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
        synchronize(AllData){
            if (isSampling){
                BufferEntry* buffer = &(stats->Buffer[1]);
                FastStats->Refresh(buffer, numElements, tid);
                if(EitherAddressRangeOrSimulation){
                    for (uint32_t i = 0; i < CountMemoryHandlers; i++)
                    {
                        MemoryStreamHandler* m = stats->Handlers[i];
                        ProcessBuffer(i, m, numElements, iid, tid);
                    }
                }            
                if(ReuseWindow){
                    ReuseDistance* rd=NULL;
                    rd = stats->RHandlers[ReuseHandlerIndex];
                    ProcessReuseBuffer(rd, numElements, iid, tid);       
                }
                if(SpatialWindow){    
                    ReuseDistance* sd=NULL; 
                    sd = stats->RHandlers[SpatialHandlerIndex];
                    ProcessSpatialBuffer(sd, numElements, iid, tid);
                }

                for(uint32_t i=0;i<(stats->NestedLoopCount);i++){
                    uint64_t* currInnerLevelBasicBlocks = stats->NLStats[i].InnerLevelBasicBlocks; 
                    for(uint32_t j=0;j<stats->NLStats[i].InnerLevelSize;j++){
                        if( stats->NLStats[i].GroupCount < stats->Counters[ currInnerLevelBasicBlocks[j] ] )
                            stats->NLStats[i].GroupCount = stats->Counters[ currInnerLevelBasicBlocks[j] ];
                    }
                }               
            } 
        }

        synchronize(AllData){
            if (isSampling){
                set<uint64_t> MemsRemoved;
                SimulationStats** faststats = FastStats->GetBufferStats(tid);
                for (uint32_t j = 0; j < numElements; j++){
                    SimulationStats* s = faststats[j];
                    BufferEntry* reference = BUFFER_ENTRY(s, j);

                    debug(inform << "Memseq " << dec << reference->memseq
                          << " has " << s->Stats[0]->GetAccessCount(reference->memseq)
                          << ENDL);

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

                    uint64_t groupidx = stats->GroupIds[bbid];
                    if (Sampler->ExceedsAccessLimit(s->Counters[idx])
                    || (Sampler->ExceedsAccessLimit(stats->NLStats[groupidx].GroupCount)) ){

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
                if (ReuseWindow || SpatialWindow){
                    if(ReuseWindow) {    
                        ReuseDistance* r = stats->RHandlers[ReuseHandlerIndex];
                        r->SkipAddresses(numElements);
                    }
                    if(SpatialWindow) {
                        ReuseDistance* s = stats->RHandlers[SpatialHandlerIndex];
                        s->SkipAddresses(numElements);
                    }
                }
            }

            Sampler->IncrementAccessCount(numElements);
        }

        DONE_WITH_BUFFER();
    }

    // conditionally called at first memop in each block
    void* process_buffer(image_key_t* key){
        // forgo this since we shouldn't be printing anything during production
        SAVE_STREAM_FLAGS(cout);

        image_key_t iid = *key;
        process_thread_buffer(iid, pthread_self());

        RESTORE_STREAM_FLAGS(cout);
    }

    void* tool_image_fini(image_key_t* key){
        image_key_t iid = *key;

        AllData->SetTimer(iid, 1);
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
        SimulationStats* stats = (SimulationStats*)AllData->GetData(iid, pthread_self());
        if (stats == NULL){
            ErrorExit("Cannot retreive image data using key " << dec << (*key), MetasimError_NoImage);
            return NULL;
        }

        // only print stats when the master image exits
        if (!stats->Master){
            RESTORE_STREAM_FLAGS(cout);
            return NULL;
        }

        // clear all threads' buffers
        for (set<thread_key_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
            process_thread_buffer(iid, (*it));
        }

        ofstream MemFile,RangeFile;
        string oFile;
        const char* fileName;
  
        if (ReuseWindow){
            ofstream ReuseDistFile;
            ReuseDistFileName(stats, oFile);
            fileName = oFile.c_str();
            inform << "Printing reuse distance results to " << fileName << ENDL;
            TryOpen(ReuseDistFile, fileName);

            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                for(DataManager<SimulationStats*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it) {

                    thread_key_t thread = it->first;
                    SimulationStats* s = it->second;
                    ReuseDistFile << "IMAGE" << TAB << hex << (*iit) << TAB << "THREAD" << TAB << dec << AllData->GetThreadSequence(thread) << ENDL;
            
                    ReuseDistance* rd = s->RHandlers[ReuseHandlerIndex];
                    assert(rd);
                    inform << "Reuse distance bins for " << hex << s->Application << " Thread " << AllData->GetThreadSequence(thread) << ENDL;
                    rd->Print();
                    rd->Print(ReuseDistFile);
                }
            }
            ReuseDistFile.close();
        }
        if (SpatialWindow){
            ofstream SpatialDistFile;
            SpatialDistFileName(stats, oFile);
            fileName = oFile.c_str();
            inform << "Printing spatial locality results to " << fileName << ENDL;
            TryOpen(SpatialDistFile, fileName);

            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                for(DataManager<SimulationStats*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it){
                    thread_key_t thread = it->first;
                    SimulationStats* s = it->second;

                    SpatialDistFile << "IMAGE" << TAB << hex << (*iit) << TAB << "THREAD" << TAB << dec << AllData->GetThreadSequence(thread) << ENDL;

                    ReuseDistance* sd = s->RHandlers[SpatialHandlerIndex];
                    assert(sd);
                    inform << "Spatial locality bins for " << hex << s->Application << " Thread " << AllData->GetThreadSequence(thread) << ENDL;
                    sd->Print();
                    sd->Print(SpatialDistFile);
                }
            }
            SpatialDistFile.close();
        } 
        if( EitherAddressRangeOrSimulation ){
            //inform<<"\t EitherAddressRangeOrSimulation ?? "<<(EitherAddressRangeOrSimulation)
            // dump cache simulation results
            bool BothRangeAndSimulation = (AddressRangeEnable&&CacheSimulation); 
            if(CacheSimulation){
                    SimulationFileName(stats, oFile);
                    fileName = oFile.c_str();
                    inform << "Printing cache simulation results to " << fileName << ENDL;
                    TryOpen(MemFile, fileName);
            }
            if(EitherAddressRangeOrSimulation){
                RangeFileName(stats,oFile);
                fileName=oFile.c_str();
                inform << "Printing address range results to " << fileName << ENDL;
                TryOpen(RangeFile,fileName);
            }

            uint64_t sampledCount = 0;
            uint64_t totalMemop = 0;
            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                for(DataManager<SimulationStats*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it) {
                    thread_key_t thread = it->first;
                    SimulationStats* s = it->second;

                    if(EitherAddressRangeOrSimulation){
                        RangeStats* r = (RangeStats*)s->Stats[RangeHandlerIndex];
                        assert(r);
                        for (uint32_t i = 0; i < r->Capacity; i++){
                            sampledCount += r->Counts[i];
                        }
                    }
                    if(CacheSimulation){
                        for (uint32_t i = 0; i < s->BlockCount; i++){
                        uint32_t idx;
                        if (s->Types[i] == CounterType_basicblock){
                            idx = i;
                        } else if (s->Types[i] == CounterType_instruction){
                            idx = s->Counters[i];
                        }
                            totalMemop += (s->Counters[idx] * s->MemopsPerBlock[idx]);
                        }
                        inform << "Total memop: " << dec << totalMemop << TAB << " sampledCount "<<sampledCount<< ENDL;
                    }
                }
            }

            if(CacheSimulation){
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
                    << TAB << (s->Master ? "Executable" : "SharedLib") // FIXME master is not necessarily the executable
                    << TAB << s->Application
                    << ENDL;
                }
                MemFile << ENDL;
            }
            if(EitherAddressRangeOrSimulation){
                RangeFile
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
        
                RangeFile
                << "# IMG"
                << TAB << "ImageHash"
                << TAB << "ImageSequence"
                << TAB << "ImageType"
                << TAB << "Name"
                << ENDL;
                
                for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                    SimulationStats* s = (SimulationStats*)AllData->GetData((*iit), pthread_self());
                    RangeFile 
                    << "IMG"
                    << TAB << hex << (*iit)
                    << TAB << dec << AllData->GetImageSequence((*iit))
                    << TAB << (s->Master ? "Executable" : "SharedLib")
                    << TAB << s->Application
                    << ENDL;
                }
                RangeFile << ENDL;        
            }
            if(CacheSimulation){
                for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
                    for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                            bool first = true;
                            for(DataManager<SimulationStats*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it) {
                            SimulationStats* s = it->second;
                            thread_key_t thread = it->first;
                            assert(s);

                            CacheStats* c = (CacheStats*)s->Stats[sys];
                            assert(c->Capacity == s->AllocCount);//assert(c->Capacity == s->InstructionCount);
                            if(!c->Verify()) {
                                warn << "Cache structure failed verification for  system " << c->SysId << ", image " << hex << *iit << ", thread " << hex << thread << ENDL;
                            }

                            if (first){
                                MemFile << "# sysid" << dec << c->SysId << " in image " << hex << (*iit) << ENDL;
                                first = false;
                            }

                            MemFile << "#" << TAB << dec << AllData->GetThreadSequence(thread) << " ";
                            for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                                uint64_t h = c->GetHits(lvl);
                                uint64_t m = c->GetMisses(lvl);
                                uint64_t t = h + m;
                                MemFile << "l" << dec << lvl << "[" << h << "/" << t << "(" << CacheStats::GetHitRate(h, m) << ")] ";
                            }
                            if(LoadStoreLogging){
                                MemFile<<"\n#Load store stats ";
                                for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                                    uint64_t l = c->GetLoads(lvl);
                                    uint64_t s = c->GetStores(lvl);
                                    uint64_t t = l + s;
                                    double ratio=0.0f;
                                    if(t!=0)
                                      ratio= (double) l/t;
                                    MemFile << " l" << dec << lvl << "[" << l << "/" << t << "(" << (ratio)<<")] ";
                                }                                   
                            }
                            if(c->hybridCache){    
                                uint64_t h=c->GetHybridHits();
                                uint64_t m=c->GetHybridMisses();
                                uint64_t t_hm= h+m; 
                                double ratio_hm,ratio_ls;
                                 if(t_hm!=0)
                                    ratio_hm= (double) h/t_hm;
                                MemFile << ENDL ;     
                                MemFile<<"#Hybrid cache stats\tHits " << "[" << h << "/" << t_hm << "(" << (ratio_hm)<< ")]";

                                if(LoadStoreLogging){
                                    uint64_t l=c->GetHybridLoads();
                                    uint64_t s=c->GetHybridStores();
                                    uint64_t t_ls= l+s; 
                                    if(t_ls!=0)
                                        ratio_ls=(double) l/t_ls;                                                                
                                    MemFile<<" ; Loads " << "[" << l << "/" << t_ls << "(" << (ratio_ls)<< ")]";
                                }
                             }                               
                             
                            MemFile << ENDL;
                        }
                    }
                    MemFile << ENDL;
                }
            }

            uint32_t* HybridCacheStatus=(uint32_t*)malloc( CountCacheStructures * sizeof(uint32_t) );
            for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
                CacheStructureHandler* CheckHybridStructure= (CacheStructureHandler*)stats->Handlers[sys];
                HybridCacheStatus[sys]=CheckHybridStructure->hybridCache;
            }

            if(BothRangeAndSimulation){
                MemFile << "# " << "BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageSequence" 
                << TAB << "ThreadId"<< TAB << "BlockCounter" << TAB << "InstructionSimulated" << TAB 
                << "MinAddress" << TAB << "MaxAddress" << TAB << "AddrRange " << ENDL;  
            }
            else if(CacheSimulation){
                MemFile 
                << "# " << "BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageSequence" << TAB << "ThreadId " << ENDL;        
                if(LoadStoreLogging)
                    MemFile<< "# " << TAB << "SysId" << TAB << "Level" << TAB << "HitCount" << TAB << "MissCount" << TAB << "LoadCount" << TAB << "StoreCount" << ENDL;                
                else
                    MemFile<< "# " << TAB << "SysId" << TAB << "Level" << TAB << "HitCount" << TAB << "MissCount" << ENDL;   
            }   
            if(EitherAddressRangeOrSimulation){
                RangeFile << "# " << "BLK" << TAB << "Sequence" << TAB << "Hashcode" << TAB << "ImageSequence" 
                << TAB << "ThreadId"<< TAB << "BlockCounter" << TAB << "InstructionSimulated" << TAB 
                << "MinAddress" << TAB << "MaxAddress" << TAB << "AddrRange " << ENDL;
            }

            for (set<image_key_t>::iterator iit = AllData->allimages.begin(); iit != AllData->allimages.end(); iit++){
                for(DataManager<SimulationStats*>::iterator it = AllData->begin(*iit); it != AllData->end(*iit); ++it){

                    SimulationStats* st = it->second;
                    assert(st);
                    CacheStats** aggstats;
                    RangeStats* aggrange;
                    // compile per-instruction stats into blocks
                    if(EitherAddressRangeOrSimulation){
                        //RangeStats* aggrange = new RangeStats(st->InstructionCount);
                        aggrange = new RangeStats(st->AllocCount);//RangeStats(st->InstructionCount);

                        for (uint32_t memid = 0; memid < st->AllocCount; memid++){
                            uint32_t bbid;
                            RangeStats* r = (RangeStats*)st->Stats[RangeHandlerIndex];
                            if (st->PerInstruction){
                                bbid = memid;
                            } else {
                                bbid = st->BlockIds[memid];
                            }

                            aggrange->Update(bbid, r->GetMinimum(memid), 0);
                            aggrange->Update(bbid, r->GetMaximum(memid), r->GetAccessCount(memid));
                        }
                    }
                    if(CacheSimulation){
                        //CacheStats** aggstats = new CacheStats*[CountCacheStructures];
                        aggstats = new CacheStats*[CountCacheStructures];
                        for (uint32_t sys = 0; sys < CountCacheStructures; sys++){

                            CacheStats* s = (CacheStats*)st->Stats[sys];
                            assert(s);
                            s->Verify();
                            CacheStats* c = new CacheStats(s->LevelCount, s->SysId, st->BlockCount, s->hybridCache);
                            aggstats[sys] = c;

                            for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                                    for (uint32_t memid = 0; memid < st->AllocCount; memid++){
                                        uint32_t bbid;
                                        if (st->PerInstruction){
                                            bbid = memid;
                                        } else {
                                            bbid = st->BlockIds[memid];
                                        }
                                        c->Hit(bbid, lvl, s->GetHits(memid, lvl));
                                        c->Miss(bbid, lvl, s->GetMisses(memid, lvl));
                                        if(LoadStoreLogging){
                                            c->Load(bbid, lvl, s->GetLoads(memid, lvl));
                                            c->Store(bbid, lvl, s->GetStores(memid, lvl));                                    
                                        }
                                }
                                if(!c->Verify()) {
                                    warn << "Failed check on aggregated cache stats" << ENDL;
                                }
                            }
                            if(c->hybridCache){
                                for (uint32_t memid = 0; memid < st->AllocCount; memid++){
                                    uint32_t bbid;
                                    if (st->PerInstruction){
                                        bbid = memid;
                                    } else {
                                        bbid = st->BlockIds[memid];
                                    }          
                                    //inform<<"\t memid "<< memid;                         
                                    c->HybridHit(bbid,s->GetHybridHits(memid)) ;
                                    c->HybridMiss(bbid,s->GetHybridMisses(memid));  
                                    if(LoadStoreLogging){
                                        c->HybridLoad(bbid,s->GetHybridLoads(memid)); 
                                        c->HybridStore(bbid,s->GetHybridStores(memid));                                        
                                    }
                                }                                    
                            }                               
                        }
                        uint32_t MaxCapacity;
                        CacheStats* root;
                        if(CacheSimulation){
                            root= aggstats[0];
                            MaxCapacity=root->Capacity;
                        }
                        else if(EitherAddressRangeOrSimulation)
                            MaxCapacity=aggrange->Capacity;
                        
                        for (uint32_t bbid = 0; bbid < MaxCapacity; bbid++){
                            // dont print blocks which weren't touched
                            if (CacheSimulation &&(root->GetAccessCount(bbid) == 0)){
                                continue;
                            }
                            else if ( EitherAddressRangeOrSimulation && (aggrange->GetAccessCount(bbid)==0)){
                                continue;
                            }
                            // this isn't necessarily true since this tool can suspend threads at any point,
                            // potentially shutting off instrumention in a block while a thread is midway through
                            if (AllData->CountThreads() == 1){
                                if(CacheSimulation){
                                    if (root->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] != 0){
                                        inform << "bbid " << dec << bbid << " image " << hex << (*iit) << " accesses " << dec << root->GetAccessCount(bbid) << " memops " << st->MemopsPerBlock[bbid] << ENDL;
                                    }
                                    assert(root->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] == 0);
                                }
                                else if (EitherAddressRangeOrSimulation){
                                    if (aggrange->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] != 0){
                                        inform << "bbid " << dec << bbid << " image " << hex << (*iit) << " accesses " << dec << aggrange->GetAccessCount(bbid) << " memops " << st->MemopsPerBlock[bbid] << ENDL;
                                    }
                                    assert(aggrange->GetAccessCount(bbid) % st->MemopsPerBlock[bbid] == 0);                        
                                }
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
                                    << TAB << dec << AllData->GetThreadSequence(st->threadid);
                                    //<<ENDL;
                            if(BothRangeAndSimulation){
                                MemFile<< TAB << dec << st->Counters[idx]
                                << TAB << dec << aggrange->GetAccessCount(bbid)
                                << TAB << hex << aggrange->GetMinimum(bbid)
                                << TAB << hex << aggrange->GetMaximum(bbid)
                                << TAB << hex << (aggrange->GetMaximum(bbid) - aggrange->GetMinimum(bbid));
                                // << ENDL;
                            }
                            if(EitherAddressRangeOrSimulation){
                                RangeFile  << "BLK" 
                                << TAB << dec << bbid
                                << TAB << hex << st->Hashes[bbid]
                                << TAB << dec << AllData->GetImageSequence((*iit))
                                << TAB << dec << AllData->GetThreadSequence(st->threadid)
                                << TAB << dec << st->Counters[idx]
                                << TAB << dec << aggrange->GetAccessCount(bbid)
                                << TAB << hex << aggrange->GetMinimum(bbid)
                                << TAB << hex << aggrange->GetMaximum(bbid)
                                << TAB << hex << (aggrange->GetMaximum(bbid) - aggrange->GetMinimum(bbid))<<ENDL;                
                            }
                            if(CacheSimulation){
                                MemFile<< ENDL;
                                for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
                                    CacheStats* c = aggstats[sys];
                                    if (AllData->CountThreads() == 1){
                                        assert(root->GetAccessCount(bbid) == c->GetHits(bbid, 0) + c->GetMisses(bbid, 0));
                                    }

                                    for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
                                         if(LoadStoreLogging){
                                              MemFile
                                              << TAB << dec << c->SysId
                                              << TAB << dec << (lvl+1)
                                              << TAB << dec << c->GetHits(bbid, lvl)
                                              << TAB << dec << c->GetMisses(bbid, lvl)
                                              << TAB << dec << c->GetLoads(bbid,lvl)
                                              << TAB << dec << c->GetStores(bbid,lvl)
                                              << ENDL;  
                                         }else{
                                              MemFile
                                              << TAB << dec << c->SysId
                                              << TAB << dec << (lvl+1)
                                              << TAB << dec << c->GetHits(bbid, lvl)
                                              << TAB << dec << c->GetMisses(bbid, lvl)
                                              << ENDL;                                                                                
                                         }
                                    }

                                    if(HybridCacheStatus[sys]){
                                        //CacheStructureHandler* CHSH=(CacheHybridStructureHandler*)stats->Handlers[sys] ;
                                        MemFile << TAB << dec << c->SysId
                                        << TAB << dec << (c->LevelCount)
                                        << TAB << dec << c->GetHybridHits(bbid)
                                        << TAB << dec << c->GetHybridMisses(bbid)
                                        << TAB << dec << c->GetHybridLoads(bbid) //,c->LevelCount-1)
                                        << TAB << dec << c->GetHybridStores(bbid) //,c->LevelCount-1)
                                        << ENDL;
                                    }
                                }
                            }
                        }

                        if(CacheSimulation){
                            for (uint32_t i = 0; i < CountCacheStructures; i++){
                                delete aggstats[i];
                            }
                            delete[] aggstats;
                        }
                    }
                }
            }
            if(CacheSimulation)
                MemFile.close();
            if(EitherAddressRangeOrSimulation)
                RangeFile.close();
            
            double t = (AllData->GetTimer(*key, 1) - AllData->GetTimer(*key, 0));
            inform << "CXXX Total Execution time for instrumented application " << t << ENDL;
            double m = (double)(CountCacheStructures * Sampler->AccessCount);
            inform << "CXXX Memops simulated (includes only sampled memops in cache structures) per second: " << (m/t) << ENDL;
            if(NonmaxKeys){
                delete NonmaxKeys;
            }
        }    
        RESTORE_STREAM_FLAGS(cout);
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
        CacheStats* c = new CacheStats(s->LevelCount, s->SysId, stats->BlockCount,s->hybridCache);
        aggstats[sys] = c;

        for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){

            for (uint32_t memid = 0; memid < stats->AllocCount; memid++){
                uint32_t bbid = stats->BlockIds[memid];
                c->Hit(bbid, lvl, s->GetHits(memid, lvl));
                c->Miss(bbid, lvl, s->GetMisses(memid, lvl));
            }
        }
        if(c->hybridCache){
            for (uint32_t memid = 0; memid < stats->AllocCount; memid++){
                uint32_t bbid = stats->BlockIds[memid];
                c->HybridHit(bbid,s->GetHybridHits(memid)) ;
                c->HybridMiss(bbid,s->GetHybridMisses(memid));
                if(LoadStoreLogging){
                    c->HybridLoad(bbid,s->GetHybridLoads(memid));                           
                    c->HybridStore(bbid,s->GetHybridStores(memid));
                }    
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

void SimulationFileName(SimulationStats* stats, string& oFile){
    oFile.clear();
    const char* prefix = getenv(ENV_OUTPUT_PREFIX);
    if(prefix != NULL) {
        oFile.append(prefix);
        oFile.append("/");
    }
    oFile.append(stats->Application);
    oFile.append(".r");
    AppendRankString(oFile);
    oFile.append(".t");
    AppendTasksString(oFile);
    oFile.append(".");
    oFile.append(stats->Extension);
}


void ReuseDistFileName(SimulationStats* stats, string& oFile){
    oFile.clear();
    oFile.append(stats->Application);
    oFile.append(".r");
    AppendRankString(oFile);
    oFile.append(".t");
    AppendTasksString(oFile);
    oFile.append(".dist");
    //oFile.append(stats->Extension);
}

void SpatialDistFileName(SimulationStats* stats, string& oFile){
    oFile.clear();
    oFile.append(stats->Application);
    oFile.append(".r");
    AppendRankString(oFile);
    oFile.append(".t");
    AppendTasksString(oFile);
    oFile.append(".spatial");
    //oFile.append(stats->Extension);
}

void RangeFileName(SimulationStats* stats, string& oFile){
    oFile.clear();
    oFile.append(stats->Application);
    oFile.append(".r");
    AppendRankString(oFile);
    oFile.append(".t");
    AppendTasksString(oFile);
    oFile.append(".");
    oFile.append("dfp");
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

bool RangeStats::Verify(){
    return true;
}

AddressRangeHandler::AddressRangeHandler(){
}
AddressRangeHandler::AddressRangeHandler(AddressRangeHandler& h){
    pthread_mutex_init(&mlock, NULL);
}
AddressRangeHandler::~AddressRangeHandler(){
}

void AddressRangeHandler::Print(ofstream& f){
    f << "AddressRangeHandler" << ENDL;
}

uint32_t AddressRangeHandler::Process(void* stats, BufferEntry* access){
    uint32_t memid = (uint32_t)access->memseq;
    uint64_t addr = access->address;
    RangeStats* rs = (RangeStats*)stats;
    rs->Update(memid, addr);
     return 0; // Nonsensical to return anything, but to keep it compatbile with base class method which should return when used by CacheHandler.  
}

CacheStats::CacheStats(uint32_t lvl, uint32_t sysid, uint32_t capacity,uint32_t hybridcache){
    LevelCount = lvl;
    SysId = sysid;
    Capacity = capacity;
    hybridCache=hybridcache;

    Stats = new LevelStats*[Capacity];
    if(hybridCache){   
        HybridMemStats=new LevelStats[Capacity];
        for (uint32_t i = 0; i < Capacity; i++){
            memset(&HybridMemStats[i],0,sizeof(LevelStats));
        }       
    }

    for (uint32_t i = 0; i < Capacity; i++){
        NewMem(i);
    }
    assert(Verify());
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

void CacheStats::Load(uint32_t memid, uint32_t lvl){
    Load(memid, lvl, 1);
}

void CacheStats::Load(uint32_t memid, uint32_t lvl, uint32_t cnt){
    Stats[memid][lvl].loadCount += cnt;
}

void CacheStats::HybridLoad(uint32_t memid){
    HybridLoad(memid, 1);
}

void CacheStats::HybridLoad(uint32_t memid, uint32_t cnt){
    HybridMemStats[memid].loadCount += cnt;
}


void CacheStats::Store(uint32_t memid, uint32_t lvl){
    Store(memid, lvl, 1);
}

void CacheStats::Store(uint32_t memid, uint32_t lvl, uint32_t cnt){
    Stats[memid][lvl].storeCount += cnt;
}

void CacheStats::HybridStore(uint32_t memid){
    HybridStore(memid, 1);
}

void CacheStats::HybridStore(uint32_t memid, uint32_t cnt){
    HybridMemStats[memid].storeCount += cnt;
}


uint64_t CacheStats::GetLoads(uint32_t memid, uint32_t lvl){
    return Stats[memid][lvl].loadCount;
}

uint64_t CacheStats::GetLoads(uint32_t lvl){
    uint64_t loads = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        loads += Stats[i][lvl].loadCount;
    }
    return loads;
}

uint64_t CacheStats::GetHybridLoads(uint32_t memid){
    return HybridMemStats[memid].loadCount;
}


uint64_t CacheStats::GetHybridLoads(){
    uint64_t loads = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        loads += HybridMemStats[i].loadCount;
    }
    return loads;
}


uint64_t CacheStats::GetStores(uint32_t memid, uint32_t lvl){
    return Stats[memid][lvl].storeCount;
}

uint64_t CacheStats::GetStores(uint32_t lvl){
    uint64_t stores = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        stores += Stats[i][lvl].storeCount;
    }
    return stores;
}

uint64_t CacheStats::GetHybridStores(uint32_t memid){
    return HybridMemStats[memid].storeCount;
}

uint64_t CacheStats::GetHybridStores(){
    uint64_t stores = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        stores += HybridMemStats[i].storeCount;
    }
    return stores;
}

void CacheStats::Hit(uint32_t memid, uint32_t lvl){
    Hit(memid, lvl, 1);
}

void CacheStats::Miss(uint32_t memid, uint32_t lvl){
    Miss(memid, lvl, 1);
}

void CacheStats::Miss(uint32_t memid, uint32_t lvl, uint32_t cnt){
    Stats[memid][lvl].missCount += cnt;
}

void CacheStats::HybridHit(uint32_t memid){
    HybridHit(memid, 1);
}

void CacheStats::HybridHit(uint32_t memid, uint32_t cnt){
    HybridMemStats[memid].hitCount += cnt;
}

void CacheStats::HybridMiss(uint32_t memid){
    HybridMiss(memid, 1);
}

void CacheStats::HybridMiss(uint32_t memid, uint32_t cnt){
    HybridMemStats[memid].missCount += cnt;
}

void CacheStats::Hit(uint32_t memid, uint32_t lvl, uint32_t cnt){
    Stats[memid][lvl].hitCount += cnt;
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


uint64_t CacheStats::GetHybridHits(uint32_t memid){
    return HybridMemStats[memid].hitCount;
}

uint64_t CacheStats::GetHybridHits(){
    uint64_t hits = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        hits += HybridMemStats[i].hitCount;
    }
    return hits;
}

uint64_t CacheStats::GetHybridMisses(uint32_t memid){
    return HybridMemStats[memid].missCount;
}

uint64_t CacheStats::GetHybridMisses(){
    uint64_t misses = 0;
    for (uint32_t i = 0; i < Capacity; i++){
        misses += HybridMemStats[i].missCount;
    }
    return misses;
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

bool CacheStats::Verify(){
    for(uint32_t memid = 0; memid < Capacity; ++memid){

        uint64_t prevMisses = Stats[memid][0].missCount;

        for(uint32_t level = 1; level < LevelCount; ++level){
            uint64_t hits = Stats[memid][level].hitCount;
            uint64_t misses = Stats[memid][level].missCount;
            if(hits + misses != prevMisses){
                warn << "Inconsistent hits/misses for memid " << memid << " level " << level << " " << hits << " + " << misses << " != " << prevMisses << ENDL;
                return false;
            }
            prevMisses = misses;
        }
    }
    return true;
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
    string s (e);
    if (!ParseInt32(s, var, 0)){
        return false;
        inform << "unable to parse " << name << " in environment" << ENDL;
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
    return false;
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
    toEvict=false;
    toEvictAddresses=new vector<uint64_t>;

    countsets = size / (linesize * associativity);

    linesizeBits = 0;
    while (lineSz > 0){
        linesizeBits++;
        lineSz = (lineSz >> 1);
    }
    linesizeBits--;
    contents = new uint64_t*[countsets];
    dirtystatus= new bool*[countsets];
    for (uint32_t i = 0; i < countsets; i++){
        contents[i] = new uint64_t[associativity];
        memset(contents[i], 0, sizeof(uint64_t) * associativity);

        dirtystatus[i]=new bool[associativity];
        memset(dirtystatus[i],0, sizeof(bool) * associativity );   // initialized to false i.e. cacheline is not dirty.
    }

    recentlyUsed = NULL;
    historyUsed = NULL;
    if (replpolicy == ReplacementPolicy_nmru){
        recentlyUsed = new uint32_t[countsets];
        memset(recentlyUsed, 0, sizeof(uint32_t) * countsets);
    }
    else if (replpolicy == ReplacementPolicy_trulru){
        recentlyUsed = new uint32_t[countsets];
        memset(recentlyUsed, 0, sizeof(uint32_t) * countsets);
        historyUsed = new history*[countsets];
        for(int s = 0; s < countsets; ++s) {
            historyUsed[s] = new history[assoc];
            historyUsed[s][0].prev = assoc-1;
            historyUsed[s][0].next = 1;
            for(int a = 1; a < assoc; ++a) {
                historyUsed[s][a].prev = a-1;
                historyUsed[s][a].next = (a+1)%assoc;
            }
        }
    }
}

void HighlyAssociativeCacheLevel::Init(CacheLevel_Init_Interface){
    assert(associativity >= MinimumHighAssociativity);
    fastcontents = new pebil_map_type<uint64_t, uint32_t>*[countsets];
    fastcontentsdirty = new pebil_map_type<uint64_t, bool>*[countsets];
    for (uint32_t i = 0; i < countsets; i++){
        fastcontents[i] = new pebil_map_type<uint64_t, uint32_t>();
        fastcontents[i]->clear();

        fastcontentsdirty[i] = new pebil_map_type<uint64_t, bool>();
        fastcontentsdirty[i]->clear(); // initialized to false i.e. cacheline is not dirty.
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
    if (fastcontentsdirty){
        for (uint32_t i = 0; i < countsets; i++){
            if (fastcontentsdirty[i]){
                delete fastcontentsdirty[i];
            }
        }
        delete[] fastcontentsdirty;
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
    if (historyUsed){
        for(int s = 0; s < countsets; ++s)
            delete[] historyUsed[s];
        delete[] historyUsed;
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
    //if(size == 32768) return store % 64;
    //else if(size == 262144) return store % 512; // FIXME
    return (store % countsets);
}

uint32_t CacheLevel::LineToReplace(uint32_t setid){
    if (replpolicy == ReplacementPolicy_nmru){
        return (recentlyUsed[setid] + 1) % associativity;
    } else if (replpolicy == ReplacementPolicy_trulru){
        return recentlyUsed[setid];
    } else if (replpolicy == ReplacementPolicy_random){
        return RandomInt(associativity);
    } else if (replpolicy == ReplacementPolicy_direct){
        return 0;
    } else {
        assert(0);
    }
    return 0;
}

bool HighlyAssociativeCacheLevel::GetDirtyStatus(uint32_t setid,uint32_t lineid,uint64_t store){
    {
        if( fastcontentsdirty[setid]->count(store) > 0 )
            return (*(fastcontentsdirty[setid]))[store]; //.second; 
        else
            return false; // Since it is a miss, it cannot be dirty!
    }        
}

void HighlyAssociativeCacheLevel::SetDirty(uint32_t setid,uint32_t lineid,uint64_t store){
    (*(fastcontentsdirty[setid]))[lineid]=true;
}

void HighlyAssociativeCacheLevel::ResetDirty(uint32_t setid,uint32_t lineid,uint64_t store){
    (*(fastcontentsdirty[setid]))[lineid]=false;
}

uint64_t CacheLevel::Replace(uint64_t store, uint32_t setid, uint32_t lineid,uint64_t loadstoreflag){
    uint64_t prev = contents[setid][lineid];
 
    if(GetDirtyStatus(setid,lineid,prev)){
        toEvict=true;
        toEvictAddresses->push_back(prev);
     }
    // Since the new address 'store' has been loaded just now and is not touched yet, we can reset the dirty flag if it is indeed dirty!
    contents[setid][lineid] = store;
    if(LoadStoreLogging){
        if(loadstoreflag)
            ResetDirty(setid,lineid,store);
        else
            SetDirty(setid,lineid,store);
    }
    
    MarkUsed(setid, lineid,loadstoreflag);  
    return prev;
}

uint64_t HighlyAssociativeCacheLevel::Replace(uint64_t store, uint32_t setid, uint32_t lineid,uint64_t loadstoreflag){
    uint64_t prev = contents[setid][lineid];
    contents[setid][lineid] = store;

    pebil_map_type<uint64_t, uint32_t>* fastset = fastcontents[setid];
    if (fastset->count(prev) > 0){
        //assert((*fastset)[prev] == lineid);
        fastset->erase(prev);
    }
    (*fastset)[store] = lineid; //(*fastset)[store].first = lineid;
 
     if(GetDirtyStatus(setid,lineid,store)){
        toEvict=true;
        toEvictAddresses->push_back(prev);
    }
   
    if(LoadStoreLogging){
        if(loadstoreflag)
            ResetDirty(setid,lineid,store);
        else
            SetDirty(setid,lineid,store);
    }

    MarkUsed(setid, lineid,loadstoreflag);
    return prev;
}
inline void CacheLevel::MarkUsed(uint32_t setid, uint32_t lineid,uint64_t loadstoreflag){
    if(LoadStoreLogging){
        if(!(loadstoreflag))
            SetDirty(setid,lineid,contents[setid][lineid]);
    }
    if (USES_MARKERS(replpolicy)){
        debug(inform << "level " << dec << level << " USING set " << dec << setid << " line " << lineid << ENDL << flush);
        recentlyUsed[setid] = lineid;
    }
    else if(replpolicy == ReplacementPolicy_trulru) {
        debug(inform << "level " << dec << level << " USING set " << dec << setid << " line " << lineid << ENDL << flush);
        if(recentlyUsed[setid] == lineid)
            recentlyUsed[setid] = historyUsed[setid][lineid].next;
        else {
            historyUsed[setid][historyUsed[setid][lineid].next].prev = historyUsed[setid][lineid].prev;
            historyUsed[setid][historyUsed[setid][lineid].prev].next = historyUsed[setid][lineid].next;
            historyUsed[setid][lineid].prev = historyUsed[setid][recentlyUsed[setid]].prev;
            historyUsed[setid][recentlyUsed[setid]].prev = lineid;
            historyUsed[setid][lineid].next = recentlyUsed[setid];
            historyUsed[setid][historyUsed[setid][lineid].prev].next = lineid;
        }
    }
}

bool HighlyAssociativeCacheLevel::Search(uint64_t store, uint32_t* set, uint32_t* lineInSet){
    uint32_t setId = GetSet(store);
    debug(inform << TAB << TAB << "stored " << hex << store << " set " << dec << setId << endl << flush);
    if (set){
        (*set) = setId;
    }

    pebil_map_type<uint64_t, uint32_t>* fastset = fastcontents[setId];
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

void CacheLevel::SetDirty(uint32_t setid, uint32_t lineid,uint64_t store){
    dirtystatus[setid][lineid]=true;
}

void CacheLevel::ResetDirty(uint32_t setid, uint32_t lineid,uint64_t store){
    dirtystatus[setid][lineid]=false;
}

bool CacheLevel::GetDirtyStatus(uint32_t setid, uint32_t lineid,uint64_t store){
    return dirtystatus[setid][lineid];
}

uint32_t CacheLevel::Process(CacheStats* stats, uint32_t memid, uint64_t addr,uint64_t loadstoreflag,bool* anyEvict,void* info){ // FuncChanged.
    uint32_t set = 0, lineInSet = 0;
    uint64_t store = GetStorage(addr);

    debug(assert(stats));
    debug(assert(stats->Stats));
    debug(assert(stats->Stats[memid]));


    if(LoadStoreLogging){
        if(loadstoreflag){
                stats->Stats[memid][level].loadCount++;
        }else{
                stats->Stats[memid][level].storeCount++;
        }         
    }    

    // hit
    if (Search(store, &set, &lineInSet)){
        stats->Stats[memid][level].hitCount++;    
        MarkUsed(set, lineInSet,loadstoreflag);
        return INVALID_CACHE_LEVEL;
    }

    // miss
    stats->Stats[memid][level].missCount++;
    Replace(store, set, LineToReplace(set),loadstoreflag);
    (*anyEvict)|=toEvict;

    return level + 1;
}

uint32_t CacheLevel::EvictProcess(CacheStats* stats, uint32_t memid, uint64_t addr, uint64_t loadstoreflag,void* info){
    uint32_t set = 0, lineInSet = 0;
    uint64_t store = addr;
    debug(assert(stats));
    debug(assert(stats->Stats));
    debug(assert(stats->Stats[memid]));

    if (Search(store, &set, &lineInSet)){
        stats->Stats[memid][level].storeCount++;
        MarkUsed(set, lineInSet,loadstoreflag);
        return INVALID_CACHE_LEVEL;
    }
        
    return level + 1;
}

uint32_t ExclusiveCacheLevel::Process(CacheStats* stats, uint32_t memid, uint64_t addr,uint64_t loadstoreflag,bool* anyEvict, void* info){
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

        loadstoreflag = ( 1 & *(anyEvict) );
        *(anyEvict) = GetDirtyStatus(set,lineInSet,e->addr);
        e->addr = Replace(e->addr,set,lineInSet,loadstoreflag);
        toEvict = false;

        if (level == e->level){
            *(anyEvict) = false;
            if(  *(anyEvict)  && ( (level+1) == levelCount )  ) {
                toEvict=true;
                toEvictAddresses->push_back(e->addr);
            }
            return INVALID_CACHE_LEVEL;
        } else {
            return level + 1;
        }
    }

    if(LoadStoreLogging){
        if(loadstoreflag){
            stats->Stats[memid][level].loadCount++;
        }else{
            stats->Stats[memid][level].storeCount++;
        }            
    }

    // hit
    if (Search(store, &set, &lineInSet)){
        stats->Stats[memid][level].hitCount++;

        e->level = level;
        e->addr = store;
        e->setid = set;
        e->lineid = lineInSet;

        toEvict = false;
        if (level == FirstExclusive){
            MarkUsed(set, lineInSet,loadstoreflag);
            return INVALID_CACHE_LEVEL;
        }
        MarkUsed(set, lineInSet,GetDirtyStatus(set,lineInSet,store));
        *(anyEvict) = ( 1 & loadstoreflag);
        return FirstExclusive;
    }

    // miss
    stats->Stats[memid][level].missCount++;
    toEvict = false;
    if (level == LastExclusive){
        e->level = LastExclusive + 1;
        e->addr = store;

        *(anyEvict) = ( 1 & loadstoreflag);
        return FirstExclusive;
    }
    *(anyEvict) = false;
    return level + 1;
}

void CacheLevel::EvictDirty(CacheStats* stats,CacheLevel** levels,uint32_t memid,void* info){
    // Should be only called by InclusiveCache.
    uint64_t victim;
    victim=toEvictAddresses->back();    // vector "toEvictAddresses" will be empty by the end of this. Vector was designed to handle 
    toEvictAddresses->pop_back();
    uint32_t next=level+1;
    uint64_t loadstoreflag=0;
   
    /* next=levels[next]->EvictProcess(stats,memid,victim,loadstoreflag,(void*)info);   
    assert( next == INVALID_CACHE_LEVEL); // If assert fails, implies the memory address which was to be evicted was not found which is violation in an inclusive cache.
    assert(toEvictAddresses->size()==0); // INFO:Should be removed before merging to dev branch, altho this checks a legal case but is it needed?\ */

    while(toEvictAddresses->size()){ // To handle cases where an address from Ln is missing in Ln+1 (e.g  missing in L2, found in L1). 
        victim=toEvictAddresses->back();
        toEvictAddresses->pop_back();
        next=level+1;
        loadstoreflag=0;
        if(next<levelCount) next=levels[next]->EvictProcess(stats,memid,victim,loadstoreflag,(void*)info);   
        if(next<levelCount){
            inform<<"\t Cannot retire victim "<<victim<<" to level "<<(level+1)<<" since  it has already been evicted "<<ENDL;
        }
    }

    toEvict=false;
    return;
}

bool CacheLevel::GetEvictStatus(){
    return toEvict;
}

MemoryStreamHandler::MemoryStreamHandler(){
    pthread_mutex_init(&mlock, NULL);
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

void CacheStructureHandler::ExtractAddresses(){
    stringstream tokenizer(description);
    int whichTok=0;
    string token;
    vector<uint64_t> Start;
    vector<uint64_t> End;
    int NumLevelsToken=1;
    int HybridAddressCount=0;

    for ( ; tokenizer >> token; whichTok++){
        if (token.compare(0, 1, "#") == 0){
            break;
        }
        uint64_t Dummy;
        if(whichTok>=(levelCount * 4+ (NumLevelsToken+1) )){
            istringstream stream(token);
            stream>>Dummy;
            if(Dummy<0x1){
                ErrorExit("\n\t The boundary address of Cache structure: "<<sysId<<" token "<<token<<" is "<<Dummy<<" is not positive!! \n",MetasimError_StringParse);
            }else{
                if(HybridAddressCount%2==0){
                    Start.push_back(Dummy);
                }else{
                    End.push_back(Dummy);
                }                   
                HybridAddressCount+=1;  
            }
        }
    }
    
    if( (HybridAddressCount%2!=0) || (HybridAddressCount==0)){
        warn<<"\n\t NEED TO FEED THIS TO DEBUG/WARNING STREAMS ASAP.";
    }
    
    AddressRangesCount=Start.size() ;// Should be equal to End.size()
    RamAddressStart=(uint64_t*)malloc(AddressRangesCount*sizeof(uint64_t));
    RamAddressEnd=(uint64_t*)malloc(AddressRangesCount*sizeof(uint64_t));

    for(int AddCopy=0; AddCopy < AddressRangesCount ; AddCopy++){
        if(Start[AddCopy]<=End[AddCopy]){
            RamAddressStart[AddCopy]=Start[AddCopy];
            RamAddressEnd[AddCopy]=End[AddCopy];
        }
        else{
            ErrorExit("\n\t Address range with start: "<<Start[AddCopy]<<" end: "<<End[AddCopy]<<" is illegal, starting address is smaller than ending address ",MetasimError_StringParse);
        }
    }

}

CacheStructureHandler::CacheStructureHandler(CacheStructureHandler& h){
    sysId = h.sysId;
    levelCount = h.levelCount;
    description.assign(h.description);
    hybridCache=h.hybridCache;
    hits=0;
    misses=0;
#define LVLF(__i, __feature) (h.levels[__i])->Get ## __feature
#define Extract_Level_Args(__i) LVLF(__i, Level()), LVLF(__i, SizeInBytes()), LVLF(__i, Associativity()), LVLF(__i, LineSize()), LVLF(__i, ReplacementPolicy())
    levels = new CacheLevel*[levelCount];
    for (uint32_t i = 0; i < levelCount; i++){
        if (LVLF(i, Type()) == CacheLevelType_InclusiveLowassoc){
            InclusiveCacheLevel* l = new InclusiveCacheLevel();
            l->Init(Extract_Level_Args(i));
            levels[i] = l;
            l->SetLevelCount(levelCount);
        } else if (LVLF(i, Type()) == CacheLevelType_InclusiveHighassoc){
            HighlyAssociativeInclusiveCacheLevel* l = new HighlyAssociativeInclusiveCacheLevel();
            l->Init(Extract_Level_Args(i));
            levels[i] = l;
            l->SetLevelCount(levelCount);
        } else if (LVLF(i, Type()) == CacheLevelType_ExclusiveLowassoc){
            ExclusiveCacheLevel* l = new ExclusiveCacheLevel();
            ExclusiveCacheLevel* p = dynamic_cast<ExclusiveCacheLevel*>(h.levels[i]);
            assert(p->GetType() == CacheLevelType_ExclusiveLowassoc);
            l->Init(Extract_Level_Args(i), p->FirstExclusive, p->LastExclusive);
            inform<<"\t p->LastExclusive "<<p->LastExclusive<<ENDL;
            levels[i] = l;
            l->SetLevelCount(levelCount);
        } else if (LVLF(i, Type()) == CacheLevelType_ExclusiveHighassoc){
            HighlyAssociativeExclusiveCacheLevel* l = new HighlyAssociativeExclusiveCacheLevel();
            ExclusiveCacheLevel* p = dynamic_cast<ExclusiveCacheLevel*>(h.levels[i]);
            assert(p->GetType() == CacheLevelType_ExclusiveHighassoc);
            l->Init(Extract_Level_Args(i), p->FirstExclusive, p->LastExclusive);
            levels[i] = l;
            l->SetLevelCount(levelCount);
        } else {
            assert(false);
        }
    }
}

bool CacheStructureHandler::CheckRange(CacheStats* stats,uint64_t addr,uint64_t loadstoreflag, uint32_t memid){
    bool AddressNotFound= true; 
    for(int CurrRange=0 ; (CurrRange < AddressRangesCount) && AddressNotFound ; CurrRange++){
        if( ( addr > RamAddressStart[CurrRange] ) && (addr <= RamAddressEnd[CurrRange] ) ){
            AddressNotFound = false;
            stats->HybridMemStats[memid].hitCount++; 
            if(loadstoreflag)
                stats->HybridMemStats[memid].loadCount++;
            else
                stats->HybridMemStats[memid].storeCount++;
        }
    }

    if(AddressNotFound){
        stats->HybridMemStats[memid].missCount++;     
    }
    return true; // CAUTION: No known use of returning 'bool'!! 
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
    hybridCache=-1;

    uint32_t whichTok = 0;
    uint32_t firstExcl = INVALID_CACHE_LEVEL;

    for ( ; (tokenizer >> token) && (whichTok < levelCount * 4+ 2) ; whichTok++){

        // comment reached on line
        if (token.compare(0, 1, "#") == 0){
            break;
        }

        // 2 special tokens appear first
        if (whichTok == 0){
            if( token.size() > 6){
                if( token.compare(token.size()-6,6,"hybrid")==0) {
                    token.resize(token.size()-6);
                    hybridCache=1;
                }else{
                    hybridCache=0;
                }
            }else{
                hybridCache=0;
            }

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
        //inform<<"\t idx "<<idx<<"\t whichTok "<<whichTok<<"\t cacheValues[idx] "<<cacheValues[idx]<<ENDL;
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
                repl = ReplacementPolicy_trulru;
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
                    inform<<"\t Boo-yeah level is HIGHLY exclusive "<<ENDL;
                    l->Init(levelId, sizeInBytes, assoc, lineSize, repl, firstExcl, levelCount - 1);
                    levels[levelId] = (CacheLevel*)l;
                } else {
                    HighlyAssociativeInclusiveCacheLevel* l = new HighlyAssociativeInclusiveCacheLevel();
                    l->Init(levelId, sizeInBytes, assoc, lineSize, repl);
                    levels[levelId] = (CacheLevel*)l;
                }
            } else {
                if (firstExcl != INVALID_CACHE_LEVEL){
                    inform<<"\t Boo-yeah level is exclusive "<<ENDL;
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

uint32_t CacheStructureHandler::Process(void* stats_in, BufferEntry* access){
    uint32_t next = 0,tmpNext = 0;
    uint64_t victim = access->address;

    CacheStats* stats = (CacheStats*)stats_in;

    EvictionInfo evictInfo;
    evictInfo.level = INVALID_CACHE_LEVEL;
    uint64_t loadstoreflag= access->loadstoreflag;
    bool anyEvict = false;
    uint32_t resLevel = 0;

    while (next < levelCount){
        resLevel = next;
        next = levels[next]->Process(stats, access->memseq, victim, loadstoreflag,&anyEvict,(void*)(&evictInfo));
        if(next!=0) loadstoreflag=1; // If next level is checked, then it should be a miss from current level, which implies next operation is a load to a next level!!
    }

    if(DirtyCacheHandling&&anyEvict){
        while( (tmpNext<levelCount) ){
            if(levels[tmpNext]->GetEvictStatus()){
                levels[tmpNext]->EvictDirty(stats, levels,access->memseq,(void*)(&evictInfo));
            }
            tmpNext++;
        }
    } 
      
    if( (hybridCache) && (next!=INVALID_CACHE_LEVEL) && (next>=levelCount) ){ // Implies miss at LLC 
        CheckRange(stats,victim,access->loadstoreflag,access->memseq); 
        uint32_t lastLevel = levelCount-1;
        if(levels[lastLevel]->GetEvictStatus()){
            levels[lastLevel]->EvictDirty(stats,levels,access->memseq,(void*)(&evictInfo));
            vector<uint64_t>* toEvictAddresses = levels[lastLevel]->passEvictAddresses();

            while(toEvictAddresses->size()){ // To handle cases where an address from Ln is missing in Ln+1 (e.g  missing in L2, found in L1). 
                victim=toEvictAddresses->back();
                toEvictAddresses->pop_back();
                loadstoreflag=0; // Since its dirty and written back.
                CheckRange(stats,victim,loadstoreflag,access->memseq); 
            }
        }
        resLevel = levelCount+1;
    } 
    return resLevel;
}

// called for every new image and thread
SimulationStats* GenerateCacheStats(SimulationStats* stats, uint32_t typ, image_key_t iid, thread_key_t tid, image_key_t firstimage){
 
    assert(stats);
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
    assert(stats);
    stats->threadid = tid;
    stats->imageid = iid;
    // every thread and image gets its own statistics
    if(stats->InstructionCount > stats->BlockCount)
    stats->AllocCount = stats->InstructionCount;
    else
        stats->AllocCount = stats->BlockCount;

    if(EitherAddressRangeOrSimulation){
        stats->Stats = new StreamStats*[CountMemoryHandlers];
        bzero(stats->Stats, sizeof(StreamStats*) * CountMemoryHandlers);    
        
        if(CacheSimulation)
        for (uint32_t i = 0; i < CountCacheStructures; i++){
            CacheStructureHandler* c = (CacheStructureHandler*)MemoryHandlers[i];
            stats->Stats[i] = new CacheStats(c->levelCount, c->sysId, stats->AllocCount,c->hybridCache); //stats->Stats[i] = new CacheStats(c->levelCount, c->sysId, stats->InstructionCount);
       }

        if(EitherAddressRangeOrSimulation){           
            stats->Stats[RangeHandlerIndex] = new RangeStats(s->AllocCount); // stats->Stats[RangeHandlerIndex] = new RangeStats(s->InstructionCount);
        }
    }
    if (typ == AllData->ThreadType || (iid == firstimage)){
        stats->Handlers = new MemoryStreamHandler*[CountMemoryHandlers];   
    
        if(CacheSimulation){
            // all images within a thread share a set of memory handlers, but they don't exist for any image
            for (uint32_t i = 0; i < CountCacheStructures; i++){
                CacheStructureHandler* p = (CacheStructureHandler*)MemoryHandlers[i];
                CacheStructureHandler* c = new CacheStructureHandler(*p);
                if(p->hybridCache) c->ExtractAddresses();    
                stats->Handlers[i] = c;
            }
        }
        if(EitherAddressRangeOrSimulation){
        // all images within a thread share a set of memory handlers, but they don't exist for any image
        AddressRangeHandler* p = (AddressRangeHandler*)MemoryHandlers[RangeHandlerIndex];
        AddressRangeHandler* r = new AddressRangeHandler(*p);
        stats->Handlers[RangeHandlerIndex] = r;
        
        }
        if (ReuseWindow || SpatialWindow){
            stats->RHandlers = new ReuseDistance*[CountReuseHandlers]; // We have a set of reuse handlers (reuse, spatial) per thread
                if(ReuseWindow)
                stats->RHandlers[ReuseHandlerIndex] = new ReuseDistance(ReuseWindow, ReuseBin);
                if(SpatialWindow)
                    stats->RHandlers[SpatialHandlerIndex] = new SpatialLocality(SpatialWindow, SpatialBin, SpatialNMAX);
        } 
    }
    else{
        SimulationStats * fs = AllData->GetData(firstimage, tid);
        stats->Handlers = fs->Handlers;
    }

    // each thread gets its own buffer
    if (typ == AllData->ThreadType){
        stats->Buffer = new BufferEntry[BUFFER_CAPACITY(stats) + 1];
        bzero(BUFFER_ENTRY(stats, 0), (BUFFER_CAPACITY(stats) + 1) * sizeof(BufferEntry));
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
    if(ReuseWindow) {
        if (!ReadEnvUint32("METASIM_REUSE_BIN", &ReuseBin)){
            ReuseBin = 1;
        }
    }
    
    if (!ReadEnvUint32("METASIM_SPATIAL_WINDOW", &SpatialWindow)){
        SpatialWindow = 0;
    }
    if(SpatialWindow) {
    if (!ReadEnvUint32("METASIM_SPATIAL_BIN", &SpatialBin)){
            SpatialBin = 1;
        }

        if (!ReadEnvUint32("METASIM_SPATIAL_NMAX", &SpatialNMAX)){
            SpatialNMAX = ReuseDistance::Infinity;
        }
    }
    if (!ReadEnvUint32("METASIM_CACHE_SIMULATION", &CacheSimulation)){
       CacheSimulation = 0;
    }
    if (!ReadEnvUint32("METASIM_ADDRESS_RANGE", &AddressRangeEnable)){
       AddressRangeEnable = 0;
    } 
    if(!ReadEnvUint32("METASIM_LOAD_LOG",&LoadStoreLogging)){
        LoadStoreLogging = 0;
    }
    if(!ReadEnvUint32("METASIM_DIRTY_CACHE",&DirtyCacheHandling)){
        DirtyCacheHandling = 0;
    }
    if(!ReadEnvUint32("METASIM_ADAMANT_LOG",&AdamantLogging)){
        AdamantLogging = 0;
    }    

    if(DirtyCacheHandling){
        if(!LoadStoreLogging){
            ErrorExit(" DirtyCacheHandling is enabled without LoadStoreLogging ",MetasimError_FileOp);
        }
    }

    EitherAddressRangeOrSimulation= (CacheSimulation || AddressRangeEnable); 
    inform<<" Cache Simulation "<<CacheSimulation<<" AddressRangeEnable "<<AddressRangeEnable<<" EitherAddressRangeOrSimulation "<<EitherAddressRangeOrSimulation<<endl;
    inform<<" LoadStoreLogging "<<LoadStoreLogging<<" DirtyCacheHandling "<<DirtyCacheHandling<<" AdamantLogging "<<AdamantLogging<<ENDL;

    // read caches to simulate
    string cachedf = GetCacheDescriptionFile();
    const char* cs = cachedf.c_str();
    ifstream CacheFile(cs);
    if (CacheFile.fail()){
        ErrorExit("cannot open cache descriptions file: " << cachedf, MetasimError_FileOp);
    }
    
    if(CacheSimulation){
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
          assert(CountCacheStructures > 0 && "No cache structures found for simulation");
        MemoryHandlers = new MemoryStreamHandler*[CountMemoryHandlers];
        for (uint32_t i = 0; i < CountCacheStructures; i++){
            MemoryHandlers[i] = caches[i];
        }
     }
    
    if(EitherAddressRangeOrSimulation){
        RangeHandlerIndex = CountMemoryHandlers;
        CountMemoryHandlers++;

            MemoryStreamHandler** tmp = new MemoryStreamHandler*[CountMemoryHandlers];
            for(uint32_t i = 0; i < RangeHandlerIndex; ++i) {
                tmp[i] = MemoryHandlers[i];
            }
            if(MemoryHandlers != NULL) {
                delete[] MemoryHandlers;
            }
            MemoryHandlers = tmp;
            MemoryHandlers[RangeHandlerIndex] = new AddressRangeHandler();   
    }

    if(ReuseWindow){
    ReuseHandlerIndex=0;
    CountReuseHandlers++;
    }
    else
    ReuseHandlerIndex=-1;
    if(SpatialWindow) {
    SpatialHandlerIndex=ReuseHandlerIndex+1;
    CountReuseHandlers++;
    }
    else
        SpatialHandlerIndex=-1;    
    
    if (ReuseWindow || SpatialWindow){
        ReuseDistanceHandlers = new ReuseDistance*[CountReuseHandlers];
    if(ReuseWindow)
        ReuseDistanceHandlers[ReuseHandlerIndex] = new ReuseDistance(ReuseWindow, ReuseBin);      
    if(SpatialWindow)
        ReuseDistanceHandlers[SpatialHandlerIndex] = new SpatialLocality(SpatialWindow, SpatialBin, SpatialNMAX);
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

