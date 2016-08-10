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
#include <map>
#include <string>

#include <TimerFunctions.hpp>

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
}

void FunctionTimer::declare(){
    InstrumentationTool::declare();

    // declare any shared library that will contain instrumentation functions
//    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    programEntry = declareFunction("tool_image_init");
    ASSERT(programEntry);

    programExit = declareFunction("tool_image_fini");
    ASSERT(programExit);

    functionEntry = declareFunction("function_entry");
    ASSERT(functionEntry);

    functionExit = declareFunction("function_exit");
    ASSERT(functionExit);
}

void FunctionTimer::instrumentEntry(BasicBlock* bb, uint32_t functionEntryIndexRegister, uint32_t functionIndex) {
        // Instrument the entry block
        FlagsProtectionMethods prot = FlagsProtectionMethod_full;
        X86Instruction* bestinst = bb->getExitInstruction();
        InstLocations loc = InstLocation_prior;
        for (int32_t j = bb->getNumberOfInstructions() - 1; j >= 0; j--){
            if (bb->getInstruction(j)->allFlagsDeadIn()){
                bestinst = bb->getInstruction(j);
                prot = FlagsProtectionMethod_none;
                break;
            }
        }
        InstrumentationPoint* p = addInstrumentationPoint(bestinst, functionEntry, InstrumentationMode_tramp, loc);
        assignStoragePrior(p, functionIndex, functionEntryIndexRegister);
}

void FunctionTimer::instrument(){
    InstrumentationTool::instrument();

    uint32_t temp32;
    uint64_t temp64;

    /*
     * Create input for function timer instrumentation
     */

    FunctionTimers funcInfo;
    uint64_t functionInfoStruct = reserveDataOffset(sizeof(FunctionTimers));

    funcInfo.master = getElfFile()->isExecutable();

    char* appName = getElfFile()->getAppName();
    uint64_t app = reserveDataOffset(strlen(appName) + 1);
    initializeReservedPointer(app, functionInfoStruct + offsetof(FunctionTimers, application));
    initializeReservedData(getInstDataAddress() + app, strlen(appName) + 1, (void*)appName);

    char extName[__MAX_STRING_SIZE];
    sprintf(extName, "%s\0", getExtension());
    uint64_t ext = reserveDataOffset(strlen(extName) + 1);
    initializeReservedPointer(ext, functionInfoStruct + offsetof(FunctionTimers, extension));
    initializeReservedData(getInstDataAddress() + ext, strlen(extName) + 1, (void*)extName);

    funcInfo.functionCount = getNumberOfExposedFunctions();

    uint64_t funcNameArray = reserveDataOffset(getNumberOfExposedFunctions() * sizeof(char*));
    initializeReservedPointer(funcNameArray, functionInfoStruct + offsetof(FunctionTimers, functionNames));

    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        Function* f = getExposedFunction(i);
        
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        initializeReservedPointer(funcname, funcNameArray + sizeof(char*) * i);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());
    }

    funcInfo.functionTimerAccum = NULL;
    funcInfo.functionTimerLast = NULL;

    initializeReservedData(getInstDataAddress() + functionInfoStruct, sizeof(FunctionTimers), (void*)&funcInfo);
   
    // Add arguments to instrumentation functions
    programEntry->addArgument(functionInfoStruct);
    programEntry->addArgument(imageKey);
    programEntry->addArgument(threadHash);

    programExit->addArgument(imageKey);

    uint32_t functionEntryIndexRegister = functionEntry->addConstantArgument();
    functionEntry->addArgument(imageKey);

    uint32_t functionExitIndexRegister = functionExit->addConstantArgument();
    functionExit->addArgument(imageKey);
    
    // Add program-entry instrumentation
    if (isMultiImage()){
        for (uint32_t i = 0; i < getNumberOfExposedFunctions(); ++i){
            Function* f = getExposedFunction(i);
            InstrumentationPoint* p = addInstrumentationPoint(f, programEntry, InstrumentationMode_tramp, InstLocation_prior);
            ASSERT(p);

            dynamicPoint(p, getElfFile()->getUniqueId(), true);
        }
    } else {
        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp);
        ASSERT(p);

    }

    // Add program-exit instrumentation
    {
        InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp);
        ASSERT(p);
    }

    /*
     * Add entry-exit instrumentation to each function
     */
    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); ++i){
        Function* f = getExposedFunction(i);

        BasicBlock* bb = f->getFlowGraph()->getEntryBlock();
        Vector<BasicBlock*>* exitBlocks = f->getFlowGraph()->getExitBlocks();

        instrumentEntry(bb, functionEntryIndexRegister, i);

        // Instrument entry blocks of sub-functions
        uint32_t ninstructions = f->getNumberOfInstructions();
        X86Instruction** finstructions = new X86Instruction*[ninstructions];
        f->getAllInstructions(finstructions, 0);

        for( uint32_t j = 0; j < ninstructions; ++j) {
            X86Instruction* ins = finstructions[j];
            if(ins->isCall() && f->inRange(ins->getTargetAddress()) ) {

                BasicBlock* callTarget = f->getBasicBlockAtAddress(ins->getTargetAddress());
                assert(callTarget);
                PRINT_INFOR("Instrumenting call to self in function %s at 0x%llx\n", f->getName(), callTarget->getBaseAddress());

                instrumentEntry(callTarget, functionEntryIndexRegister, i);

            }
        }
        delete finstructions;

        // Instrument each exit block
        for (uint32_t j = 0; j < (*exitBlocks).size(); j++){

            if( !(*exitBlocks)[j]->getExitInstruction()->isReturn() )
                continue;

            PRINT_INFOR("Instrumenting exit block for %s at 0x%llx\n", f->getName(), (*exitBlocks)[j]->getBaseAddress());
            FlagsProtectionMethods prot = FlagsProtectionMethod_full;
            X86Instruction* bestinst = (*exitBlocks)[j]->getExitInstruction();
            InstLocations loc = InstLocation_prior;
            for (int32_t k = (*exitBlocks)[j]->getNumberOfInstructions() - 1; k >= 0; k--){
                if ((*exitBlocks)[j]->getInstruction(k)->allFlagsDeadIn()){
                    bestinst = (*exitBlocks)[j]->getInstruction(k);
                    prot = FlagsProtectionMethod_none;
                    break;
                }
            }
            InstrumentationPoint* p = addInstrumentationPoint(bestinst, functionExit, InstrumentationMode_tramp, loc);
            assignStoragePrior(p, i, functionExitIndexRegister);
        }
        if (!(*exitBlocks).size()){

            PRINT_WARN(10, "No exit blocks could be found for function %s, instrumenting last (linear) block", f->getName());

            BasicBlock* lastbb = f->getBasicBlock(f->getNumberOfBasicBlocks()-1);
            X86Instruction* lastin = lastbb->getInstruction(lastbb->getNumberOfInstructions()-1);
            if (lastin){
                FlagsProtectionMethods prot = FlagsProtectionMethod_full;
                X86Instruction* bestinst = lastbb->getExitInstruction();
                InstLocations loc = InstLocation_prior;
                for (int32_t j = lastbb->getNumberOfInstructions() - 1; j >= 0; j--){
                    if (lastbb->getInstruction(j)->allFlagsDeadIn()){
                        bestinst = lastbb->getInstruction(j);
                        prot = FlagsProtectionMethod_none;
                        break;
                    }
                }
                InstrumentationPoint* p = addInstrumentationPoint(bestinst, functionExit, InstrumentationMode_tramp, loc);
                assignStoragePrior(p, i, functionExitIndexRegister);

            } else {
                PRINT_WARN(10, "No exit from function %s", f->getName());
            }
        }

        delete exitBlocks;
    }
}

extern "C" {
    InstrumentationTool* ExternalFunctionTimerMaker(ElfFile* elf){
        return new ExternalFunctionTimer(elf);
    }
}

ExternalFunctionTimer::ExternalFunctionTimer(ElfFile* elf)
    : FunctionTimer(elf)
{
}

void ExternalFunctionTimer::declare(){
    InstrumentationTool::declare();

    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    programEntry = declareFunction("ext_program_entry");
    ASSERT(programEntry);

    programExit = declareFunction("ext_program_exit");
    ASSERT(programExit);

    functionEntry = declareFunction("function_pre");
    ASSERT(functionEntry);

    functionExit = declareFunction("function_post");
    ASSERT(functionExit);
}

void ExternalFunctionTimer::instrument(){
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

    Vector<char*>* fileLines = new Vector<char*>();
    initializeFileList(inputFile, fileLines);

    uint64_t siteIndexAddr = reserveDataOffset(sizeof(uint32_t));

    Vector<std::string> names;
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* x = getExposedInstruction(i);
        ASSERT(x->getContainer()->isFunction());
        Function* function = (Function*)x->getContainer();

        if (x->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(x->getTargetAddress());
            
            if (functionSymbol){
                //PRINT_INFOR("looking for function %s", functionSymbol->getSymbolName());
                uint32_t funcIdx = searchFileList(fileLines, functionSymbol->getSymbolName());
                if (funcIdx < (*fileLines).size()){
                    if (x->getTargetAddress() == function->getBaseAddress()){
                        PRINT_WARN(20, "skipping recursive call of %s", function->getName());
                        continue;
                    }

                    BasicBlock* bb = function->getBasicBlockAtAddress(x->getBaseAddress());
                    ASSERT(bb->containsCallToRange(0,-1));
                    ASSERT(x->getSizeInBytes() == Size__uncond_jump);

                    InstrumentationPoint* prior = addInstrumentationPoint(x, functionEntry, InstrumentationMode_tramp, InstLocation_prior);
                    InstrumentationPoint* after = addInstrumentationPoint(x, functionExit, InstrumentationMode_tramp, InstLocation_after);

                    assignStoragePrior(prior, names.size(), getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
                    assignStoragePrior(after, names.size(), getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());

                    std::string c;
                    c.append(functionSymbol->getSymbolName());
                    char faddr[__MAX_STRING_SIZE];
                    sprintf(faddr, "%#llx", x->getBaseAddress());
                    c.append(faddr);
                    c.append("@");
                    c.append(function->getName());
                    names.append(c);
                }
            }            
        }
    }

    uint64_t functionCountAddr = reserveDataOffset(sizeof(uint64_t));
    programEntry->addArgument(functionCountAddr);
    temp64 = names.size();
    initializeReservedData(getInstDataAddress() + functionCountAddr, sizeof(uint64_t), &temp64);

    uint64_t funcNameArray = reserveDataOffset(names.size() * sizeof(char*));
    programEntry->addArgument(funcNameArray);

    programEntry->addArgument(siteIndexAddr);

    for (uint32_t i = 0; i < names.size(); i++){
        uint64_t fname = reserveDataOffset(names[i].length() + 1);
        uint64_t fnameAddr = getInstDataAddress() + fname;

        initializeReservedData(getInstDataAddress() + funcNameArray + (i * sizeof(char*)), sizeof(char*), &fnameAddr);
        initializeReservedData(getInstDataAddress() + fname, names[i].length() + 1, (void*)names[i].c_str());
    }


    for (uint32_t i = 0; i < fileLines->size(); i++){
        delete[] (*fileLines)[i];
    }
    delete fileLines;
}
