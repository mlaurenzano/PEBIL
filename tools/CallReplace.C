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

#include <CallReplace.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <SymbolTable.h>

#define PROGRAM_ENTRY  "_pebil_init"
#define PROGRAM_EXIT   "_pebil_fini"
#define NOSTRING "__pebil_no_string__"
#define TIMER_BEGIN "PMaC_exported_timer_beginp"
#define TIMER_END "PMaC_exported_timer_endp"

#define MAX_TIMER_COUNT 128

CallReplace::~CallReplace(){
    for (uint32_t i = 0; i < (*functionList).size(); i++){
        delete[] (*functionList)[i];
    }
    delete functionList;
}

char* CallReplace::getFunctionName(uint32_t idx){
    ASSERT(idx < (*functionList).size());
    return (*functionList)[idx];

}
char* CallReplace::getWrapperName(uint32_t idx){
    ASSERT(idx < (*functionList).size());
    char* both = (*functionList)[idx];
    both += strlen(both) + 1;
    return both;
}

CallReplace::CallReplace(ElfFile* elf, char* traceFile, char* inpFile, bool doI)
    : InstrumentationTool(elf)
{
    programEntry = NULL;
    programExit = NULL;

    doIntro = doI;

    functionList = new Vector<char*>();
    initializeFileList(traceFile, functionList);

    // replace any ':' character with a '\0'
    for (uint32_t i = 0; i < (*functionList).size(); i++){
        char* both = (*functionList)[i];
        uint32_t numrepl = 0;
        for (uint32_t j = 0; j < strlen(both); j++){
            if (both[j] == ':'){
                both[j] = '\0';
                numrepl++;
            }
        }
        if (numrepl != 1){
            PRINT_ERROR("input file %s line %d should contain a ':'", traceFile, i+1);
        }
    }

    if (inpFile){
        timerFunctions = new Vector<char*>();
        initializeFileList(inpFile, timerFunctions);
        ASSERT((*timerFunctions).size() <= MAX_TIMER_COUNT);
    } else {
        timerFunctions = NULL;
    }
}

void CallReplace::declare(){
    //    InstrumentationTool::declare();

    // declare any instrumentation functions that will be used
    if (doIntro){
        programEntry = declareFunction(PROGRAM_ENTRY);
        ASSERT(programEntry);
        programExit = declareFunction(PROGRAM_EXIT);
        ASSERT(programExit);
    }

    for (uint32_t i = 0; i < (*functionList).size(); i++){
        functionWrappers.append(declareFunction(getWrapperName(i)));
        functionWrappers.back()->setSkipWrapper();
    }

    if (timerFunctions){
        timerBegin = declareFunction(TIMER_BEGIN);
        timerEnd = declareFunction(TIMER_END);
    }
}


void CallReplace::instrument(){
    //    InstrumentationTool::instrument();

    uint32_t temp32;
    uint64_t temp64;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    uint64_t siteIndex = reserveDataOffset(sizeof(uint32_t));

    if (doIntro){
        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
        ASSERT(p);
        if (!p->getInstBaseAddress()){
            PRINT_ERROR("Cannot find an instrumentation point at the exit function");
        }

        p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
        ASSERT(p);
        if (!p->getInstBaseAddress()){
            PRINT_ERROR("Cannot find an instrumentation point at the exit function");
        }

        programEntry->addArgument(siteIndex);
    }

    Vector<X86Instruction*> myInstPoints;
    Vector<uint32_t> myInstList;
    Vector<LineInfo*> myLineInfos;
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();
        
        if (instruction->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(instruction->getTargetAddress());
            
            if (functionSymbol){
                //PRINT_INFOR("looking for function %s", functionSymbol->getSymbolName());
                uint32_t funcIdx = searchFileList(functionList, functionSymbol->getSymbolName());
                if (funcIdx < (*functionList).size()){
                    BasicBlock* bb = function->getBasicBlockAtAddress(instruction->getBaseAddress());
                    ASSERT(bb->containsCallToRange(0,-1));
                    ASSERT(instruction->getSizeInBytes() == Size__uncond_jump);

                    myInstPoints.append(instruction);
                    myInstList.append(funcIdx);
                    LineInfo* li = NULL;
                    if (lineInfoFinder){
                        li = lineInfoFinder->lookupLineInfo(bb);
                    }
                    myLineInfos.append(li);
                }
            }
        } 
    }
    ASSERT(myInstPoints.size() == myInstList.size());
    ASSERT(myLineInfos.size() == myInstList.size());

    if (doIntro){
        uint64_t fileNames = reserveDataOffset(sizeof(char*) * myInstList.size());
        uint64_t lineNumbers = reserveDataOffset(sizeof(uint32_t) * myInstList.size());
        for (uint32_t i = 0; i < myInstList.size(); i++){
            uint32_t line = 0;
            char* fname = NOSTRING;
            if (myLineInfos[i]){
                line = myLineInfos[i]->GET(lr_line);
                fname = myLineInfos[i]->getFileName();
            }
            uint64_t filenameaddr = getInstDataAddress() + reserveDataOffset(strlen(fname) + 1);
            initializeReservedData(getInstDataAddress() + fileNames + i*sizeof(char*), sizeof(char*), &filenameaddr);
            initializeReservedData(filenameaddr, strlen(fname), fname);
            initializeReservedData(getInstDataAddress() + lineNumbers + i*sizeof(uint32_t), sizeof(uint32_t), &line);
        }
        programEntry->addArgument(fileNames);
        programEntry->addArgument(lineNumbers);
    }

    for (uint32_t i = 0; i < myInstPoints.size(); i++){
        PRINT_INFOR("(site %d) %#llx: replacing call %s -> %s in function %s", i, myInstPoints[i]->getBaseAddress(), getFunctionName(myInstList[i]), getWrapperName(myInstList[i]), myInstPoints[i]->getContainer()->getName());
        InstrumentationPoint* pt = addInstrumentationPoint(myInstPoints[i], functionWrappers[myInstList[i]], InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
        if (getElfFile()->is64Bit()){
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(i, X86_REG_CX));
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
        } else {
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(i, X86_REG_CX));
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));            
        }
    }



    if (timerFunctions){
        uint64_t timerIdxAddr = reserveDataOffset(sizeof(int32_t));
        timerBegin->addArgument(timerIdxAddr);
        timerEnd->addArgument(timerIdxAddr);
        
        Vector<X86Instruction*> timerInstPoints;
        Vector<uint32_t> timerInstList;
        Vector<LineInfo*> timerLineInfos;
        for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
            X86Instruction* instruction = getExposedInstruction(i);
            ASSERT(instruction->getContainer()->isFunction());
            Function* function = (Function*)instruction->getContainer();
            
            if (instruction->isFunctionCall()){
                Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(instruction->getTargetAddress());
                
                if (functionSymbol){
                    uint32_t funcIdx = searchFileList(timerFunctions, functionSymbol->getSymbolName());
                    if (funcIdx < (*timerFunctions).size()){
                        BasicBlock* bb = function->getBasicBlockAtAddress(instruction->getBaseAddress());
                        ASSERT(bb->containsCallToRange(0,-1));
                        ASSERT(instruction->getSizeInBytes() == Size__uncond_jump);
                        
                        timerInstPoints.append(instruction);
                        timerInstList.append(funcIdx);
                        LineInfo* li = NULL;
                        if (lineInfoFinder){
                            li = lineInfoFinder->lookupLineInfo(bb);
                        }
                        timerLineInfos.append(li);
                    }
                } 
            }
        }
        ASSERT(timerInstPoints.size() == timerInstList.size());
        ASSERT(timerLineInfos.size() == timerInstList.size());
        
        for (uint32_t i = 0; i < timerInstPoints.size(); i++){
            PRINT_INFOR("(accumulator %d) %#llx: inserting timers around call to %s in function %s", timerInstList[i], timerInstPoints[i]->getBaseAddress(), (*timerFunctions)[timerInstList[i]], timerInstPoints[i]->getContainer()->getName());
            InstrumentationPoint* pt = addInstrumentationPoint(timerInstPoints[i], timerBegin, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
            if (getElfFile()->is64Bit()){
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(timerInstList[i], X86_REG_CX));
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + timerIdxAddr));
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
            } else {
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(timerInstList[i], X86_REG_CX));
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + timerIdxAddr));
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));            
            }
            
            pt = addInstrumentationPoint(timerInstPoints[i], timerEnd, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_after);
            if (getElfFile()->is64Bit()){
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(timerInstList[i], X86_REG_CX));
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + timerIdxAddr));
                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
            } else {
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(timerInstList[i], X86_REG_CX));
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + timerIdxAddr));
                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));            
            }
        }
    }

}

