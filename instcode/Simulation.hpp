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

#ifndef _Simulation_hpp_
#define _Simulation_hpp_

#include <string>
#include <DFPattern.h>
#include <Metasim.hpp>

using namespace std;

#define DEFAULT_CACHE_FILE "instcode/CacheDescriptions.txt"
#define DEFAULT_SAMPLE_ON  1000000
#define DEFAULT_SAMPLE_OFF 10000000
#define DEFAULT_SAMPLE_MAX 0

#define KILO (1024)
#define MEGA (KILO*KILO)
#define GIGA (MEGA*KILO)

#define INVALID_CACHE_LEVEL (0xffffffff)

typedef struct {
    uint64_t    address;
    uint64_t    memseq;
} BufferEntry;
#define __buf_current  address
#define __buf_capacity memseq

class CacheStats;
typedef struct {
    // metadata
    pthread_t threadid;
    pthread_key_t imageid;
    bool Initialized;
    bool PerInstruction;
    uint32_t InstructionCount;
    uint32_t BlockCount;
    char* Application;
    char* Extension;

    // memory buffer
    BufferEntry* Buffer;

    // per-memop data
    uint32_t* BlockIds;
    uint32_t* MemopIds;

    // per-block data
    CounterTypes* Types;
    uint64_t* Counters;
    uint32_t* MemopsPerBlock;
    char** Files;
    uint32_t* Lines;
    char** Functions;
    uint64_t* Hashes;
    uint64_t* Addresses;
    CacheStats** Stats;
} SimulationStats;
#define BUFFER_CAPACITY(__stats) (__stats->Buffer[0].__buf_capacity)
#define BUFFER_CURRENT(__stats) (__stats->Buffer[0].__buf_current)


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

static const char* ReplacementPolicyNames[ReplacementPolicy_Total] = {
    "undefined",
    "truelru",
    "nmru",
    "random",
    "direct"
};

struct EvictionInfo {
    uint64_t addr;
    uint32_t level;
    uint32_t setid;
    uint32_t lineid;
};

typedef struct {
    uint64_t       minAddress;
    uint64_t       maxAddress;
} DFPatternRange;

typedef struct {
    void*           basicBlock; // points to BB info?
    DFPatternType   type;
    uint64_t        rangeCnt;
    DFPatternRange* ranges;
} DFPatternInfo;

struct LevelStats {
    uint64_t hitCount;
    uint64_t missCount;
};

static uint32_t RandomInt();
static uint32_t Low32(uint64_t f);
static uint32_t High32(uint64_t f);
static char ToLowerCase(char c);
static bool IsEmptyComment(string str);
static string GetCacheDescriptionFile();
static bool ParsePositiveInt32(string token, uint32_t* value);
static bool ParseInt32(string token, uint32_t* value, uint32_t min);
static bool ParsePositiveInt32Hex(string token, uint32_t* value);
static void ReadKnobs();
static void* GenerateCacheStats(void* args, uint32_t typ, pthread_key_t iid, pthread_t tid);
static uint64_t ReferenceCacheStats(void* args);
static void DeleteCacheStats(void* args);
static bool ReadEnvUint32(string name, uint32_t* var);
static void PrintSimulationStats(ofstream& f, SimulationStats* stats, pthread_t tid);
static const char* SimulationFileName(SimulationStats* stats);
static void TryOpen(ofstream& f, const char* name);

extern "C" {
    void* tool_mpi_init();
    void* tool_thread_init(pthread_t tid);
    void* process_buffer(uint64_t* key);
    void* tool_image_fini(uint64_t* key);
};

class CacheStats {
public:
    uint32_t LevelCount;
    uint32_t SysId;
    LevelStats** Stats;
    uint32_t Capacity;

    CacheStats(uint32_t lvl, uint32_t sysid, uint32_t capacity);
    ~CacheStats();

    bool HasMemId(uint32_t memid);
    void ExtendCapacity(uint32_t newSize);
    void NewMem(uint32_t memid);

    void Hit(uint32_t memid, uint32_t lvl);
    void Miss(uint32_t memid, uint32_t lvl);
    void Hit(uint32_t memid, uint32_t lvl, uint32_t cnt);
    void Miss(uint32_t memid, uint32_t lvl, uint32_t cnt);

    static float GetHitRate(LevelStats* stats);
    static float GetHitRate(uint64_t hits, uint64_t misses);
    uint64_t GetHits(uint32_t memid, uint32_t lvl);
    uint64_t GetMisses(uint32_t memid, uint32_t lvl);
    uint64_t GetHits(uint32_t lvl);
    uint64_t GetMisses(uint32_t lvl);
    LevelStats* GetLevelStats(uint32_t memid, uint32_t lvl);
    uint64_t GetAccessCount(uint32_t memid);
    float GetHitRate(uint32_t memid, uint32_t lvl);
    float GetCumulativeHitRate(uint32_t memid, uint32_t lvl);
};

class SamplingMethod {
public:
    uint32_t AccessLimit;
    uint32_t SampleOn;
    uint32_t SampleOff;
    uint64_t AccessCount;

    SamplingMethod(uint32_t limit, uint32_t on, uint32_t off);
    ~SamplingMethod();

    void Print();

    void IncrementAccessCount(uint64_t count);

    bool CurrentlySampling();
    bool ExceedsAccessLimit(uint64_t count);
};

#define USES_MARKERS(__pol) (__pol == ReplacementPolicy_nmru)
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

    CacheLevel();
    CacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol);
    ~CacheLevel();

    CacheLevelType GetType();
    uint32_t GetLevel();
    uint32_t GetSetCount();
    uint64_t CountColdMisses();

    uint64_t GetStorage(uint64_t addr);
    uint32_t GetSet(uint64_t addr);
    uint32_t LineToReplace(uint32_t setid);
    bool Search(uint64_t addr, uint32_t* set, uint32_t* lineInSet);
    bool MultipleLines(uint64_t addr, uint32_t width);

    uint64_t Replace(uint64_t addr, uint32_t setid, uint32_t lineid);
    void MarkUsed(uint32_t setid, uint32_t lineid);

    void Print(uint32_t sysid);

    virtual uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info) = 0;
    virtual const char* TypeString() = 0;
};

class InclusiveCacheLevel : public CacheLevel {
public:
    InclusiveCacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol);
    uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info);
    const char* TypeString();
};

class ExclusiveCacheLevel : public CacheLevel {
public:
    uint32_t FirstExclusive;
    uint32_t LastExclusive;

    ExclusiveCacheLevel(uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol, uint32_t firstExcl, uint32_t lastExcl);
    uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info);
    const char* TypeString();
};

// DFP and other interesting memory things extend this class.
class MemoryStreamHandler {
private:
    pthread_mutex_t lock;
public:
    MemoryStreamHandler();
    ~MemoryStreamHandler();

    virtual void Print() = 0;
    virtual void Process(void* stats, BufferEntry* access) = 0;
};

class PatternExtraction : public MemoryStreamHandler{
public:
};

class CacheStructure : public MemoryStreamHandler {
public:
    uint32_t sysId;
    uint32_t levelCount;

    CacheLevel** levels;
    string description;
    EvictionInfo evictInfo;

    // note that this doesn't contain any stats gathering code. that is done at the
    // thread level and is therefore done in ThreadData

    CacheStructure();
    ~CacheStructure();
    bool Init(string desc);

    void Print();
    bool Verify();

    void Process(void* stats, BufferEntry* access);
};


#endif /* _Simulation_hpp_ */
