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

#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

 #include <LinkedList.h>

#define ENTRY_FUNCTION "tool_image_init"
#define SIM_FUNCTION "process_buffer"
#define EXIT_FUNCTION "tool_image_fini"
#define INST_LIB_NAME "libsimulator.so"

#define NOSTRING "__pebil_no_string__"
#define BUFFER_ENTRIES 0x10000

extern "C" {
    InstrumentationTool* CacheSimulationMaker(ElfFile* elf){
        return new CacheSimulation(elf);
    }
}

/*
 * Read input file and create a list of blocks to instrument
 * initializes blocksToInst
 */
void CacheSimulation::filterBBs(){
    Vector<char*>* fileLines = new Vector<char*>();
    if (!strcmp("+", inputFile)){
        for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
            BasicBlock* bb = getExposedBasicBlock(i);
            blocksToInst.insert(bb->getHashCode().getValue(), bb);
        }
    } else {

        initializeFileList(inputFile, fileLines);

        for (uint32_t i = 0; i < (*fileLines).size(); i++){
            char* ptr = strchr((*fileLines)[i],'#');
            if(ptr) *ptr = '\0';

            if(!strlen((*fileLines)[i]) || allSpace((*fileLines)[i]))
                continue;

            int32_t err;
            uint64_t inputHash = 0;
            uint64_t imgHash = 0;

            err = sscanf((*fileLines)[i], "%llx %llx", &inputHash, &imgHash);
            if(err <= 0){
                PRINT_ERROR("Line %d of %s has a wrong format", i+1, inputFile);
            }

            // First number is a blockhash
            HashCode* hashCode = new HashCode(inputHash);

            // Second number, if present, is image id
            if(err == 2 && getElfFile()->getUniqueId() != imgHash)
                continue;

            BasicBlock* bb = findExposedBasicBlock(*hashCode);
            delete hashCode;

            printf("\n\t Exploring BB 0x%012llx \n",bb->getHashCode().getValue());

            if (!bb){
                PRINT_WARN(10, "cannot find basic block for hash code %#llx found in input file", inputHash);
            } else {
                //        ASSERT(bb && "cannot find basic block for hash code found in input file");
                blocksToInst.insert(bb->getHashCode().getValue(), bb);

                // also include any block that is in this loop (including child loops)
                uint64_t topLoopID;
                if (loopIncl){
                    if (bb->isInLoop()){

                        // For now use the BB-ID of top-most loop as hash-key of the group. Should change this by generating a new hash.
                        SimpleHash<Loop*> loopsToCheck;
                        Vector<Loop*> loopsVec;
                        Vector<uint64_t> BB_NestedLoop;

                        FlowGraph* fg = bb->getFlowGraph();
                        Loop* lp = fg->getInnermostLoopForBlock(bb->getIndex());
                        BasicBlock** allBlocks = new BasicBlock*[lp->getNumberOfBlocks()];
                        lp->getAllBlocks(allBlocks);
                        
                        BasicBlock* HeadBB=lp->getHead();
                        BasicBlock* TailBB=lp->getTail();

                        topLoopID=HeadBB->getHashCode().getValue();

                        printf ("\t\t BB->HashCode(): 0x%012llx Head-loop: 0x%012llx TailLoop: 0x%012llx \n",bb->getHashCode(),HeadBB->getHashCode(),TailBB->getHashCode());
                        if(!loopsToCheck.exists(topLoopID,lp)){    
                            loopsToCheck.insert(topLoopID,lp);
                            loopsVec.insert(lp, loopsVec.size() ) ;
                        }

                        for (uint32_t k = 0; k < lp->getNumberOfBlocks(); k++){
                            uint64_t code = allBlocks[k]->getHashCode().getValue();
                            BB_NestedLoop.insert(allBlocks[k]->getHashCode().getValue(),k);
                            FlowGraph* FgInner = allBlocks[k]->getFlowGraph();
                            Loop* LpInner = FgInner->getInnermostLoopForBlock(allBlocks[k]->getIndex());

                            BasicBlock* HeadBB=LpInner->getHead();
                            BasicBlock* TailBB=LpInner->getTail();

                            printf ("\t\t\t BB->HashCode(): 0x%012llx Head-loop: 0x%012llx TailLoop: 0x%012llx \n",allBlocks[k]->getHashCode(),HeadBB->getHashCode(),TailBB->getHashCode());    

                            if(!loopsToCheck.exists(HeadBB->getHashCode().getValue(),LpInner))
                            {
                                loopsToCheck.insert(HeadBB->getHashCode().getValue(),LpInner);
                                loopsVec.insert(LpInner, loopsVec.size() ) ;
                            }

                            blocksToInst.insert(code, allBlocks[k]);
                        }
                        
                        printf("\t #Loops in the hash table is %d and loopsVec.Size(): %d BB_NestedLoop.Size(): %d \n",loopsToCheck.size(),loopsVec.size(),BB_NestedLoop.size());
                        Vector<Vector<Loop*>*> BBStruct;
                        Vector<Loop*>* FirstLevelNode=new Vector<Loop*>; // Could this cause a memory leak?                     
                        Vector<uint64_t>* tmpInnermostBasicBlocksForGroup; 
                        tmpInnermostBasicBlocksForGroup=new Vector<uint64_t>;
                        if(loopsVec.size()>1){
                            for(uint32_t i=0;i<(loopsVec.size()-1);i++) 
                            {
                                FirstLevelNode->insert(loopsVec[i],i);
                                printf(" i: %d Node-BB: 0x%012llx \n",i,(*FirstLevelNode)[i]->getHead()->getHashCode().getValue());
                            }

                            BBStruct.insert(FirstLevelNode,0);
                            printf("\t Size of BBStruct is: %d \n",BBStruct.size());

                            
                            for(uint32_t BBStructIdx=0; BBStructIdx<BBStruct.size();BBStructIdx++){
                                printf("\n Entering next iteration and size of BBStruct is: %d \n",BBStruct.size());
                                Vector<Loop*>* currLoopVec=BBStruct[BBStructIdx];
                                Vector<Loop*>* nextLoopVec=new Vector<Loop*>;
                                uint32_t numLoopsNextLevel=0; 

                                for(uint32_t i=0;i<currLoopVec->size();i++){
                                    printf("\t CurrLoop is 0x%012llx \n",(*currLoopVec)[i]->getHead()->getHashCode().getValue());
                                }

                                for(uint32_t i=0;i<currLoopVec->size();i++){
                                    printf("\t CurrOuterLoop is 0x%012llx \n",(*currLoopVec)[i]->getHead()->getHashCode().getValue());
                                    for(uint32_t j=0;j<currLoopVec->size();j++){
                                        printf("\t\t CurrInnerLoop is 0x%012llx \n",(*currLoopVec)[j]->getHead()->getHashCode().getValue());
                                        if( (*currLoopVec)[j]->isInnerLoopOf((*currLoopVec)[i]) ){
                                            if(! ( (*currLoopVec)[j]->isIdenticalLoop( (*currLoopVec)[i] ) ) ){
                                                
                                                printf("\t\t\t  Loop 0x%012llx is inner loop of 0x%012llx \n",(*currLoopVec)[j]->getHead()->getHashCode().getValue(),(*currLoopVec)[i]->getHead()->getHashCode().getValue() );
                                                nextLoopVec->insert((*currLoopVec)[j],numLoopsNextLevel);
                                                numLoopsNextLevel++;
                                                currLoopVec->remove(j);
                                            } // else do nothing since its the same loop.
                                        }
                                    }
                                }

                                if( nextLoopVec->size() > 0 ){ 
                                    BBStruct.insert(nextLoopVec,BBStructIdx+1);
                                    printf(" Boo yeah added new node-since nextLoopVec->size() is : %d and BBStruct-size: %d \n",nextLoopVec->size(),BBStruct.size());
                                }
                                printf("\n Entering next iteration and size of BBStruct is: %d and nextLoopVec's size is: %d \n",BBStruct.size(),nextLoopVec->size());
                            }

                            for(uint32_t BBStructIdx=( BBStruct.size()-1); BBStructIdx< BBStruct.size(); BBStructIdx++){
                                for(uint32_t i=0;i< (*BBStruct[BBStructIdx]).size(); i++){
                                    Loop* lp = (*BBStruct[BBStructIdx])[i];
                                    printf("\t --#-- Current loop's ID is: 0x%012llx \n",lp->getHead()->getHashCode().getValue());
                                        
                                    BasicBlock** allBlocks = new BasicBlock*[lp->getNumberOfBlocks()];
                                    lp->getAllBlocks(allBlocks);    
                                    for(uint32_t k = 0; k < lp->getNumberOfBlocks(); k++){
                                        uint64_t code = allBlocks[k]->getHashCode().getValue();
                                        printf("\t\t Code is: 0x%012llx\n", code); 
                                        tmpInnermostBasicBlocksForGroup->insert(code,tmpInnermostBasicBlocksForGroup->size());
                                    }
                                }                                
                            }
                        }
                        else if(loopsVec.size()==1){ // since its possible to have only one loop. Should skip "searching for next-level BBs"                      
                            printf("\t Boo yeah here at loopsVec.size==1 \n ");
                            Loop* lp=loopsVec[0];
                            printf("\t --#-- Current loop's ID is: 0x%012llx \n",lp->getHead()->getHashCode().getValue());
                                        
                            BasicBlock** allBlocks = new BasicBlock*[lp->getNumberOfBlocks()];
                            lp->getAllBlocks(allBlocks);    
                            for(uint32_t k = 0; k < lp->getNumberOfBlocks(); k++){
                                uint64_t code = allBlocks[k]->getHashCode().getValue();
                                printf("\t\t Code is: 0x%012llx\n", code); 
                                tmpInnermostBasicBlocksForGroup->insert(code,tmpInnermostBasicBlocksForGroup->size());
                            }
                        }                            

                        printf("\n\t So the inner most BBs for this Group < Yo find me a Group-ID, cant live without a hash forever!! > \n");
                        uint64_t* innermostBasicBlocksForGroup;
                        innermostBasicBlocksForGroup= new uint64_t[tmpInnermostBasicBlocksForGroup->size()];
                        for(uint32_t i=0;i<tmpInnermostBasicBlocksForGroup->size();i++){
                            printf("\t\t 0x%012llx \n",(*tmpInnermostBasicBlocksForGroup)[i]);
                            innermostBasicBlocksForGroup[i] = (*tmpInnermostBasicBlocksForGroup)[i];
                        }

                        NestedLoopStruct* currLoopStats = new NestedLoopStruct;
                        currLoopStats->GroupId = topLoopID;
                        currLoopStats->InnerLevelSize = tmpInnermostBasicBlocksForGroup->size();
                        currLoopStats->GroupCount = 0;
                        currLoopStats->InnerLevelBasicBlocks = innermostBasicBlocksForGroup;
                        nestedLoopGrouping.insert(topLoopID,currLoopStats);

                        for(uint32_t i=0; i < BB_NestedLoop.size(); i++){
                            printf("\t Added 0x%012llx to mapBB hashing! \n",BB_NestedLoop[i]);
                            mapBBToGroupId.insert(BB_NestedLoop[i],topLoopID);
                        }

                        // TODO: Should I delete the hashes/vectors used for book keeping of figuring out loop structure ?
                        delete[] allBlocks; 
                       // delete MainNode;
                    }else{
                        uint64_t* innermostBasicBlocksForGroup;
                        innermostBasicBlocksForGroup=new uint64_t;  
                        *innermostBasicBlocksForGroup=(bb->getHashCode().getValue()); // Since we know this is only a BB, just adding this BB.
                        NestedLoopStruct* currLoopStats = new NestedLoopStruct;
                        currLoopStats->GroupId = (bb->getHashCode().getValue());
                        currLoopStats->InnerLevelSize = 1;
                        currLoopStats->GroupCount = 0;
                        currLoopStats->InnerLevelBasicBlocks = innermostBasicBlocksForGroup;
                        printf("\n\t ***** Muhaha just now added a BB to the hash for nestedloop-group..... \n");
                        nestedLoopGrouping.insert(bb->getHashCode().getValue(),currLoopStats); // Only 1 BB, so third term is 1.
                        mapBBToGroupId.insert(bb->getHashCode().getValue(),bb->getHashCode().getValue()); 
                    }
                }
            }

            // Should add GroupId in either of the cases where BB is included in a loop/not.
        }
        for (uint32_t i = 0; i < (*fileLines).size(); i++){
            delete[] (*fileLines)[i];
        }
        delete fileLines;
    }

    if (!blocksToInst.size()){
        // for executables, instrument everything
        if (getElfFile()->isExecutable()){
            for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
                BasicBlock* bb = getExposedBasicBlock(i);
                blocksToInst.insert(bb->getHashCode().getValue(), bb);
            }
        }
        // for shared libraries, just instrument the entry block
        else {
            BasicBlock* bb = getProgramEntryBlock();
            blocksToInst.insert(bb->getHashCode().getValue(), bb);
        }
    }
}

CacheSimulation::CacheSimulation(ElfFile* elf)
    : InstrumentationTool(elf)
{
    simFunc = NULL;
    exitFunc = NULL;
    entryFunc = NULL;

    ASSERT(isPowerOfTwo(sizeof(BufferEntry)));
}


CacheSimulation::~CacheSimulation(){
}

void CacheSimulation::declare(){
    InstrumentationTool::declare();
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    simFunc = declareFunction(SIM_FUNCTION);
    ASSERT(simFunc && "Cannot find memory print function, are you sure it was declared?");
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");
    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");
}
 
void CacheSimulation::instrument(){
    InstrumentationTool::instrument();
    filterBBs();

    if (dfpFile){
        PRINT_WARN(20, "--dfp is an accepted argument but it does nothing. range finding is done for every block included in the simulation by default");
    }

    uint32_t temp32;
    uint64_t temp64;
    
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    bool usePIC = false;
    if (isThreadedMode() || isMultiImage()){
        usePIC = true;
    }

    // count number of memory ops
    uint32_t memopSeq = 0;
    uint32_t blockSeq = 0;
    std::set<Base*> functionsToInst;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        if (blocksToInst.get(bb->getHashCode().getValue())){
            blockSeq++;
            Function* f = (Function*)bb->getLeader()->getContainer();
            functionsToInst.insert(f);
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);
                if (memop->isMemoryOperation()){
                    memopSeq++;
                }
            }
        }
    }

    Vector<uint64_t> GroupIdsVec; // TODO: Mostly only being used for testing.
    SimpleHash<uint64_t> groupsInitialized;
    SimpleHash<uint64_t> mapBBToIdxOfGroup;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        if( blocksToInst.get(bb->getHashCode().getValue()) ){
            uint64_t myGroupId,myGroupStatsSize;
            if( mapBBToGroupId.get(bb->getHashCode().getValue()) ){
                myGroupId= mapBBToGroupId.getVal(bb->getHashCode().getValue());
                NestedLoopStruct* myNestedLoopStruct= ( nestedLoopGrouping.getVal(myGroupId ) );
                printf("\t For BB: 0x%012llx the GroupId is 0x%012llx and my last-level has %d blocks \n",bb->getHashCode().getValue(),myGroupId,myNestedLoopStruct->InnerLevelSize);

                if(!groupsInitialized.exists(myGroupId,myGroupId)){
                    groupsInitialized.insert(myGroupId,myGroupId);    
                    GroupIdsVec.insert(myGroupId,GroupIdsVec.size());
                    printf("\t\t Adding GroupId 0x%012llx to vec",myGroupId);
                }
                mapBBToIdxOfGroup.insert(bb->getHashCode().getValue(),(GroupIdsVec.size()-1));
            }
        }
    }

    uint64_t noData = reserveDataOffset(strlen(NOSTRING) + 1);
    char* nostring = new char[strlen(NOSTRING) + 1];
    sprintf(nostring, "%s\0", NOSTRING);
    initializeReservedData(getInstDataAddress() + noData, strlen(NOSTRING) + 1, nostring);

    Vector<Base*>* allBlocks = new Vector<Base*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allBlockLineInfos = new Vector<LineInfo*>();

    std::map<uint64_t, ThreadRegisterMap*>* functionThreading;
    if (usePIC){
        functionThreading = threadReadyCode(functionsToInst);
    }

    // first entry in buffer is treated specially
    BufferEntry intro;
    intro.__buf_current = 0;
    intro.__buf_capacity = BUFFER_ENTRIES;

    SimulationStats stats;

    stats.Initialized = true;
    stats.InstructionCount = memopSeq;
    stats.Master = isMasterImage();
    stats.Phase = phaseNo;
    stats.Stats = NULL;
    if (isPerInstruction()){
        stats.PerInstruction = true;
        stats.BlockCount = memopSeq;
    } else {
        stats.PerInstruction = false;
        stats.BlockCount = blockSeq;
    }
    stats.NestedLoopCount = GroupIdsVec.size(); // Amogha edits.

    // allocate Counters and SimulationStats contiguously to avoid an extra memory ref in counter updates
    uint64_t simulationStruct = reserveDataOffset(sizeof(SimulationStats) + (sizeof(uint64_t) * stats.BlockCount));
    stats.Counters = (uint64_t*)(simulationStruct + sizeof(SimulationStats));
    initializeReservedPointer((uint64_t)stats.Counters, simulationStruct + offsetof(SimulationStats, Counters));

    temp32 = BUFFER_ENTRIES + 1;
    stats.Buffer = (BufferEntry*)reserveDataOffset(temp32 * sizeof(BufferEntry));
    initializeReservedData(getInstDataAddress() + (uint64_t)stats.Buffer, sizeof(BufferEntry), &intro);

    initializeReservedPointer((uint64_t)stats.Buffer, simulationStruct + offsetof(SimulationStats, Buffer));

#define INIT_INSN_ELEMENT(__typ, __nam)\
    stats.__nam = (__typ*)reserveDataOffset(stats.InstructionCount * sizeof(__typ));  \
    initializeReservedPointer((uint64_t)stats.__nam, simulationStruct + offsetof(SimulationStats, __nam))

    INIT_INSN_ELEMENT(uint64_t, BlockIds);
    INIT_INSN_ELEMENT(uint64_t, MemopIds);


#define INIT_BLOCK_ELEMENT(__typ, __nam)\
    stats.__nam = (__typ*)reserveDataOffset(stats.BlockCount * sizeof(__typ));  \
    initializeReservedPointer((uint64_t)stats.__nam, simulationStruct + offsetof(SimulationStats, __nam))

    INIT_BLOCK_ELEMENT(CounterTypes, Types);
    INIT_BLOCK_ELEMENT(uint32_t, MemopsPerBlock);
    INIT_BLOCK_ELEMENT(uint64_t, Addresses);
    INIT_BLOCK_ELEMENT(uint64_t, Hashes);
    INIT_BLOCK_ELEMENT(uint32_t, Lines);
    INIT_BLOCK_ELEMENT(char*, Files);
    INIT_BLOCK_ELEMENT(char*, Functions);
    INIT_BLOCK_ELEMENT(uint64_t, GroupIds);

    char* appName = getElfFile()->getAppName();
    uint64_t app = reserveDataOffset(strlen(appName) + 1);
    initializeReservedPointer(app, simulationStruct + offsetof(SimulationStats, Application));
    initializeReservedData(getInstDataAddress() + app, strlen(appName) + 1, (void*)appName);

    char extName[__MAX_STRING_SIZE];
    sprintf(extName, "%s\0", getExtension());
    uint64_t ext = reserveDataOffset(strlen(extName) + 1);
    initializeReservedPointer(ext, simulationStruct + offsetof(SimulationStats, Extension));
    initializeReservedData(getInstDataAddress() + ext, strlen(extName) + 1, (void*)extName);

    stats.TestArray = (uint64_t*) reserveDataOffset(stats.NestedLoopCount * sizeof(uint64_t));
    initializeReservedPointer((uint64_t)stats.TestArray, simulationStruct + offsetof(SimulationStats,TestArray));

    stats.NLStats = (NestedLoopStruct*) reserveDataOffset(stats.NestedLoopCount * sizeof(NestedLoopStruct));
    initializeReservedPointer((uint64_t)stats.NLStats, simulationStruct + offsetof(SimulationStats,NLStats));    

    SimpleHash<uint32_t> mapBBToArrayIdx;
    for(uint32_t i=0;i < GroupIdsVec.size() ; i++){
        uint64_t myGroupId = GroupIdsVec[i];

        NestedLoopStruct* myNestedLoopStruct = ( nestedLoopGrouping.getVal(myGroupId ) );
        printf("\t\t Initializing GroupId 0x%012llx in TestArray and it's actual instance has %d blocks at lower level while instance's size is %d \n",myGroupId,myNestedLoopStruct->InnerLevelSize);
        uint64_t currNestLoopStatsInstance = ( (uint64_t) stats.NLStats + ( i * sizeof(NestedLoopStruct) ) ) ; 
        
        uint64_t currGroupId = myNestedLoopStruct->GroupId; // Mostly dont need this since GroupIds are already stored!! 
        initializeReservedData( getInstDataAddress() + currNestLoopStatsInstance + offsetof(NestedLoopStruct,GroupId) , sizeof(uint64_t) , (void*) ( &currGroupId) );
        uint64_t temp= getInstDataAddress() + currNestLoopStatsInstance ;
        printf("\n\t\t ---- currGroupId is 0x%012llx and address of NLStats[i]: 0x%012llx NLStats[i].GroupID: 0x%012llx NLStats[i].InnerLevelSize: 0x%012llx NLStats[i].InnerLevelBasicBlocks: 0x%012llx \n ",myGroupId,temp,(temp + offsetof(NestedLoopStruct,GroupId)),(temp + offsetof(NestedLoopStruct,InnerLevelSize)),(temp + offsetof(NestedLoopStruct,InnerLevelBasicBlocks)));

        uint64_t tmpCount=0;
        initializeReservedData( getInstDataAddress() + currNestLoopStatsInstance + offsetof(NestedLoopStruct,GroupCount) , sizeof(uint64_t) , (void*) ( &tmpCount) );
        
        uint64_t currInnerLevelSize = myNestedLoopStruct->InnerLevelSize; // Mostly dont need this since GroupIds are already stored!! 
        initializeReservedData( getInstDataAddress() + currNestLoopStatsInstance + offsetof(NestedLoopStruct,InnerLevelSize) , sizeof(uint32_t) , (void*) ( &currInnerLevelSize) );        
        initializeReservedData(getInstDataAddress() + (uint64_t)stats.TestArray + i * sizeof(uint64_t), sizeof(uint64_t),(void*)(&myGroupId));        
    }
    initializeReservedData(getInstDataAddress() + simulationStruct, sizeof(SimulationStats), (void*)(&stats));

    // Add arguments to instrumentation functions
    entryFunc->addArgument(simulationStruct);
    entryFunc->addArgument(imageKey);
    entryFunc->addArgument(threadHash);

    simFunc->addArgument(imageKey);

    exitFunc->addArgument(imageKey);


    InstrumentationPoint* p;

    // Setup the entry point(s) for this image to call entryFunc to do initialization
    if (isMultiImage()){
        for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
            Function* f = getExposedFunction(i);

            p = addInstrumentationPoint(f, entryFunc, InstrumentationMode_tramp, InstLocation_prior);
            ASSERT(p);
            p->setPriority(InstPriority_sysinit);
            if (!p->getInstBaseAddress()){
                PRINT_ERROR("Cannot find an instrumentation point at the entry function");
            }            

            dynamicPoint(p, getElfFile()->getUniqueId(), true);
        }
    } else {
        p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
        ASSERT(p);
        p->setPriority(InstPriority_sysinit);
        if (!p->getInstBaseAddress()){
            PRINT_ERROR("Cannot find an instrumentation point at the entry function");
        }
    }

    // TODO: remove all FP work from cache simulation?
    //simFunc->assumeNoFunctionFP();

    // Instrument exit block to call exitFunc
    p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_sysinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    uint64_t imageHash = getElfFile()->getUniqueId();

    /* Begin instrumenting each block in the function */
    blockSeq = 0;
    memopSeq = 0;
    uint32_t currentLeader = 0;
    uint32_t countBBInstrumented = 0;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        Function* f = (Function*)bb->getLeader()->getContainer();

        uint32_t threadReg = X86_REG_INVALID;
        if (usePIC){
            ThreadRegisterMap* threadMap = (*functionThreading)[f->getBaseAddress()];
            threadReg = threadMap->getThreadRegister(bb);
        }

        // Check if we should skip this block
        if (!blocksToInst.get(bb->getHashCode().getValue()))
            continue;

        if (!isPerInstruction()){
            LineInfo* li = NULL;
            if (lineInfoFinder){
                li = lineInfoFinder->lookupLineInfo(bb);
            }
        
            (*allBlocks).append(bb);
            (*allBlockIds).append(i);
            (*allBlockLineInfos).append(li);

            if (li){
                uint32_t line = li->GET(lr_line);
                initializeReservedData(getInstDataAddress() + (uint64_t)stats.Lines + sizeof(uint32_t)*blockSeq, sizeof(uint32_t), &line);

                uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
                initializeReservedPointer(filename, (uint64_t)stats.Files + blockSeq*sizeof(char*));
                initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());
            } else {
                temp32 = 0;
                initializeReservedData(getInstDataAddress() + (uint64_t)stats.Lines + sizeof(uint32_t)*blockSeq, sizeof(uint32_t), &temp32);
                initializeReservedPointer(noData, (uint64_t)stats.Files + blockSeq*sizeof(char*));
            }

            uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
            initializeReservedPointer(funcname, (uint64_t)stats.Functions + blockSeq*sizeof(char*));
            initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

            uint64_t hashValue = bb->getHashCode().getValue();
            uint64_t addr = bb->getProgramAddress();        
            uint64_t groupId = mapBBToIdxOfGroup.getVal(hashValue);
            mapBBToArrayIdx.insert(hashValue,countBBInstrumented);

            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Hashes + blockSeq*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Addresses + blockSeq*sizeof(uint64_t), sizeof(uint64_t), &addr);
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.GroupIds + blockSeq*sizeof(uint64_t),sizeof(uint64_t), (void*) &groupId);

            CounterTypes tmpct = CounterType_basicblock;
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Types + blockSeq*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);

            temp64 = 0;
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Counters + blockSeq*sizeof(uint64_t), sizeof(uint64_t), &temp64);

            temp32 = bb->getNumberOfMemoryOps();
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopsPerBlock + blockSeq*sizeof(uint32_t), sizeof(uint32_t), &temp32);
            countBBInstrumented+=1;
        }

        uint32_t memopIdInBlock = 0;
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* memop = bb->getInstruction(j);
            uint64_t currentOffset = (uint64_t)stats.Buffer + offsetof(BufferEntry, __buf_current);

            if (usePIC){
                currentOffset -= (uint64_t)stats.Buffer;
            }

            if (memop->isMemoryOperation()){
                // at the first memop in each block, check for a full buffer, clear if full
                if (memopIdInBlock == 0){

                    // set up a block counter that is distinct from all other inst points in the block
                    if (!isPerInstruction()){
                        uint64_t counterOffset = (uint64_t)stats.Counters + (blockSeq * sizeof(uint64_t));

                        if (usePIC){
                            counterOffset -= simulationStruct;
                        }
                        InstrumentationTool::insertBlockCounter(counterOffset, bb, true, threadReg);
                    }

                    // grab 2 scratch registers
                    uint32_t sr1 = X86_REG_INVALID, sr2 = X86_REG_INVALID;
                    
                    BitSet<uint32_t>* inv = new BitSet<uint32_t>(X86_ALU_REGS);
                    inv->insert(X86_REG_AX);
                    inv->insert(X86_REG_SP);
                    inv->insert(X86_REG_BP);
                    if (threadReg != X86_REG_INVALID){
                        inv->insert(threadReg);
                        sr1 = threadReg;
                    }
                    for (uint32_t k = X86_64BIT_GPRS; k < X86_ALU_REGS; k++){
                        inv->insert(k);
                    }
                    BitSet<uint32_t>* dead = memop->getDeadRegIn(inv, 2);
                    ASSERT(dead->size() >= 2);

                    for (uint32_t k = 0; k < X86_64BIT_GPRS; k++){
                        if (dead->contains(k)){
                            if (sr1 == X86_REG_INVALID){
                                sr1 = k;
                            } else if (sr2 == X86_REG_INVALID){
                                sr2 = k;
                                break;
                            }
                        }
                    }
                    ASSERT(sr1 != X86_REG_INVALID && sr2 != X86_REG_INVALID);

                    delete inv;
                    delete dead;
                    InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_tramp, InstLocation_prior);
                    pt->setPriority(InstPriority_userinit);
                    dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_buffercheck), true);
                    Vector<X86Instruction*>* bufferDumpInstructions = new Vector<X86Instruction*>();

                    // put current buffer into sr2
                    // if thread data addr is not in sr1 already, load it
                    if (threadReg == X86_REG_INVALID && usePIC){
                        Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
                        for (uint32_t k = 0; k < tdata->size(); k++){
                            bufferDumpInstructions->append((*tdata)[k]);
                        }
                        delete tdata;
                    }
                    
                    if (usePIC){
                        // sr2 =((SimulationStats)sr1)->Buffer
                        bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
                    } else {
                        // sr2 = stats.Buffer
                        //bufferDumpInstructions->append(X86InstructionFactory64::emitMoveImmToReg(getInstDataAddress() + (uint64_t)stats.Buffer + offsetof(BufferEntry, __buf_current), sr2));
                        bufferDumpInstructions->append(X86InstructionFactory64::emitMoveImmToReg(getInstDataAddress() + (uint64_t)stats.Buffer, sr2));
                    }
                    // sr2 = ((BufferEntry)sr2)->__buf_current
                    bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr2, offsetof(BufferEntry, __buf_current), sr2));                            

                    // compare current buffer to buffer max
                    bufferDumpInstructions->append(X86InstructionFactory64::emitCompareImmReg(BUFFER_ENTRIES - bb->getNumberOfMemoryOps(), sr2));

                    // jump to non-buffer-jump code
                    bufferDumpInstructions->append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));

                    ASSERT(bufferDumpInstructions);
                    while (bufferDumpInstructions->size()){
                        pt->addPrecursorInstruction(bufferDumpInstructions->remove(0));
                    }
                    delete bufferDumpInstructions;

                    // Increment current buffer size
                    // if we include the buffer increment as part of the buffer check, it increments the buffer pointer even when we try to disable this point during buffer clearing
                    InstrumentationSnippet* snip = addInstrumentationSnippet();
                    pt = addInstrumentationPoint(memop, snip, InstrumentationMode_inline, InstLocation_prior);
                    pt->setPriority(InstPriority_regular);
                    dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_bufferinc), true);

                    // sr1 = stats
                    if (threadReg == X86_REG_INVALID && usePIC){
                        Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
                        for (uint32_t k = 0; k < tdata->size(); k++){
                            snip->addSnippetInstruction((*tdata)[k]);
                        }
                        delete tdata;
                    }

                    if (usePIC){
                        // sr2 = ((SimulationStats*)sr1)->Buffer
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
                        // ((BufferEntry*)sr2)->__buf_current++
                        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(bb->getNumberOfMemoryOps(), sr2, offsetof(BufferEntry, __buf_current)));
                    } else {
                        // stats.Buffer[0].__buf_current++
                        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToMem(bb->getNumberOfMemoryOps(), getInstDataAddress() + currentOffset));
                    }
                } // END first memop instrumentation


                // at every memop, fill a buffer entry
                InstrumentationSnippet* snip = addInstrumentationSnippet();
                InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, InstLocation_prior);
                pt->setPriority(InstPriority_low);
                dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_bufferfill), true);

                // grab 3 scratch registers
                uint32_t sr1 = X86_REG_INVALID, sr2 = X86_REG_INVALID, sr3 = X86_REG_INVALID;

                BitSet<uint32_t>* inv = new BitSet<uint32_t>(X86_ALU_REGS);
                inv->insert(X86_REG_AX);
                inv->insert(X86_REG_SP);
                //inv->insert(X86_REG_BP);
                if (threadReg != X86_REG_INVALID){
                    inv->insert(threadReg);
                    sr1 = threadReg;
                }
                
                RegisterSet* regused = memop->getUnusableRegisters();
                for (uint32_t k = 0; k < X86_64BIT_GPRS; k++){
                    if (regused->containsRegister(k)){
                        inv->insert(k);
                    }
                }
                delete regused;
                for (uint32_t k = X86_64BIT_GPRS; k < X86_ALU_REGS; k++){
                    inv->insert(k);
                }
                BitSet<uint32_t>* dead = memop->getDeadRegIn(inv, 3);
                ASSERT(dead->size() >= 3);

                for (uint32_t k = 0; k < X86_64BIT_GPRS; k++){
                    if (dead->contains(k)){
                        if (sr1 == X86_REG_INVALID){
                            sr1 = k;
                        } else if (sr2 == X86_REG_INVALID){
                            sr2 = k;
                        } else if (sr3 == X86_REG_INVALID){
                            sr3 = k;
                            break;
                        }
                    }
                }
                ASSERT(sr1 != X86_REG_INVALID && sr2 != X86_REG_INVALID && sr3 != X86_REG_INVALID);
                delete inv;
                delete dead;

                // if thread data addr is not in sr1 already, load it
                // sr1 = stats
                if (threadReg == X86_REG_INVALID && usePIC){
                    Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
                    for (uint32_t k = 0; k < tdata->size(); k++){
                        snip->addSnippetInstruction((*tdata)[k]);
                    }
                    delete tdata;
                }

                if (usePIC){
                    // sr2 = ((SimulationStats*)sr1)->Buffer
                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
                } else {
                    // sr2 = stats.Buffer
                    //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(getInstDataAddress() + (uint64_t)stats.Buffer + offsetof(BufferEntry, __buf_current), sr2));
                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(getInstDataAddress() + (uint64_t)stats.Buffer, sr2));
                }
                // sr3 = ((BufferEntry*)sr2)->__buf_current;
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr2, offsetof(BufferEntry, __buf_current), sr3));
                // sr3 = shl 5 sr3 
                snip->addSnippetInstruction(X86InstructionFactory64::emitShiftLeftLogical(logBase2(sizeof(BufferEntry)), sr3));

                // sr1 holds the thread data addr (which points to SimulationStats)
                // sr2 holds the base address of the buffer 
                // sr3 holds the offset (in bytes) of the access

                ASSERT(memopIdInBlock < bb->getNumberOfMemoryOps());
                uint32_t bufferIdx = 1 + memopIdInBlock - bb->getNumberOfMemoryOps();
                snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(sr2, sr3, 1, sizeof(BufferEntry) * bufferIdx, sr2, true, true));
                // sr2 now holds the base of this memop's buffer entry

                Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, sr3);
                while (!(*addrStore).empty()){
                    snip->addSnippetInstruction((*addrStore).remove(0));
                }
                delete addrStore;
                // sr3 holds the memory address being used by memop

                
                // put the 4 elements of a BufferEntry into place
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, address), true));
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(memopSeq, sr3));
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, memseq), true));
                // this uses the value stored in the image key storage location
                //snip->addSnippetInstruction(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, sr3), this, getInstDataAddress() + imageKey, false));
                //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr3, 0, sr3));
                //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, imageid), true));
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImm64ToReg(imageHash, sr3));
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, imageid), true));
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveThreadIdToReg(sr3));
                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, threadid), true));

                if (isPerInstruction()){
                    LineInfo* li = NULL;
                    if (lineInfoFinder){
                        li = lineInfoFinder->lookupLineInfo(memop);
                    }

                    (*allBlocks).append(memop);
                    (*allBlockIds).append(j);
                    (*allBlockLineInfos).append(li);

                    if (li){
                        uint32_t line = li->GET(lr_line);
                        initializeReservedData(getInstDataAddress() + (uint64_t)stats.Lines + sizeof(uint32_t)*memopSeq, sizeof(uint32_t), &line);

                        uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
                        initializeReservedPointer(filename, (uint64_t)stats.Files + memopSeq*sizeof(char*));
                        initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());
                    } else {
                        temp32 = 0;
                        initializeReservedData(getInstDataAddress() + (uint64_t)stats.Lines + sizeof(uint32_t)*memopSeq, sizeof(uint32_t), &temp32);
                        initializeReservedPointer(noData, (uint64_t)stats.Files + memopSeq*sizeof(char*));
                    }

                    uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
                    initializeReservedPointer(funcname, (uint64_t)stats.Functions + memopSeq*sizeof(char*));
                    initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

                    HashCode* hc = memop->generateHashCode(bb);
                    uint64_t hashValue = hc->getValue();
                    uint64_t addr = memop->getProgramAddress();
                    delete hc;

                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.Hashes + memopSeq*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.Addresses + memopSeq*sizeof(uint64_t), sizeof(uint64_t), &addr);

                    CounterTypes tmpct;
                    if (memopIdInBlock == 0){
                        tmpct = CounterType_basicblock;
                        temp64 = 0;
                        currentLeader = memopSeq;

                        uint64_t counterOffset = (uint64_t)stats.Counters + (memopSeq * sizeof(uint64_t));
                        if (usePIC){
                            counterOffset -= simulationStruct;
                        }
                        InstrumentationTool::insertBlockCounter(counterOffset, bb, true, threadReg);

                    } else {
                        tmpct = CounterType_instruction;
                        temp64 = currentLeader;
                    }

                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.Types + memopSeq*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.Counters + (memopSeq * sizeof(uint64_t)), sizeof(uint64_t), &temp64);

                    temp32 = 1;
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopsPerBlock + memopSeq*sizeof(uint32_t), sizeof(uint32_t), &temp32);

                    temp64 = memopSeq;
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.BlockIds + memopSeq*sizeof(uint64_t), sizeof(uint64_t), &temp64);
                    temp64 = blockSeq;
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopIds + memopSeq*sizeof(uint64_t), sizeof(uint64_t), &temp64);
                } else {
                    temp64 = blockSeq;
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.BlockIds + memopSeq*sizeof(uint64_t), sizeof(uint64_t), &temp64);
                    temp64 = memopIdInBlock;
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopIds + memopSeq*sizeof(uint64_t), sizeof(uint64_t), &temp64);
                }

                memopIdInBlock++;
                memopSeq++;
            }
        }
        blockSeq++;
    }

    for(uint32_t i=0;i < GroupIdsVec.size() ; i++){
        uint64_t myGroupId = GroupIdsVec[i];
        NestedLoopStruct* myNestedLoopStruct = ( nestedLoopGrouping.getVal(myGroupId ) );
        printf("\t\t Initializing GroupId 0x%012llx in TestArray and it's actual instance has %d blocks at lower level. \n",myGroupId,myNestedLoopStruct->InnerLevelSize);

        uint64_t* tmpInnerLevelBasicBlocksPtr;
        tmpInnerLevelBasicBlocksPtr = (uint64_t*) reserveDataOffset( myNestedLoopStruct->InnerLevelSize * sizeof(uint64_t) );
        initializeReservedPointer( (uint64_t)tmpInnerLevelBasicBlocksPtr, ( (uint64_t) stats.NLStats + ( i * sizeof(NestedLoopStruct) ) ) + offsetof(NestedLoopStruct,InnerLevelBasicBlocks) );
        uint64_t addrCurrInnerLevelBasicBlocks =  (uint64_t)tmpInnerLevelBasicBlocksPtr; //currNestLoopStatsInstance + offsetof(NestedLoopStruct,InnerLevelBasicBlocks); // assuming already all data is in uint64_t.

        uint64_t* currInnerLevelBasicBlocks = myNestedLoopStruct->InnerLevelBasicBlocks;
        for(uint32_t j=0; j < myNestedLoopStruct->InnerLevelSize; j++){
            uint64_t tempBlkId = (uint64_t) mapBBToArrayIdx.getVal(currInnerLevelBasicBlocks[j]);
            initializeReservedData(  getInstDataAddress() + addrCurrInnerLevelBasicBlocks + ( j * sizeof(uint64_t)) , sizeof(uint64_t) , (void*) ( & tempBlkId) );
            printf("\t j: %d mapping hash 0x%012llx to index %ld and it should be at address: 0x%012llx \n",j,currInnerLevelBasicBlocks[j],tempBlkId,(getInstDataAddress()+addrCurrInnerLevelBasicBlocks+( j * sizeof(uint64_t))));
        }  

        initializeReservedData(getInstDataAddress() + (uint64_t)stats.TestArray + i * sizeof(uint64_t), sizeof(uint64_t),(void*)(&myGroupId));        
    }

    char* extension = new char[__MAX_STRING_SIZE];
    if (phaseNo > 0){
        sprintf(extension, "phase.1.%s", getExtension());
    } else {
        sprintf(extension, "%s", getExtension());
    }

    if (isPerInstruction()){
        printStaticFilePerInstruction(extension, allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());
    } else {
        printStaticFile(extension, allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());
    }


    delete[] extension;
    delete[] nostring;

    delete allBlocks;
    delete allBlockIds;
    delete allBlockLineInfos;

    if (usePIC){
        delete functionThreading;
    }

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

