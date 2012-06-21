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
#include <string.h>
#include <strings.h>
#include <DFPattern.h>


#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <string.h>
#include <assert.h>
#include <InstrumentationCommon.hpp>

#include <Simulation.hpp>
#include <CacheSimulationCommon.hpp>

using namespace std;


#define METASIM_ID "Metasim"
#define METASIM_VERSION "3.0.0"
#define METASIM_ENV "PEBIL_ROOT"
#define DEFAULT_CACHE_FILE "instcode/CacheDescriptions.txt"

#define TAB "\t"
#define ENDL "\n"
uint32_t MemCapacity = 0x1000;

#define DISPLAY_ERROR cerr << "[" << METASIM_ID << "-r" << getTaskId() << "] " << "Error: "
#define warn cerr << "[" << METASIM_ID << "-r" << getTaskId() << "] " << "Warning: "
#define ErrorExit(__msg, __errno) DISPLAY_ERROR << __msg << endl << flush; exit(__errno);
enum MetasimErrors {
    MetasimError_None = 0,
    MetasimError_MemoryAlloc,
    MetasimError_NoThread,
    MetasimError_TooManyInsnReads,
    MetasimError_StringParse,
    MetasimError_FileOp,
    MetasimError_Env,
    MetasimError_Total,
};


DataManager<SimulationStats*>* alldata = NULL;

uint32_t RandomInt(uint32_t max){
    return rand() % max;
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


#define KILO (1024)
#define MEGA (KILO*KILO)
#define GIGA (MEGA*KILO)

// returns true on success... allows things to continue on failure if desired
bool ParsePositiveInt32(string token, uint32_t* value){
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

    if (val <= 0){
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

void* gen_cache_stats(void* args, uint32_t typ, pthread_key_t iid, pthread_t tid){
    SimulationStats* stats = (SimulationStats*)args;
    SimulationStats* s = (SimulationStats*)malloc(sizeof(SimulationStats));
    assert(s);

    // create or copy parts of existing struct as necessary

    return (void*)s;
}

uint64_t ref_cache_stats(void* args){
    SimulationStats* stats = (SimulationStats*)args;
    return (uint64_t)stats->Buffer;
}

void del_cache_stats(void* args){
    SimulationStats* stats = (SimulationStats*)args;
    if (!stats->Initialized){
        // free allocated parts
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

bool read_env_uint32(string name, uint32_t* var){
    char* e = getenv(name.c_str());
    if (e == NULL){
        return false;
    }
    string s = (string)e;
    if (!ParsePositiveInt32(s, var)){
        return false;
    }

    return true;
}

class SamplingMethod {
public:
    uint32_t AccessLimit;
    uint32_t SampleOn;
    uint32_t SampleOff;

    uint64_t AccessCount;

    SamplingMethod(uint32_t limit, uint32_t on, uint32_t off){
        AccessLimit = limit;
        SampleOn = on;
        SampleOff = off;
    }

    ~SamplingMethod(){
    }

    void IncrementAccessCount(uint64_t count){
        AccessCount += count;
    }

    bool CurrentlySampling(){
        uint32_t PeriodLength = SampleOn + SampleOff;
        if (PeriodLength == 0){
            return true;
        }
        if (AccessCount % PeriodLength < SampleOn){
            return true;
        }
        return false;
    }

    bool ExceedsAccessLimit(uint64_t count){
        if (AccessLimit > 0 && count > AccessLimit){
            return true;
        }
        return false;
    }
};
static SamplingMethod* SamplingMethod_ = NULL;



extern "C" {
    void* tool_mpi_init(){
        return NULL;
    }

    void* tool_image_init(SimulationStats* stats, uint64_t* key, ThreadData* td){

        PRINT_INSTR(stdout, "raw args %#lx, %#lx, %#lx", (uint64_t)stats, (uint64_t)key, (uint64_t)td);

        assert(stats->Initialized == true);
        if (alldata == NULL){
            alldata = new DataManager<SimulationStats*>(gen_cache_stats, del_cache_stats, ref_cache_stats);
        }

        BufferEntry* intro = &(stats->Buffer[0]);
        BufferEntry* buf = &(stats->Buffer[1]);

        PRINT_INSTR(stdout, "Buffer features: capacity=%ld, current=%ld", intro->__buf_capacity, intro->__buf_current);

        *key = alldata->AddImage(stats, td);
        stats->imageid = *key;
        stats->threadid = pthread_self();

        alldata->SetTimer(*key, 0);
        
        return NULL;
    }

    void* process_buffer(uint64_t* key){
        alldata->SetTimer(*key, 2);
        if (alldata == NULL){
            PRINT_INSTR(stderr, "data manager does not exist. no images were initialized");
            return NULL;
        }
        SimulationStats* stats = (SimulationStats*)alldata->GetData(*key, pthread_self());
        if (stats == NULL){
            PRINT_INSTR(stderr, "Cannot retreive image data using key %ld", *key);
            return NULL;
        }        

        //PRINT_INSTR(stdout, "counter %ld\tcapacity %d", stats->Buffer[0].__buf_current, stats->Buffer[0].__buf_capacity);
        for (uint32_t i = 0; i < stats->Buffer[0].__buf_current; i++){
            BufferEntry* buf = &(stats->Buffer[1]);
            //PRINT_INSTR(stdout, "\t\tbuf[%d] addr %#lx\tbb %d\tmem %d", i, buf[i].address, buf[i].blockid, buf[i].memopid);
        }

        stats->Buffer[0].__buf_current = 0;

        alldata->SetTimer(*key, 3);
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
        SimulationStats* stats = (SimulationStats*)alldata->GetData(*key, pthread_self());
        if (stats == NULL){
            PRINT_INSTR(stderr, "Cannot retreive image data using key %ld", *key);
            return NULL;
        }        

        process_buffer(key);
    }

};

struct LevelStats {
    uint64_t hitCount;
    uint64_t missCount;
};

class CacheStats {
public:
    uint32_t LevelCount;
    uint32_t SysId;
    LevelStats** Stats;

    CacheStats(uint32_t lvl, uint32_t sysid){
        LevelCount = lvl;
        SysId = sysid;

        Stats = new LevelStats*[MemCapacity];
    }

    ~CacheStats(){
        if (Stats){
            for (uint32_t i = 0; i < MemCapacity; i++){
                if (Stats[i]){
                    delete Stats[i];
                }
            }
            delete[] Stats;
        }
    }

    static float GetHitRate(LevelStats* stats){
        if (stats->hitCount + stats->missCount == 0){
            return 0.0;
        }
        return ((float)stats->hitCount) / ((float)stats->hitCount + (float)stats->missCount);
    }

    void ExtendMemCapacity(uint32_t newSize){
        LevelStats** nn = new LevelStats*[newSize];

        memset(nn, 0, sizeof(LevelStats*) * newSize);
        memcpy(nn, Stats, sizeof(LevelStats*) * MemCapacity);

        delete[] Stats;
        Stats = nn;
    }

    void NewMem(uint32_t memid){
        assert(memid < MemCapacity);

        LevelStats* mem = new LevelStats[LevelCount];
        memset(mem, 0, sizeof(LevelStats) * LevelCount);
        Stats[memid] = mem;
    }

    void Hit(uint32_t memid, uint32_t lvl){
        Stats[memid][lvl].hitCount++;
    }

    void Miss(uint32_t memid, uint32_t lvl){
        Stats[memid][lvl].missCount++;
    }

    uint64_t GetHits(uint32_t memid, uint32_t lvl){
        return Stats[memid][lvl].hitCount;
    }

    uint64_t GetMisses(uint32_t memid, uint32_t lvl){
        return Stats[memid][lvl].missCount;
    }

    bool HasMemId(uint32_t memid){
        if (memid >= MemCapacity){
            return false;
        }
        if (Stats[memid] == NULL){
            return false;
        }
        return true;
    }

    LevelStats* GetLevelStats(uint32_t memid, uint32_t lvl){
        return &(Stats[memid][lvl]);
    }

    uint64_t GetAccessCount(uint32_t memid){
        LevelStats* l1 = GetLevelStats(memid, 0);
        if (l1){
            return (l1->hitCount + l1->missCount);
        }
        return 0;
    }

    float GetHitRate(uint32_t memid, uint32_t lvl){
        return GetHitRate(GetLevelStats(memid, lvl));
    }

    float GetCumulativeHitRate(uint32_t memid, uint32_t lvl){
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
};

enum CacheLevelType {
    CacheLevelType_Undefined,
    CacheLevelType_Inclusive,
    CacheLevelType_Exclusive,
    CacheLevelType_Total
};

enum ReplacementPolicy {
    ReplacementPolicy_Undefined,
    ReplacementPolicy_truelru,
    ReplacementPolicy_nmru,
    ReplacementPolicy_random,
    ReplacementPolicy_direct,
    ReplacementPolicy_Total
};

struct EvictionInfo {
    uint64_t addr;
    uint32_t level;
    uint32_t setid;
    uint32_t lineid;
};

#define INVALID_CACHE_LEVEL (0xffffffff)
class CacheLevel {
public:

    CacheLevelType type;

    uint32_t level;
    uint32_t size;
    uint32_t associativity;
    uint32_t linesize;
    ReplacementPolicy replpolicy;

    uint32_t countsets;
    uint32_t linesizeBits;

    uint64_t** contents;
    uint32_t* recentlyUsed;

    CacheLevel(){
    }

    CacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol){
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

    ~CacheLevel(){
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

    CacheLevelType GetType(){
        return type;
    }

    uint32_t GetLevel(){
        return level;
    }

    uint32_t GetSetCount(){
        return countsets;
    }

    uint64_t CountColdMisses(){
        return (countsets * associativity);
    }

    void Print(ofstream& s, uint32_t sysid){
        s   << dec << sysid
            << TAB << dec << level
            << TAB << dec << size
            << TAB << dec << associativity
            << TAB << dec << linesize
            << TAB << TypeString()
            << ENDL;
    }

    uint64_t GetStorage(uint64_t addr){
        return (addr >> linesizeBits);
    }

    uint32_t GetSet(uint64_t addr){
        return (addr % countsets);
    }

    uint32_t LineToReplace(uint32_t setid){
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

    uint64_t Replace(uint64_t addr, uint32_t setid, uint32_t lineid){
        uint64_t prev = contents[setid][lineid];
        contents[setid][lineid] = addr;
        MarkUsed(setid, lineid);
        return prev;
    }

    void MarkUsed(uint32_t setid, uint32_t lineid){
        //inform << "level " << dec << level << " USING set " << dec << setid << " line " << lineid << ENDL << flush;
        recentlyUsed[setid] = lineid;
    }

    bool Search(uint64_t addr, uint32_t* set, uint32_t* lineInSet){
        uint32_t setId = GetSet(addr);
        //inform << TAB << TAB << "stored " << hex << addr << " set " << dec << setId << endl << flush;
        if (set){
            (*set) = setId;
        }

        uint64_t* thisset = contents[setId];
        for (uint32_t i = 0; i < associativity; i++){
            //inform << TAB << TAB << TAB << "checking assoc=" << dec << i << " contains " << hex << thisset[i] << endl << flush;
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
    bool MultipleLines(uint64_t addr, uint32_t width){
        return false;
    }
    
    virtual uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info) = 0;
    virtual const char* TypeString() = 0;
};

class InclusiveCacheLevel : public CacheLevel {
public:
    InclusiveCacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol)
        : CacheLevel(lvl, sizeInBytes, assoc, lineSz, pol)
    {
        type = CacheLevelType_Inclusive;
    }

    uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info){
        uint32_t set = 0, lineInSet = 0;
        uint64_t store = GetStorage(addr);
        //inform << TAB << "level " << dec << level << endl << flush;

        // hit
        if (Search(store, &set, &lineInSet)){
            stats->Stats[memid][level].hitCount++;
            //inform << TAB << TAB << "hit" << endl << flush;
            MarkUsed(set, lineInSet);

            return INVALID_CACHE_LEVEL;
        }
        //inform << TAB << TAB << "missed" << endl << flush;

        // miss
        stats->Stats[memid][level].missCount++;
        Replace(store, set, LineToReplace(set));
        return level + 1;
    }

    const char* TypeString(){
        return "inclusive";
    }
};

class ExclusiveCacheLevel : public CacheLevel {

public:
    uint32_t FirstExclusive;
    uint32_t LastExclusive;

    ExclusiveCacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol, uint32_t firstExcl, uint32_t lastExcl)
        : CacheLevel(lvl, sizeInBytes, assoc, lineSz, pol)
    {
        type = CacheLevelType_Exclusive;
        FirstExclusive = firstExcl;
        LastExclusive = lastExcl;
    }

    uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info){
        uint32_t set = 0;
        uint32_t lineInSet = 0;

        uint64_t store = GetStorage(addr);
        //inform << TAB << "level " << dec << level << " store " << hex << store << endl << flush;

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

            //inform << TAB << "up-evicting " << hex << e->addr << " into level " << dec << level << " set " << dec << set << " line " << dec << lineInSet << ENDL << flush;
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

            //inform << TAB << TAB << "hit" << endl << flush;
            e->level = level;
            e->addr = store;
            e->setid = set;
            e->lineid = lineInSet;

            if (level == FirstExclusive){
                return INVALID_CACHE_LEVEL;
            }
            return FirstExclusive;
        }

        //inform << TAB << TAB << "missed" << endl << flush;

        // miss
        stats->Stats[memid][level].missCount++;

        if (level == LastExclusive){
            e->level = LastExclusive + 1;
            e->addr = store;
            //inform << "airball!" << ENDL << flush;

            return FirstExclusive;
        }
        return level + 1;
    }

    const char* TypeString(){
        return "exclusive";
    }
};

class MemoryStreamHandler {
public:
    MemoryStreamHandler(){
    }
    ~MemoryStreamHandler(){
    }

    virtual void Process(void* stats, BufferEntry* access) = 0;
};

class PatternExtraction : public MemoryStreamHandler{
public:
};

static uint32_t CountCacheStructures = 0;
class CacheStructure : public MemoryStreamHandler {
public:
    uint32_t sysId;
    uint32_t levelCount;

    CacheLevel** levels;
    string description;
    EvictionInfo evictInfo;

    // note that this doesn't contain any stats gathering code. that is done at the
    // thread level and is therefore done in ThreadData

    CacheStructure(){
    }

    void Print(ofstream& s){
        s   << "#" << dec << sysId
            << TAB << dec << levelCount
            << ENDL;

        for (uint32_t i = 0; i < levelCount; i++){
            levels[i]->Print(s, sysId);
        }

    }

    bool Verify(){
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

    bool Init(string desc){
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

    ~CacheStructure(){
        if (levels){
            for (uint32_t i = 0; i < levelCount; i++){
                if (levels[i]){
                    delete levels[i];
                }
            }
            delete[] levels;
        }
    }

    void Process(void* stats, BufferEntry* access){
        uint32_t next = 0;
        uint64_t victim = access->address;

        evictInfo.level = INVALID_CACHE_LEVEL;
        //inform << "Processing sysid " << dec << sysId << " memory id " << dec << access->memseq << " addr " << hex << access->address << endl << flush;
        while (next < levelCount){
            //inform << TAB << "next=" << dec << next << ENDL << flush;
            next = levels[next]->Process((CacheStats*)stats, access->memseq, victim, (void*)(&evictInfo));
        }
    }
};

static inline uint32_t Low32(uint64_t f){
    return (uint32_t)f & 0xffffffff;
}

static inline uint32_t High32(uint64_t f){
    return (uint32_t)((f & 0xffffffff00000000) >> 32);
}

void read_knobs(uint32_t* sample_max, uint32_t* sample_on, uint32_t* sample_off){

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
    }
    CountCacheStructures = caches.size();

    uint32_t SampleMax;
    uint32_t SampleOn;
    uint32_t SampleOff;

    read_env_uint32("METASIM_SAMPLE_MAX", &SampleMax);
    read_env_uint32("METASIM_SAMPLE_ON", &SampleOn);
    read_env_uint32("METASIM_SAMPLE_OFF", &SampleOff);
    
    SamplingMethod_ = new SamplingMethod(SampleMax, SampleOn, SampleOff);

}

