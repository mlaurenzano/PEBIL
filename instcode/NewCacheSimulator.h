#ifndef _NewCacheSimulator_h_
#define _NewCacheSimulator_h_

typedef uint32_t     Attribute_t;

#ifdef METASIM_32_BIT_LIB
    typedef uint32_t     Address_t;
#else
    typedef uint64_t     Address_t;
#endif

typedef struct {
    Attribute_t blockId;
    Attribute_t memOpId;
#ifdef METASIM_32_BIT_LIB
    Attribute_t unused;
#endif
    Address_t   address;
} AddressEntry;

#define lastFreeIdx blockId

void processSamples(AddressEntry*,Attribute_t,Attribute_t);

/*** Define data structures needed for cache 
     simulation functions, such as to keep track of
     hit/miss counts, memory hierarchies and etc. 
     |            |           |             |
     V            V           V             V
***/


#endif
