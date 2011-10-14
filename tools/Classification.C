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

#include <Classification.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <LineInformation.h>

#define ENTRY_FUNCTION "bins_entry_function"
#define BIN_FUNCTION "bins_classify_function"
#define EXIT_FUNCTION "bins_exit_function"
#define INST_LIB_NAME "libclassifier.so"

#define BINS_NUMBER (27)
#define INSTRUCTIONS_THRESHOLD (1000000)

Classification::Classification(ElfFile* elf, char* ext, bool lpi, bool dtl)
    : InstrumentationTool(elf, ext, 0, lpi, dtl)
{
    binFunc = NULL;
    exitFunc = NULL;
    entryFunc = NULL;
}


Classification::~Classification(){
}

void Classification::declare(){
    InstrumentationTool::declare();
    declareLibrary(INST_LIB_NAME);
    binFunc = declareFunction(BIN_FUNCTION);
    ASSERT(binFunc && "Cannot find binning print function, are you sure it was declared?");
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");
    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");
}

void Classification::addInt_Store(Vector<X86Instruction*>& instructions, int x, uint64_t store){
    while(x >= 127) {
        if (is64Bit())
            instructions.append(X86InstructionFactory64::emitAddImmByteToMem64(127, getInstDataAddress()+store));
        else
            instructions.append(X86InstructionFactory32::emitAddImmByteToMem(127, getInstDataAddress()+store));
        x -= 127;
    }
    if (x > 0) {
        if (is64Bit())
            instructions.append(X86InstructionFactory64::emitAddImmByteToMem64(x, getInstDataAddress()+store));
        else
            instructions.append(X86InstructionFactory32::emitAddImmByteToMem(x, getInstDataAddress()+store));
    }
}

void Classification::instrument(){
    InstrumentationTool::instrument();

    uint64_t bufferStore = reserveDataOffset(BINS_NUMBER*sizeof(uint64_t));
    uint64_t instructionsCountStore = reserveDataOffset(sizeof(uint64_t));

    uint32_t traceNameSize = strlen(getElfFile()->getAppName())+7+strlen(extension);
    uint64_t traceNameStore = reserveDataOffset(traceNameSize);
    char* traceName = new char[traceNameSize];
    sprintf(traceName,"%s.0000.%s", getElfFile()->getAppName(), extension);
    initializeReservedData(getInstDataAddress()+traceNameStore, traceNameSize, traceName);
    delete[] traceName;

    entryFunc->addArgument(bufferStore);
    entryFunc->addArgument(instructionsCountStore);
    entryFunc->addArgument(traceNameStore);

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
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation())
        lineInfoFinder = getLineInfoFinder();

    uint32_t blockId = 0;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        uint64_t tmpBufferStore = bufferStore;

        allBlocks->append(bb);
        allBlockIds->append(blockId);
        if(lineInfoFinder)
            allLineInfos->append(lineInfoFinder->lookupLineInfo(bb));

        FlagsProtectionMethods prot = FlagsProtectionMethod_full;
        X86Instruction* bestinst = bb->getExitInstruction();
        for (int32_t j = bb->getNumberOfInstructions() - 1; j >= 0; j--){
            if (bb->getInstruction(j)->allFlagsDeadIn()){
                bestinst = bb->getInstruction(j);
                prot = FlagsProtectionMethod_none;
                break;
            }
        }
        
        uint32_t tmpReg = X86_REG_AX;
        p = addInstrumentationPoint(bestinst, binFunc, InstrumentationMode_trampinline, prot, InstLocation_prior);
        p->setPriority(InstPriority_low);
        Vector<X86Instruction*> bufferDumpInstructions;

        bb->setBins();
        addInt_Store(bufferDumpInstructions, bb->getNumberOfInstructions(), instructionsCountStore);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinUnknown(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinInvalid(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinCond(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinUncond() , tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinBin(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinBinv(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinByte(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinBytev(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinWord(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinWordv(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinDword(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinDwordv(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinQword(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinQwordv(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinSingle(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinSinglev(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinSingles(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinDouble(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinDoublev(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinDoubles(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinMove(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinStack(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinString(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinSystem(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinCache(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinMem(), tmpBufferStore); tmpBufferStore += sizeof(uint64_t);
        addInt_Store(bufferDumpInstructions, bb->getNumberOfBinOther(), tmpBufferStore);
        
        if (is64Bit()) {
            bufferDumpInstructions.append(X86InstructionFactory64::emitMoveRegToMem(tmpReg, getInstDataAddress()+getRegStorageOffset()+(sizeof(uint64_t))));
            bufferDumpInstructions.append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress()+instructionsCountStore, tmpReg, true));
            bufferDumpInstructions.append(X86InstructionFactory64::emitCompareImmReg(INSTRUCTIONS_THRESHOLD, tmpReg));
            bufferDumpInstructions.append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress()+getRegStorageOffset()+(sizeof(uint64_t)), tmpReg, true));
        } else {
            bufferDumpInstructions.append(X86InstructionFactory32::emitMoveRegToMem(tmpReg, getInstDataAddress()+getRegStorageOffset()+(sizeof(uint64_t))));
            bufferDumpInstructions.append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress()+instructionsCountStore, tmpReg));
            bufferDumpInstructions.append(X86InstructionFactory32::emitCompareImmReg(INSTRUCTIONS_THRESHOLD, tmpReg));
            bufferDumpInstructions.append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress()+getRegStorageOffset()+sizeof(uint64_t), tmpReg));
        }
        bufferDumpInstructions.append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));

        while (bufferDumpInstructions.size())
            p->addPrecursorInstruction(bufferDumpInstructions.remove(0));

        blockId++;
    }
    
    if(allLineInfos->size())
        printStaticFile(allBlocks, allBlockIds, allLineInfos, allBlocks->size());
    delete allBlocks;
    delete allBlockIds;
    delete allLineInfos;
}
