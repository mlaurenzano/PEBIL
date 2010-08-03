/* This program is free software: you can redistribute it and/or modify
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
#include <InstrumentationCommon.h>

#define MAX_OF(__a,__b) ((__a) > (__b) ? (__a) : (__b))

#include <Simulation.h>
#include <CacheSimulationCommon.h>

void* instrumentationPoints;
int32_t numberOfInstrumentationPoints;
int32_t numberOfBasicBlocks;
char* blockIsKilled;
int32_t numberKilled;
uint64_t* blockCounters;

//#define ENABLE_INSTRUMENTATION_KILL
//#define DEBUG_INST_KILL

void clearBlockCounters(){
    bzero(blockCounters, sizeof(uint64_t) * numberOfBasicBlocks);
}

// should concider doing initialization here rather than checking
// for "first hit" on every buffer dump
// PROBLEM^^ on some systems (cray XT) it is unsafe to call functions
// from the entry function
int entry_function(void* instpoints, int32_t* numpoints, int32_t* numblocks, uint64_t* counters, char* killed){
    instrumentationPoints = instpoints;
    numberOfInstrumentationPoints = *numpoints;
    numberOfBasicBlocks = *numblocks;
    blockCounters = counters;
    blockIsKilled = killed;
    numberKilled = 0;

    instpoint_info* ip;
    int i;
}

void disableInstrumentationPointsInBlock(BufferEntry* currentEntry){
    int32_t blockId = currentEntry->blockId;
    instpoint_info* ip;
    int i;

    int32_t killedPoints = 0;
    for (i = 0; i < numberOfInstrumentationPoints; i++){
        ip = (instpoint_info*)(instrumentationPoints + (i * sizeof(instpoint_info)));
        if (ip->pt_blockid == blockId){
            int32_t size = ip->pt_size;
            int64_t vaddr = ip->pt_vaddr;
            char* program_point = (char*)vaddr;
            
#ifdef DEBUG_INST_KILL
            PRINT_INSTR(stdout, "killing instrumentation for memop %d in block %d at %#llx", i, currentEntry->blockId, vaddr);
#endif
            memcpy(ip->pt_content, program_point, size);
            memcpy(program_point, ip->pt_disable, size);
            killedPoints++;
        }
    }

    numberKilled += killedPoints;
    PRINT_INSTR(stdout, "Killing instrumentation points for block %d (%d points) -- %d killed so far", blockId, killedPoints, numberKilled);
}

#ifdef USE_SAFE_STD_FUNCTIONS

#define __MAX_BLOCK_AREA  (0x1000 * (sizeof(BasicBlockInfo)/sizeof(Address_t) + 1))
#define __MAX_MALLOC_SIZE __MAX_CACHE_AREA + __MAX_BLOCK_AREA
Address_t allocation_area[__MAX_MALLOC_SIZE];
Address_t* alloc_ptr = allocation_area;
void* local_malloc(uint32_t n){
    Address_t* end_ptr = alloc_ptr + __MAX_MALLOC_SIZE;
    Address_t* ret = alloc_ptr;
    uint32_t wcount = (n + (n % sizeof(Address_t) ? sizeof(Address_t) : 0))/sizeof(Address_t);
    alloc_ptr += wcount;
    if(alloc_ptr >= end_ptr){
    printf("FATAL Error FATAL Error in Cache Simulation\n");
    exit(-1);
    }
    return ret;
}

int getRandomNumber();
uint32_t local_random() {
    uint32_t dummy = 0;
    uint32_t first_base = (uint32_t)&dummy;
    first_base >>= 4;
    first_base &= 0xffff;
    uint32_t second_base = getRandomNumber();
    return (first_base + second_base);
}

#define bzero(__ptr,__n) memset((__ptr),0,(__n))
#define malloc local_malloc
#define random local_random

#endif

#define STATUS_IDX(__sys,__lvl,__sta) ((__sys)*(__MAX_LEVEL*Total_AccessStatus) + (__lvl) * Total_AccessStatus + (__sta))

Counter_t       lastSaturationAccess = 0;
Counter_t       saturatedBlockCount = 0;
Counter_t       totalNumberOfAccesses = 0;
Counter_t       totalNumberOfSamples = 0;

uint32_t        blockCount = 0;
BasicBlockInfo* blocks = NULL;
DFPatternInfo*  dfPatterns = NULL;

/******** these variables/macros support the address stream dump tool **********/
FILE* streamDumpOutputFile = NULL;
uint32_t dumpCode = 0;
#define DUMPCODE_HASVALUE_DUMP(__dc) (__dc != 0)
#define DUMPCODE_HASVALUE_SIMU(__dc) (!__dc || __dc > 1)
#define DUMPCODE_IS_VALID(__dc)      (__dc < 3)
#define STREAM_DUMP_FILE_EXT "addrdmp"
#define NATIVE_IS_BIG_ENDIAN 1
//#define NATIVE_IS_BIG_ENDIAN 0
#define GET_DUMP_FLAGS(__endian,__32b) 'p','a','d',(char)((__endian << 1) | (__32b))
/******** these variables/macros support the address stream dump tool **********/

static unsigned int SEED = 93186752;
int getRandomNumber()
{
    double x;
    static unsigned a = 1588635695, m = 4294967291U, q = 2, r = 1117695901;
    SEED = a*(SEED % q) - r*(SEED / q);
    x = (double)SEED / (double)m; /* return the next random number x: 0 <= x < 1 */

    return ( (int)128*x +1);
}

uint32_t lookupRankId(){
    return __taskid;
    uint32_t rankId = 0;
    char strBuffer[__MAX_STRING_SIZE];

    uint32_t processId = getpid();
    uint32_t hostId = gethostid();

    FILE* fp = fopen("RankPid","r");
    if(fp){
        uint32_t entryFound = 0;
        while(fgets(strBuffer,1024,fp)){
            strBuffer[1023] = '\0';
            uint32_t corresPid = 0,corresHid = 0;
            sscanf(strBuffer,"%u %u %u",&rankId,&corresPid,&corresHid);
            if((corresPid == processId) && (corresHid == hostId)){
                entryFound = 1;
                break;
            }
        }
        fclose(fp);
        if(!entryFound){
            rankId = processId;
        }
    } else {
        rankId = processId;
    }

    return rankId;
}

FILE* openOutputFile(const char* comment,FILE** dfpFp){

    uint32_t rankId = lookupRankId();

    char strBuffer[__MAX_STRING_SIZE];

    char appName[__MAX_STRING_SIZE];
    uint32_t phaseId = 0;
    char extension[__MAX_STRING_SIZE];

    sscanf(comment,"%s %u %s",appName,&phaseId,extension);

    if(phaseId > 0){
        sprintf(strBuffer,"%s.phase.%d.meta_%04d.%s",appName,phaseId,rankId,extension);
    } else {
        sprintf(strBuffer,"%s.meta_%04d.%s",appName,rankId,extension);
    }
    
    FILE* fp = fopen(strBuffer,"w");
    if(!fp){
        PRINT_INSTR(stderr,"Error : Can not open %s to write results",strBuffer);
        return NULL;
    }

    fprintf(fp,"# appname   = %s\n",appName);
    fprintf(fp,"# extension = %s\n",extension);
    fprintf(fp,"# phase     = %d\n",phaseId);
    fprintf(fp,"# rank      = %d\n",rankId);

    if(dfPatterns && dfpFp){
        if(phaseId > 0){
            sprintf(strBuffer,"%s.phase.%d.meta_%04d.%s",appName,phaseId,rankId,"dfp");
        } else {
            sprintf(strBuffer,"%s.meta_%04d.%s",appName,rankId,"dfp");
        }
        
        *dfpFp = fopen(strBuffer,"w");
        if(!(*dfpFp)){
            PRINT_INSTR(stderr,"Error : Can not open %s to write results",strBuffer);
            return NULL;
        }

        fprintf(*dfpFp,"# appname   = %s\n",appName);
        fprintf(*dfpFp,"# extension = %s\n",extension);
        fprintf(*dfpFp,"# phase     = %d\n",phaseId);
        fprintf(*dfpFp,"# rank      = %d\n",rankId);
    }

    return fp;
}


const char* DFPatternTypeNames[] = {
    "dfTypePattern_undefined",
    "dfTypePattern_Other",
    "dfTypePattern_Stream",
    "dfTypePattern_Transpose",
    "dfTypePattern_Random",
    "dfTypePattern_Reduction",
    "dfTypePattern_Stencil",
    "dfTypePattern_Gather",
    "dfTypePattern_Scatter",
    "dfTypePattern_FunctionCallGS",
    "dfTypePattern_Init",
    "dfTypePattern_Default",
    "dfTypePattern_Scalar",
    "dfTypePattern_None"
};


#define DFPATTERN_INTEREST(__t) (((__t) == dfTypePattern_Gather) || \
                                 ((__t) == dfTypePattern_Scatter) || \
                                 ((__t) == dfTypePattern_FunctionCallGS))

void initDfPatterns(DFPatternSpec* dfps,uint32_t n,BasicBlockInfo* bbs){
    register uint32_t i = 0, j = 0;
    dfPatterns = NULL;

    if(dfps->type == DFPattern_Active){
        PRINT_INSTR(stdout,"DFPatterns are activated with %u entries",n);
        uint32_t anyTagged = 0;
        for(i=1;i<=n;i++){
            if(dfps[i].type == dfTypePattern_undefined){
                PRINT_INSTR(stdout, "Error in dfpattern type %i %s",i,DFPatternTypeNames[dfps[i].type]);
                assert (dfps[i].type != dfTypePattern_undefined);
            }
            if(DFPATTERN_INTEREST(dfps[i].type)){
               anyTagged++;
            }
        }
        PRINT_INSTR(stdout,"DFPatterns are tagged in only %u of %u",anyTagged,n);
        if(anyTagged){
            dfPatterns = (DFPatternInfo*)malloc(sizeof(DFPatternInfo)*n);
            bzero(dfPatterns,sizeof(DFPatternInfo)*n);
            for(i=0;i<n;i++){
                dfPatterns[i].basicBlock = bbs + i;
                dfPatterns[i].type = dfps[i+1].type;
                dfPatterns[i].rangeCnt = dfps[i+1].memopCnt;
                if(DFPATTERN_INTEREST(dfPatterns[i].type) &&
                   (dfPatterns[i].rangeCnt > 0)){
                    dfPatterns[i].ranges = (DFPatternRange*)malloc(sizeof(DFPatternRange)*dfPatterns[i].rangeCnt);
                    for(j=0;j<dfPatterns[i].rangeCnt;j++){
                        dfPatterns[i].ranges[j].maxAddress = 0x0;
                        dfPatterns[i].ranges[j].minAddress = (Address_t)-1;
                    }
                } else {
                    dfPatterns[i].ranges = NULL;
                }
            }
        }
    } else {
        PRINT_INSTR(stdout,"DFPatterns are not activated");
    }
}


void processDFPatternEntry(BufferEntry* entries,Attribute_t startIndex,Attribute_t lastIndex){
    register Attribute_t i = 0;

    if(!dfPatterns){
        return;
    }
    for(i=startIndex;i<=lastIndex;i++){
        BufferEntry* currentEntry = (entries + i);
        Address_t addr = currentEntry->address;
        Attribute_t memopIdx = currentEntry->memOpId;
        BasicBlockInfo* bb = blocks + currentEntry->blockId;
        DFPatternInfo* info = (dfPatterns + currentEntry->blockId);

        if(info->basicBlock != bb){
            assert (0 && "Fatal: Something is wrong");
        }
        if((info->type == dfTypePattern_None) ||
           (info->type >= dfTypePattern_Total_Types)){
            continue;
        }
        if(memopIdx >= info->rangeCnt){
            assert (0 && "Fatal: How come memopidx is larger than all count");
        }

        DFPatternRange* range = info->ranges + memopIdx;

        if(range->minAddress > addr){
            range->minAddress = addr;
        }
        if(range->maxAddress < addr){
            range->maxAddress = addr;
        }
    }
}

#define DUMP_CHAR_MASK 0x000000ff
#define INT32_BIT_MASK 0xffffffff
void print32bitBinaryValue(FILE* outFile, uint32_t value){
    char c1, c2, c3, c4;
    c1 = (value >> 24) & DUMP_CHAR_MASK;
    c2 = (value >> 16) & DUMP_CHAR_MASK;
    c3 = (value >> 8 ) & DUMP_CHAR_MASK;
    c4 = (value >> 0 ) & DUMP_CHAR_MASK;
    fprintf(outFile, "%c%c%c%c", c1, c2, c3, c4);
}

void print64bitBinaryValue(FILE* outFile, uint64_t value){
    print32bitBinaryValue(outFile, (uint32_t)((value >> 32) & INT32_BIT_MASK));
    print32bitBinaryValue(outFile, (uint32_t)((value >> 0 ) & INT32_BIT_MASK));
}

void processSamples_StreamDump(BufferEntry* entries,Attribute_t startIndex,Attribute_t lastIndex){
    register uint32_t systemIdx;
    register uint32_t accessIdx;
    register uint32_t level;

    for(accessIdx=entries[startIndex-1].memOpId;accessIdx<=lastIndex;){
        register BufferEntry* currentEntry = (entries + accessIdx);
        register Attribute_t currentMemOp = currentEntry->memOpId;
        register Address_t currentAddress = currentEntry->address;
        
#ifndef SHIFT_ADDRESS_BUFFER
        // currentMemOp is set in such a way that values will be skipped by this code due to sampling.
        // This is a bit of a hack. Ideally, we would use something like 0xffffffffffffffff as the "bad"
        // address value since addresses can be 0 sometimes.
        if(!currentAddress){
            if(currentMemOp > accessIdx){
                accessIdx = currentMemOp;
                continue;
            }
        }
#endif // SHIFT_ADDRESS_BUFFER
        accessIdx++;
        
#ifdef METASIM_32_BIT_LIB
        // is a 32bit address to print
        print32bitBinaryValue(streamDumpOutputFile, currentAddress);
#else
        // is a 64bit address to print
        print64bitBinaryValue(streamDumpOutputFile, currentAddress);
#endif
    }
}


/*
  Run cache simulation on entries
*/
void processSamples_Simulate(BufferEntry* entries,Attribute_t startIndex,Attribute_t lastIndex){
static int ntimes;
static int ntimes2;
#ifdef DEBUG_RUN_2
    static Attribute_t howManyProcessSamplesCall = 0;
    printf("processSamples_Simulate is called for %dth\n",howManyProcessSamplesCall++);
    fflush(stdout);

#endif
    register uint32_t systemIdx;
    register uint32_t accessIdx;
    register uint32_t level;

    for (systemIdx = 0; systemIdx < systemCount; systemIdx++){
        register MemoryHierarchy* memoryHierarchy = (systems + systemIdx);

        for(accessIdx=entries[startIndex-1].memOpId;accessIdx<=lastIndex;){
            register BufferEntry* currentEntry = (entries + accessIdx);
            register Attribute_t currentMemOp = currentEntry->memOpId;
            register Address_t currentAddress = currentEntry->address;

#ifndef SHIFT_ADDRESS_BUFFER
            // currentMemOp is set in such a way that values will be skipped by this code due to sampling.
            // This is a bit of a hack. Ideally, we would use something like 0xffffffffffffffff as the "bad"
            // address value since addresses can be 0 sometimes.
            if(currentAddress == INVALID_ADDRESS){
                if(currentMemOp > accessIdx){
                    accessIdx = currentMemOp;
                    continue;
                }
            }
#endif // SHIFT_ADDRESS_BUFFER
            ++accessIdx;

            //PRINT_INSTR(stdout, "Buffer Entry: %d %d %#llx", currentEntry->blockId, currentEntry->memOpId, currentEntry->address);

            register BasicBlockInfo* currentBlock = (blocks + currentEntry->blockId);

            register uint32_t isStrideCheckSystem = (memoryHierarchy->index == __STRIDE_TARGET_SYSTEM);

            register uint8_t levelCount = memoryHierarchy->levelCount;

            AccessStatus status;
            Address_t    victim;
            Cache*       prevLevel;
            Cache*       cache;
            uint8_t      nvictims;

            status = cache_miss;
            level = 0;
            prevLevel = NULL;
            nvictims = 0;

            do{
              cache = &memoryHierarchy->levels[level];
              if( prevLevel != NULL && IS_REPL_POLICY_VC(prevLevel->attributes[replacement_policy])) {
                processVictimCache(currentAddress, &victim, &nvictims, &status, cache);
              } else {
                processInclusiveCache(currentAddress, &victim, &nvictims, &status, cache);
              }

/*
              switch(cache->attributes[cache_type]) {

                case inclusive_cache:
                  processInclusiveCache(currentAddress, &victim, &status, cache);
                  break;

                case victim_cache:
                  processVictimCache(currentAddress, &victim, &nvictims, &status, cache);
                  break;

                case prediction_cache:
                  processPredictionCache(currentAddress,&victim, &nvictims, &status, cache);
                  // FIXME Need to handly possibility of multiple victims
                  break;
              }
*/
              ++level;
              prevLevel = cache;
              if(status == cache_miss) {
                  ++currentBlock->hitMissCounters[STATUS_IDX(
                                                             systemIdx, level - 1, cache_miss)];
              } else {
                  ++currentBlock->hitMissCounters[STATUS_IDX(
                                                             systemIdx, level - 1, cache_hit)];
              }
              
            } while( status == cache_miss && level < memoryHierarchy->levelCount);

        }
    }
}


void MetaSim_simulFuncCall_Simu(char* base,int32_t* entryCountPtr,const char* comment){
#ifdef IGNORE_ACTUAL_SIMULATION
    register BufferEntry* entries = (BufferEntry*)base;
    entries->lastFreeIdx = 1;
    return;
#else
    register uint32_t      i;
    register BufferEntry* entries = (BufferEntry*)base;
    register Attribute_t startIndex = 1;
    register Attribute_t lastIndex = entries->lastFreeIdx;
    lastIndex--;
    //    PRINT_INSTR(stdout, "starting sim function -- startIndex %d, lastfree %d, entryCount %d", startIndex, entries->lastFreeIdx, *entryCountPtr);

    totalNumberOfAccesses += lastIndex;

    if(!blocks){
#ifndef HAVE_MPI
        taskid = getpid();
#endif
        char      appName[__MAX_STRING_SIZE];
        uint32_t  phaseId = 0;
        char      extension[__MAX_STRING_SIZE];

        sscanf(comment,"%s %u %s %u %u",appName,&phaseId,extension,&blockCount,&dumpCode);
        PRINT_INSTR(stdout, "comment handled -- %s %u %s %u %u", appName, phaseId, extension, blockCount, dumpCode);
        blocks = (BasicBlockInfo*)malloc(sizeof(BasicBlockInfo) * blockCount);
        bzero(blocks,sizeof(BasicBlockInfo)*blockCount);
        initCaches(systems, systemCount);

        DFPatternSpec* dfps = (DFPatternSpec*)(entries + *entryCountPtr);

        uint32_t rankId = lookupRankId();
        char strBuffer[__MAX_STRING_SIZE];
        
        if (DUMPCODE_HASVALUE_DUMP(dumpCode)){
            
            if(phaseId > 0){
                sprintf(strBuffer,"%s.phase.%d.meta_%04d.%s.%s",appName,phaseId,rankId,extension,STREAM_DUMP_FILE_EXT);
            } else {
                sprintf(strBuffer,"%s.meta_%04d.%s.%s",appName,rankId,extension,STREAM_DUMP_FILE_EXT);
            }
    
            streamDumpOutputFile = fopen(strBuffer, "w");
            printf("Opened output file %s for address stream dump\n", strBuffer);
            fflush(stdout);
        }
        if (DUMPCODE_HASVALUE_DUMP(dumpCode)){
#ifdef METASIM_32_BIT_LIB
            fprintf(streamDumpOutputFile, "%c%c%c%c", GET_DUMP_FLAGS(NATIVE_IS_BIG_ENDIAN, 1));
#else
            fprintf(streamDumpOutputFile, "%c%c%c%c", GET_DUMP_FLAGS(NATIVE_IS_BIG_ENDIAN, 0));
#endif
        }

        if (!DUMPCODE_HASVALUE_SIMU(dumpCode)){
            PRINT_INSTR(stdout, "WARNING: this run has simulation turned off via an option to the --dump flag");
            PRINT_INSTR(stderr, "WARNING: this run has simulation turned off via an option to the --dump flag");
        }


        initDfPatterns(dfps,blockCount,blocks);
    }

#ifdef DEBUG_RUN_2
    PRINT_INSTR(stdout,"MetaSim_simulFuncCall_Simu(0x%p,%d,%s,%d)",base,*entryCountPtr,comment,entries->lastFreeIdx);
#endif

    if(dfPatterns){ 
        processDFPatternEntry(entries,startIndex,lastIndex);
    }

    if(blockCount && (saturatedBlockCount == blockCount)){
        entries->lastFreeIdx = 1;
        //        PRINT_INSTR(stdout, "leaving sim function 1-- startIndex %d, lastIndex %d", startIndex, entries->lastFreeIdx);
        return;
    }

    lastSaturationAccess = totalNumberOfAccesses;

#ifndef NO_SAMPLING_MODE
    if(currentSamplingStatus == ignoring_accesses){
        alreadyIgnored += lastIndex;
        if(alreadyIgnored > __WHICH_IGNORING_VALUE){
            currentSamplingStatus = sampling_accesses;
            alreadySampled = alreadyIgnored - __WHICH_IGNORING_VALUE;
#ifdef EXTENDED_SAMPLING
            if(alreadySampled > lastIndex){
                startIndex = 1;
            } else {
#endif
            startIndex = (lastIndex - alreadySampled + 1);
#ifdef EXTENDED_SAMPLING
            }
            Attribute_t tmp_rand = rand_value;
            rand_value = (Attribute_t)(random() % (SEGMENT_COUNT+1));
#ifdef DEBUG_RUN_1
            PRINT_INSTR(stdout,"* %12lld IGNORED > %8lld sampled %8lld ignored %8lld [%6d,%6d] rand (%2d,%2d)",
                    totalNumberOfAccesses,__WHICH_IGNORING_VALUE,alreadySampled,alreadyIgnored,
                    startIndex,lastIndex,tmp_rand,rand_value);
#endif
            __WHICH_IGNORING_VALUE = __IGNORING_INTERVAL_MAX - (tmp_rand*(__IGNORING_INTERVAL_MAX/SEGMENT_COUNT));
            __WHICH_IGNORING_VALUE += (Counter_t)(rand_value*(__IGNORING_INTERVAL_MAX/SEGMENT_COUNT));
#endif
            alreadyIgnored = 0;
        } else {
            entries->lastFreeIdx = 1;
            //            PRINT_INSTR(stdout, "leaving sim function 2-- startIndex %d, lastIndex %d", startIndex, entries->lastFreeIdx);
            return;
        }
    } else {
        alreadySampled += lastIndex;
        if(alreadySampled > __WHICH_SAMPLING_VALUE){
            currentSamplingStatus = ignoring_accesses;
            alreadyIgnored = alreadySampled - __WHICH_SAMPLING_VALUE;
#ifdef EXTENDED_SAMPLING
            if(alreadyIgnored > lastIndex){
                lastIndex = 0;
            } else {
#endif
            lastIndex = (lastIndex - alreadyIgnored);
#ifdef EXTENDED_SAMPLING
            }
            Attribute_t tmp_rand = (random() % (SEGMENT_COUNT+1)) + (SEGMENT_COUNT/2);
#ifdef DEBUG_RUN_1
            PRINT_INSTR(stdout,"- %12lld SAMPLED > %8lld ignored %8lld sampled %8lld [%6d,%6d] rand (%2d)",
                    totalNumberOfAccesses,__WHICH_SAMPLING_VALUE,alreadyIgnored,alreadySampled,
                    startIndex,lastIndex,tmp_rand);
#endif
            __WHICH_SAMPLING_VALUE = (Counter_t)((1.0*tmp_rand/SEGMENT_COUNT)*__SAMPLING_INTERVAL_MAX);
#endif
            alreadySampled = 0;
        }
    }
#endif /* NO_SAMPLING_MODE */

#ifdef EXTENDED_SAMPLING
    if(lastIndex < startIndex){
        entries->lastFreeIdx = 1;
#ifdef DEBUG_RUN_1
        PRINT_INSTR(stdout,"DONE lastIndex < startIndex");
#endif
        //        PRINT_INSTR(stdout, "leaving sim function 3-- startIndex %d, lastIndex %d", startIndex, entries->lastFreeIdx); 
        return;
    }
#endif

    totalNumberOfSamples += (lastIndex-startIndex+1);

    register int32_t lastInvalidEntry = startIndex-1;
    register BasicBlockInfo* previousBlock = NULL;
    register int32_t firstRecordForBB = 0;
#ifdef SHIFT_ADDRESS_BUFFER
    register Attribute_t shiftIdx = startIndex;
    entries[lastInvalidEntry].memOpId = startIndex;
#endif // SHIFT_ADDRESS_BUFFER
    for(i = startIndex;i <= lastIndex; i++){
        register BufferEntry* currentEntry = (entries + i);
        register BasicBlockInfo* currentBlock = (blocks + currentEntry->blockId);
        register Attribute_t currentMemOp = currentEntry->memOpId;

        if(!currentMemOp || (currentBlock != previousBlock)){
            currentBlock->visitCount++;
            previousBlock = currentBlock;
            firstRecordForBB = 1;
        }
#ifdef SHIFT_ADDRESS_BUFFER
        if(currentBlock->visitCount <= __MAXIMUM_BLOCK_VISIT){
            if(i != shiftIdx){
                entries[shiftIdx] = entries[i];
            }
            shiftIdx++;
            currentBlock->sampleCount++;
            if(currentBlock->visitCount == __MAXIMUM_BLOCK_VISIT){
                currentBlock->saturationPoint = totalNumberOfAccesses;
                if (firstRecordForBB) saturatedBlockCount++;
#ifdef ENABLE_INSTRUMENTATION_KILL
                if (firstRecordForBB){
                    if (!blockIsKilled[currentEntry->blockId]){
                        blockIsKilled[currentEntry->blockId] = 1;
                        disableInstrumentationPointsInBlock(currentEntry);
                    }
                }
#endif // ENABLE_INSTRUMENTATION_KILL
            }
        }
#else // !SHIFT_ADDRESS_BUFFER
        if(currentBlock->visitCount > __MAXIMUM_BLOCK_VISIT){
            currentEntry->address = INVALID_ADDRESS;
            if(lastInvalidEntry < 0){
                lastInvalidEntry = i;
            }
#ifdef ENABLE_INSTRUMENTATION_KILL
            if (!blockIsKilled[currentEntry->blockId]){
                assert(0 && "fatal: instrumentation should be killed when visitcount == MAX_VISIT");
            }
#endif // ENABLE_INSTRUMENTATION_KILL
        } else {
            //PRINT_INSTR(stdout, "current entry: %d %d %#llx", currentEntry->blockId, currentEntry->memOpId, currentEntry->address);
            if(currentEntry->address == INVALID_ADDRESS){
                assert(0 && "Fatal: Dangerous for assumption that addr can not be 0");
            }
            currentBlock->sampleCount++;
            //PRINT_INSTR(stdout, "updating sample count in block %d to %lld", currentEntry->blockId, currentBlock->sampleCount);
            if(lastInvalidEntry > -1){
                entries[lastInvalidEntry].memOpId = i;
                lastInvalidEntry = -1;
            }
            if(currentBlock->visitCount == __MAXIMUM_BLOCK_VISIT){
                currentBlock->saturationPoint = totalNumberOfAccesses;
                saturatedBlockCount++;
#ifdef ENABLE_INSTRUMENTATION_KILL
                if (!blockIsKilled[currentEntry->blockId]){
                    blockIsKilled[currentEntry->blockId] = 1;
                    disableInstrumentationPointsInBlock(currentEntry);
                }
#endif // ENABLE_INSTRUMENTATION_KILL
            }
        }
#endif // SHIFT_ADDRESS_BUFFER
    }
    if(lastInvalidEntry > -1){
        entries[lastInvalidEntry].memOpId = i;
    }

    if (DUMPCODE_HASVALUE_SIMU(dumpCode)){
        processSamples_Simulate(entries,startIndex,lastIndex);
    }
    if (DUMPCODE_HASVALUE_DUMP(dumpCode)){
        processSamples_StreamDump(entries,startIndex,lastIndex);
    }

    entries->lastFreeIdx = 1;
    //        PRINT_INSTR(stdout, "leaving sim function 4-- startIndex %d, lastIndex %d", startIndex, entries->lastFreeIdx); 
#endif
}

float getPercentage(Counter_t val1,Counter_t val2){
    if(!val2){
        return 0.0;
    }
    return ((100.0*val1)/val2);
}

float getHitPercentage(BasicBlockInfo* block,uint32_t sys,uint32_t lvl){
    Counter_t total = block->hitMissCounters[STATUS_IDX(sys,lvl,cache_hit)] + 
                      block->hitMissCounters[STATUS_IDX(sys,lvl,cache_miss)];
    return getPercentage(block->hitMissCounters[STATUS_IDX(sys,lvl,cache_hit)],total);
}

int rangeSortFunc(const void* a, const void* b){
    DFPatternRange* arg1 = (DFPatternRange*)a;
    DFPatternRange* arg2 = (DFPatternRange*)b;
    if(arg1->minAddress < arg2->minAddress){
        return -1;
    } else if(arg1->minAddress > arg2->minAddress){
        return 1;
    } 
    return 0;
}

void printDFPatternInfo(int blockSeq,FILE* dfpFp,BasicBlockInfo* bb){

    int validRCnt = 0, i = 0;

    if(!dfpFp || !dfPatterns){
        return;
    }

    DFPatternInfo* dfpInfo = dfPatterns + blockSeq;
    if(bb != dfpInfo->basicBlock){
        assert (0 && "Something wrong about block associations");
    }

    if((dfpInfo->type == dfTypePattern_None) ||
       (dfpInfo->type >= dfTypePattern_Total_Types)){
        return;
    }

    fprintf(dfpFp,"block\t%d\t%s\t%d\n",
                  blockSeq,DFPatternTypeNames[dfpInfo->type],dfpInfo->rangeCnt);

    if(dfpInfo->ranges){
        validRCnt = 0;
        for(i=0;i<dfpInfo->rangeCnt;i++){
            if(dfpInfo->ranges[i].minAddress > dfpInfo->ranges[i].maxAddress){
                continue;
            }
#ifdef DEBUG_RUN_1
#ifdef METASIM_32_BIT_LIB
            fprintf(dfpFp,"\tXrange\t0x%x\t0x%x\n",
#else
            fprintf(dfpFp,"\tXrange\t0x%llx\t0x%llx\n",
#endif
                dfpInfo->ranges[i].minAddress,dfpInfo->ranges[i].maxAddress);
#endif
            dfpInfo->ranges[validRCnt++] = dfpInfo->ranges[i];
        }

        qsort(dfpInfo->ranges,validRCnt,sizeof(DFPatternRange),rangeSortFunc);
        for(i=0;i<(validRCnt-1);i++){
            Address_t nextStart =  dfpInfo->ranges[i+1].minAddress;
            if((dfpInfo->ranges[i].minAddress <= nextStart) && 
               (nextStart <= dfpInfo->ranges[i].maxAddress)){
               dfpInfo->ranges[i+1].minAddress = dfpInfo->ranges[i].minAddress;
               dfpInfo->ranges[i+1].maxAddress = MAX_OF(dfpInfo->ranges[i].maxAddress,
                                                        dfpInfo->ranges[i+1].maxAddress);
               dfpInfo->ranges[i].minAddress = (Address_t)-1;
               dfpInfo->ranges[i].maxAddress = 0;
            }
        }
        for(i=0;i<validRCnt;i++){
            if(dfpInfo->ranges[i].minAddress > dfpInfo->ranges[i].maxAddress){
                continue;
            }
#ifdef METASIM_32_BIT_LIB
            fprintf(dfpFp,"\trange\t0x%x\t0x%x\n",
#else
            fprintf(dfpFp,"\trange\t0x%llx\t0x%llx\n",
#endif
                dfpInfo->ranges[i].minAddress,dfpInfo->ranges[i].maxAddress);
        }
    }
}

void MetaSim_endFuncCall_Simu(char* base,uint32_t* entryCountPtr,const char* comment){
#ifdef IGNORE_ACTUAL_SIMULATION
    BufferEntry* entries = (BufferEntry*)base;
    return;
#else
    uint32_t      i;
    uint32_t      j;
    uint32_t      k;

    BufferEntry* entries = (BufferEntry*)base;
    uint32_t lastIndex = entries->lastFreeIdx;
    lastIndex--;

    PRINT_INSTR(stdout,"MetaSim_endFuncCall(0x%p,%d,%s,%d)",base,*entryCountPtr,comment,entries->lastFreeIdx);

#ifdef ENABLE_INSTRUMENTATION_KILL
    PRINT_INSTR(stdout, "Killed instrumentation in %d memory ops", numberKilled);
#endif
#ifdef DEBUG_RUN_1
    PRINT_INSTR(stdout,"HashSearches(%lld) Traversals(%lld)",howManyHashSearches,howManyHashTraverses);
#endif

    MetaSim_simulFuncCall_Simu(base,entryCountPtr,comment);

    if (DUMPCODE_HASVALUE_DUMP(dumpCode)){
        PRINT_INSTR(stdout, "Closing output file for address stream dump");
        fclose(streamDumpOutputFile);
    }
    if (!DUMPCODE_HASVALUE_SIMU(dumpCode)){
        return;
    }

    Counter_t processedSampleCount = 0;
    FILE* dfpFp = NULL;
    FILE* fp = openOutputFile(comment,&dfpFp);
    if(fp){
        BasicBlockInfo* currentBlock = NULL;
        for(i=0;i<blockCount;i++){
            currentBlock = (blocks + i);
            if(blocks && currentBlock->sampleCount){
                processedSampleCount += currentBlock->sampleCount;
            }
        }
        fprintf(fp,"# buffer    = %d\n",*entryCountPtr);              
        fprintf(fp,"# total     = %lld\n",totalNumberOfAccesses); 
        fprintf(fp,"# sampled   = %lld\n",totalNumberOfSamples);  
        fprintf(fp,"# processed = %lld\n",processedSampleCount);  
        fprintf(fp,"# saturate  = %lld\n",lastSaturationAccess); 

        if(dfpFp) {
            fprintf(dfpFp,"# buffer    = %d\n",*entryCountPtr);
            fprintf(dfpFp,"# total     = %lld\n",totalNumberOfAccesses);
            fprintf(dfpFp,"# sampled   = %lld\n",totalNumberOfSamples);
            fprintf(dfpFp,"# processed = %lld\n",processedSampleCount);
        }

#ifdef NO_SAMPLING_MODE
        fprintf(fp,"# sampling  = no\n");                         
#else
        fprintf(fp,"# sampling  = yes\n");                        
#ifdef EXTENDED_SAMPLING
        fprintf(fp,"# extended-sampling  = yes (intervals are averages)\n");
#endif
        fprintf(fp,"# ignoreint = %lld\n",(uint64_t)__IGNORING_INTERVAL_MAX);
        fprintf(fp,"# sampleint = %lld\n",(uint64_t)__SAMPLING_INTERVAL_MAX);
#endif

        fprintf(fp,"# maxvisits = %lld\n",(uint64_t)__MAXIMUM_BLOCK_VISIT);

#ifdef PER_SET_RECENT
        fprintf(fp,"# recentptr = per set\n");
#else
        fprintf(fp,"# recentptr = per cache\n");
#endif
        fprintf(fp,"#\n");
        for(j=0;j<systemCount;j++){
            MemoryHierarchy* memoryHierarchy = (systems + j);
            fprintf(fp,"# sysid%d =",memoryHierarchy->index);
            for(k=0;k<memoryHierarchy->levelCount;k++){
                Cache* cache = &(memoryHierarchy->levels[k]);
                Counter_t total = cache->hitMissCounters[cache_hit]+cache->hitMissCounters[cache_miss];
                fprintf(fp," l%1d[%lld,%lld(%5.2f)]", k,
                        total,
                        cache->hitMissCounters[cache_hit],
                        getPercentage(cache->hitMissCounters[cache_hit],total));
            }
            fprintf(fp,"\n");
        }

        fprintf(fp,"#\n");

        if(dfpFp){
            fprintf(dfpFp,"#block <seqid> <idiom> <rangecnt>\n");
            fprintf(dfpFp,"#range <ranid> <minaddress> <maxaddress>\n");
        }

        fprintf(fp,"#block <seqid> <visitcount> <samplecount>\n");
        fprintf(fp,"#\tsys <sysid> lvl <cachelvl> <hitcount> <miscount> <hitpercent>\n");
        fprintf(fp,"#\tsaturation <satpercent>\n");

        for(i=0;i<blockCount;i++){
            currentBlock = (blocks + i);
            if(blocks && currentBlock->sampleCount){

                fprintf(fp,"block\t%d\t%llu\t%lld\t%lld\n",i,blockCounters[i],currentBlock->visitCount,currentBlock->sampleCount);

                for(j=0;j<systemCount;j++){
                    MemoryHierarchy* memoryHierarchy = (systems + j);
                    for(k=0;k<memoryHierarchy->levelCount;k++){
                        Cache* cache = &(memoryHierarchy->levels[k]);
                        Counter_t total = currentBlock->hitMissCounters[STATUS_IDX(j,k,cache_hit)] + 
                                          currentBlock->hitMissCounters[STATUS_IDX(j,k,cache_miss)];
                        fprintf(fp,"\tsys\t%3d\tlvl\t%1d\t%lld\t%lld\t%10.6f",
                            memoryHierarchy->index,k+1,
                            currentBlock->hitMissCounters[STATUS_IDX(j,k,cache_hit)],
                            currentBlock->hitMissCounters[STATUS_IDX(j,k,cache_miss)],
                            getHitPercentage(currentBlock,j,k));
                        fprintf(fp,"\n");
                    }
                }

                if(!currentBlock->saturationPoint){
                    fprintf(fp,"\tsaturation\t%5.2f\n",100.0);
                } else {
                    fprintf(fp,"\tsaturation\t%5.2f\n",getPercentage(currentBlock->saturationPoint,
                                                                   totalNumberOfAccesses));
                }
            }
            if(dfpFp){
                printDFPatternInfo(i,dfpFp,currentBlock);
            }
        }

        fclose(fp);

    }
#endif
}






/*** more efficient implementation of (uint32_t)(log2((double)i)) ***/

#define DFPATTERN_BIN_COUNT 6
uint32_t findMostSigOne(uint32_t value){
    if(!value){
        return 0;
    }
    uint32_t index = 0;
    uint32_t count = 0;
    if((value >> 16) & 0xffff){
        value = (value >> 16) & 0xffff;
        index = 32;
        count = 16;
    } else if((value >> 8) & 0xff){
        value = (value >> 8) & 0xff;
        index = 16;
        count = 8;
    } else if((value >> 4) & 0xf){
        value = (value >> 4) & 0xf;
        index = 8;
        count = 4;
    } else {
        value = value & 0xf;
        index = 4;
        count = 4;
    }
    do {
        --index;
        if((value >> (--count)) & 0x1){
            break;
        }
    } while(count);

    return (index <= DFPATTERN_BIN_COUNT ? index : DFPATTERN_BIN_COUNT);
}

#define BUCKET_MULTIPLE_BASE 0x100000
int32_t findRangeBucket(Address_t first,Address_t next){
    Address_t diff = next - first;
    uint32_t  direction = 1;

    if(first > next){
        direction = 0;
        diff = first - next;
    } 

    diff /= BUCKET_MULTIPLE_BASE;
    int32_t retValue = findMostSigOne((uint32_t)diff);
    if(!direction){
        retValue = retValue * -1;
    }
    return retValue;
}
