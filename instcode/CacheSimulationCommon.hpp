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

#ifndef _CacheSimulationCommon_hpp_
#define _CacheSimulationCommon_hpp_

#include <stdint.h>

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

typedef struct {
    uint64_t    address;
    uint64_t    memseq;
} BufferEntry;
#define __buf_current  address
#define __buf_capacity memseq

typedef struct {
    bool Initialized;
    bool PerInstruction;
    uint32_t Size;
    pthread_t threadid;
    pthread_key_t imageid;
    BufferEntry* Buffer;
    char* Application;
    char* Extension;
    uint32_t* BlockId;
    uint32_t* MemopId;
    char** Files;
    uint32_t* Lines;
    char** Functions;
    uint64_t* Hashes;
    uint64_t* Addresses;

    CounterTypes* Types;
    uint64_t* Counters;
} SimulationStats;

#endif //_CacheSimulationCommon_hpp_
