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

#include <FunctionTimer.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>

#define PROGRAM_ENTRY  "program_entry"
#define PROGRAM_EXIT   "program_exit"
#define FUNCTION_ENTRY "function_entry"
#define FUNCTION_EXIT  "function_exit"
#define INST_LIB_NAME "libtimer.so"

extern "C" {
    InstrumentationTool* FunctionTimerMaker(ElfFile* elf){
        return new FunctionTimer(elf);
    }
}

FunctionTimer::FunctionTimer(ElfFile* elf)
    : InstrumentationTool(elf)
{
    programEntry = NULL;
    programExit = NULL;

    functionEntry = NULL;
    functionExit = NULL;
    functionEntryFlagsSafe = NULL;
    functionExitFlagsSafe = NULL;
}

void FunctionTimer::declare(){
    InstrumentationTool::declare();

    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    programEntry = declareFunction(PROGRAM_ENTRY);
    ASSERT(programEntry);
    programExit = declareFunction(PROGRAM_EXIT);
    ASSERT(programExit);
    functionEntry = declareFunction(FUNCTION_ENTRY);
    functionEntryFlagsSafe = declareFunction(FUNCTION_ENTRY);
    ASSERT(functionEntry);
    ASSERT(functionEntryFlagsSafe);
    functionExit = declareFunction(FUNCTION_EXIT);
    functionExitFlagsSafe = declareFunction(FUNCTION_EXIT);
    ASSERT(functionExit);
    ASSERT(functionExitFlagsSafe);
}

void FunctionTimer::instrument(){
    InstrumentationTool::instrument();

    uint32_t temp32;
    uint64_t temp64;

    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    uint64_t functionCountAddr = reserveDataOffset(sizeof(uint64_t));
    programEntry->addArgument(functionCountAddr);
    temp64 = getNumberOfExposedFunctions();
    initializeReservedData(getInstDataAddress() + functionCountAddr, sizeof(uint64_t), &temp64);

    uint64_t funcNameArray = reserveDataOffset(getNumberOfExposedFunctions() * sizeof(char*));
    programEntry->addArgument(funcNameArray);

    uint64_t functionIndexAddr = reserveDataOffset(sizeof(uint64_t));
    programEntry->addArgument(functionIndexAddr);

    functionEntry->assumeNoFunctionFP();
    functionEntryFlagsSafe->assumeNoFunctionFP();

    functionExit->assumeNoFunctionFP();
    functionExitFlagsSafe->assumeNoFunctionFP();

    uint32_t noProtPoints = 0;
    uint32_t totalPoints = 0;
    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        Function* f = getExposedFunction(i);
        
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        BasicBlock* bb = f->getFlowGraph()->getEntryBlock();
        Vector<BasicBlock*>* exitBlocks = f->getFlowGraph()->getExitBlocks();

        Vector<X86Instruction*> fillEntry = Vector<X86Instruction*>();

        if (getElfFile()->is64Bit()){
            fillEntry.append(X86InstructionFactory64::emitMoveImmToMem(i, getInstDataAddress() + functionIndexAddr));
        } else {
            fillEntry.append(X86InstructionFactory32::emitMoveImmToMem(i, getInstDataAddress() + functionIndexAddr));
        }

        FlagsProtectionMethods prot = FlagsProtectionMethod_full;
        X86Instruction* bestinst = bb->getExitInstruction();
        InstLocations loc = InstLocation_prior;
        for (int32_t j = bb->getNumberOfInstructions() - 1; j >= 0; j--){
            if (bb->getInstruction(j)->allFlagsDeadIn()){
                bestinst = bb->getInstruction(j);
                noProtPoints++;
                prot = FlagsProtectionMethod_none;
                break;
            }
        }
        if (prot == FlagsProtectionMethod_full){
            p = addInstrumentationPoint(bestinst, functionEntry, InstrumentationMode_tramp, prot, loc);
        } else {
            p = addInstrumentationPoint(bestinst, functionEntryFlagsSafe, InstrumentationMode_tramp, prot, loc);
        }
        totalPoints++;
        for (uint32_t j = 0; j < fillEntry.size(); j++){
            p->addPrecursorInstruction(fillEntry[j]);
        }

        for (uint32_t j = 0; j < (*exitBlocks).size(); j++){
            Vector<X86Instruction*> fillExit = Vector<X86Instruction*>();
 
            if (getElfFile()->is64Bit()){
                fillExit.append(X86InstructionFactory64::emitMoveImmToMem(i, getInstDataAddress() + functionIndexAddr));
            } else {
                fillExit.append(X86InstructionFactory32::emitMoveImmToMem(i, getInstDataAddress() + functionIndexAddr));
            }

            FlagsProtectionMethods prot = FlagsProtectionMethod_full;
            X86Instruction* bestinst = (*exitBlocks)[j]->getExitInstruction();
            InstLocations loc = InstLocation_prior;
            for (int32_t k = (*exitBlocks)[j]->getNumberOfInstructions() - 1; k >= 0; k--){
                if ((*exitBlocks)[j]->getInstruction(k)->allFlagsDeadIn()){
                    bestinst = (*exitBlocks)[j]->getInstruction(k);
                    noProtPoints++;
                    prot = FlagsProtectionMethod_none;
                    break;
                }
            }
            if (prot == FlagsProtectionMethod_full){
                p = addInstrumentationPoint(bestinst, functionExit, InstrumentationMode_tramp, prot, loc);
            } else {
                p = addInstrumentationPoint(bestinst, functionExitFlagsSafe, InstrumentationMode_tramp, prot, loc);
            }
            totalPoints++;
            for (uint32_t k = 0; k < fillExit.size(); k++){
                p->addPrecursorInstruction(fillExit[k]);
            }
        }
        if (!(*exitBlocks).size()){
            Vector<X86Instruction*> fillExit = Vector<X86Instruction*>();

            if (getElfFile()->is64Bit()){
                fillExit.append(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                fillExit.append(X86InstructionFactory64::emitMoveImmToReg(i, X86_REG_CX));
                fillExit.append(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
                fillExit.append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
            } else {
                fillExit.append(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                fillExit.append(X86InstructionFactory32::emitMoveImmToReg(i, X86_REG_CX));
                fillExit.append(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
                fillExit.append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));
            }

            BasicBlock* lastbb = f->getBasicBlock(f->getNumberOfBasicBlocks()-1);
            X86Instruction* lastin = lastbb->getInstruction(lastbb->getNumberOfInstructions()-1);
            if (lastin){
                FlagsProtectionMethods prot = FlagsProtectionMethod_full;
                X86Instruction* bestinst = lastbb->getExitInstruction();
                InstLocations loc = InstLocation_prior;
                for (int32_t j = lastbb->getNumberOfInstructions() - 1; j >= 0; j--){
                    if (lastbb->getInstruction(j)->allFlagsDeadIn()){
                        bestinst = lastbb->getInstruction(j);
                        noProtPoints++;
                        prot = FlagsProtectionMethod_none;
                        break;
                    }
                }
                if (prot == FlagsProtectionMethod_full){
                    p = addInstrumentationPoint(bestinst, functionExit, InstrumentationMode_tramp, prot, loc);
                } else {
                    p = addInstrumentationPoint(bestinst, functionExitFlagsSafe, InstrumentationMode_tramp, prot, loc);
                }
                totalPoints++;
                for (uint32_t k = 0; k < fillExit.size(); k++){
                    p->addPrecursorInstruction(fillExit[k]);
                }
            } else {
                PRINT_WARN(10, "No exit from function %s", f->getName());
            }
        }

        delete exitBlocks;
    }
    PRINT_INFOR("Excluding protection from %d/%d instrumentation points", noProtPoints, totalPoints);

}
