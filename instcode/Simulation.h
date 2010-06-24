#ifndef _TracerMethods_h_
#define _TracerMethods_h_

#include <DFPattern.h>
#include <CacheSimulationCommon.h>
#include <CacheStructures.h>

#define INVALID_ADDRESS 0xdeadbeef

#ifdef NO_SAMPLING_MODE
  #define __MAXIMUM_BLOCK_VISIT      500000
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
