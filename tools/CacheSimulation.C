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

#define ENTRY_FUNCTION "tool_image_init"
#define SIM_FUNCTION "process_buffer"
#define EXIT_FUNCTION "tool_image_fini"
#define INST_LIB_NAME "libsimulator.so"

#define NOSTRING "__pebil_no_string__"
#define BUFFER_ENTRIES 0x10000

#define LOAD 1
#define STORE 0

extern "C" {
    InstrumentationTool* CacheSimulationMaker(ElfFile* elf){
        return new CacheSimulation(elf);
    }
}

// also include any block that is in this loop (including child loops)
void CacheSimulation::includeLoopBlocks(BasicBlock* bb) {
    if (bb->isInLoop()){
        // For now use the BB-ID of top-most loop as hash-key of the group.
        // Should change this by generating a new hash.
        SimpleHash<Loop*> loopsToCheck;
        Vector<Loop*> loopsVec;
        Vector<uint64_t> BB_NestedLoop;

        FlowGraph* fg = bb->getFlowGraph();
        Loop* lp = fg->getInnermostLoopForBlock(bb->getIndex());
        BasicBlock** allBlocks = new BasicBlock*[lp->getNumberOfBlocks()];
        lp->getAllBlocks(allBlocks);
        
        BasicBlock* HeadBB=lp->getHead(); 
        uint64_t topLoopID=HeadBB->getHashCode().getValue();

        for (uint32_t k = 0; k < lp->getNumberOfBlocks(); k++){
            uint64_t code = allBlocks[k]->getHashCode().getValue();
            BB_NestedLoop.insert(allBlocks[k]->getHashCode().getValue(),k);
            blocksToInst.insert(code, allBlocks[k]);
        }                      
        
        Vector<uint64_t>* tmpInnermostBasicBlocksForGroup; 
        tmpInnermostBasicBlocksForGroup=new Vector<uint64_t>;

        uint32_t CountNumBBAdded=0;
        for(uint32_t i=0; i < BB_NestedLoop.size(); i++){
            if( !(mapBBToGroupId.get(BB_NestedLoop[i])) ){
                mapBBToGroupId.insert(BB_NestedLoop[i],topLoopID); 
                tmpInnermostBasicBlocksForGroup->insert(BB_NestedLoop[i],tmpInnermostBasicBlocksForGroup->size());
                CountNumBBAdded++;
            }  
        }

        uint64_t* innermostBasicBlocksForGroup;
        innermostBasicBlocksForGroup = new uint64_t[tmpInnermostBasicBlocksForGroup->size()];
        for(uint32_t i=0;i<tmpInnermostBasicBlocksForGroup->size();i++){
            innermostBasicBlocksForGroup[i] = (*tmpInnermostBasicBlocksForGroup)[i];
        }

        NestedLoopStruct* currLoopStats = new NestedLoopStruct;
        currLoopStats->GroupId = topLoopID;
        currLoopStats->InnerLevelSize = CountNumBBAdded; //tmpInnermostBasicBlocksForGroup->size();
        currLoopStats->GroupCount = 0;
        currLoopStats->InnerLevelBasicBlocks = innermostBasicBlocksForGroup;
        if(!nestedLoopGrouping.get(topLoopID))
            nestedLoopGrouping.insert(topLoopID,currLoopStats);

        // TODO: Should I delete the hashes/vectors used for book keeping of figuring out loop structure ?
        delete[] allBlocks;
        delete tmpInnermostBasicBlocksForGroup;
       // delete MainNode;
    } else {
        uint64_t* innermostBasicBlocksForGroup;
        innermostBasicBlocksForGroup=new uint64_t;  
        *innermostBasicBlocksForGroup=(bb->getHashCode().getValue()); // Since we know this is only a BB, just adding this BB.
        NestedLoopStruct* currLoopStats = new NestedLoopStruct;
        currLoopStats->GroupId = (bb->getHashCode().getValue());
        currLoopStats->InnerLevelSize = 1;
        currLoopStats->GroupCount = 0;
        currLoopStats->InnerLevelBasicBlocks = innermostBasicBlocksForGroup;
        nestedLoopGrouping.insert(bb->getHashCode().getValue(),currLoopStats); // Only 1 BB, so third term is 1.
        if( !(mapBBToGroupId.get(bb->getHashCode().getValue())) )
            mapBBToGroupId.insert(bb->getHashCode().getValue(),bb->getHashCode().getValue()); 
    } // not in loop

}


/*
 * Read input file and create a list of blocks to instrument
 * initializes blocksToInst
 */
void CacheSimulation::filterBBs(){

    if (!strcmp("+", inputFile)){
        for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
            BasicBlock* bb = getExposedBasicBlock(i);
            blocksToInst.insert(bb->getHashCode().getValue(), bb);
        }

    } else {
        Vector<char*> fileLines;
        initializeFileList(inputFile, &fileLines);

        for (uint32_t i = 0; i < fileLines.size(); i++){
            char* ptr = strchr(fileLines[i],'#');
            if(ptr) *ptr = '\0';

            if(!strlen(fileLines[i]) || allSpace(fileLines[i]))
                continue;

            int32_t err;
            uint64_t inputHash = 0;
            uint64_t imgHash = 0;

            err = sscanf(fileLines[i], "%llx %llx", &inputHash, &imgHash);
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

            if (!bb){
                PRINT_WARN(10, "cannot find basic block for hash code %#llx found in input file", inputHash);        continue;
            }
            blocksToInst.insert(bb->getHashCode().getValue(), bb);

            if (loopIncl){
                includeLoopBlocks(bb);
            }
        }

        for (uint32_t i = 0; i < fileLines.size(); i++){
            delete[] fileLines[i];
        }
    }

    // Default Behavior
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

   // ASSERT(isPowerOfTwo(sizeof(BufferEntry)));
   // PRINT_WARN(20,"\n\t WARNING: sizeof(BufferEntry) is not checked for being power of two!!! ");
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

void CacheSimulation::instrumentEntryPoint() {
     if (isMultiImage()){
        for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
            Function* f = getExposedFunction(i);

            InstrumentationPoint* point = addInstrumentationPoint(
                f, entryFunc, InstrumentationMode_tramp, InstLocation_prior);

            ASSERT(point);
            point->setPriority(InstPriority_sysinit);
            if (!point->getInstBaseAddress()){
                PRINT_ERROR("Cannot find an instrumentation point at the entry function");
            }            

            dynamicPoint(point, getElfFile()->getUniqueId(), true);
        }
    } else {
        InstrumentationPoint* point = addInstrumentationPoint(
            getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
        ASSERT(point);
        point->setPriority(InstPriority_sysinit);
        if (!point->getInstBaseAddress()){
            PRINT_ERROR("Cannot find an instrumentation point at the entry function");
        }
    }
}

void CacheSimulation::instrumentExitPoint() {
    InstrumentationPoint* point = addInstrumentationPoint(
        getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(point);
    point->setPriority(InstPriority_sysinit);
    if (!point->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }
}

void CacheSimulation::initializeLineInfo(
        SimulationStats& stats,
        Function* func,
        BasicBlock* bb,
        uint32_t blockSeq,
        uint64_t noData) {

    LineInfo* li = NULL;
    LineInfoFinder* lineInfoFinder = NULL;

    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    if (lineInfoFinder){
        li = lineInfoFinder->lookupLineInfo(bb);
    }
    allBlockLineInfos.append(li);

    if (li){
        uint32_t line = li->GET(lr_line);
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.Lines + sizeof(uint32_t)*blockSeq,
            sizeof(uint32_t),
            &line);

        uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
        initializeReservedPointer(
            filename,
            (uint64_t)stats.Files + blockSeq*sizeof(char*));
        initializeReservedData(
            getInstDataAddress() + filename,
            strlen(li->getFileName()) + 1,
            (void*)li->getFileName());
    } else {
        uint32_t temp32 = 0;
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.Lines + sizeof(uint32_t)*blockSeq,
            sizeof(uint32_t),
            &temp32);
        initializeReservedPointer(
            noData,
            (uint64_t)stats.Files + blockSeq*sizeof(char*));
    }
    uint64_t funcname = reserveDataOffset(strlen(func->getName()) + 1);
    initializeReservedPointer(funcname,
        (uint64_t)stats.Functions + blockSeq*sizeof(char*));
    initializeReservedData(
        getInstDataAddress() + funcname,
        strlen(func->getName()) + 1,
        (void*)func->getName());
}

// checks if buffer is full and conditionally clears it
void CacheSimulation::insertBufferClear(
        uint32_t numMemops,
        X86Instruction* inst,
        InstLocations loc,
        uint64_t blockSeq,
        uint32_t threadReg,
        SimulationStats& stats)
{


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
    BitSet<uint32_t>* dead = inst->getDeadRegIn(inv, 2);
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

    // Create Instrumentation Point
    InstrumentationPoint* pt = addInstrumentationPoint(inst, simFunc, InstrumentationMode_tramp, loc);
    pt->setPriority(InstPriority_userinit);
    dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_buffercheck), true);
    Vector<X86Instruction*>* bufferDumpInstructions = new Vector<X86Instruction*>();

    // if thread data addr is not in sr1 already, load it
    if (threadReg == X86_REG_INVALID && usePIC()){
        Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
        for (uint32_t k = 0; k < tdata->size(); k++){
            bufferDumpInstructions->append((*tdata)[k]);
        }
        delete tdata;
    }

    // put current buffer into sr2
    if (usePIC()){
        // sr2 =((SimulationStats)sr1)->Buffer
        bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
    } else {
        // sr2 = stats.Buffer
        bufferDumpInstructions->append(X86InstructionFactory64::emitMoveImmToReg(getInstDataAddress() + (uint64_t)stats.Buffer, sr2));
    }
    // sr2 = ((BufferEntry)sr2)->__buf_current
    bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr2, offsetof(BufferEntry, __buf_current), sr2));                            

    // compare current buffer+blockMemops to buffer max
    bufferDumpInstructions->append(X86InstructionFactory64::emitCompareImmReg(BUFFER_ENTRIES - numMemops, sr2));

    // jump to non-buffer-jump code
    bufferDumpInstructions->append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));

    ASSERT(bufferDumpInstructions);
    while (bufferDumpInstructions->size()){
        pt->addPrecursorInstruction(bufferDumpInstructions->remove(0));
    }
    delete bufferDumpInstructions;

    // Increment current buffer size
    // if we include the buffer increment as part of the buffer check, it increments
    // the buffer pointer even when we try to disable this point during buffer clearing
    InstrumentationSnippet* snip = addInstrumentationSnippet();
    pt = addInstrumentationPoint(inst, snip, InstrumentationMode_inline, loc);
    pt->setPriority(InstPriority_regular);
    dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_bufferinc), true);

    // sr1 = stats
    if (threadReg == X86_REG_INVALID && usePIC()){
        Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
        for (uint32_t k = 0; k < tdata->size(); k++){
            snip->addSnippetInstruction((*tdata)[k]);
        }
        delete tdata;
    }

    if (usePIC()){
        // sr2 = ((SimulationStats*)sr1)->Buffer
        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(
            sr1,
            offsetof(SimulationStats, Buffer),
            sr2));
        // ((BufferEntry*)sr2)->__buf_current++
        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(
            numMemops,
            sr2,
            offsetof(BufferEntry, __buf_current)));
    } else {
        // stats.Buffer[0].__buf_current++
        uint64_t currentOffset = (uint64_t)stats.Buffer + offsetof(BufferEntry, __buf_current);
        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToMem(
            numMemops,
            getInstDataAddress() + currentOffset));
    }
}

void CacheSimulation::writeBufferBase(
        InstrumentationSnippet* snip,
        uint32_t sr2,
        uint32_t sr3,
        enum EntryType type,
        uint8_t loadstoreflag,
        uint32_t memseq) {

    // set entry type
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(type, sr3));
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, type), true));

    // set Load-store flag
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(loadstoreflag, sr3));
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, loadstoreflag), true));

    // set imageid
    uint64_t imageHash = getElfFile()->getUniqueId();
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImm64ToReg(imageHash, sr3));
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, imageid), true));

    // set memseq
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(memseq, sr3));
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, memseq), true));
}

// Fills a buffer entry for memop
void CacheSimulation::instrumentMemop(
        Function* func,
        BasicBlock* bb,
        X86Instruction* memop,
        uint64_t loadstoreflag,
        uint64_t blockSeq,
        uint32_t threadReg,
        SimulationStats& stats,
        uint32_t memopIdInBlock,
        uint32_t memopSeq,
        uint32_t instructionIdx,
        uint64_t noData,
        uint32_t leader,
        uint64_t simulationStruct){

    // First we build the actual instrumentation point
    InstrumentationSnippet* snip = addInstrumentationSnippet();
    InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, InstLocation_prior);
    pt->setPriority(InstPriority_low);
    dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_bufferfill), true);

    // Then we fill the snippet with instructions

    // grab 3 scratch registers
    uint32_t sr1 = X86_REG_INVALID, sr2 = X86_REG_INVALID, sr3 = X86_REG_INVALID;
    // check if sr1 is already set for us
    if (threadReg != X86_REG_INVALID){
        sr1 = threadReg;
    }
    grabScratchRegisters(memop, InstLocation_prior, &sr1, &sr2, &sr3);
    ASSERT(sr1 != X86_REG_INVALID && sr2 != X86_REG_INVALID && sr3 != X86_REG_INVALID);

    // if thread data addr is not in sr1 already, load it
    // sr1 = stats
    if (threadReg == X86_REG_INVALID && usePIC()){
        Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
        for (uint32_t k = 0; k < tdata->size(); k++){
            snip->addSnippetInstruction((*tdata)[k]);
        }
        delete tdata;
    }

    ASSERT(memopIdInBlock < bb->getNumberOfMemoryOps());
    setupBufferEntry(snip, 1+memopIdInBlock-bb->getNumberOfMemoryOps(), sr1, sr2, sr3, stats);
    writeBufferBase(snip, sr2, sr3, MEM_ENTRY, loadstoreflag, memopSeq);

    // set address
    Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, sr3);
    while (!(*addrStore).empty()){
        snip->addSnippetInstruction((*addrStore).remove(0));
    }
    delete addrStore;
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, address), true));

    // Only for adamant
    //uint64_t programAddress = memop->getProgramAddress();
    //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(programAddress, sr3));
    //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, programAddress), true));  

    // Only for debugging
    //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveThreadIdToReg(sr3));
    //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, threadid), true));

    if (isPerInstruction()){
        allBlocks.append(memop);
        allBlockIds.append(instructionIdx);
        initializeLineInfo(stats, func, bb, memopSeq, noData);

        HashCode* hc = memop->generateHashCode(bb);
        uint64_t hashValue = hc->getValue();
        uint64_t addr = memop->getProgramAddress();
        delete hc;

        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.Hashes + memopSeq*sizeof(uint64_t),
            sizeof(uint64_t),
            &hashValue);

        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.Addresses + memopSeq*sizeof(uint64_t),
            sizeof(uint64_t),
            &addr);

        CounterTypes tmpct;
        uint64_t temp64 = 0;
        if (memopIdInBlock == 0){
            tmpct = CounterType_basicblock;

            uint64_t counterOffset = (uint64_t)stats.Counters + (memopSeq * sizeof(uint64_t));
            if (usePIC()){
                counterOffset -= simulationStruct;
            }
            InstrumentationTool::insertBlockCounter(counterOffset, bb, true, threadReg);

        } else {
            tmpct = CounterType_instruction;
            temp64 = leader;
        }

        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.Types + memopSeq*sizeof(CounterTypes),
            sizeof(CounterTypes),
            &tmpct);

        // initialize counter to 0 or leader seq?
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.Counters + (memopSeq * sizeof(uint64_t)),
            sizeof(uint64_t),
            &temp64);

        uint32_t temp32 = 1;
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.MemopsPerBlock + memopSeq*sizeof(uint32_t),
            sizeof(uint32_t),
            &temp32);

        temp64 = memopSeq;
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.BlockIds + memopSeq*sizeof(uint64_t),
            sizeof(uint64_t),
            &temp64);

        temp64 = blockSeq;
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.MemopIds + memopSeq*sizeof(uint64_t),
            sizeof(uint64_t),
            &temp64);

    } else {
        uint64_t temp64 = blockSeq;
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.BlockIds + memopSeq*sizeof(uint64_t),
            sizeof(uint64_t),
            &temp64);

        temp64 = memopIdInBlock;
        initializeReservedData(
            getInstDataAddress() + (uint64_t)stats.MemopIds + memopSeq*sizeof(uint64_t),
            sizeof(uint64_t),
            &temp64);
    }
}

void CacheSimulation::initializeBlockInfo(BasicBlock* bb,
                         uint32_t blockInd,
                         SimulationStats& stats,
                         Function* func,
                         uint32_t blockSeq,
                         uint64_t noData,
                         SimpleHash<uint64_t>& mapBBToIdxOfGroup,
                         SimpleHash<uint32_t>& mapBBToArrayIdx,
                         uint32_t countBBInstrumented) {
    allBlocks.append(bb);
    allBlockIds.append(blockInd);

    initializeLineInfo(stats, func, bb, blockSeq, noData);

    uint64_t hashValue = bb->getHashCode().getValue();
    uint64_t addr = bb->getProgramAddress();        
    uint64_t groupId = mapBBToIdxOfGroup.getVal(hashValue);
    mapBBToArrayIdx.insert(hashValue,countBBInstrumented);

    initializeReservedData(
        getInstDataAddress() + (uint64_t)stats.Hashes + blockSeq*sizeof(uint64_t),
        sizeof(uint64_t),
        &hashValue);

    initializeReservedData(
        getInstDataAddress() + (uint64_t)stats.Addresses + blockSeq*sizeof(uint64_t),
        sizeof(uint64_t),
        &addr);

    initializeReservedData(
        getInstDataAddress() + (uint64_t)stats.GroupIds + blockSeq*sizeof(uint64_t),
        sizeof(uint64_t),
        (void*) &groupId);

    CounterTypes tmpct = CounterType_basicblock;
    initializeReservedData(
        getInstDataAddress() + (uint64_t)stats.Types + blockSeq*sizeof(CounterTypes),
        sizeof(CounterTypes),
        &tmpct);

    uint64_t temp64 = 0;
    initializeReservedData(
        getInstDataAddress() + (uint64_t)stats.Counters + blockSeq*sizeof(uint64_t),
        sizeof(uint64_t),
        &temp64);

    uint32_t temp32 = bb->getNumberOfMemoryOps();
    initializeReservedData(
        getInstDataAddress() + (uint64_t)stats.MemopsPerBlock + blockSeq*sizeof(uint32_t),
        sizeof(uint32_t),
        &temp32);
}

void CacheSimulation::grabScratchRegisters(
        X86Instruction* instRefPoint,
        InstLocations loc,
        uint32_t* sr1,
        uint32_t* sr2,
        uint32_t* sr3) {

    // start with all gpu regs except ax and sp
    BitSet<uint32_t>* inv = new BitSet<uint32_t>(X86_ALU_REGS);
    inv->insert(X86_REG_AX);
    inv->insert(X86_REG_SP);

    // invalidate presets TODO other regs
    if(sr1 && *sr1 != X86_REG_INVALID) {
       inv->insert(*sr1);
    }

    // invalidate any registers used by this instruction FIXME why?
    RegisterSet* regused = instRefPoint->getUnusableRegisters();
    for (uint32_t k = 0; k < X86_64BIT_GPRS; k++){
        if (regused->containsRegister(k)){
            inv->insert(k);
        }
    }
    delete regused;

    // Invalidate non-gprs FIXME just allocate a bitset without them
    for (uint32_t k = X86_64BIT_GPRS; k < X86_ALU_REGS; k++){
        inv->insert(k);
    }

    // Look for dead registers in remaining valid
    BitSet<uint32_t>* dead = NULL;
    if(loc == InstLocation_prior)
        dead = instRefPoint->getDeadRegIn(inv, 3);
    else if(loc == InstLocation_after)
        dead = instRefPoint->getDeadRegOut(inv, 3);
    else
        assert(0 && "Invalid inst location");

    for (uint32_t k = 0; k < X86_64BIT_GPRS; k++){
        if (dead->contains(k)){
            if (sr1 && *sr1 == X86_REG_INVALID){
                *sr1 = k;
            } else if (sr2 && *sr2 == X86_REG_INVALID){
                *sr2 = k;
            } else if (sr3 && *sr3 == X86_REG_INVALID){
                *sr3 = k;
                break;
            }
        }
    }
    delete inv;
    delete dead;

}

void CacheSimulation::setupBufferEntry(
        InstrumentationSnippet* snip,
        uint32_t bufferIdx,
        uint32_t sr1,
        uint32_t sr2,
        uint32_t sr3,
        SimulationStats& stats) {

    // sr2 = start of buffer
    if (usePIC()){
        // sr2 = ((SimulationStats*)sr1)->Buffer
        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
    } else {
        // sr2 = stats.Buffer
        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(getInstDataAddress() + (uint64_t)stats.Buffer, sr2));
    }

    // sr3 = ((BufferEntry*)sr2)->__buf_current;
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr2, offsetof(BufferEntry, __buf_current), sr3));
    // sr3 = sr3 + sizeof(BuffestEntry)
    snip->addSnippetInstruction(X86InstructionFactory64::emitRegImmMultReg(sr3, sizeof(BufferEntry), sr3)); 
    // sr3 holds the offset (in bytes) of the access

    // sr2 = pointer to memop's buffer entry
    snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(
        sr2, sr3, 1, sizeof(BufferEntry) * bufferIdx, sr2, true, true));

}


void CacheSimulation::bufferVectorEntry(
        X86Instruction* instRefPoint,
        InstLocations   loc,
        X86Instruction* vectorIns,
        uint32_t        threadReg,
        SimulationStats& stats,
        uint32_t blockSeq,
        uint32_t memseq) {

    // First we build the actual instrumentation point
    InstrumentationSnippet* snip = addInstrumentationSnippet();
    InstrumentationPoint* point = addInstrumentationPoint(
        instRefPoint, snip, InstrumentationMode_trampinline, loc);
    point->setPriority(InstPriority_low);
    dynamicPoint(point, GENERATE_KEY(blockSeq, PointType_bufferfill), true);

    uint32_t sr1 = X86_REG_INVALID, sr2 = X86_REG_INVALID, sr3 = X86_REG_INVALID;
    if(threadReg != X86_REG_INVALID)
        sr1 = threadReg;
    grabScratchRegisters(instRefPoint, loc, &sr1, &sr2, &sr3);
    assert(sr1 != X86_REG_INVALID && sr2 != X86_REG_INVALID && sr3 != X86_REG_INVALID);

    // sr1 = stats
    if(threadReg == X86_REG_INVALID && usePIC()) {
        Vector<X86Instruction*>* insns = storeThreadData(sr2, sr1);
        for(uint32_t ins = 0; ins < insns->size(); ++ins) {
            snip->addSnippetInstruction((*insns)[ins]);
        }
        delete insns;
    }

    setupBufferEntry(snip, 0, sr1, sr2, sr3, stats);
    int8_t loadstoreflag;
    if(vectorIns->isLoad())
        loadstoreflag = LOAD;
    else if(vectorIns->isStore())
        loadstoreflag = STORE;
    else
        assert(0);
    writeBufferBase(snip, sr2, sr3, VECTOR_ENTRY, loadstoreflag, memseq);

    OperandX86* vectorOp = NULL;
    // vgatherdps (%r14,%zmm0,8), %zmm2 {k4}
    if(vectorIns->isLoad()) {
        Vector<OperandX86*>* ops = vectorIns->getSourceOperands();
        assert(ops->size() == 1);
        vectorOp = (*ops)[0];
        delete ops;
    } else if(vectorIns->isStore()) {
        vectorOp = vectorIns->getDestOperand();
        assert(vectorOp);
    } else assert(0);
    assert(vectorOp->getType() == UD_OP_MEM);

    uint32_t zmmReg  = vectorOp->getIndexRegister();
    uint32_t baseReg = vectorOp->getBaseRegister();
    uint8_t scale   = vectorOp->GET(scale);
    uint32_t kreg    = vectorIns->getVectorMaskRegister();

    // write base
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(
        baseReg,
        sr2,
        offsetof(BufferEntry, vectorAddress) + offsetof(VectorAddress, base),
        true));

    // write scale
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(
        scale,
        sr2,
        offsetof(BufferEntry, vectorAddress) + offsetof(VectorAddress, scale)));

    // write mask
    //   kmov k, sr3
    //   store sr3
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveKToReg(kreg, sr3));
    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(
        sr3,
        sr2,
        offsetof(BufferEntry, vectorAddress) + offsetof(VectorAddress, mask),
        true));

    // write index vector
    Vector<X86Instruction*>* insns = X86InstructionFactory64::emitUnalignedPackstoreRegaddrImm(
        zmmReg,
        kreg,
        sr2,
        offsetof(BufferEntry, vectorAddress) + offsetof(VectorAddress, indexVector));

    for(int idx = 0; idx < insns->size(); ++idx) {
        snip->addSnippetInstruction((*insns)[idx]);
    }
    delete insns;

}

void CacheSimulation::instrumentScatterGather(Loop* lp,
        uint32_t blockSeq,
        uint32_t memseq,
        uint32_t threadReg,
        SimulationStats& stats)
{
    // instrument every source path to loop
    BasicBlock* head = lp->getHead();
    X86Instruction* vectorMemOp = head->getInstruction(0);

    Vector<BasicBlock*> entryInterpositions;
    uint32_t nsources = head->getNumberOfSources();
    for(int srci = 0; srci < nsources; ++srci) {
        BasicBlock* source = head->getSourceBlock(srci);
        if(lp->isBlockIn(source->getIndex()))
            continue;

        // fallthrough
        if(source->getBaseAddress() + source->getNumberOfBytes() == head->getBaseAddress()) {
            // instrument after exit instruction of source
            uint32_t numMemops = 1;
            insertBufferClear(numMemops, source->getExitInstruction(), InstLocation_after, blockSeq, threadReg, stats);
            
            // write a buffer entry for gather-scatter ops in loop
            bufferVectorEntry(source->getExitInstruction(), InstLocation_after, vectorMemOp, threadReg, stats, blockSeq, memseq);

        } else {
            entryInterpositions.append(source);
        }
    }
    FlowGraph* fg = head->getFlowGraph();
    for(int srci = 0; srci < entryInterpositions.size(); ++srci) {
        BasicBlock* source = entryInterpositions[srci];
        BasicBlock* interp = initInterposeBlock(fg, source->getIndex(), head->getIndex());
        // instrument before exit instruction of interp
        // TODO
    }


}

void CacheSimulation::instrument(){
    InstrumentationTool::instrument();
    filterBBs();

    if (dfpFile){
        PRINT_WARN(20, "--dfp is an accepted argument but it does nothing. range finding is done for every block included in the simulation by default");
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
                if (memop->isLoad()){
                    memopSeq++;
                }
                if (memop->isStore()){
                    memopSeq++;
                }
            }
        }
    }

    Vector<uint64_t> GroupIdsVec;
    SimpleHash<uint64_t> groupsInitialized;
    SimpleHash<uint64_t> mapBBToIdxOfGroup;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        if(blocksToInst.get(bb->getHashCode().getValue())){
            uint64_t myGroupId, myGroupStatsSize;
            if(mapBBToGroupId.get(bb->getHashCode().getValue())) {
                myGroupId = mapBBToGroupId.getVal(bb->getHashCode().getValue());
                NestedLoopStruct* myNestedLoopStruct = nestedLoopGrouping.getVal(myGroupId);

                if(!groupsInitialized.exists(myGroupId, myGroupId)){
                    groupsInitialized.insert(myGroupId, myGroupId);    
                    GroupIdsVec.insert(myGroupId, GroupIdsVec.size());
                }
                if(!mapBBToIdxOfGroup.get(bb->getHashCode().getValue()))
                    mapBBToIdxOfGroup.insert(bb->getHashCode().getValue(),(GroupIdsVec.size()-1));
            }
        }
    }

    // Setup null lineinfo value
    uint64_t noData = reserveDataOffset(strlen(NOSTRING) + 1);
    initializeReservedData(getInstDataAddress() + noData, strlen(NOSTRING) + 1, NOSTRING);

    // Analyze code for thread registers
    std::map<uint64_t, ThreadRegisterMap*>* functionThreading;
    if (usePIC()){
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
    stats.NestedLoopCount = GroupIdsVec.size();

    // allocate Counters and SimulationStats contiguously to
    // avoid an extra memory ref in counter updates
    uint64_t simulationStruct =
        reserveDataOffset(sizeof(SimulationStats) +
                         (sizeof(uint64_t) * stats.BlockCount));
    stats.Counters = (uint64_t*)(simulationStruct + sizeof(SimulationStats));
    initializeReservedPointer((uint64_t)stats.Counters,
        simulationStruct + offsetof(SimulationStats, Counters));

    uint32_t temp32 = BUFFER_ENTRIES + 1;
    stats.Buffer = (BufferEntry*)reserveDataOffset(temp32 * sizeof(BufferEntry));
    initializeReservedData(getInstDataAddress() + (uint64_t)stats.Buffer,
                           sizeof(BufferEntry),
                           &intro);

    initializeReservedPointer((uint64_t)stats.Buffer,
        simulationStruct + offsetof(SimulationStats, Buffer));

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

    // Initialize stats.NLStats
    stats.NLStats = (NestedLoopStruct*)reserveDataOffset(
        stats.NestedLoopCount * sizeof(NestedLoopStruct));
    initializeReservedPointer(
        (uint64_t)stats.NLStats,
        simulationStruct + offsetof(SimulationStats,NLStats));    

    for(uint32_t i = 0; i < GroupIdsVec.size(); i++){
        uint64_t myGroupId = GroupIdsVec[i];
        NestedLoopStruct* myNestedLoopStruct = nestedLoopGrouping.getVal(myGroupId);

        uint64_t currNestLoopStatsInstance =
            ((uint64_t)stats.NLStats + (i * sizeof(NestedLoopStruct)));
        
        // Mostly dont need this since GroupIds are already stored!! 
        uint64_t currGroupId = myNestedLoopStruct->GroupId;
        initializeReservedData(
            getInstDataAddress() + currNestLoopStatsInstance + offsetof(NestedLoopStruct,GroupId),
            sizeof(uint64_t),
            (void*)(&currGroupId));

        uint64_t temp = getInstDataAddress() + currNestLoopStatsInstance;
        uint64_t tmpCount = 0;
        initializeReservedData(
            getInstDataAddress() + currNestLoopStatsInstance + offsetof(NestedLoopStruct,GroupCount),
            sizeof(uint64_t),
            (void*)(&tmpCount));
        
        // Mostly dont need this since GroupIds are already stored!! 
        uint64_t currInnerLevelSize = myNestedLoopStruct->InnerLevelSize;
        initializeReservedData(
            getInstDataAddress() + currNestLoopStatsInstance + offsetof(NestedLoopStruct,InnerLevelSize),
            sizeof(uint32_t),
            (void*)(&currInnerLevelSize));
    }

    // Initialize SimulationStats
    initializeReservedData(
        getInstDataAddress() + simulationStruct,
        sizeof(SimulationStats),
        (void*)(&stats));

    // Add arguments to instrumentation functions
    entryFunc->addArgument(simulationStruct);
    entryFunc->addArgument(imageKey);
    entryFunc->addArgument(threadHash);
    simFunc->addArgument(imageKey);
    exitFunc->addArgument(imageKey);

    instrumentEntryPoint();
    instrumentExitPoint();

    // TODO: remove all FP work from cache simulation?
    //simFunc->assumeNoFunctionFP();
    //uint64_t imageHash = getElfFile()->getUniqueId();

    // Begin instrumenting each block in the function
    blockSeq = 0;
    memopSeq = 0;
    uint32_t countBBInstrumented = 0;
    SimpleHash<uint32_t> mapBBToArrayIdx;
    for (uint32_t blockInd = 0; blockInd < getNumberOfExposedBasicBlocks(); blockInd++){
        BasicBlock* bb = getExposedBasicBlock(blockInd);
        Function* func = (Function*)bb->getLeader()->getContainer();

        // Check if we should skip this block
        if (!blocksToInst.get(bb->getHashCode().getValue()))
            continue;

        // initialize block info
        if (!isPerInstruction()){
            initializeBlockInfo(bb, blockInd, stats, func, blockSeq,
                noData, mapBBToIdxOfGroup, mapBBToArrayIdx, countBBInstrumented);
            countBBInstrumented+=1;
        }

        uint32_t threadReg = X86_REG_INVALID;
        if (usePIC()){
            ThreadRegisterMap* threadMap = (*functionThreading)[func->getBaseAddress()];
            threadReg = threadMap->getThreadRegister(bb);
        }

        // Check if block is part of gather-scatter loop
        if(bb->getInstruction(0)->isScatterGatherOp()) {
            // verify loop pattern
            PRINT_INFOR("Found scatter-gather block");

            // instrument outside of loop
            FlowGraph* fg = bb->getFlowGraph();
            Loop* lp = fg->getInnermostLoopForBlock(bb->getIndex());
            instrumentScatterGather(lp, blockSeq, memopSeq, threadReg, stats);
            ++memopSeq; // FIXME move inside call if we instrument more than one scatter-gather

            // advance blocks to end of loop
            blockInd += lp->getNumberOfBlocks()-1;
            continue;
        } else {

            uint32_t memopIdInBlock = 0;
            uint32_t leader = 0;
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);


                if (memop->isMemoryOperation()){

                    if (memopIdInBlock == 0){

                        if (!isPerInstruction()){
                            uint64_t counterOffset = (uint64_t)stats.Counters + (blockSeq * sizeof(uint64_t));
                            if (usePIC()) counterOffset -= simulationStruct;
                            InstrumentationTool::insertBlockCounter(counterOffset, bb, true, threadReg);
                        }

                        insertBufferClear(bb->getNumberOfMemoryOps(), memop, InstLocation_prior, blockSeq, threadReg,
                            stats);

                        leader = memopSeq;
                    }

                    if(memop->isLoad()) {
                        instrumentMemop(func, bb, memop, LOAD, blockSeq, threadReg, stats, memopIdInBlock, memopSeq,
                            j, noData, leader, simulationStruct);
                        ++memopIdInBlock;
                        ++memopSeq;
                    }

                    if(memop->isStore()) {
                        instrumentMemop(func, bb, memop, STORE, blockSeq, threadReg, stats, memopIdInBlock, memopSeq,
                            j, noData, leader, simulationStruct);
                        ++memopIdInBlock;
                        ++memopSeq;
                    }
                }
            }
        }
        blockSeq++;
    }

    for(uint32_t i = 0; i < GroupIdsVec.size(); i++){
        uint64_t myGroupId = GroupIdsVec[i];
        NestedLoopStruct* myNestedLoopStruct = nestedLoopGrouping.getVal(myGroupId);

        uint64_t* tmpInnerLevelBasicBlocksPtr;
        tmpInnerLevelBasicBlocksPtr = (uint64_t*)reserveDataOffset(
            myNestedLoopStruct->InnerLevelSize * sizeof(uint64_t));

        initializeReservedPointer(
            (uint64_t)tmpInnerLevelBasicBlocksPtr,
            ((uint64_t) stats.NLStats + (i * sizeof(NestedLoopStruct))) + offsetof(NestedLoopStruct,
            InnerLevelBasicBlocks) );

        uint64_t addrCurrInnerLevelBasicBlocks = (uint64_t)tmpInnerLevelBasicBlocksPtr;
            //currNestLoopStatsInstance + offsetof(NestedLoopStruct,InnerLevelBasicBlocks);
            //// assuming already all data is in uint64_t.

        uint64_t* currInnerLevelBasicBlocks = myNestedLoopStruct->InnerLevelBasicBlocks;
        for(uint32_t j=0; j < myNestedLoopStruct->InnerLevelSize; j++){
            uint64_t tempBlkId = (uint64_t) mapBBToArrayIdx.getVal(currInnerLevelBasicBlocks[j]);
            initializeReservedData(
                getInstDataAddress() + addrCurrInnerLevelBasicBlocks + (j * sizeof(uint64_t)),
                sizeof(uint64_t),
                (void*)(&tempBlkId));
        }
    }


    if (usePIC()){
        delete functionThreading;
    }

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

void CacheSimulation::writeStaticFile() {
    char* extension = new char[__MAX_STRING_SIZE];
    if (phaseNo > 0){
        sprintf(extension, "phase.1.%s", getExtension());
    } else {
        sprintf(extension, "%s", getExtension());
    }

    if (isPerInstruction()){
        printStaticFilePerInstruction(extension, &allBlocks, &allBlockIds, &allBlockLineInfos, allBlocks.size());
    } else {
        printStaticFile(extension, &allBlocks, &allBlockIds, &allBlockLineInfos, allBlocks.size());
    }
    delete[] extension;
}

