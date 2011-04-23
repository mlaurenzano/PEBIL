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

#ifndef _TracerMethods_h_
#define _TracerMethods_h_

#include <DFPattern.h>
#include <CacheSimulationCommon.h>
#include <CacheStructures.h>

#ifndef SHIFT_ADDRESS_BUFFER
  #define INVALID_ADDRESS 0xdeadbeef
#endif // SHIFT_ADDRESS_BUFFER

#ifdef NO_SAMPLING_MODE
  #define __MAXIMUM_BLOCK_VISIT     10000000
#else
  #define __MAXIMUM_BLOCK_VISIT        50000
  #ifdef FINE_GRAIN_SAMPLING
    #define __SAMPLING_INTERVAL_MAX     100000
    #define __IGNORING_INTERVAL_MAX    1000000
  #else
    #define __SAMPLING_INTERVAL_MAX    1000000
    #define __IGNORING_INTERVAL_MAX   10000000
  #endif

  #ifdef EXTENDED_SAMPLING
    Counter_t sampling_interval_max = __SAMPLING_INTERVAL_MAX;
    Counter_t ignoring_interval_max = __IGNORING_INTERVAL_MAX;
    #define __WHICH_SAMPLING_VALUE  sampling_interval_max
    #define __WHICH_IGNORING_VALUE  ignoring_interval_max
    #define SEGMENT_COUNT 10  /* needs to divide both intervals */
    Attribute_t rand_value = SEGMENT_COUNT;
  #else
    #define __WHICH_SAMPLING_VALUE  __SAMPLING_INTERVAL_MAX
    #define __WHICH_IGNORING_VALUE  __IGNORING_INTERVAL_MAX
  #endif
#endif

#ifndef NO_SAMPLING_MODE
typedef enum {
    sampling_accesses = 0,
    ignoring_accesses
} SamplingStatus;

SamplingStatus currentSamplingStatus = ignoring_accesses;
Counter_t alreadyIgnored = 0;
Counter_t alreadySampled = 0;
#endif

#define lastFreeIdx blockId

typedef struct {
    Attribute_t blockId;
    Attribute_t memOpId;
#ifdef METASIM_32_BIT_LIB
    Attribute_t unused;
#endif
    Address_t   address;
} BufferEntry;

typedef struct {
    Counter_t saturationPoint;
    Counter_t visitCount;
    Counter_t sampleCount;
    Counter_t hitMissCounters[__SYSTEM_COUNT*__MAX_LEVEL*Total_AccessStatus];
} BasicBlockInfo;

typedef struct {
    Address_t       minAddress;
    Address_t       maxAddress;
} DFPatternRange;

typedef struct {
    BasicBlockInfo* basicBlock;
    DFPatternType   type;
    Counter_t       rangeCnt;
    DFPatternRange* ranges;
} DFPatternInfo;

#endif /* _TracerMethods_h_ */
