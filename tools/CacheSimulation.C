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
#include <DFPattern.h>

#include <Simulation.hpp>

#define ENTRY_FUNCTION "tool_image_init"
#define SIM_FUNCTION "process_buffer"
#define EXIT_FUNCTION "tool_image_fini"
#define INST_LIB_NAME "libsimulator.so"

#define NOSTRING "__pebil_no_string__"
#define BUFFER_ENTRIES 0x1000

#define GENERATE_KEY(__bid, __typ) ((__typ & 0xf) | (__bid << 4))
#define GET_BLOCKID(__key) ((__key >> 4))
#define GET_TYPE(__key) ((__key& 0xf))

extern "C" {
    InstrumentationTool* CacheSimulationMaker(ElfFile* elf){
        return new CacheSimulation(elf);
    }
}

DFPatternType convertDFPatternType(char* patternString){
    if(!strcmp(patternString,"dfTypePattern_Gather")){
        return dfTypePattern_Gather;
    } else if(!strcmp(patternString,"dfTypePattern_Scatter")){
        return dfTypePattern_Scatter;
    } else if(!strcmp(patternString,"dfTypePattern_FunctionCallGS")){
        return dfTypePattern_FunctionCallGS;
    }
    return dfTypePattern_undefined;
}

void CacheSimulation::filterBBs(){
    Vector<char*>* fileLines = new Vector<char*>();
    initializeFileList(inputFile, fileLines);

    int32_t err;
    uint64_t inputHash;
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        char* ptr = strchr((*fileLines)[i],'#');
        if(ptr) *ptr = '\0';

        if(!strlen((*fileLines)[i]) || allSpace((*fileLines)[i]))
            continue;

        err = sscanf((*fileLines)[i], "%lld", &inputHash);
        if(err <= 0){
            PRINT_ERROR("Line %d of %s has a wrong format", i+1, inputFile);
        }
        HashCode* hashCode = new HashCode(inputHash);
#ifdef STATS_PER_INSTRUCTION
        if(!hashCode->isBlock() && !hashCode->isInstruction()){
#else //STATS_PER_INSTRUCTION
        if(!hashCode->isBlock()){
#endif //STATS_PER_INSTRUCTION
            PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block/instruction", i+1, inputFile);
        }
        BasicBlock* bb = findExposedBasicBlock(*hashCode);
        delete hashCode;

        if (!bb){
            PRINT_WARN(10, "cannot find basic block for hash code %#llx found in input file", inputHash);
        } else {
            //        ASSERT(bb && "cannot find basic block for hash code found in input file");
            blocksToInst.insert(bb->getHashCode().getValue(), bb);

            // also include any block that is in this loop (including child loops)
            if (loopIncl){
                if (bb->isInLoop()){
                    FlowGraph* fg = bb->getFlowGraph();
                    Loop* lp = fg->getInnermostLoopForBlock(bb->getIndex());
                    BasicBlock** allBlocks = new BasicBlock*[lp->getNumberOfBlocks()];
                    lp->getAllBlocks(allBlocks);
                    for (uint32_t k = 0; k < lp->getNumberOfBlocks(); k++){
                        uint64_t code = allBlocks[k]->getHashCode().getValue();
                        blocksToInst.insert(code, allBlocks[k]);
                    }
                    delete[] allBlocks;
                }
            }
        }
    }
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        delete[] (*fileLines)[i];
    }
    delete fileLines;

    if (!blocksToInst.size()){
        for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
            BasicBlock* bb = getExposedBasicBlock(i);
            blocksToInst.insert(bb->getHashCode().getValue(), bb);
        }
    }

    if (dfpFile){

        Vector<char*>* dfpFileLines = new Vector<char*>();
        initializeFileList(dfpFile, dfpFileLines);

        ASSERT(!dfpSet.size());

        for (uint32_t i = 0; i < dfpFileLines->size(); i++){
            PRINT_INFOR("dfp line %d: %s", i, (*dfpFileLines)[i]);

            uint64_t id = 0;
            char patternString[__MAX_STRING_SIZE];
            int32_t err = sscanf((*dfpFileLines)[i], "%lld %s", &id, patternString);
            if(err <= 0){
                PRINT_ERROR("Line %d of %s has a wrong format", i+1, dfpFile);
            }
            DFPatternType dfpType = convertDFPatternType(patternString);
            if(dfpType == dfTypePattern_undefined){
                PRINT_ERROR("Line %d of %s is a wrong pattern type [%s]", i+1, dfpFile, patternString);
            } else {
                PRINT_INFOR("found valid pattern %s -> %d", patternString, dfpType);
            }
            HashCode hashCode(id);
#ifdef STATS_PER_INSTRUCTION
            if(!hashCode.isInstruction()){
#else //STATS_PER_INSTRUCTION
            if(!hashCode.isBlock()){
#endif //STATS_PER_INSTRUCTION
                PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block/instruction", i+1, dfpFile);
            }

            // if the bb is not in the list already but is a valid block, include it!
            BasicBlock* bb = findExposedBasicBlock(hashCode);
            if(!bb){
                PRINT_ERROR("Line %d of %s is not a valid basic block id", i+1, dfpFile);
                continue;
            }
            blocksToInst.insert(bb->getHashCode().getValue(), bb);

            if (dfpType != dfTypePattern_None){
                dfpSet.insert(bb->getHashCode().getValue(), dfpType);
            }
        }

        for (uint32_t i = 0; i < dfpFileLines->size(); i++){
            delete[] (*dfpFileLines)[i];
        }

        delete dfpFileLines;

        PRINT_INFOR("**** Number of basic blocks tagged for DFPattern %d (out of %d) ******",
                    dfpSet.size(), blocksToInst.size());
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

    uint32_t temp32;
    uint64_t temp64;
    
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    if (!isThreadedMode()){
        PRINT_WARN(20, "No optimization done for non-threaded mode");
        __FUNCTION_NOT_IMPLEMENTED;
    }

    // count number of memory ops
    uint32_t memopSeq = 0;
    uint32_t blockSeq = 0;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        blockSeq++;
        if (blocksToInst.get(bb->getHashCode().getValue())){
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);
                if (memop->isMemoryOperation()){
                    memopSeq++;
                }
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

    std::map<uint64_t, uint32_t>* functionThreading = threadReadyCode();

    // first entry in buffer is treated specially
    BufferEntry intro;
    intro.__buf_current = 0;
    intro.__buf_capacity = BUFFER_ENTRIES;

    SimulationStats stats;

    stats.Initialized = true;
    stats.InstructionCount = memopSeq;
    stats.Stats = NULL;
    if (isPerInstruction()){
        stats.PerInstruction = true;
        stats.BlockCount = memopSeq;
    } else {
        stats.PerInstruction = false;
        stats.BlockCount = blockSeq;
    }

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

    INIT_INSN_ELEMENT(uint32_t, BlockIds);
    INIT_INSN_ELEMENT(uint32_t, MemopIds);


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


    char* appName = getElfFile()->getAppName();
    uint64_t app = reserveDataOffset(strlen(appName) + 1);
    initializeReservedPointer(app, simulationStruct + offsetof(SimulationStats, Application));
    initializeReservedData(getInstDataAddress() + app, strlen(appName) + 1, (void*)appName);

    char extName[__MAX_STRING_SIZE];
    sprintf(extName, "%s\0", getExtension());
    uint64_t ext = reserveDataOffset(strlen(extName) + 1);
    initializeReservedPointer(ext, simulationStruct + offsetof(SimulationStats, Extension));
    initializeReservedData(getInstDataAddress() + ext, strlen(extName) + 1, (void*)extName);

    initializeReservedData(getInstDataAddress() + simulationStruct, sizeof(SimulationStats), (void*)(&stats));

    entryFunc->addArgument(simulationStruct);
    entryFunc->addArgument(imageKey);
    entryFunc->addArgument(threadHash);


    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_sysinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry function");
    }

    simFunc->addArgument(imageKey);
    simFunc->assumeNoFunctionFP();
    exitFunc->addArgument(imageKey);

    p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_sysinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    blockSeq = 0;
    memopSeq = 0;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        Function* f = (Function*)bb->getLeader()->getContainer();

        ASSERT(blockSeq == i);

        // set up a block counter that is distinct from all other inst points in the block
        uint64_t counterOffset = (uint64_t)stats.Counters + (i * sizeof(uint64_t));
        uint32_t threadReg = X86_REG_INVALID;

        if (isThreadedMode()){
            counterOffset -= simulationStruct;
            threadReg = (*functionThreading)[f->getBaseAddress()];
        }
        //InstrumentationTool::insertBlockCounter(counterOffset, bb, true, threadReg);
        
        temp64 = 0;
        initializeReservedData(getInstDataAddress() + (uint64_t)stats.Counters + (i * sizeof(uint64_t)), sizeof(uint64_t), &temp64);

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

            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Hashes + blockSeq*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Addresses + blockSeq*sizeof(uint64_t), sizeof(uint64_t), &addr);

            CounterTypes tmpct = CounterType_basicblock;
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Types + blockSeq*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);

            temp64 = 0;
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.Counters + blockSeq*sizeof(uint64_t), sizeof(uint64_t), &temp64);

            temp32 = bb->getNumberOfMemoryOps();
            initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopsPerBlock + blockSeq*sizeof(uint32_t), sizeof(uint32_t), &temp32);
        }

        if (blocksToInst.get(bb->getHashCode().getValue())){

            uint32_t memopIdInBlock = 0;
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);
                uint64_t currentOffset = (uint64_t)stats.Buffer + offsetof(BufferEntry, __buf_current);

                if (isThreadedMode()){
                    currentOffset -= (uint64_t)stats.Buffer;
                }

                if (memop->isMemoryOperation()){
                    // at the first memop in each block, check for a full buffer, clear if full
                    if (memopIdInBlock == 0){
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
                        delete inv;
                        delete dead;

                        InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_tramp, InstLocation_prior);
                        pt->setPriority(InstPriority_userinit);
                        dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_buffercheck), true);
                        Vector<X86Instruction*>* bufferDumpInstructions = new Vector<X86Instruction*>();

                        // put current buffer into sr2
                        // if thread data addr is not in sr1 already, load it
                        if (threadReg == X86_REG_INVALID){
                            Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
                            for (uint32_t k = 0; k < tdata->size(); k++){
                                bufferDumpInstructions->append((*tdata)[k]);
                            }
                            delete tdata;
                        }
                        
                        bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
                        bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr2, offsetof(BufferEntry, __buf_current), sr2));

                        //bufferDumpInstructions->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(BufferEntry, __buf_current), sr2));

                        // compare current buffer to buffer max
                        bufferDumpInstructions->append(X86InstructionFactory64::emitCompareImmReg(BUFFER_ENTRIES - bb->getNumberOfMemoryOps(), sr2));

                        // jump to non-buffer-jump code
                        bufferDumpInstructions->append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));

                        ASSERT(bufferDumpInstructions);
                        while (bufferDumpInstructions->size()){
                            pt->addPrecursorInstruction(bufferDumpInstructions->remove(0));
                        }

                        // if we include the buffer increment as part of the buffer check, it increments the buffer pointer even when we try to disable this point during buffer clearing
                        delete bufferDumpInstructions;

                        InstrumentationSnippet* snip = addInstrumentationSnippet();
                        pt = addInstrumentationPoint(memop, snip, InstrumentationMode_inline, InstLocation_prior);
                        pt->setPriority(InstPriority_regular);
                        dynamicPoint(pt, GENERATE_KEY(blockSeq, PointType_bufferinc), true);

                        if (threadReg == X86_REG_INVALID){
                            Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
                            for (uint32_t k = 0; k < tdata->size(); k++){
                                snip->addSnippetInstruction((*tdata)[k]);
                            }
                            delete tdata;
                        }

                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(bb->getNumberOfMemoryOps(), sr2, offsetof(BufferEntry, __buf_current)));
                    }

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
                    inv->insert(X86_REG_BP);
                    if (threadReg != X86_REG_INVALID){
                        inv->insert(threadReg);
                        sr1 = threadReg;
                    }
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
                    delete inv;
                    delete dead;

                    // if thread data addr is not in sr1 already, load it
                    if (threadReg == X86_REG_INVALID){
                        Vector<X86Instruction*>* tdata = storeThreadData(sr2, sr1);
                        for (uint32_t k = 0; k < tdata->size(); k++){
                            snip->addSnippetInstruction((*tdata)[k]);
                        }
                        delete tdata;
                    }

                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(SimulationStats, Buffer), sr2));
                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr2, offsetof(BufferEntry, __buf_current), sr3));
                    //snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(sr1, offsetof(BufferEntry, __buf_current), sr2));                    
                    snip->addSnippetInstruction(X86InstructionFactory64::emitShiftLeftLogical(logBase2(sizeof(BufferEntry)), sr3));

                    // sr1 holds the thread data addr (which points to SimulationStats)
                    // sr2 holds the base address of the buffer 
                    // sr3 holds the offset (in bytes) of the access

                    uint32_t bufferIdx = 1 + memopIdInBlock - bb->getNumberOfMemoryOps();
                    snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(sr2, sr3, 1, sizeof(BufferEntry) * bufferIdx, sr2, true, true));
                    // sr2 now holds the base of this memop's buffer entry

                    Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, sr3);
                    while (!(*addrStore).empty()){
                        snip->addSnippetInstruction((*addrStore).remove(0));
                    }
                    delete addrStore;
                    // sr3 holds the memory address being used by memop

                    
                    // put the 3 elements of a BufferEntry into place
                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, 0, true));
                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(memopSeq, sr3));
                    snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(sr3, sr2, offsetof(BufferEntry, memseq), true));

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
                        } else {
                            tmpct = CounterType_instruction;
                            temp64 = blockSeq;
                        }
                        initializeReservedData(getInstDataAddress() + (uint64_t)stats.Types + memopSeq*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);
                        initializeReservedData(getInstDataAddress() + (uint64_t)stats.Counters + (memopSeq * sizeof(uint64_t)), sizeof(uint64_t), &temp64);

                        temp32 = 1;
                        initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopsPerBlock + memopSeq*sizeof(uint32_t), sizeof(uint32_t), &temp32);
                    }
        
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.BlockIds + memopSeq*sizeof(uint32_t), sizeof(uint32_t), &blockSeq);
                    initializeReservedData(getInstDataAddress() + (uint64_t)stats.MemopIds + memopSeq*sizeof(uint32_t), sizeof(uint32_t), &memopIdInBlock);

                    memopIdInBlock++;
                    memopSeq++;
                }
            }
        }
        blockSeq++;
    }

    if (isPerInstruction()){
        printStaticFilePerInstruction(allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());
    } else {
        printStaticFile(allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());
    }

    delete[] nostring;

    delete allBlocks;
    delete allBlockIds;
    delete allBlockLineInfos;

    if (isThreadedMode()){
        delete functionThreading;
    }

    /*

#ifdef STATS_PER_INSTRUCTION
    PRINT_WARN(10, "Performing instrumentation to gather PER-INSTRUCTION statistics");
    uint32_t dfpCount = getNumberOfExposedInstructions();
#else //STATS_PER_INSTRUCTION
    uint32_t dfpCount = getNumberOfExposedBasicBlocks();
#endif //STATS_PER_INSTRUCTION
    uint64_t dfPatternStore = reserveDataOffset(sizeof(DFPatternSpec) * (dfpCount + 1));

    if(dfpSet.size()){
        DFPatternSpec dfInfo;
        dfInfo.memopCnt = 0;
        dfInfo.type = DFPattern_Active;

        initializeReservedData(getInstDataAddress() + dfPatternStore, sizeof(DFPatternSpec), (void*)&dfInfo);
    }

    uint64_t entryCountStore = reserveDataOffset(sizeof(uint64_t));
    uint32_t startValue = BUFFER_ENTRIES;

    initializeReservedData(getInstDataAddress() + entryCountStore, sizeof(uint64_t), &startValue);

    uint64_t blockSizeStore = reserveDataOffset(sizeof(uint64_t));

    char* appName = getElfFile()->getAppName();
    char ext[__MAX_STRING_SIZE];
    sprintf(ext, "%s\0", getExtension());

    uint32_t phaseId = phaseNo;

    // this sets ext to ext without the phase string
    uint32_t ndots = 0;
    if (phaseNo > 0){
        int i = 0;
        int l = strlen(ext);
        while (i < l){
            if (ext[i] == '.'){
                ndots++;
            }
            if (ndots == 2){
                break;
            }
            i++;
        }        
        sprintf(ext, "%s", getExtension() + i + 1);
    }

    uint32_t dumpCode = 0;
    uint32_t commentSize = strlen(appName) + sizeof(uint32_t) + strlen(ext) + sizeof(uint32_t) + sizeof(uint32_t) + 4;
    uint64_t commentStore = reserveDataOffset(commentSize);
    char* comment = new char[commentSize];
#ifndef STATS_PER_INSTRUCTION
    sprintf(comment, "%s %u %s %u %u", appName, phaseId, ext, getNumberOfExposedBasicBlocks(), dumpCode);
#else
    sprintf(comment, "%s %u %s %u %u", appName, phaseId, ext, getNumberOfExposedInstructions(), dumpCode);
    uint32_t insnToBlock[getNumberOfExposedInstructions()];
#endif
    initializeReservedData(getInstDataAddress() + commentStore, commentSize, comment);

    simFunc->addArgument(bufferStore);
    simFunc->addArgument(entryCountStore);
    simFunc->addArgument(commentStore);

    exitFunc->addArgument(bufferStore);
    exitFunc->addArgument(entryCountStore);
    exitFunc->addArgument(commentStore);

    uint64_t counterArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(uint64_t));
    uint64_t killedArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(char));


    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
    ASSERT(p);

    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();
#ifdef STATS_PER_INSTRUCTION
    Vector<X86Instruction*>* allInstructions = new Vector<X86Instruction*>();
    Vector<uint32_t>* allInstructionIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allInstructionLineInfos = new Vector<LineInfo*>();
#endif //STATS_PER_INSTRUCTION

    uint32_t blockId = 0;
    uint32_t memopId = 0;
    uint32_t regDefault = 0;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);

        if (blocksToInst.get(bb->getHashCode().getValue())){

            (*allBlocks).append(bb);
            (*allBlockIds).append(blockId);
            if (lineInfoFinder){
                (*allLineInfos).append(lineInfoFinder->lookupLineInfo(bb));
            } else {
                (*allLineInfos).append(NULL);
            }
            uint32_t memopIdInBlock = 0;
            
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);
#ifdef STATS_PER_INSTRUCTION
                ASSERT(blockId == i);
                insnToBlock[memopId] = blockId;
#endif
                if (memop->isMemoryOperation()){            
                    //PRINT_INFOR("The following instruction has %d membytes", memop->getNumberOfMemoryBytes());
                    //memop->print();
#ifdef STATS_PER_INSTRUCTION
                    (*allInstructions).append(memop);
                    (*allInstructionIds).append(memopId);
                    if (lineInfoFinder){
                        (*allInstructionLineInfos).append(lineInfoFinder->lookupLineInfo(memop));
                    } else {
                        (*allInstructionLineInfos).append(NULL);
                    }
#endif //STATS_PER_INSTRUCTION
                    
                    if (getElfFile()->is64Bit()){

                        // put the memory address in tmp1
                        Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, tmpReg1);
                        while (!(*addrStore).empty()){
                            snip->addSnippetInstruction((*addrStore).remove(0));
                        }
                        delete addrStore;
                        
                        // 24 bytes per buffer entry -- 8 bytes for mem addr, 8 bytes for threadid, 8 bytes for source identification.
                        // put the current buffer address in tmp2
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2, false));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                        
                        // fill the buffer entry with this block's info
#ifdef STATS_PER_INSTRUCTION
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(0, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(memopId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#else //STATS_PER_INSTRUCTION
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#endif //STATS_PER_INSTRUCTION
                        if (usesLiveReg){
                            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t), tmpReg2, true));
                            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t), tmpReg1, true));
                        }
                        
                        // generateAddressComputation doesn't def any flags, so no protection is necessary
                        InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, InstLocation_prior);
                        memInstPoints.append(pt);
                    }
#ifdef STATS_PER_INSTRUCTION
                    memInstBlockIds.append(memopId);
#else //STATS_PER_INSTRUCTION
                    memInstBlockIds.append(blockId);
#endif
                    memopIdInBlock++;
                }
                if (memopIdInBlock >= MAX_MEMOPS_PER_BLOCK){
                    PRINT_ERROR("Block @%#llx in function %s has %d memops (limited by MAX_MEMOPS_PER_BLOCK=%d)", 
                                bb->getProgramAddress(), bb->getFunction()->getName(), bb->getNumberOfMemoryOps(), MAX_MEMOPS_PER_BLOCK);
                   
                }
                ASSERT(memopIdInBlock < MAX_MEMOPS_PER_BLOCK && "Too many memory ops in some basic block... try increasing MAX_MEMOPS_PER_BLOCK");

                memopId++;
            }

#ifndef DISABLE_BLOCK_COUNT
            uint64_t counterOffset = counterArray + (i * sizeof(uint64_t));
            InstrumentationTool::insertBlockCounter(counterOffset, bb);
#endif
        } else {
            memopId += bb->getNumberOfInstructions();
        }
        
        blockId++;
    }
        
    ASSERT(memInstPoints.size() && "There are no memory operations found through the filter");

#ifdef STATS_PER_INSTRUCTION
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* ins = getExposedInstruction(i);
        Function* f = (Function*)ins->getContainer();
        BasicBlock* bb = f->getBasicBlockAtAddress(ins->getBaseAddress());
        
        DFPatternSpec spec;
        spec.memopCnt = (uint32_t)ins->isMemoryOperation();
        spec.type = dfTypePattern_None;

        DFPatternType dfpType = dfTypePattern_None;
        if (dfpSet.get(bb->getHashCode().getValue(), &dfpType) && ins->isMemoryOperation()){
            spec.type = dfpType;
        }
        initializeReservedData(getInstDataAddress() + dfPatternStore + (i+1)*sizeof(DFPatternSpec), sizeof(DFPatternSpec), &spec);
    }
#else //STATS_PER_INSTRUCTION
    for (uint32_t i = 0; i < dfpCount; i++){
        BasicBlock* bb = getExposedBasicBlock(i);

        DFPatternType dfpType = dfTypePattern_None;
        DFPatternSpec spec;
        if (dfpSet.get(bb->getHashCode().getValue(), &dfpType)){
            //            PRINT_INFOR("found dfpattern for block %d (hash %#lld)", i, bb->getHashCode().getValue());
        } else {
            //            PRINT_INFOR("not doing dfpattern for block %d (hash %#lld)", i, bb->getHashCode().getValue());
        }
        spec.type = dfpType;
        spec.memopCnt = bb->getNumberOfMemoryOps();
        initializeReservedData(getInstDataAddress() + dfPatternStore + (i+1)*sizeof(DFPatternSpec), sizeof(DFPatternSpec), &spec);
    }
#endif //STATS_PER_INSTRUCTION

    instPointInfo = reserveDataOffset(sizeof(instpoint_info) * memInstPoints.size());
    entryFunc->addArgument(instPointInfo);

    uint64_t instPointCount = reserveDataOffset(sizeof(uint32_t));
    uint64_t blockCount = reserveDataOffset(sizeof(uint32_t));
    temp32 = memInstPoints.size();
    initializeReservedData(getInstDataAddress() + instPointCount, sizeof(uint32_t), &temp32);
    temp32 = blockId;
    initializeReservedData(getInstDataAddress() + blockCount, sizeof(uint32_t), &temp32);
    entryFunc->addArgument(instPointCount);
    entryFunc->addArgument(blockCount);
    entryFunc->addArgument(counterArray);
    entryFunc->addArgument(killedArray);
#ifdef STATS_PER_INSTRUCTION
    uint64_t mapArray = reserveDataOffset(sizeof(int32_t) * getNumberOfExposedInstructions());
    initializeReservedData(getInstDataAddress() + mapArray, sizeof(int32_t) * getNumberOfExposedInstructions(), &insnToBlock);
    entryFunc->addArgument(mapArray);
#endif

#ifdef NO_REG_ANALYSIS
    PRINT_WARN(10, "Warning: register analysis disabled");
#endif

#ifdef STATS_PER_INSTRUCTION
    printStaticFilePerInstruction(allInstructions, allInstructionIds, allInstructionLineInfos, allInstructions->size());
#else //STATS_PER_INSTRUCTION
    printStaticFile(allBlocks, allBlockIds, allLineInfos, BUFFER_ENTRIES);
#endif //STATS_PER_INSTRUCTION

    delete allBlocks;
    delete allBlockIds;
    delete allLineInfos;
    delete[] comment;

#ifdef STATS_PER_INSTRUCTION
    delete allInstructions;
    delete allInstructionIds;
    delete allInstructionLineInfos;
#endif //STATS_PER_INSTRUCTION
*/
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

void CacheSimulation::printDFPStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allLineInfos){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    ASSERT((*allBlocks).size() == (*allBlockIds).size());

    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), getExtension(), "dfp");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# phase     = %d\n", 0);
    fprintf(staticFD, "# blocks    = %d\n", dfpSet.size());
    char* sha1sum = getElfFile()->getSHA1Sum();
    fprintf(staticFD, "# sha1sum   = %s\n", sha1sum);
    delete[] sha1sum;
    for (uint32_t i = 0; i < getNumberOfInstrumentationLibraries(); i++){
        fprintf(staticFD, "# library   = %s\n", getInstrumentationLibrary(i));
    }
    fprintf(staticFD, "# libTag    = %s\n", "revision REVISION");
    fprintf(staticFD, "# <sequence> <block_unqid> <idiom> <loads> <stores> <line> <fname> # <vaddr>\n");
    if (printDetail){
        fprintf(staticFD, "# +lpi <loopcnt> <loopid> <ldepth>\n");
    }

    for (uint32_t i = 0; i < numberOfInstPoints; i++){
        BasicBlock* bb = (*allBlocks)[i];
        LineInfo* li = (*allLineInfos)[i];
        Function* f = bb->getFunction();

        uint32_t loopId = Invalid_UInteger_ID;
        Loop* loop = bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex());
        if (loop){
            loopId = loop->getIndex();
        }
        uint32_t loopDepth = bb->getFlowGraph()->getLoopDepth(bb->getIndex());
        uint32_t loopCount = bb->getFlowGraph()->getNumberOfLoops();

        char* fileName;
        uint32_t lineNo;
        if (li){
            fileName = li->getFileName();
            lineNo = li->GET(lr_line);
        } else {
            fileName = INFO_UNKNOWN;
            lineNo = 0;
        }

        DFPatternType dfpType = dfTypePattern_None;
        if (dfpSet.get((*allBlocks)[i]->getHashCode().getValue(), &dfpType)){
            fprintf(staticFD, "%d\t%lld\t%d\t%d\t%d\t%s:%d\t%s # %llx\n",
                    (*allBlockIds)[i], bb->getHashCode().getValue(), dfpType, bb->getNumberOfLoads(), 
                    bb->getNumberOfStores(), fileName, lineNo, bb->getFunction()->getName(),
                    bb->getLeader()->getProgramAddress());

            if (printDetail){
                fprintf(staticFD, "\t+lpi\t%d\t%d\t%d # %#llx\n", loopCount, loopId, loopDepth, bb->getHashCode().getValue());
            }
        }

    }
}
