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


// stuff in this file can be shared with both sides of any tool
#ifndef _Metasim_hpp_
#define _Metasim_hpp_

#include <iostream>
#include <set>

//#define debug(...) __VA_ARGS__
#define debug(...)

typedef uint64_t image_key_t;
typedef pthread_t thread_key_t;

typedef enum {
    CounterType_undefined = 0,
    CounterType_instruction,
    CounterType_basicblock,
    CounterType_loop,
    CounterType_function,
    CounterType_total
} CounterTypes;

static const char* CounterTypeNames[CounterType_total] = {
    "undefined",
    "instruction",
    "basicblock",
    "loop"
    "function"
};

//enum EntryType : uint8_t {
//    MEMENRY = 0
//};
  
//typedef struct {
//    enum EntryType type;
//    char entry[];
//} BufferEntry;

typedef struct MemEntry {
    uint64_t address;
    uint64_t memseq;
} MemEntry;

typedef struct ImageMemEntry : MemEntry {
    uint64_t imageid;
} ImageMemEntry;

typedef struct PrefetchEntry : MemEntry {} PrefetchEntry;
typedef struct ImagePrefetchEntry : ImageMemEntry {} ImagePrefetchEntry;

typedef struct VectorMemEntry {
    uint32_t indexVector[16];
    uint64_t base;
    uint8_t  scale;
    uint64_t memseq;
} VectorCacheEntry;

typedef struct ImageVectorMemEntry : VectorMemEntry {
    uint64_t imageid;
} ImageVectorMemEntry;

typedef struct {
    uint64_t    loadstoreflag;   // Dirty Caching
    uint64_t    imageid;         // Multi-image
    uint64_t    memseq;          // identifies memop in image
    uint64_t    address;         // value simulated
//    uint64_t    threadid;        // Error-checking
//    uint64_t    programAddress;  // only used for adamant
} BufferEntry;
#define __buf_current  address
#define __buf_capacity memseq

class StreamStats;
class MemoryStreamHandler;
class ReuseDistance;

typedef struct{
    uint64_t GroupId; // for now, same as BB-ID/ Top-most loop of 
    uint32_t InnerLevelSize;
    uint64_t GroupCount;
    uint64_t* InnerLevelBasicBlocks; // Since there can be >1 
} NestedLoopStruct;

typedef struct {
    // memory buffer
    BufferEntry* Buffer;

    // metadata
    thread_key_t threadid;
    image_key_t imageid;
    bool Initialized;
    bool PerInstruction;
    bool Master;
    uint32_t Phase;
    uint32_t InstructionCount;
    uint32_t BlockCount;
    uint32_t AllocCount;
    char* Application;
    char* Extension;

    // per-memop data
    uint64_t* BlockIds;
    uint64_t* MemopIds;

    // per-block data
    CounterTypes* Types;
    uint64_t* Counters;
    uint32_t* MemopsPerBlock;
    char** Files;
    uint32_t* Lines;
    char** Functions;
    uint64_t* Hashes;
    uint64_t* Addresses;
    uint64_t* GroupIds;
    StreamStats** Stats; // indexed by handler
    MemoryStreamHandler** Handlers;
    ReuseDistance** RHandlers;


    uint64_t NestedLoopCount;
    NestedLoopStruct* NLStats; 

} SimulationStats;

#define BUFFER_ENTRY(__stats, __n) (&(__stats->Buffer[__n+1]))
#define BUFFER_CAPACITY(__stats) (__stats->Buffer[0].__buf_capacity)
#define BUFFER_CURRENT(__stats) (__stats->Buffer[0].__buf_current)

typedef enum {
    PointType_undefined = 0,
    PointType_blockcount,
    PointType_buffercheck,
    PointType_bufferinc,
    PointType_bufferfill,
    PointType_total
} PointTypes;

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

#define GENERATE_KEY(__bid, __typ) ((__typ & 0xf) | (__bid << 4))
#define GET_BLOCKID(__key) ((__key >> 4))
#define GET_TYPE(__key) ((__key & 0xf))

#endif //_Metasim_hpp_

