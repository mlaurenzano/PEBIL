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
#include <unordered_map>
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

enum CacheLevelType {
    CacheLevelType_Undefined,
    CacheLevelType_InclusiveLowassoc,
    CacheLevelType_ExclusiveLowassoc,
    CacheLevelType_InclusiveHighassoc,
    CacheLevelType_ExclusiveHighassoc,
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
static void ReadSettings();
static void* GenerateCacheStats(void* args, uint32_t typ, image_key_t iid, thread_key_t tid);
static uint64_t ReferenceCacheStats(void* args);
static void DeleteCacheStats(void* args);
static bool ReadEnvUint32(string name, uint32_t* var);
static void PrintSimulationStats(ofstream& f, SimulationStats* stats, thread_key_t tid, bool perThread);
static const char* SimulationFileName(SimulationStats* stats);
static const char* LegacySimulationFileName(SimulationStats* stats);
static const char* RangeFileName(SimulationStats* stats);
static const char* LegacyRangeFileName(SimulationStats* stats);

extern "C" {
    void* tool_mpi_init();
    void* tool_thread_init(pthread_t tid);
    void* process_buffer(image_key_t* key);
    void* tool_image_fini(image_key_t* key);
};

class StreamStats {
public:
};

class CacheStats : public StreamStats {
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

struct AddressRange {
    uint64_t Minimum;
    uint64_t Maximum;
};

class RangeStats : public StreamStats {
private:
    static const uint64_t MAX_64BIT_VALUE = 0xffffffffffffffff;
public:
    uint32_t Capacity;
    AddressRange** Ranges;
    uint64_t* Counts;

    RangeStats(uint32_t capacity);
    ~RangeStats();

    bool HasMemId(uint32_t memid);
    uint64_t GetMinimum(uint32_t memid);
    uint64_t GetMaximum(uint32_t memid);

    void Update(uint32_t memid, uint64_t addr);
};

class SamplingMethod {
protected:
    pthread_mutex_t mlock;
public:
    uint32_t AccessLimit;
    uint32_t SampleOn;
    uint32_t SampleOff;
    uint64_t AccessCount;

    SamplingMethod(uint32_t limit, uint32_t on, uint32_t off);
    ~SamplingMethod();

    void Print();

    void IncrementAccessCount(uint64_t count);

    bool SwitchesMode(uint64_t count);
    bool CurrentlySampling();
    bool CurrentlySampling(uint64_t count);
    bool ExceedsAccessLimit(uint64_t count);
};

#define USES_MARKERS(__pol) (__pol == ReplacementPolicy_nmru)
#define CacheLevel_Init_Interface uint32_t lvl, uint32_t sizeInBytes, uint32_t assoc, uint32_t lineSz, ReplacementPolicy pol
#define CacheLevel_Init_Arguments lvl, sizeInBytes, assoc, lineSz, pol

class CacheLevel {
protected:

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

public:
    CacheLevel();
    ~CacheLevel();

    bool IsExclusive() { return (type == CacheLevelType_ExclusiveLowassoc || type == CacheLevelType_ExclusiveHighassoc); }

    CacheLevelType GetType() { return type; }
    ReplacementPolicy GetReplacementPolicy() { return replpolicy; }
    uint32_t GetLevel() { return level; }
    uint32_t GetSizeInBytes() { return size; }
    uint32_t GetAssociativity() { return associativity; }
    uint32_t GetSetCount() { return countsets; }
    uint32_t GetLineSize() { return linesize; }
    uint64_t CountColdMisses();

    uint64_t GetStorage(uint64_t addr);
    uint32_t GetSet(uint64_t addr);
    uint32_t LineToReplace(uint32_t setid);
    bool MultipleLines(uint64_t addr, uint32_t width);

    void MarkUsed(uint32_t setid, uint32_t lineid);
    void Print(uint32_t sysid);

    // re-implemented by HighlyAssociativeCacheLevel
    virtual bool Search(uint64_t addr, uint32_t* set, uint32_t* lineInSet);
    virtual uint64_t Replace(uint64_t addr, uint32_t setid, uint32_t lineid);

    // re-implemented by Exclusive/InclusiveCacheLevel
    virtual uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info);
    virtual const char* TypeString() = 0;
    virtual void Init (CacheLevel_Init_Interface);
};

class InclusiveCacheLevel : public virtual CacheLevel {
public:
    InclusiveCacheLevel() {}

    virtual void Init (CacheLevel_Init_Interface){
        CacheLevel::Init(CacheLevel_Init_Arguments);
        type = CacheLevelType_InclusiveLowassoc;
    }
    virtual const char* TypeString() { return "inclusive"; }
};

class ExclusiveCacheLevel : public virtual CacheLevel {
public:
    uint32_t FirstExclusive;
    uint32_t LastExclusive;

    ExclusiveCacheLevel() {}
    uint32_t Process(CacheStats* stats, uint32_t memid, uint64_t addr, void* info);
    virtual void Init (CacheLevel_Init_Interface, uint32_t firstExcl, uint32_t lastExcl){
        CacheLevel::Init(CacheLevel_Init_Arguments);
        type = CacheLevelType_ExclusiveLowassoc;
        FirstExclusive = firstExcl;
        LastExclusive = lastExcl;
    }
    virtual const char* TypeString() { return "exclusive"; }
};

class HighlyAssociativeCacheLevel : public virtual CacheLevel {
protected:
    unordered_map <uint64_t, uint32_t>** fastcontents;

public:
    HighlyAssociativeCacheLevel() {}
    ~HighlyAssociativeCacheLevel();

    bool Search(uint64_t addr, uint32_t* set, uint32_t* lineInSet);
    uint64_t Replace(uint64_t addr, uint32_t setid, uint32_t lineid);
    virtual void Init (CacheLevel_Init_Interface);
};

class HighlyAssociativeInclusiveCacheLevel : public InclusiveCacheLevel, public HighlyAssociativeCacheLevel {
public:
    HighlyAssociativeInclusiveCacheLevel() {}
    virtual void Init (CacheLevel_Init_Interface){
        InclusiveCacheLevel::Init(CacheLevel_Init_Arguments);
        HighlyAssociativeCacheLevel::Init(CacheLevel_Init_Arguments);
        type = CacheLevelType_InclusiveHighassoc;
    }
    const char* TypeString() { return "inclusive_H"; }
};

class HighlyAssociativeExclusiveCacheLevel : public ExclusiveCacheLevel, public HighlyAssociativeCacheLevel {
public:
    HighlyAssociativeExclusiveCacheLevel() {}
    virtual void Init (CacheLevel_Init_Interface, uint32_t firstExcl, uint32_t lastExcl){
        ExclusiveCacheLevel::Init(CacheLevel_Init_Arguments, firstExcl, lastExcl);
        HighlyAssociativeCacheLevel::Init(CacheLevel_Init_Arguments);
        type = CacheLevelType_ExclusiveHighassoc;
    }
    const char* TypeString() { return "exclusive_H"; }
};

// DFP and other interesting memory things extend this class.
class MemoryStreamHandler {
protected:
    pthread_mutex_t mlock;
public:
    MemoryStreamHandler();
    ~MemoryStreamHandler();

    virtual void Print() = 0;
    virtual void Process(void* stats, BufferEntry* access) = 0;
    virtual bool Verify() = 0;
    bool Lock();
    bool UnLock();
    bool TryLock();
};

typedef enum {
    StreamHandlerType_undefined = 0,
    StreamHandlerType_CacheStructure,
    StreamHandlerType_AddressRange,
    StreamHandlerType_Total
} StreamHandlerTypes;

class AddressRangeHandler : public MemoryStreamHandler {
public:
    AddressRangeHandler();
    AddressRangeHandler(AddressRangeHandler& h);
    ~AddressRangeHandler();

    void Print();
    void Process(void* stats, BufferEntry* access);
    bool Verify();
};

class CacheStructureHandler : public MemoryStreamHandler {
public:
    uint32_t sysId;
    uint32_t levelCount;

    CacheLevel** levels;
    string description;

    // note that this doesn't contain any stats gathering code. that is done at the
    // thread level and is therefore done in ThreadData

    CacheStructureHandler();
    CacheStructureHandler(CacheStructureHandler& h);
    ~CacheStructureHandler();
    bool Init(string desc);

    void Print();
    void Process(void* stats, BufferEntry* access);
    bool Verify();
};


#endif /* _Simulation_hpp_ */
