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

#include <FunctionCounter.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <Loop.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>

#define ENTRY_FUNCTION "initcounter"
#define EXIT_FUNCTION "blockcounter"
#define INST_LIB_NAME "libcounter.so"
#define NOSTRING "__pebil_no_string__"

extern "C" {
    InstrumentationTool* FunctionCounterMaker(ElfFile* elf){
        return new FunctionCounter(elf);
    }
}

FunctionCounter::FunctionCounter(ElfFile* elf)
    : InstrumentationTool(elf)
{
    entryFunc = NULL;
    exitFunc = NULL;
}

void FunctionCounter::declare(){
    InstrumentationTool::declare();

    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");
}

void FunctionCounter::instrument(){
    InstrumentationTool::instrument();

    uint32_t temp32;
    uint64_t temp64;
    
    uint32_t numberOfPoints = getNumberOfExposedFunctions();
    uint64_t lineArray = reserveDataOffset(numberOfPoints * sizeof(uint32_t));
    uint64_t fileNameArray = reserveDataOffset(numberOfPoints * sizeof(char*));
    uint64_t funcNameArray = reserveDataOffset(numberOfPoints * sizeof(char*));
    uint64_t hashCodeArray = reserveDataOffset(numberOfPoints * sizeof(uint64_t));

    uint64_t appName = reserveDataOffset((strlen(getApplicationName()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + appName, strlen(getApplicationName()) + 1, getApplicationName());
    uint64_t instExt = reserveDataOffset((strlen(getExtension()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + instExt, strlen(getExtension()) + 1, (void*)getExtension());

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint64_t));

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(numberOfPoints * sizeof(uint64_t));
    temp64 = 0;
    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        initializeReservedData(getInstDataAddress() + counterArray + i*sizeof(uint64_t), sizeof(uint64_t), &temp64);
    }

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    // the number of inst points
    temp64 = numberOfPoints;
    initializeReservedData(getInstDataAddress() + counterArrayEntries, sizeof(uint64_t), &temp64);

    uint64_t noDataAddr = getInstDataAddress() + reserveDataOffset(strlen(NOSTRING) + 1);
    char* nostring = new char[strlen(NOSTRING) + 1];
    sprintf(nostring, "%s\0", NOSTRING);
    initializeReservedData(noDataAddr, strlen(NOSTRING) + 1, nostring);
    delete[] nostring;

    entryFunc->addArgument(counterArrayEntries);
    entryFunc->addArgument(counterArray);
    entryFunc->addArgument(hashCodeArray);

    exitFunc->addArgument(lineArray);
    exitFunc->addArgument(fileNameArray);
    exitFunc->addArgument(funcNameArray);
    exitFunc->addArgument(appName);
    exitFunc->addArgument(instExt);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp, InstLocation_prior);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        Function* f = getExposedFunction(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(f);
        }

        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(getInstDataAddress() + lineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
            uint64_t filenameAddr = getInstDataAddress() + filename;
            initializeReservedData(getInstDataAddress() + fileNameArray + i*sizeof(char*), sizeof(char*), &filenameAddr);
            initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());

        } else {
            temp32 = 0;
            initializeReservedData(getInstDataAddress() + lineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &temp32);
            initializeReservedData(getInstDataAddress() + fileNameArray + i*sizeof(char*), sizeof(char*), &noDataAddr);
        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        uint64_t hashValue = f->getBasicBlockAtAddress(f->getBaseAddress())->getHashCode().getValue();
        initializeReservedData(getInstDataAddress() + hashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);

        uint64_t counterOffset = counterArray + (i * sizeof(uint64_t));
        InstrumentationTool::insertBlockCounter(counterOffset, f);
    }

    Vector<Base*>* allBlocks = new Vector<Base*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allBlockLineInfos = new Vector<LineInfo*>();
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();

        (*allBlocks).append(bb);
        (*allBlockIds).append(i);
        (*allBlockLineInfos).append(li);
    }
    printStaticFile(getExtension(), allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());

    delete allBlocks;
    delete allBlockIds;
    delete allBlockLineInfos;
}
