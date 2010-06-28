#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <strings.h>
#include <CacheSimulationCommon.h>


#ifndef PRINT_INSTR
#define PRINT_INSTR(file, ...) fprintf(file, __VA_ARGS__); \
    fprintf(file, "\n"); \
    fflush(file);
#endif

/************* Helpers ***********/
#define __HASH_FUNC(__x,__y,__z) (((__x) / __y) % (__z))
HashEntry* initHash(Attribute_t setCount,Attribute_t assocCount){
    uint32_t setIdx;
    uint32_t linIdx;
    HashEntry* hashLines = NULL;

    if(assocCount >= __MAX_LINEAR_SEARCH_ASSOC){
        Attribute_t entryCount = setCount * assocCount * 2;
        hashLines = (HashEntry*) malloc (sizeof(HashEntry)*entryCount);
        bzero(hashLines,sizeof(HashEntry)*entryCount);

        for(setIdx=0;setIdx<setCount;setIdx++){
            HashEntry* nextFreePtr = hashLines + (setIdx * assocCount * 2) + assocCount;
            for(linIdx=0;linIdx<assocCount;linIdx++){
                nextFreePtr[linIdx].next = (linIdx+1) % assocCount;
            }
        }
    }

    return hashLines;
}


#ifdef DEBUG_RUN_2
uint64_t howManyHashSearches = 0;
uint64_t howManyHashTraverses = 0;
#endif

uint16_t findInHash(Address_t lineIdx,Attribute_t setCount,Attribute_t assocCount,
                    HashEntry* cacheHash,uint32_t setIdx)
{
#ifdef DEBUG_RUN_2
    howManyHashSearches++;
#endif
    uint16_t ret = assocCount;
    if(lineIdx && cacheHash){
        HashEntry* hashLines = cacheHash + (setIdx * assocCount * 2);
        HashEntry* nextFreePtr = hashLines + assocCount;
        uint16_t hashCode = __HASH_FUNC(lineIdx,setCount,assocCount);
        HashEntry* curr = hashLines + hashCode;
        while(curr){
#ifdef DEBUG_RUN_2
            howManyHashTraverses++;
#endif
            if(curr->key == lineIdx){
                ret = curr->value;
                break;
            }
            curr = ( curr->next ? (nextFreePtr + curr->next) : NULL );
        }
    }
    return ret;
}

void deleteFromHash(Address_t lineIdx,Attribute_t setCount,Attribute_t assocCount,
                    HashEntry* cacheHash,uint32_t setIdx)
{
    if(lineIdx && cacheHash){
        HashEntry* hashLines = cacheHash + (setIdx * assocCount * 2);
        HashEntry* nextFreePtr = hashLines + assocCount;
        uint16_t hashCode = __HASH_FUNC(lineIdx,setCount,assocCount);
        HashEntry* curr = hashLines + hashCode;
        if(curr->key == lineIdx){
            curr->key = 0;
            curr->value = assocCount;
        } else {
            HashEntry* prev = curr;
            while(prev){
                uint16_t currIdx = prev->next;
                curr = ( currIdx ? (nextFreePtr + currIdx) : NULL );
                if(curr && (curr->key == lineIdx)){
                    curr->key = 0;
                    curr->value = assocCount;
                    prev->next = curr->next;
                    curr->next = nextFreePtr->next;
                    nextFreePtr->next = currIdx;
                    break;
                }
                prev = curr;
            }
        }
    }
}

void insertInHash(Address_t lineIdx,Attribute_t setCount,Attribute_t assocCount,
                  HashEntry* cacheHash,uint32_t setIdx,uint16_t assocIdx)
{
    if(lineIdx && cacheHash){
        HashEntry* hashLines = cacheHash + (setIdx * assocCount * 2);
        HashEntry* nextFreePtr = hashLines + assocCount;
        uint16_t hashCode = __HASH_FUNC(lineIdx,setCount,assocCount);
        HashEntry* curr = hashLines + hashCode;
        if(!curr->key){
            curr->key = lineIdx;
            curr->value = assocIdx;
        } else {
            HashEntry* prev = curr;

            uint16_t currIdx = nextFreePtr->next; 
            curr = (currIdx ? (nextFreePtr + currIdx) : NULL);

#ifdef DEBUG_RUN_2
            if(!curr){
                printf("FATAL Error FATAL Error in Cache Simulation\n");
                exit(-1);
            }
#endif
            curr->key = lineIdx;
            curr->value = assocIdx;

            nextFreePtr->next = curr->next;
            curr->next = prev->next;
            prev->next = currIdx;
        }
    }
}
/************* End Helpers ******/

/*
  Initialize cache structures
*/
void initCaches(
    MemoryHierarchy* systems,
    uint32_t systemCount)
{
    register uint32_t i;
    register uint32_t systemIdx;
    register uint32_t level;
    register uint32_t setIdx;

    for(systemIdx=0;systemIdx<systemCount;systemIdx++){
        register MemoryHierarchy* memoryHierarchy = (systems + systemIdx);
        register uint32_t totalLineCount = 0;
        for(level=0;level<memoryHierarchy->levelCount;level++){

            register Attribute_t setCount = memoryHierarchy->levels[level].attributes[number_of_sets];
            register Attribute_t linePerSet = memoryHierarchy->levels[level].attributes[set_associativity];
            register Attribute_t sizeInLines = setCount*linePerSet;
            totalLineCount += sizeInLines;
        }

        Address_t* allLines = (Address_t*)malloc(sizeof(Address_t)*totalLineCount);
        bzero(allLines,sizeof(Address_t)*totalLineCount);

        for(level=0;level<memoryHierarchy->levelCount;level++){

            register Attribute_t setCount = memoryHierarchy->levels[level].attributes[number_of_sets];
            register Attribute_t linePerSet = memoryHierarchy->levels[level].attributes[set_associativity];
            register Attribute_t sizeInLines = setCount*linePerSet;

            memoryHierarchy->levels[level].hitMissCounters[cache_hit] = 0;
            memoryHierarchy->levels[level].hitMissCounters[cache_miss] = 0;
            memoryHierarchy->levels[level].content = allLines;
#ifdef PER_SET_RECENT
            memoryHierarchy->levels[level].mostRecent = (uint16_t*)malloc(sizeof(uint16_t)*setCount);
            for(setIdx=0;setIdx<setCount;setIdx++){
                memoryHierarchy->levels[level].mostRecent[setIdx] = (linePerSet-1);
            }
#else
            memoryHierarchy->levels[level].mostRecent = (linePerSet-1);
#endif
            allLines += sizeInLines;

            memoryHierarchy->levels[level].highAssocHash = NULL;
            
            if(linePerSet >= __MAX_LINEAR_SEARCH_ASSOC){
                memoryHierarchy->levels[level].highAssocHash = initHash(setCount,linePerSet);
            }
        }

        for (i = 0; i < memoryHierarchy->levelCount; i++){
            Cache* cache = &(memoryHierarchy->levels[i]);

            // make sure an unknown replacement policy is not used
            if(!IS_REPL_POLICY_RAN(cache->attributes[replacement_policy]) && 
               !IS_REPL_POLICY_LRU(cache->attributes[replacement_policy]) && 
               !IS_REPL_POLICY_DIR(cache->attributes[replacement_policy]) &&
               !IS_REPL_POLICY_VC(cache->attributes[replacement_policy])){
                PRINT_INSTR(stderr,"***** fatal error in instrumentation lib: unknown replacement policy found in level %d of cache structure %d, %d", i, memoryHierarchy->index, cache->attributes[replacement_policy]);
                exit(-1);
            }

            // make sure that the last cache level does not use a VC policy
            if (i == memoryHierarchy->levelCount - 1){
                if (IS_REPL_POLICY_VC(cache->attributes[replacement_policy])){
                    PRINT_INSTR(stderr, "***** fatal error in instrumentation lib: cannot use a victim cache policy in the highest cache level");
                    exit(-1);
                }
            }
        }

        // ensure that each consecutive level of victim cache has the same number of sets
        uint32_t numOfSets = 0;
        for (i = 0; i < memoryHierarchy->levelCount; i++){
            Cache* cache = &(memoryHierarchy->levels[i]);
            if (IS_REPL_POLICY_VC(cache->attributes[replacement_policy])){
                if (numOfSets){
                    if (numOfSets != cache->attributes[number_of_sets]){
                        //PRINT_INSTR(stderr, "***** fatal error in instrumentation lib: all victim cache levels must have the same number of sets");
                        exit(-1);
                    }
                } else {
                    numOfSets = cache->attributes[number_of_sets];
                }
            }
        }
    }
}

/*  searchCache

  address: the address to search for
  status:  outvalue, indicates whether we hit or miss
  cache: invalue, pointer to this cache
  invalidiate: invalue, if true, remove the item from the cache
*/
void searchCache(Address_t address,
                 AccessStatus* status,
                 Cache* cache,
                 uint32_t invalidate) {
    Attribute_t lineSizeInBits;
    Attribute_t setCount;
    Attribute_t assocCount;
    uint32_t setIdx, lineInSet;
    Address_t* thisSet;

    lineSizeInBits = cache->attributes[line_size_in_bits];
    setCount = cache->attributes[number_of_sets];
    assocCount = cache->attributes[set_associativity];

    setIdx = CACHE_LINE_INDEX(address, lineSizeInBits) % setCount;
    thisSet = &cache->content[setIdx * assocCount];

    *status = cache_miss;

    // Search for the address
//printf("Searching for cli %llx\n", CACHE_LINE_INDEX(address, lineSizeInBits));
    if (assocCount >= __MAX_LINEAR_SEARCH_ASSOC){
        lineInSet = findInHash(CACHE_LINE_INDEX(address,lineSizeInBits),
                       setCount,assocCount,cache->highAssocHash,setIdx);
        if(lineInSet < assocCount){
            *status = cache_hit;
        }
    } else {
        for (lineInSet = 0; lineInSet < assocCount; ++lineInSet){
            if(CACHE_LINE_INDEX(thisSet[lineInSet],lineSizeInBits) ==
               CACHE_LINE_INDEX(address,lineSizeInBits)){
                *status = cache_hit;
                break;
            }
        }
    }

    // Update the most recent access and hit counters
    if(*status == cache_hit){
        if(invalidate){
            cache->MOSTRECENT(setIdx) = (lineInSet - 1) % assocCount;
        } else {
            cache->MOSTRECENT(setIdx) = lineInSet;
        }
        ++cache->hitMissCounters[cache_hit];
    } else{
        ++cache->hitMissCounters[cache_miss];
    }
}

/* insertIntoCache

  address: invalue, the address to insert
  victim: outvalue, the address we evicted
  cache: invalue, pointer to this cache
*/
void insertIntoCache(Address_t address,
                     Address_t* victim,
                     Cache* cache) {

    Attribute_t lineSizeInBits;
    Attribute_t setCount;
    Attribute_t assocCount;
    uint32_t setIdx, lineInSet;
    Address_t* thisSet;

    lineSizeInBits = cache->attributes[line_size_in_bits];
    setCount = cache->attributes[number_of_sets];
    assocCount = cache->attributes[set_associativity];

    setIdx = CACHE_LINE_INDEX(address, lineSizeInBits) % setCount;
    thisSet = &cache->content[setIdx * assocCount];
    lineInSet = (cache->MOSTRECENT(setIdx) + 1) % assocCount;

    *victim = thisSet[lineInSet];
//printf("Inserting cli %llx at Set: %llu Line: %llu\n",
//  CACHE_LINE_INDEX(address, cache->attributes[line_size_in_bits]),
//  setIdx,
//  lineInSet);

    thisSet[lineInSet] = address;
    cache->MOSTRECENT(setIdx) = lineInSet;
}

/* Inclusive Caching

  toFind: invalue, the address to search for it is inserted
  victim: outvalue, the evicted address
  status: outvalue, indicates hit or miss in this level
  cache:  invalue, pointer to this cache

*/
void processInclusiveCache(Address_t toFind,
                           Address_t* victim,
                           AccessStatus* status,
                           Cache* cache) {
//printf("Searching inclusive cache for cli %llx\n", CACHE_LINE_INDEX(toFind, cache->attributes[line_size_in_bits]));
    searchCache(toFind, status, cache, 0);
//if(*status == cache_hit) printf("Hit cli %llx in inclusive cache\n", CACHE_LINE_INDEX(toFind, cache->attributes[line_size_in_bits]));
    if(*status == cache_miss){
//printf("Missed, inserting cli %llx in inclusive cache\n",CACHE_LINE_INDEX(toFind, cache->attributes[line_size_in_bits]));
        insertIntoCache(toFind, victim, cache);
//printf("had to evict cli %llx from inclusive cache\n", CACHE_LINE_INDEX(*victim, cache->attributes[line_size_in_bits]));
    }
}

/* Victim Caching

  toFind: invalue, address to search for
  victim: in-outvalue, insert this value and set to value we evict
  status: outvalue, indicates hit or miss in this level
  cache:  invalue, pointer to this cache
*/
void processVictimCache(Address_t toFind,
                        Address_t* victim,
                        uint8_t*   nVictims,
                        AccessStatus* status,
                        Cache* cache) {
    int i;
//printf("Processing victim cache: total lines %llu\n", cache->attributes[number_of_sets] * cache->attributes[set_associativity]);
//printf("Searching victim cache for cli %llx\n", CACHE_LINE_INDEX(toFind, cache->attributes[line_size_in_bits]));
    searchCache(toFind, status, cache, 1);
//printf("Inserting victim cli %llx into victim cache\n", CACHE_LINE_INDEX(*victim, cache->attributes[line_size_in_bits]));
    for( i = 0; i < *nVictims; ++i ) {
      insertIntoCache(victim[i], &victim[i], cache);
    }
//printf("Evicted cli %llx from victim cache\n", CACHE_LINE_INDEX(*victim, cache->attributes[line_size_in_bits]));
}


/*
  Predictive Caching/prefetching

  Predicts unit stride access patterns

  toFind: invalue, the address to search for
  victim: outvalue, addresses we evict
  status: outvalue, indicates hit or miss at this level
  cache: invalue, pointer to this cache level
*/
void processPredictionCache(Address_t toFind,
                            Address_t* victim,
                            uint8_t*   nVictims,
                            AccessStatus* status,
                            Cache* cache) {

  Address_t cli;

  searchCache(toFind, status, cache, 0);

  *nVictims = 1;  
  cli = CACHE_LINE_INDEX(toFind, cache->attributes[line_size_in_bits]);

  // If status is miss, pull in data and train
  if( *status == cache_miss ) {
    insertIntoCache(toFind, victim, cache);
    
    // If we last accessed the previous cache line, train to the next
    if( cache->trainer == cli - 1 ) {
      cache->stream = cli;
      // prefetch to fetchDistance
      // check to see if they are already in cache?
      for(cache->nFetched = 0; cache->nFetched < cache->fetchDistance; ++cache->nFetched) {
        insertIntoCache((cli + cache->nFetched + 1) << cache->attributes[line_size_in_bits],
                        &victim[cache->nFetched + 1], cache);
      }
      *nVictims = cache->nFetched;
    }
  }

  // If status is hit, check if address is in our current prediction stream
  // if it is, propagate stream
  if( *status == cache_hit ) {
    if( cli == cache->stream + 1 ) {
      cache->stream = cli;
      // Pull in the next line in the stream 
      insertIntoCache((cli + cache->fetchDistance) << cache->attributes[line_size_in_bits],
                      &victim[1], cache);
      *nVictims = 2;
    }
  }

  // Update the last cache line used
  cache->trainer = cli;
}
