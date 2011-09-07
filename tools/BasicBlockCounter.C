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

#include <BasicBlockCounter.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#ifndef STATS_PER_INSTRUCTION
#endif //STATS_PER_INSTRUCTION
#define COUNT_LOOP_ENTRY

#ifdef COUNT_LOOP_ENTRY
#define LOOP_EXT "loopcnt"
#define ENTRY_LOOP_COUNT "initloop"
#define EXIT_LOOP_COUNT "loopcounter"
#endif

#define ENTRY_FUNCTION "initcounter"
#define EXIT_FUNCTION "blockcounter"
#define INST_LIB_NAME "libcounter.so"
#define NOSTRING "__pebil_no_string__"

BasicBlockCounter::BasicBlockCounter(ElfFile* elf, char* ext, bool lpi, bool dtl)
    : InstrumentationTool(elf, ext, 0, lpi, dtl)
{
    entryFunc = NULL;
    exitFunc = NULL;
#ifdef COUNT_LOOP_ENTRY
    loopEntry = NULL;
    loopExit = NULL;
#endif
}

void BasicBlockCounter::declare()
{
    InstrumentationTool::declare();
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");

#ifdef COUNT_LOOP_ENTRY
    loopExit = declareFunction(EXIT_LOOP_COUNT);
    ASSERT(loopExit && "Cannot find exit function, are you sure it was declared?");

    loopEntry = declareFunction(ENTRY_LOOP_COUNT);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");
#endif

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void BasicBlockCounter::instrument() 
{
    InstrumentationTool::instrument();
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    uint32_t temp32;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }


#ifdef STATS_PER_INSTRUCTION
    PRINT_WARN(10, "Performing instrumentation to gather PER-INSTRUCTION statistics");
    uint32_t numberOfPoints = getNumberOfExposedInstructions();
#else //STATS_PER_INSTRUCTION
    uint32_t numberOfPoints = getNumberOfExposedBasicBlocks();
#endif //STATS_PER_INSTRUCTION
    uint64_t lineArray = reserveDataOffset(numberOfPoints * sizeof(uint32_t));
    uint64_t fileNameArray = reserveDataOffset(numberOfPoints * sizeof(char*));
    uint64_t funcNameArray = reserveDataOffset(numberOfPoints * sizeof(char*));
    uint64_t hashCodeArray = reserveDataOffset(numberOfPoints * sizeof(uint64_t));

    uint64_t appName = reserveDataOffset((strlen(getApplicationName()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + appName, strlen(getApplicationName()) + 1, getApplicationName());
    uint64_t instExt = reserveDataOffset((strlen(getInstSuffix()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + instExt, strlen(getInstSuffix()) + 1, getInstSuffix());

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint64_t));

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(numberOfPoints * sizeof(uint64_t));

    exitFunc->addArgument(counterArray);
    exitFunc->addArgument(appName);
    exitFunc->addArgument(instExt);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    temp32 = 0;
    for (uint32_t i = 0; i < numberOfPoints; i++){
        initializeReservedData(getInstDataAddress() + counterArray + i*sizeof(uint32_t), sizeof(uint32_t), &temp32);
    }

    // the number of inst points
    entryFunc->addArgument(counterArrayEntries);
    temp32 = numberOfPoints;
    initializeReservedData(getInstDataAddress() + counterArrayEntries, sizeof(uint32_t), &temp32);

    // an array for line numbers
    entryFunc->addArgument(lineArray);
    // an array for file name pointers
    entryFunc->addArgument(fileNameArray);
    // an array for function name pointers
    entryFunc->addArgument(funcNameArray);
    // an array for hashcodes
    entryFunc->addArgument(hashCodeArray);

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    uint64_t noDataAddr = getInstDataAddress() + reserveDataOffset(strlen(NOSTRING) + 1);
    char* nostring = new char[strlen(NOSTRING) + 1];
    sprintf(nostring, "%s\0", NOSTRING);
    initializeReservedData(noDataAddr, strlen(NOSTRING) + 1, nostring);

    PRINT_DEBUG_MEMTRACK("There are %d instrumentation points", numberOfPoints);
#ifdef STATS_PER_INSTRUCTION
    Vector<X86Instruction*>* allInstructions = new Vector<X86Instruction*>();
    Vector<uint32_t>* allInstructionIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allInstructionLineInfos = new Vector<LineInfo*>();
#else //STATS_PER_INSTRUCTION
    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allBlockLineInfos = new Vector<LineInfo*>();
#endif //STATS_PER_INSTRUCTION

#ifdef COUNT_LOOP_ENTRY
    Vector<Loop*> loopsFound;
#endif
    for (uint32_t i = 0; i < numberOfPoints; i++){

#ifdef STATS_PER_INSTRUCTION
        X86Instruction* ins = getExposedInstruction(i);

        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(ins);
        }
        Function* f = (Function*)ins->getContainer();
        BasicBlock* bb = f->getBasicBlockAtAddress(ins->getBaseAddress());
        ASSERT(bb && "exposed instruction should be in a basic block");

        (*allInstructions).append(ins);
        (*allInstructionIds).append(i);
        (*allInstructionLineInfos).append(li);
#else //STATS_PER_INSTRUCTION
        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();

        (*allBlocks).append(bb);
        (*allBlockIds).append(i);
        (*allBlockLineInfos).append(li);
#endif //STATS_PER_INSTRUCTION

#ifdef COUNT_LOOP_ENTRY
        if (li && bb->isInLoop()){
            FlowGraph* fg = bb->getFlowGraph();
            Loop* outerMost = fg->getOuterMostLoopForLoop(fg->getInnermostLoopForBlock(bb->getIndex())->getIndex());
            //            Loop* outerMost = fg->getInnermostLoopForBlock(bb->getIndex());

            bool loopAlreadyInstrumented = false;
            for (uint32_t i = 0; i < loopsFound.size(); i++){
                if (outerMost->isIdenticalLoop(loopsFound[i]) || outerMost->hasSharedHeader(loopsFound[i])){
                    loopAlreadyInstrumented = true;
                }
            }
            if (!loopAlreadyInstrumented){
                loopsFound.append(outerMost);
            }
        }
#endif

        if (i % 1000 == 0){
            PRINT_DEBUG_MEMTRACK("inst point %d", i);
            PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);            
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

#ifdef STATS_PER_INSTRUCTION
        HashCode* hc = ins->generateHashCode(bb);
        uint64_t hashValue = hc->getValue();
        delete hc;
#else //STATS_PER_INSTRUCTION
        uint64_t hashValue = bb->getHashCode().getValue();
#endif //STATS_PER_INSTRUCTION

        initializeReservedData(getInstDataAddress() + hashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
        
        uint64_t counterOffset = counterArray + (i * sizeof(uint64_t));

        InstrumentationTool::insertInlinedTripCounter(counterOffset, bb);
    }
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
#ifdef NO_REG_ANALYSIS
    PRINT_WARN(10, "Warning: register analysis disabled");
#endif

#ifdef COUNT_LOOP_ENTRY
    PRINT_INFOR("Instrumenting %d loops for counting", loopsFound.size());

    p = addInstrumentationPoint(getProgramExitBlock(), loopExit, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    uint64_t loopCounterEntries = reserveDataOffset(sizeof(uint64_t));
    uint64_t loopLineArray = reserveDataOffset(loopsFound.size() * sizeof(uint32_t));
    uint64_t loopFileNameArray = reserveDataOffset(loopsFound.size() * sizeof(char*));
    uint64_t loopFuncNameArray = reserveDataOffset(loopsFound.size() * sizeof(char*));
    uint64_t loopHashCodeArray = reserveDataOffset(loopsFound.size() * sizeof(uint64_t));

    temp32 = loopsFound.size();
    initializeReservedData(getInstDataAddress() + loopCounterEntries, sizeof(uint32_t), &temp32);

    loopEntry->addArgument(loopCounterEntries);
    loopEntry->addArgument(loopLineArray);
    loopEntry->addArgument(loopFileNameArray);
    loopEntry->addArgument(loopFuncNameArray);
    loopEntry->addArgument(loopHashCodeArray);

    p = addInstrumentationPoint(getProgramEntryBlock(), loopEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    // an array of counters. note that everything is passed by reference
    uint64_t loopCounters = reserveDataOffset(loopsFound.size() * sizeof(uint64_t));
    loopExit->addArgument(loopCounters);
    loopExit->addArgument(appName);

    uint64_t loopExt = reserveDataOffset((strlen(LOOP_EXT) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + loopExt, strlen(LOOP_EXT) + 1, (void*)LOOP_EXT);
    loopExit->addArgument(loopExt);

    uint32_t numCalls = 0;
    for (uint32_t i = 0; i < loopsFound.size(); i++){
        uint64_t counterOffset = loopCounters + (i * sizeof(uint64_t));

        Function* f = loopsFound[i]->getHead()->getFunction();
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(loopsFound[i]->getHead());
        }
        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(getInstDataAddress() + loopLineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
            uint64_t filenameAddr = getInstDataAddress() + filename;
            initializeReservedData(getInstDataAddress() + loopFileNameArray + i*sizeof(char*), sizeof(char*), &filenameAddr);
            initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());

        } else {
            temp32 = 0;
            initializeReservedData(getInstDataAddress() + loopLineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &temp32);
            initializeReservedData(getInstDataAddress() + loopFileNameArray + i*sizeof(char*), sizeof(char*), &noDataAddr);
        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + loopFuncNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        uint64_t hashValue = loopsFound[i]->getHead()->getHashCode().getValue();
        initializeReservedData(getInstDataAddress() + loopHashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
        
        for (uint32_t j = 0; j < loopsFound[i]->getHead()->getNumberOfSources(); j++){
            BasicBlock* source = loopsFound[i]->getHead()->getSourceBlock(j);
            FlowGraph* fg = source->getFlowGraph();
            if (!loopsFound[i]->isBlockIn(source->getIndex())){
                Vector<BasicBlock*> entryInterpositions;
                if (source->getBaseAddress() + source->getNumberOfBytes() == loopsFound[i]->getHead()->getBaseAddress()){
                    // instrument somewhere in the source block
                    InstrumentationTool::insertInlinedTripCounter(counterOffset, source);

                    //                    PRINT_INFOR("\tENTR-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx HASH %lld", numCalls, source->getBaseAddress(), loopsFound[i]->getHead()->getBaseAddress(), loopsFound[i]->getHead()->getHashCode().getValue());
                    numCalls++;
                } else {
                    // interpose a block between head of loop and source and instrument the interposed block
                    entryInterpositions.append(source);
                }

                FlagsProtectionMethods prot = FlagsProtectionMethod_light;
                InstLocations loc = InstLocation_prior;
                if (loopsFound[i]->getHead()->getLeader()->allFlagsDeadIn()){
                    prot = FlagsProtectionMethod_none;
                }
                
                for (uint32_t k = 0; k < entryInterpositions.size(); k++){
                    BasicBlock* interposed = initInterposeBlock(fg, entryInterpositions[k]->getIndex(), loopsFound[i]->getHead()->getIndex());

                    InstrumentationTool::insertInlinedTripCounter(counterOffset, interposed);

                    //                    PRINT_INFOR("\tENTR-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx HASH %lld", numCalls, entryInterpositions[k]->getBaseAddress(), loopsFound[i]->getHead()->getBaseAddress(), loopsFound[i]->getHead()->getHashCode().getValue());
                    numCalls++;
                }

            }
        }
    }
    PRINT_INFOR("Loop-counter instrumentation adding %d points", numCalls);
#endif

#ifdef STATS_PER_INSTRUCTION
    printStaticFilePerInstruction(allInstructions, allInstructionIds, allInstructionLineInfos, allInstructions->size());
#else //STATS_PER_INSTRUCTION
    printStaticFile(allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());
#endif //STATS_PER_INSTRUCTION

    delete[] nostring;

#ifdef STATS_PER_INSTRUCTION
    delete allInstructions;
    delete allInstructionIds;
    delete allInstructionLineInfos;
#else //STATS_PER_INSTRUCTION
    delete allBlocks;
    delete allBlockIds;
    delete allBlockLineInfos;
#endif //STATS_PER_INSTRUCTION

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
