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

#include <DFPattern.h>
#include <InstrumentationCommon.hpp>
#include <Simulation.hpp>

static uint32_t MemCapacity = 0;
static uint32_t CountCacheStructures = 0;

static CacheStructure** CacheStructures_ = NULL;
static SamplingMethod* SamplingMethod_ = NULL;
static DataManager<SimulationStats*>* AllData = NULL;


#define DYNAMIC_POINT_SIZE_LIMIT 128
typedef struct {
    uint64_t VirtualAddress;
    uint64_t ProgramAddress;
    uint64_t Key;
    uint64_t Flags;
    uint32_t Size;
    uint8_t  OppContent[DYNAMIC_POINT_SIZE_LIMIT];
    bool IsEnabled;
} DynamicInst;
static uint64_t CountDynamicInst = 0;
static DynamicInst* DynamicInst_ = NULL;
static set<uint64_t>* NonmaxKeys = NULL;
static bool FillPointsDead = false;

#define GENERATE_KEY(__bid, __typ) ((__typ & 0xf) | (__bid << 4))
#define GET_BLOCKID(__key) ((__key >> 4))
#define GET_TYPE(__key) ((__key & 0xf))

static void PrintDynamicPoint(DynamicInst* d){
    inform
        << TAB
        << TAB << "Key 0x" << hex << d->Key
        << TAB << "Vaddr 0x" << hex << d->VirtualAddress
        << TAB << "Oaddr 0x" << hex << d->ProgramAddress
        << TAB << "Size " << dec << d->Size
        << TAB << "Enabled " << (d->IsEnabled? "yes":"no")
        << ENDL;
}

static void PrintDynamicPoints(){
    inform << "Printing " << dec << CountDynamicInst << " dynamic inst points" << ENDL;
    for (uint32_t i = 0; i < CountDynamicInst; i++){
        PrintDynamicPoint(&DynamicInst_[i]);
    }
}

static void SetDynamicPointStatus(DynamicInst* d, bool state){

    uint8_t t[DYNAMIC_POINT_SIZE_LIMIT];
    memcpy(t, (uint8_t*)d->VirtualAddress, d->Size);
    memcpy((uint8_t*)d->VirtualAddress, d->OppContent, d->Size);
    memcpy(d->OppContent, t, d->Size);

    d->IsEnabled = state;

    (inform << "Removing instrumentation point..." << ENDL);
    (PrintDynamicPoint(d));
}

static void SetDynamicPoints(set<uint64_t>* keys, bool state){
    debug(inform << "CHECKING " << dec << CountDynamicInst << ENDL);
    for (uint32_t i = 0; i < CountDynamicInst; i++){

        debug(inform << TAB TAB << "KEY COUNT " << dec << DynamicInst_[i].Key << " = " << keys->count(DynamicInst_[i].Key) << ENDL);
        if (keys->count(DynamicInst_[i].Key) > 0){

            (inform << "Removing instrumentation point... " << hex << DynamicInst_[i].Key << ENDL);

            if (state != DynamicInst_[i].IsEnabled){
                SetDynamicPointStatus(&DynamicInst_[i], state);
            }
        }
    }
    PrintDynamicPoints();
}

extern "C" {
    void* tool_dynamic_init(uint64_t* count, DynamicInst** dyn){
        inform << "raw dynamic init " << hex << count << TAB << dyn << TAB << *dyn << ENDL;
        CountDynamicInst = *count;
        DynamicInst_ = *dyn;

        NonmaxKeys = new set<uint64_t>();

        for (uint32_t i = 0; i < CountDynamicInst; i++){
            uint64_t k = DynamicInst_[i].Key;
            if (GET_TYPE(k) == PointType_bufferfill){
                if (NonmaxKeys->count(k) == 0){
                    NonmaxKeys->insert(k);
                }
            }

            if (DynamicInst_[i].IsEnabled == false){
                SetDynamicPointStatus(&DynamicInst_[i], false);
            }
        }
        
        PrintDynamicPoints();
    }

    void* tool_mpi_init(){
        return NULL;
    }

    void* tool_thread_init(pthread_t tid){
        if (AllData){
            AllData->AddThread(tid);
        } else {
            PRINT_INSTR(stderr, "Calling PEBIL thread initialization library for thread %lx but no images have been initialized.", tid);
        }
        return NULL;
    }

    void* tool_image_init(void* s, uint64_t* key, ThreadData* td){
        SimulationStats* stats = (SimulationStats*)s;

        PRINT_INSTR(stdout, "raw args %#lx, %#lx, %#lx", (uint64_t)stats, (uint64_t)key, (uint64_t)td);

        ReadKnobs();

        assert(stats->Stats == NULL);
        stats->Stats = new CacheStats*[CountCacheStructures];
        for (uint32_t i = 0; i < CountCacheStructures; i++){
            stats->Stats[i] = new CacheStats(CacheStructures_[i]->levelCount, CacheStructures_[i]->sysId, stats->InstructionCount);
        }

        assert(stats->Initialized == true);
        if (AllData == NULL){
            AllData = new DataManager<SimulationStats*>(GenerateCacheStats, DeleteCacheStats, ReferenceCacheStats);
        }

        PRINT_INSTR(stdout, "Buffer features: capacity=%ld, current=%ld", BUFFER_CAPACITY(stats), BUFFER_CURRENT(stats));
        inform << dec << stats->InstructionCount << " memops" << ENDL;

        *key = AllData->AddImage(stats, td);
        stats->imageid = *key;
        stats->threadid = pthread_self();

        AllData->SetTimer(*key, 0);
        
        return NULL;
    }

    // TODO: thread safety here
    void* process_buffer(uint64_t* key){

        AllData->SetTimer(*key, 2);
        if (AllData == NULL){
            PRINT_INSTR(stderr, "data manager does not exist. no images were initialized");
            return NULL;
        }

        SimulationStats* stats = (SimulationStats*)AllData->GetData(*key, pthread_self());
        if (stats == NULL){
            PRINT_INSTR(stderr, "Cannot retreive image data using key %ld", *key);
            return NULL;
        }

        register uint64_t numElements = BUFFER_CURRENT(stats);
        uint64_t capacity = BUFFER_CAPACITY(stats);

        debug(PRINT_INSTR(stdout, "counter %ld\tcapacity %ld\ttotal %ld", numElements, capacity, SamplingMethod_->AccessCount));

        if (NonmaxKeys->empty()){
            BUFFER_CURRENT(stats) = 0;
            debug(inform << "1resetting buffer " << BUFFER_CURRENT(stats) << ENDL);
            return NULL;
        }

        bool DidSimulation = false;
        if (SamplingMethod_->CurrentlySampling()){
            DidSimulation = true;
            if (FillPointsDead == true){
                SetDynamicPoints(NonmaxKeys, true);
                FillPointsDead = false;
            }

            /*
            for (uint32_t i = 0; i < BUFFER_CURRENT(stats); i++){
                PRINT_INSTR(stdout, "\t\tbuf[%d] addr %#lx\tseq %ld", i, buf[i].address, buf[i].memseq);
            }
            */
            BufferEntry* buffer = &(stats->Buffer[1]);

            // loop over address buffer
            for (uint32_t i = 0; i < CountCacheStructures; i++){
                CacheStructure* c = CacheStructures_[i];
                for (uint32_t j = 0; j < numElements; j++){
                    BufferEntry* reference = &(stats->Buffer[j + 1]);
                    c->Process((void*)stats->Stats[i], reference);
                }
            }

            set<uint64_t> MemsRemoved;
            for (uint32_t j = 0; j < numElements; j++){
                BufferEntry* reference = &(stats->Buffer[j + 1]);
                debug(inform << "Memseq " << dec << reference->memseq << " has " << stats->Stats[0]->GetAccessCount(reference->memseq) << ENDL);

                // if max block count is reached, disable all buffer-related points related to this block
                if (SamplingMethod_->ExceedsAccessLimit(stats->Stats[0]->GetAccessCount(reference->memseq))){
                    uint32_t bbid = stats->BlockIds[reference->memseq];

                    uint64_t k1 = GENERATE_KEY(bbid, PointType_buffercheck);
                    if (MemsRemoved.count(k1) == 0){
                        MemsRemoved.insert(k1);
                    }
                    assert(MemsRemoved.count(k1) == 1);

                    uint64_t k2 = GENERATE_KEY(bbid, PointType_bufferinc);
                    if (MemsRemoved.count(k2) == 0){
                        MemsRemoved.insert(k2);
                    }
                    assert(MemsRemoved.count(k2) == 1);

                    uint64_t k3 = GENERATE_KEY(bbid, PointType_bufferfill);
                    if (MemsRemoved.count(k3) == 0){
                        MemsRemoved.insert(k3);
                    }
                    assert(MemsRemoved.count(k3) == 1);

                    if (NonmaxKeys->count(k2) > 0){
                        NonmaxKeys->erase(k2);
                        assert(NonmaxKeys->count(k2) == 0);
                    }
                }
            }

            if (MemsRemoved.size()){
                debug(inform << "REMOVING " << dec << MemsRemoved.size() << ENDL); 
               SetDynamicPoints(&MemsRemoved, false);
            }
        } else {

            if (FillPointsDead == false){
                SetDynamicPoints(NonmaxKeys, false);
                FillPointsDead = true;
            }
        }

        SamplingMethod_->IncrementAccessCount(numElements);

        BUFFER_CURRENT(stats) = 0;
        debug(inform << "2resetting buffer " << BUFFER_CURRENT(stats) << ENDL);

        AllData->SetTimer(*key, 3);

        if (DidSimulation){
            inform << "Simulated " << dec << numElements << " elements through " << CountCacheStructures << " caches in " << AllData->GetTimer(*key, 3) - AllData->GetTimer(*key, 2) << " seconds " << ENDL;
        }
        return NULL;
    }

    void* tool_image_fini(uint64_t* key){
        AllData->SetTimer(*key, 1);

#ifdef MPI_INIT_REQUIRED
        if (!isMpiValid()){
            PRINT_INSTR(stderr, "Process %d did not execute MPI_Init, will not print jbbinst files", getpid());
            return NULL;
        }
#endif

        if (AllData == NULL){
            PRINT_INSTR(stderr, "data manager does not exist. no images were initialized");
            return NULL;
        }
        SimulationStats* stats = (SimulationStats*)AllData->GetData(*key, pthread_self());
        if (stats == NULL){
            PRINT_INSTR(stderr, "Cannot retreive image data using key %ld", *key);
            return NULL;
        }        

        process_buffer(key);

        inform
            << "Instruction count: " << stats->InstructionCount << ENDL
            << "Block count: " << stats->BlockCount << ENDL
            << "Per Instruction: " << (stats->PerInstruction ? "yes":"no") << ENDL;

        (inform << "#seq"
              << TAB << "addr"
              << TAB << "hash"
              << TAB << "bbid"
              << TAB << "memid"
              << TAB << "lineno"
              << TAB << "func"
              << ENDL);

        for (uint32_t i = 0; i < stats->BlockCount; i++){
            (inform << dec << i 
                  << TAB << hex << stats->Addresses[i]
                  << TAB << hex << stats->Hashes[i]
                  << TAB << dec << stats->BlockIds[i]
                  << TAB << dec << stats->MemopIds[i]
                  << TAB << stats->Files[i]
                  << TAB << dec << stats->Lines[i]
                  << TAB << stats->Functions[i]
             << ENDL);
        }

        ofstream MemFile;
        const char* fileName = SimulationFileName(stats);
        TryOpen(MemFile, fileName);

        CacheStats* c = stats->Stats[0];
        assert(c);
        uint64_t sampledCount = 0;
        for (uint32_t i = 0; i < c->Capacity; i++){
            sampledCount += c->GetAccessCount(i);
        }

        MemFile
            << "# appname     = " << stats->Application << ENDL
            << "# extension   = " << stats->Extension << ENDL
            << "# rank        = " << dec << getTaskId() << ENDL
            << "# buffer      = " << BUFFER_CAPACITY(stats) << ENDL
            << "# total       = " << dec << SamplingMethod_->AccessCount << ENDL
            << "# sampled     = " << dec << sampledCount << ENDL
            << "# processed   = " << dec << sampledCount << ENDL
            << "# samplemax   = " << SamplingMethod_->AccessLimit << ENDL
            << "# sampleon    = " << SamplingMethod_->SampleOn << ENDL
            << "# sampleoff   = " << SamplingMethod_->SampleOff << ENDL
            << "# perinsn     = " << (stats->PerInstruction? "yes" : "no") << ENDL
            << "#" << ENDL;

        for (set<pthread_t>::iterator it = AllData->allthreads.begin(); it != AllData->allthreads.end(); it++){
            stats = AllData->GetData(*key, (*it));
            assert(stats);
            PrintSimulationStats(MemFile, stats, (*it));
        }

        delete NonmaxKeys;
    }

};

void PrintSimulationStats(ofstream& f, SimulationStats* stats, pthread_t tid){
    for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
        CacheStats* c = stats->Stats[sys];
        assert(c->Capacity == stats->InstructionCount);

        f << "sysid" << dec << c->SysId << " = ";
        for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){
            uint64_t h = c->GetHits(lvl);
            uint64_t m = c->GetMisses(lvl);
            uint64_t t = h + m;
            f << "l" << dec << lvl << "[" << h << "," << t << "(" << CacheStats::GetHitRate(h, m) << ")] ";
        }
        f << ENDL;
    }
    f << ENDL;

    for (uint32_t bbid = 0; bbid < stats->BlockCount; bbid++){
        if (stats->Counters[bbid] == 0){
            continue;
        }

        inform
            << "Block " << dec << bbid
            << TAB << "@" << hex << &(stats->Counters[bbid])
            << TAB << dec << stats->Counters[bbid]
            << ENDL;
    }

    // compile per-instruction stats into blocks
    CacheStats** aggstats = new CacheStats*[CountCacheStructures];
    for (uint32_t sys = 0; sys < CountCacheStructures; sys++){

        CacheStats* s = stats->Stats[sys];
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
        f << "block" << TAB << dec << bbid
          << TAB << dec << stats->Counters[bbid]
          << TAB << dec << stats->Counters[bbid]
          << TAB << dec << root->GetAccessCount(bbid)
          << ENDL;

        for (uint32_t sys = 0; sys < CountCacheStructures; sys++){
            CacheStats* c = aggstats[sys];
            for (uint32_t lvl = 0; lvl < c->LevelCount; lvl++){

                f << TAB << "sys"
                  << TAB << dec << sys
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

void TryOpen(ofstream& f, const char* name){
    f.open(name);
    f.setf(ios::showbase);
    if (f.fail()){
        ErrorExit("cannot open output file: " << name, MetasimError_FileOp);
    }
}


const char* SimulationFileName(SimulationStats* stats){
    string oFile;

    oFile.append(stats->Application);
    oFile.append(".meta_");
    AppendRankString(oFile);
    oFile.append(".");
    oFile.append(stats->Extension);

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
    return (uint64_t)stats->Buffer;
}

void DeleteCacheStats(void* args){
    SimulationStats* stats = (SimulationStats*)args;
    if (!stats->Initialized){
        delete[] stats->Counters;

        for (uint32_t i = 0; i < CountCacheStructures; i++){
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
}

SamplingMethod::~SamplingMethod(){
}

void SamplingMethod::Print(){
    inform << "SamplingMethod:" << TAB << "AccessLimit " << AccessLimit << " SampleOn " << SampleOn << " SampleOff " << SampleOff << ENDL;
}

void SamplingMethod::IncrementAccessCount(uint64_t count){
    AccessCount += count;
}

bool SamplingMethod::CurrentlySampling(){
    uint32_t PeriodLength = SampleOn + SampleOff;
    if (PeriodLength == 0){
        return true;
    }
    if (AccessCount % PeriodLength < SampleOn){
        return true;
    }
    return false;
}

bool SamplingMethod::ExceedsAccessLimit(uint64_t count){
    if (AccessLimit > 0 && count > AccessLimit){
        return true;
    }
    return false;
}

CacheLevel::CacheLevel(){
}

CacheLevel::CacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol){
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

InclusiveCacheLevel::InclusiveCacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol)
    : CacheLevel(lvl, sizeInBytes, assoc, lineSz, pol)
{
    type = CacheLevelType_Inclusive;
}

uint32_t InclusiveCacheLevel::Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info){
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

const char* InclusiveCacheLevel::TypeString(){
    return "inclusive";
}

ExclusiveCacheLevel::ExclusiveCacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol, uint32_t firstExcl, uint32_t lastExcl)
    : CacheLevel(lvl, sizeInBytes, assoc, lineSz, pol)
{
    type = CacheLevelType_Exclusive;
    FirstExclusive = firstExcl;
    LastExclusive = lastExcl;
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

const char* ExclusiveCacheLevel::TypeString(){
    return "exclusive";
}

MemoryStreamHandler::MemoryStreamHandler(){
    lock = PTHREAD_MUTEX_INITIALIZER;
}
MemoryStreamHandler::~MemoryStreamHandler(){
}

void MemoryStreamHandler::Lock(){
    pthread_mutex_lock(&lock);
}

void MemoryStreamHandler::Unlock(){
    pthread_mutex_unlock(&lock);
}

CacheStructure::CacheStructure(){
}

void CacheStructure::Print(){
    inform << "CacheStructure: "
           << "SysId " << dec << sysId
           << TAB << "Levels " << dec << levelCount
           << ENDL;

    for (uint32_t i = 0; i < levelCount; i++){
        levels[i]->Print(sysId);
    }

}

bool CacheStructure::Verify(){
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
            firstvc = (ExclusiveCacheLevel*)levels[i];
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

bool CacheStructure::Init(string desc){
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
            if (firstExcl != INVALID_CACHE_LEVEL){
                levels[levelId] = new ExclusiveCacheLevel(levelId, cacheValues[0], cacheValues[1], cacheValues[2], repl, firstExcl, levelCount - 1);
            } else {
                levels[levelId] = new InclusiveCacheLevel(levelId, cacheValues[0], cacheValues[1], cacheValues[2], repl);
            }
        }

    }

    if (whichTok != levelCount * 4 + 2){
        return false;
    }

    return Verify();
}

CacheStructure::~CacheStructure(){
    if (levels){
        for (uint32_t i = 0; i < levelCount; i++){
            if (levels[i]){
                delete levels[i];
            }
        }
        delete[] levels;
    }
}

void CacheStructure::Process(void* stats, BufferEntry* access){
    uint32_t next = 0;
    uint64_t victim = access->address;

    evictInfo.level = INVALID_CACHE_LEVEL;
    debug(inform << "Processing sysid " << dec << sysId << " memory id " << dec << access->memseq << " addr " << hex << access->address << endl << flush);
    while (next < levelCount){
        debug(inform << TAB << "next=" << dec << next << ENDL << flush);
        next = levels[next]->Process((CacheStats*)stats, access->memseq, victim, (void*)(&evictInfo));
    }
}

void* GenerateCacheStats(void* args, uint32_t typ, pthread_key_t iid, pthread_t tid){
    SimulationStats* stats = (SimulationStats*)args;
    SimulationStats* s = (SimulationStats*)malloc(sizeof(SimulationStats));
    assert(s);

    memcpy(s, stats, sizeof(SimulationStats));

    s->threadid = tid;
    s->imageid = iid;
    s->Initialized = false;

    // each thread gets its own counters and simulation stats
    s->Counters = new uint64_t[s->BlockCount];
    bzero(s->Counters, s->BlockCount * sizeof(uint64_t));

    s->Stats = new CacheStats*[CountCacheStructures];
    for (uint32_t i = 0; i < CountCacheStructures; i++){
        s->Stats[i] = new CacheStats(CacheStructures_[i]->levelCount, CacheStructures_[i]->sysId, s->InstructionCount);
    }

    return (void*)s;
}

void ReadKnobs(){

    // read caches to simulate
    string cachedf = GetCacheDescriptionFile();
    ifstream CacheFile(cachedf);
    if (CacheFile.fail()){
        ErrorExit("cannot open cache descriptions file: " << cachedf, MetasimError_FileOp);
    }
    
    string line;
    vector<CacheStructure*> caches;
    while (getline(CacheFile, line)){
        if (IsEmptyComment(line)){
            continue;
        }
        CacheStructure* c = new CacheStructure();
        if (!c->Init(line)){
            ErrorExit("cannot parse cache description line: " << line, MetasimError_StringParse);
        }
        caches.push_back(c);
        c->Print();
    }

    CountCacheStructures = caches.size();
    assert(CountCacheStructures > 0 && "No cache structures found for simulation");
    CacheStructures_ = new CacheStructure*[CountCacheStructures];
    for (uint32_t i = 0; i < CountCacheStructures; i++){
        CacheStructures_[i] = caches[i];
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
    
    SamplingMethod_ = new SamplingMethod(SampleMax, SampleOn, SampleOff);
    SamplingMethod_->Print();
}
