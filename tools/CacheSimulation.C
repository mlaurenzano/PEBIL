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
#include <SimpleHash.h>

#define ENTRY_FUNCTION "entry_function"
#define SIM_FUNCTION "MetaSim_simulFuncCall_Simu"
#define EXIT_FUNCTION "MetaSim_endFuncCall_Simu"
#define INST_LIB_NAME "libsimulator.so"
#define Size__BufferEntry 16
#define USABLE_BUFFER_SIZE 0x00010000
#define MAX_MEMOPS_PER_BLOCK 8192
#define BUFFER_ENTRIES (USABLE_BUFFER_SIZE + MAX_MEMOPS_PER_BLOCK)

//#define DISABLE_BLOCK_COUNT

void CacheSimulation::usesModifiedProgram(){
    X86Instruction* nop5Byte = X86InstructionFactory::emitNop(Size__uncond_jump);
    instpoint_info iinf;
    bzero(&iinf, sizeof(instpoint_info));
    iinf.pt_size = Size__uncond_jump;
    memcpy(iinf.pt_disable, nop5Byte->charStream(), iinf.pt_size);

    for (uint32_t i = 0; i < memInstPoints.size(); i++){
        ASSERT(memInstPoints[i]->getInstrumentationMode() != InstrumentationMode_inline);
        iinf.pt_vaddr = memInstPoints[i]->getInstSourceAddress();
        iinf.pt_blockid = memInstBlockIds[i];
        //PRINT_INFOR("mem point %d (block %d) initialized at addr %#llx", i, iinf.pt_blockid, getInstDataAddress() + instPointInfo + (i * sizeof(instpoint_info)));
        initializeReservedData(getInstDataAddress() + instPointInfo + (i * sizeof(instpoint_info)), sizeof(instpoint_info), &iinf);
    }    

    delete nop5Byte;
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
    initializeFileList(bbFile, fileLines);

    int32_t err;
    uint64_t inputHash;
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        char* ptr = strchr((*fileLines)[i],'#');
        if(ptr) *ptr = '\0';

        if(!strlen((*fileLines)[i]) || allSpace((*fileLines)[i]))
            continue;

        err = sscanf((*fileLines)[i], "%lld", &inputHash);
        if(err <= 0){
            PRINT_ERROR("Line %d of %s has a wrong format", i+1, bbFile);
        }
        HashCode* hashCode = new HashCode(inputHash);
#ifdef STATS_PER_INSTRUCTION
        if(!hashCode->isBlock() && !hashCode->isInstruction()){
#else //STATS_PER_INSTRUCTION
        if(!hashCode->isBlock()){
#endif //STATS_PER_INSTRUCTION
            PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block/instruction", i+1, bbFile);
        }
        BasicBlock* bb = findExposedBasicBlock(*hashCode);
        delete hashCode;

        if (!bb){
            PRINT_WARN(10, "cannot find basic block for hash code %#llx found in input file", inputHash);
        } else {
            //        ASSERT(bb && "cannot find basic block for hash code found in input file");
            blocksToInst.insert(bb->getHashCode().getValue(), bb);
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

    // if any loop contains blocks that are in our list, include all blocks from those loops
    if (loopIncl){
        for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
            BasicBlock* bb = getExposedBasicBlock(i);
            uint64_t hashValue = bb->getHashCode().getValue();

            if (blocksToInst.get(hashValue)){
                if (bb->isInLoop()){
                    FlowGraph* fg = bb->getFlowGraph();
                    for (uint32_t j = 0; j < fg->getNumberOfLoops(); j++){
                        Loop* lp = fg->getLoop(j);
                        if (lp->isBlockIn(bb->getIndex())){
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
        }
    }

    BasicBlock** bbs = blocksToInst.values();
    qsort(bbs, blocksToInst.size(), sizeof(BasicBlock*), compareBaseAddress);
    
    if (dfPatternFile){

        Vector<char*>* dfpFileLines = new Vector<char*>();
        initializeFileList(dfPatternFile, dfpFileLines);

        ASSERT(!dfpSet.size());

        for (uint32_t i = 0; i < dfpFileLines->size(); i++){
            PRINT_INFOR("dfp line %d: %s", i, (*dfpFileLines)[i]);

            uint64_t id = 0;
            char patternString[__MAX_STRING_SIZE];
            int32_t err = sscanf((*dfpFileLines)[i], "%lld %s", &id, patternString);
            if(err <= 0){
                PRINT_ERROR("Line %d of %s has a wrong format", i+1, dfPatternFile);
            }
            DFPatternType dfpType = convertDFPatternType(patternString);
            if(dfpType == dfTypePattern_undefined){
                PRINT_ERROR("Line %d of %s is a wrong pattern type [%s]", i+1, dfPatternFile, patternString);
            } else {
                PRINT_INFOR("found valid pattern %s -> %d", patternString, dfpType);
            }
            HashCode hashCode(id);
#ifdef STATS_PER_INSTRUCTION
            if(!hashCode.isBlock() && !hashCode.isInstruction()){
#else //STATS_PER_INSTRUCTION
            if(!hashCode.isBlock()){
#endif //STATS_PER_INSTRUCTION
                PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block/instruction", i+1, dfPatternFile);
            }

            // if the bb is not in the list already but is a valid block, include it!
            BasicBlock* bb = findExposedBasicBlock(hashCode);
            if(!bb){
                PRINT_ERROR("Line %d of %s is not a valid basic block id", i+1, dfPatternFile);
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

    ASSERT(!dfpBlocks.size() || dfpBlocks.size() == blocksToInst.size());
    delete[] bbs;
}


CacheSimulation::CacheSimulation(ElfFile* elf, char* inputFile, char* ext, uint32_t phase, bool lpi, bool dtl, char* dfpFile)
    : InstrumentationTool(elf, ext, phase, lpi, dtl)
{
    simFunc = NULL;
    exitFunc = NULL;
    entryFunc = NULL;

    bbFile = inputFile;
    dfPatternFile = dfpFile;
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
    
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    ASSERT(isPowerOfTwo(Size__BufferEntry));

    uint64_t bufferStore  = reserveDataOffset(BUFFER_ENTRIES * Size__BufferEntry);
    char* emptyBuff = new char[BUFFER_ENTRIES * Size__BufferEntry];
    bzero(emptyBuff, BUFFER_ENTRIES * Size__BufferEntry);
    emptyBuff[0] = 1;
    initializeReservedData(getInstDataAddress() + bufferStore, BUFFER_ENTRIES * Size__BufferEntry, emptyBuff);
    delete[] emptyBuff;

    
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
    char* ext = extension;
    uint32_t phaseId = phaseNo;
    uint32_t dumpCode = 0;
    uint32_t commentSize = strlen(appName) + sizeof(uint32_t) + strlen(extension) + sizeof(uint32_t) + sizeof(uint32_t) + 4;
    uint64_t commentStore = reserveDataOffset(commentSize);
    char* comment = new char[commentSize];
#ifndef STATS_PER_INSTRUCTION
    sprintf(comment, "%s %u %s %u %u", appName, phaseId, extension, getNumberOfExposedBasicBlocks(), dumpCode);
#else
    sprintf(comment, "%s %u %s %u %u", appName, phaseId, extension, getNumberOfExposedInstructions(), dumpCode);
    Vector<int32_t> insnToBlock;
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

    for (uint32_t i = 0; i < dfpCount; i++){
#ifdef STATS_PER_INSTRUCTION
        X86Instruction* ins = getExposedInstruction(i);
        Function* f = (Function*)ins->getContainer();
        BasicBlock* bb = f->getBasicBlockAtAddress(ins->getBaseAddress());

        DFPatternType dfpType = dfTypePattern_None;
        DFPatternSpec spec;
        if (dfpSet.get(bb->getHashCode().getValue(), &dfpType)){
            //            PRINT_INFOR("found dfpattern for block %d (hash %#lld)", i, bb->getHashCode().getValue());
        } else {
            //            PRINT_INFOR("not doing dfpattern for block %d (hash %#lld)", i, bb->getHashCode().getValue());
        }
        spec.memopCnt = (uint32_t)ins->isMemoryOperation();
        spec.type = dfTypePattern_None;
        if (ins->isMemoryOperation()){
            spec.type = dfpType;
        }
#else //STATS_PER_INSTRUCTION
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
#endif //STATS_PER_INSTRUCTION
        initializeReservedData(getInstDataAddress() + dfPatternStore + (i+1)*sizeof(DFPatternSpec), sizeof(DFPatternSpec), &spec);
    }

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
                insnToBlock.append(blockId);
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
                        // check the buffer at the last memop
                        if (memopIdInBlock == bb->getNumberOfMemoryOps() - 1){
                            FlagsProtectionMethods prot = FlagsProtectionMethod_full;
#ifndef NO_REG_ANALYSIS
                            if (memop->allFlagsDeadIn()){
                                prot = FlagsProtectionMethod_none;
                            }
#endif
                            uint32_t tmpReg1 = X86_REG_CX;
                            uint32_t tmpReg2 = X86_REG_DX;
                            uint32_t tmpReg3 = X86_REG_AX;
                            
                            InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_trampinline, prot, InstLocation_prior);
                            pt->setPriority(InstPriority_low);
                            Vector<X86Instruction*>* bufferDumpInstructions = new Vector<X86Instruction*>();
                            
                            // save temp regs
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t))));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t))));
                            
                            // put the memory address in tmp1
                            Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, tmpReg1);
                            
                            while (!(*addrStore).empty()){
                                (*bufferDumpInstructions).append((*addrStore).remove(0));
                            }
                            
                            delete addrStore;
                            
                            // put the current buffer address in tmp2
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2, false));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                            
                            // fill the buffer entry with this block's info
#ifdef STATS_PER_INSTRUCTION
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveImmToRegaddrImm(0, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveImmToRegaddrImm(memopId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#else //STATS_PER_INSTRUCTION
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#endif //STATS_PER_INSTRUCTION
                            // update the buffer counter
                            uint32_t maxMemopsInSuccessor = MAX_MEMOPS_PER_BLOCK;
                            ASSERT(bb->getNumberOfMemoryOps() < MAX_MEMOPS_PER_BLOCK);
                            uint32_t memcnt = bb->getNumberOfMemoryOps();
                            while (memcnt > 0x7f){
                                (*bufferDumpInstructions).append(X86InstructionFactory64::emitAddImmByteToMem64(0x7f, getInstDataAddress() + bufferStore));
                                memcnt -= 0x7f;
                            }
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitAddImmByteToMem64(memcnt, getInstDataAddress() + bufferStore));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg1, false));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitCompareImmReg(BUFFER_ENTRIES - maxMemopsInSuccessor, tmpReg1));
                            
                            // restore regs
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t)), tmpReg2, true));
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t)), tmpReg1, true));
                            
                            // jump to non-buffer-jump code
                            (*bufferDumpInstructions).append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));
                            
                            ASSERT(bufferDumpInstructions);
                            while ((*bufferDumpInstructions).size()){
                                pt->addPrecursorInstruction((*bufferDumpInstructions).remove(0));
                            }
                            delete bufferDumpInstructions;
                            memInstPoints.append(pt);
                        } else {
                        // TODO: get which gprs are dead at this point and use one of those 
                            InstrumentationSnippet* snip = new InstrumentationSnippet();
                            addInstrumentationSnippet(snip);
                            
                            uint32_t tmpReg1 = X86_REG_CX;
                            uint32_t tmpReg2 = X86_REG_DX;
                            bool usesLiveReg = true;
                            ASSERT(tmpReg1 < X86_64BIT_GPRS);
                            
                            // save 2 temp regs
                            if (usesLiveReg){
                                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t)));
                                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t)));
                            }
                            
                            // put the memory address in tmp1
                            Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, tmpReg1);
                            while (!(*addrStore).empty()){
                                snip->addSnippetInstruction((*addrStore).remove(0));
                            }
                            delete addrStore;
                            
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
                            InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none, InstLocation_prior);
                            memInstPoints.append(pt);
                        } 
                    } else {
                        
                        // check the buffer at the last memop
                        if (memopIdInBlock == bb->getNumberOfMemoryOps() - 1){
                            FlagsProtectionMethods prot = FlagsProtectionMethod_full;
#ifndef NO_REG_ANALYSIS
                            if (memop->allFlagsDeadIn()){
                                prot = FlagsProtectionMethod_none;
                            }
#endif
                            uint32_t tmpReg1 = X86_REG_CX;
                            uint32_t tmpReg2 = X86_REG_DX;
                            uint32_t tmpReg3 = X86_REG_AX;
                            
                            InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_trampinline, prot, InstLocation_prior);
                            pt->setPriority(InstPriority_low);
                            Vector<X86Instruction*>* bufferDumpInstructions = new Vector<X86Instruction*>();
                            
                            // save temp regs
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t))));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t))));
                            
                            // put the memory address in tmp1
                            Vector<X86Instruction*>* addrStore = X86InstructionFactory32::emitAddressComputation(memop, tmpReg1);
                            while (!(*addrStore).empty()){
                                (*bufferDumpInstructions).append((*addrStore).remove(0));
                            }
                            delete addrStore;
                            
                            // put the current buffer address in tmp2
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                            
                            // fill the buffer entry with this block's info
#ifdef STATS_PER_INSTRUCTION
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveImmToRegaddrImm(0, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveImmToRegaddrImm(memopId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#else //STATS_PER_INSTRUCTION
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#endif //STATS_PER_INSTRUCTION
                            // update the buffer counter
                            uint32_t maxMemopsInSuccessor = MAX_MEMOPS_PER_BLOCK;
                            ASSERT(bb->getNumberOfMemoryOps() < MAX_MEMOPS_PER_BLOCK);
                            uint32_t memcnt = bb->getNumberOfMemoryOps();
                            while (memcnt > 0x7f){
                                (*bufferDumpInstructions).append(X86InstructionFactory32::emitAddImmByteToMem(0x7f, getInstDataAddress() + bufferStore));
                                memcnt -= 0x7f;
                            }
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitAddImmByteToMem(memcnt, getInstDataAddress() + bufferStore));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg1));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitCompareImmReg(BUFFER_ENTRIES - maxMemopsInSuccessor, tmpReg1));
                            
                            // restore regs
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t)), tmpReg2));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t)), tmpReg1));
                            
                            // jump to non-buffer-jump code
                            (*bufferDumpInstructions).append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));
                            
                            ASSERT(bufferDumpInstructions);
                            while ((*bufferDumpInstructions).size()){
                                pt->addPrecursorInstruction((*bufferDumpInstructions).remove(0));
                            }
                            delete bufferDumpInstructions;
                            memInstPoints.append(pt);
                        } else {
                            // TODO: get which gprs are dead at this point and use one of those 
                            InstrumentationSnippet* snip = new InstrumentationSnippet();
                            addInstrumentationSnippet(snip);
                            
                            uint32_t tmpReg1 = X86_REG_CX;
                            uint32_t tmpReg2 = X86_REG_DX;
                            bool usesLiveReg = true;
                            ASSERT(tmpReg1 < X86_64BIT_GPRS);
                            
                            // save 2 temp regs
                            if (usesLiveReg){
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t)));
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t)));
                            }
                            
                            // put the memory address in tmp1
                            Vector<X86Instruction*>* addrStore = X86InstructionFactory32::emitAddressComputation(memop, tmpReg1);
                            while (!(*addrStore).empty()){
                                snip->addSnippetInstruction((*addrStore).remove(0));
                            }
                            delete addrStore;
                            
                            // put the current buffer address in tmp2
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                            
                            // fill the buffer entry with this block's info
#ifdef STATS_PER_INSTRUCTION
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveImmToRegaddrImm(0, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveImmToRegaddrImm(memopId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#else //STATS_PER_INSTRUCTION
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
#endif //STATS_PER_INSTRUCTION
                            if (usesLiveReg){
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t), tmpReg2));
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t), tmpReg1));
                            }
                            
                            // generateAddressComputation doesn't def any flags, so no protection is necessary
                            InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none, InstLocation_prior);
                            memInstPoints.append(pt);
                        }
                    }
                    memInstBlockIds.append(blockId);
                    memopIdInBlock++;
                }
                memopId++;
                if (memopIdInBlock >= MAX_MEMOPS_PER_BLOCK){
                    PRINT_ERROR("Block @%#llx in function %s has %d memops (limited by MAX_MEMOPS_PER_BLOCK=%d)", 
                                bb->getProgramAddress(), bb->getFunction()->getName(), bb->getNumberOfMemoryOps(), MAX_MEMOPS_PER_BLOCK);
                   
                }
                ASSERT(memopIdInBlock < MAX_MEMOPS_PER_BLOCK && "Too many memory ops in some basic block... try increasing MAX_MEMOPS_PER_BLOCK");
            }

#ifndef DISABLE_BLOCK_COUNT
            InstrumentationSnippet* snip = new InstrumentationSnippet();
            addInstrumentationSnippet(snip);
        
            uint64_t counterOffset = counterArray + (i * sizeof(uint64_t));
            ASSERT(i == blockId);
            if (is64Bit()){
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmByteToMem64(1, getInstDataAddress() + counterOffset));
            } else {
                snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
            }
            
            FlagsProtectionMethods prot = FlagsProtectionMethod_light;
            X86Instruction* bestinst = bb->getExitInstruction();
            for (int32_t j = bb->getNumberOfInstructions() - 1; j >= 0; j--){
                if (bb->getInstruction(j)->allFlagsDeadIn()){
                    bestinst = bb->getInstruction(j);
                    prot = FlagsProtectionMethod_none;
                    break;
                }
            }
            
            InstrumentationPoint* p = addInstrumentationPoint(bestinst, snip, InstrumentationMode_inline, prot, InstLocation_prior);
#endif
        }
        
        blockId++;
    }
        
    ASSERT(memInstPoints.size() && "There are no memory operations found through the filter");

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
    uint64_t mapArray = reserveDataOffset(sizeof(int32_t) * insnToBlock.size());
    initializeReservedData(getInstDataAddress() + mapArray, sizeof(int32_t) * insnToBlock.size(), &insnToBlock);
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

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

void CacheSimulation::printDFPStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allLineInfos){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed");

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    ASSERT((*allBlocks).size() == (*allBlockIds).size());

    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), getInstSuffix(), "dfp");
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
