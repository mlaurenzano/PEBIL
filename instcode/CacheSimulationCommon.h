#ifndef _TracerMethods_h_
#define _TracerMethods_h_

#ifdef PER_SET_RECENT
  #define MOSTRECENT(__setIdx) mostRecent[__setIdx]
#else
  #define MOSTRECENT(__setIdx) mostRecent
#endif

typedef uint32_t     Attribute_t;  
typedef uint64_t     Counter_t;    
#ifdef METASIM_32_BIT_LIB
  typedef uint32_t     Address_t;
#else
  typedef uint64_t     Address_t;
#endif

#ifdef NO_SAMPLING_MODE
  #define __MAXIMUM_BLOCK_VISIT       500000
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

#define __MAX_LEVEL                      3
#define __MAX_LINEAR_SEARCH_ASSOC       64
#define __MAX_STRING_SIZE             1024

typedef enum {
    repl_lru = 0,
    repl_ran,
    repl_dir,
#ifdef VICTIM_CACHE
    repl_lru_vc,
#endif // VICTIM_CACHE
    Total_ReplacementPolicy
} ReplacementPolicy;

#ifdef VICTIM_CACHE
#define IS_REPL_POLICY_VC(__policy)  (__policy == repl_lru_vc)
#define IS_REPL_POLICY_LRU(__policy) ((__policy == repl_lru) || (__policy == repl_lru_vc))
#define IS_REPL_POLICY_RAN(__policy) (__policy == repl_ran)
#define IS_REPL_POLICY_DIR(__policy) (__policy == repl_dir)
#define CACHE_LINE_INDEX(__addr,__bits) (__addr >> __bits)
#define __L1_CACHE_LEVEL 0
#endif //VICTIM_CACHE

typedef enum {
    number_of_sets = 0,
    set_associativity,
    line_size_in_bits,
    replacement_policy,
    Total_CacheAttributes
} CacheAttributes;

typedef enum {
    cache_hit = 0,
    cache_miss,
    Total_AccessStatus
} AccessStatus;
    
typedef struct {
    Address_t  key;
    uint16_t   value;
    uint16_t   next;
} HashEntry;

typedef struct {
    Attribute_t attributes[Total_CacheAttributes];
    Counter_t   hitMissCounters[Total_AccessStatus];
#ifdef PER_SET_RECENT
    uint16_t*   mostRecent;
#else
    uint16_t    mostRecent;
#endif
    Address_t*  content;
    HashEntry*  highAssocHash;
} Cache;

typedef struct {
    uint32_t  index;
    uint8_t   levelCount;
    Cache     levels[__MAX_LEVEL]; 
#ifdef VICTIM_CACHE
    uint8_t   isVictimCacheHierarchy;
#endif
} MemoryHierarchy;

#ifndef NO_SAMPLING_MODE
typedef enum {
    sampling_accesses = 0,
    ignoring_accesses
} SamplingStatus;

SamplingStatus currentSamplingStatus = ignoring_accesses;
Counter_t alreadyIgnored = 0;
Counter_t alreadySampled = 0;
#endif

typedef struct {
    Attribute_t blockId;
    Attribute_t memOpId;
#ifdef METASIM_32_BIT_LIB
    Attribute_t unused;
#endif
    Address_t   address;
} BufferEntry;

#define lastFreeIdx blockId

#include <CacheStructures.h>

typedef struct {
    Counter_t saturationPoint;
    Counter_t visitCount;
    Counter_t sampleCount;
    Counter_t hitMissCounters[__SYSTEM_COUNT*__MAX_LEVEL*Total_AccessStatus];
#ifdef COUNT_BB_EXECCOUNT
    Counter_t *counter;
#endif
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
