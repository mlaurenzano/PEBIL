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

extern "C" {
    InstrumentationTool* BasicBlockCounterMaker(ElfFile* elf){
        return new BasicBlockCounter(elf);
    }
}

BasicBlockCounter::BasicBlockCounter(ElfFile* elf)
    : InstrumentationTool(elf)
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
    uint64_t temp64;

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
    uint64_t instExt = reserveDataOffset((strlen(getExtension()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + instExt, strlen(getExtension()) + 1, (void*)getExtension());

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint64_t));

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(numberOfPoints * sizeof(uint64_t));

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    temp64 = 0;
    for (uint32_t i = 0; i < numberOfPoints; i++){
        initializeReservedData(getInstDataAddress() + counterArray + i*sizeof(uint64_t), sizeof(uint64_t), &temp64);
    }

    // the number of inst points
    temp64 = numberOfPoints;
    initializeReservedData(getInstDataAddress() + counterArrayEntries, sizeof(uint64_t), &temp64);

    entryFunc->addArgument(counterArrayEntries);
    entryFunc->addArgument(counterArray);
    entryFunc->addArgument(hashCodeArray);

    exitFunc->addArgument(lineArray);
    exitFunc->addArgument(fileNameArray);
    exitFunc->addArgument(funcNameArray);
    exitFunc->addArgument(appName);
    exitFunc->addArgument(instExt);


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
            //Loop* outerMost = fg->getOuterMostLoopForLoop(fg->getInnermostLoopForBlock(bb->getIndex())->getIndex());
            Loop* outerMost = fg->getInnermostLoopForBlock(bb->getIndex());

            bool loopAlreadyInstrumented = false;
            for (uint32_t i = 0; i < loopsFound.size(); i++){
                if (outerMost->isIdenticalLoop(loopsFound[i])){
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

    temp64 = loopsFound.size();
    initializeReservedData(getInstDataAddress() + loopCounterEntries, sizeof(uint64_t), &temp64);

    p = addInstrumentationPoint(getProgramEntryBlock(), loopEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    // an array of counters. note that everything is passed by reference
    uint64_t loopCounters = reserveDataOffset(loopsFound.size() * sizeof(uint64_t));
    uint64_t loopExt = reserveDataOffset((strlen(LOOP_EXT) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + loopExt, strlen(LOOP_EXT) + 1, (void*)LOOP_EXT);

    loopEntry->addArgument(loopCounterEntries);
    loopEntry->addArgument(loopCounters);
    loopEntry->addArgument(loopHashCodeArray);

    loopExit->addArgument(loopLineArray);
    loopExit->addArgument(loopFileNameArray);
    loopExit->addArgument(loopFuncNameArray);
    loopExit->addArgument(appName);
    loopExit->addArgument(loopExt);

    uint32_t numCalls = 0;
    for (uint32_t i = 0; i < loopsFound.size(); i++){
        uint64_t counterOffset = loopCounters + (i * sizeof(uint64_t));
        Loop* loop = loopsFound[i];
        BasicBlock* head = loop->getHead();
        BasicBlock* tail = loop->getTail();
        ASSERT(head && tail);

        Function* f = head->getFunction();
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(head);
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

#ifdef STATS_PER_INSTRUCTION
        HashCode* hc = head->getLeader()->generateHashCode(head);
        uint64_t hashValue = hc->getValue();
#else 
        uint64_t hashValue = head->getHashCode().getValue();
#endif

        initializeReservedData(getInstDataAddress() + loopHashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);

        //increment counter on each time we encounter the loop head
        InstrumentationTool::insertInlinedTripCounter(counterOffset, head);

        // decrement counter each time we traverse a back edge
        for (uint32_t j = 0; j < tail->getNumberOfTargets(); j++){
            BasicBlock* target = tail->getTargetBlock(j);
            FlowGraph* fg = target->getFlowGraph();
            if (head->getHashCode().getValue() == target->getHashCode().getValue()){
                ASSERT(head->getHashCode().getValue() == target->getHashCode().getValue());

                // if control falls from tail to head, stick a decrement at the very end of the block
                if (tail->getBaseAddress() + tail->getNumberOfBytes() == target->getBaseAddress()){
                    InstrumentationSnippet* snip = new InstrumentationSnippet();
                    if (is64Bit()){
                        snip->addSnippetInstruction(X86InstructionFactory64::emitSubImmByteToMem64(1, getInstDataAddress() + counterOffset));
                    } else {
                        snip->addSnippetInstruction(X86InstructionFactory32::emitSubImmByteToMem(1, getInstDataAddress() + counterOffset));
                    }

                    FlagsProtectionMethods prot = FlagsProtectionMethod_light;
                    if (tail->getExitInstruction()->allFlagsDeadOut()){
                        prot = FlagsProtectionMethod_none;
                    }
                    addInstrumentationSnippet(snip);
                    InstrumentationPoint* p = addInstrumentationPoint(tail->getExitInstruction(), snip, InstrumentationMode_inline, prot, InstLocation_after);
                    //PRINT_INFOR("\tEXIT-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx HASH %lld", numCalls, tail->getBaseAddress(), target->getBaseAddress(), tail->getHashCode().getValue());
                } else {
                    BasicBlock* interposed = initInterposeBlock(fg, tail->getIndex(), target->getIndex());
                    InstrumentationTool::insertInlinedTripCounter(counterOffset, interposed, false);
                    //PRINT_INFOR("\tEXIT-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx HASH %lld", numCalls, tail->getBaseAddress(), target->getBaseAddress(), tail->getHashCode().getValue());
                }
                numCalls++;
            }
        }
    }
    PRINT_INFOR("Loop-counter instrumentation adding %d points", numCalls);
#endif //COUNT_LOOP_ENTRY

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


#define FUNCTION_ICOUNT_THRESHOLD 0
#define LOOP_ICOUNT_THRESHOLD 20
#define BB_ICOUNT_THRESHOLD 12

extern "C" {
    InstrumentationTool* RareEventCounterMaker(ElfFile* elf){
        return new RareEventCounter(elf);
    }
}

RareEventCounter::RareEventCounter(ElfFile* elf)
    : BasicBlockCounter(elf)
{
}

void RareEventCounter::instrument() 
{
    InstrumentationTool::instrument();
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    uint32_t temp32;
    uint64_t temp64;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    Vector<BasicBlock*> rareBlocks;
#ifdef COUNT_LOOP_ENTRY
    Vector<Loop*> loopsFound;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();
        if (li && bb->isInLoop()){
            FlowGraph* fg = bb->getFlowGraph();
            //Loop* outerMost = fg->getOuterMostLoopForLoop(fg->getInnermostLoopForBlock(bb->getIndex())->getIndex());
            Loop* outerMost = fg->getInnermostLoopForBlock(bb->getIndex());

            if (outerMost->getNumberOfInstructions() <= LOOP_ICOUNT_THRESHOLD){
                continue;
            }

            bool loopAlreadyInstrumented = false;
            for (uint32_t i = 0; i < loopsFound.size(); i++){
                if (outerMost->isIdenticalLoop(loopsFound[i])){
                    loopAlreadyInstrumented = true;
                }
            }
            if (!loopAlreadyInstrumented){
                loopsFound.append(outerMost);
            }
        }
    }
#endif
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();

        bool blockIsRare = false;

        if (bb->getNumberOfInstructions() > BB_ICOUNT_THRESHOLD){
            blockIsRare = true;
        } 
        if (bb->getNumberOfFloatOps() > 2){
            blockIsRare = true;
        }
        if (bb->endsWithCall()){
            blockIsRare = true;
        }
        if (bb->getNumberOfMemoryOps() > 5){
            blockIsRare = true;
        }

        if (bb->getExitInstruction()->isReturn() &&
            f->getNumberOfInstructions() > FUNCTION_ICOUNT_THRESHOLD){
            blockIsRare = true;
        }

        if (blockIsRare){
            rareBlocks.append(bb);
        }
    }

    uint32_t numberOfPoints = rareBlocks.size();
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

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    temp64 = 0;
    for (uint32_t i = 0; i < numberOfPoints; i++){
        initializeReservedData(getInstDataAddress() + counterArray + i*sizeof(uint64_t), sizeof(uint64_t), &temp64);
    }

    // the number of inst points
    temp64 = numberOfPoints;
    initializeReservedData(getInstDataAddress() + counterArrayEntries, sizeof(uint64_t), &temp64);

    entryFunc->addArgument(counterArrayEntries);
    entryFunc->addArgument(counterArray);
    entryFunc->addArgument(hashCodeArray);

    exitFunc->addArgument(lineArray);
    exitFunc->addArgument(fileNameArray);
    exitFunc->addArgument(funcNameArray);
    exitFunc->addArgument(appName);
    exitFunc->addArgument(instExt);


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
    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allBlockLineInfos = new Vector<LineInfo*>();

    for (uint32_t i = 0; i < rareBlocks.size(); i++){
        BasicBlock* bb = rareBlocks[i];
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();

        (*allBlocks).append(bb);
        (*allBlockIds).append(i);
        (*allBlockLineInfos).append(li);

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

        uint64_t hashValue = bb->getHashCode().getValue();

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

    temp64 = loopsFound.size();
    initializeReservedData(getInstDataAddress() + loopCounterEntries, sizeof(uint64_t), &temp64);

    p = addInstrumentationPoint(getProgramEntryBlock(), loopEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    // an array of counters. note that everything is passed by reference
    uint64_t loopCounters = reserveDataOffset(loopsFound.size() * sizeof(uint64_t));
    uint64_t loopExt = reserveDataOffset((strlen(LOOP_EXT) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + loopExt, strlen(LOOP_EXT) + 1, (void*)LOOP_EXT);

    loopEntry->addArgument(loopCounterEntries);
    loopEntry->addArgument(loopCounters);
    loopEntry->addArgument(loopHashCodeArray);

    loopExit->addArgument(loopLineArray);
    loopExit->addArgument(loopFileNameArray);
    loopExit->addArgument(loopFuncNameArray);
    loopExit->addArgument(appName);
    loopExit->addArgument(loopExt);

    uint32_t numCalls = 0;
    for (uint32_t i = 0; i < loopsFound.size(); i++){
        uint64_t counterOffset = loopCounters + (i * sizeof(uint64_t));
        Loop* loop = loopsFound[i];
        BasicBlock* head = loop->getHead();
        BasicBlock* tail = loop->getTail();
        ASSERT(head && tail);

        Function* f = head->getFunction();
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(head);
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

        uint64_t hashValue = head->getHashCode().getValue();

        initializeReservedData(getInstDataAddress() + loopHashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);

        //increment counter on each time we encounter the loop head
        InstrumentationTool::insertInlinedTripCounter(counterOffset, head);

        // decrement counter each time we traverse a back edge
        for (uint32_t j = 0; j < tail->getNumberOfTargets(); j++){
            BasicBlock* target = tail->getTargetBlock(j);
            FlowGraph* fg = target->getFlowGraph();
            if (head->getHashCode().getValue() == target->getHashCode().getValue()){
                ASSERT(head->getHashCode().getValue() == target->getHashCode().getValue());

                // if control falls from tail to head, stick a decrement at the very end of the block
                if (tail->getBaseAddress() + tail->getNumberOfBytes() == target->getBaseAddress()){
                    InstrumentationSnippet* snip = new InstrumentationSnippet();
                    if (is64Bit()){
                        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmByteToMem64(1, getInstDataAddress() + counterOffset));
                    } else {
                        snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
                    }

                    FlagsProtectionMethods prot = FlagsProtectionMethod_light;
                    if (tail->getExitInstruction()->allFlagsDeadOut()){
                        prot = FlagsProtectionMethod_none;
                    }
                    InstrumentationPoint* p = addInstrumentationPoint(tail->getExitInstruction(), snip, InstrumentationMode_inline, prot, InstLocation_after);
                    PRINT_INFOR("\tEXIT-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx HASH %lld", numCalls, tail->getBaseAddress(), target->getBaseAddress(), tail->getHashCode().getValue());
                } else {
                    BasicBlock* interposed = initInterposeBlock(fg, tail->getIndex(), target->getIndex());
                    InstrumentationTool::insertInlinedTripCounter(counterOffset, interposed);
                    PRINT_INFOR("\tEXIT-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx HASH %lld", numCalls, tail->getBaseAddress(), target->getBaseAddress(), tail->getHashCode().getValue());
                }
                numCalls++;
            }
        }
    }
    PRINT_INFOR("Loop-counter instrumentation adding %d points", numCalls);
#endif //COUNT_LOOP_ENTRY

    printStaticFile(allBlocks, allBlockIds, allBlockLineInfos, allBlocks->size());

    delete[] nostring;

    delete allBlocks;
    delete allBlockIds;
    delete allBlockLineInfos;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
