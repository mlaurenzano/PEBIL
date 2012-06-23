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
#include <map>

#include <CacheSimulationCommon.hpp>
#include <CounterFunctions.hpp>

#define LOOP_EXT "loopcnt"
#define ENTRY_LOOP_COUNT "initloop"
#define EXIT_LOOP_COUNT "loopcounter"

#define ENTRY_FUNCTION "tool_image_init"
#define EXIT_FUNCTION "tool_image_fini"
#define INST_LIB_NAME "cxx_libcounter.so"
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

    loopEntry = NULL;
    loopExit = NULL;

    loopCount = true;
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

    loopExit = declareFunction(EXIT_LOOP_COUNT);
    ASSERT(loopExit && "Cannot find exit function, are you sure it was declared?");

    loopEntry = declareFunction(ENTRY_LOOP_COUNT);
    ASSERT(loopEntry && "Cannot find entry function, are you sure it was declared?");

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

    Vector<Loop*> loopsFound;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }

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
            if (!loopAlreadyInstrumented && loopCount){
                loopsFound.append(outerMost);
            }
        }
    }

    uint32_t numberOfBlocks = getNumberOfExposedBasicBlocks();
    if (isPerInstruction()){
        PRINT_WARN(10, "Performing instrumentation to gather PER-INSTRUCTION statistics");
        numberOfBlocks = getNumberOfExposedInstructions();
    }

    uint32_t numberOfPoints = numberOfBlocks + loopsFound.size();

    uint64_t counterStruct = reserveDataOffset(sizeof(CounterArray));

    CounterArray ctrs;
    ctrs.Size = numberOfPoints;

    ctrs.Initialized = true;
    ctrs.PerInstruction = isPerInstruction();

#define INIT_CTR_ELEMENT(__typ, __nam)\
    ctrs.__nam = (__typ*)reserveDataOffset(numberOfPoints * sizeof(__typ));\
    initializeReservedPointer((uint64_t)ctrs.__nam, counterStruct + offsetof(CounterArray, __nam))

    INIT_CTR_ELEMENT(uint64_t, Counters);
    INIT_CTR_ELEMENT(CounterTypes, Types);
    INIT_CTR_ELEMENT(uint64_t, Addresses);
    INIT_CTR_ELEMENT(uint64_t, Hashes);
    INIT_CTR_ELEMENT(uint32_t, Lines);
    INIT_CTR_ELEMENT(char*, Files);
    INIT_CTR_ELEMENT(char*, Functions);

    char* appName = getElfFile()->getAppName();
    uint64_t app = reserveDataOffset(strlen(appName) + 1);
    initializeReservedPointer(app, counterStruct + offsetof(CounterArray, Application));
    initializeReservedData(getInstDataAddress() + app, strlen(appName) + 1, (void*)appName);

    char extName[__MAX_STRING_SIZE];
    sprintf(extName, "%s\0", getExtension());
    uint64_t ext = reserveDataOffset(strlen(extName) + 1);
    initializeReservedPointer(ext, counterStruct + offsetof(CounterArray, Extension));
    initializeReservedData(getInstDataAddress() + ext, strlen(extName) + 1, (void*)extName);

    initializeReservedData(getInstDataAddress() + counterStruct, sizeof(CounterArray), (void*)(&ctrs));

    exitFunc->addArgument(imageKey);
    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp, InstLocation_prior);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    entryFunc->addArgument(counterStruct);
    entryFunc->addArgument(imageKey);
    entryFunc->addArgument(threadHash);

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp, InstLocation_prior);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    uint64_t noData = reserveDataOffset(strlen(NOSTRING) + 1);
    char* nostring = new char[strlen(NOSTRING) + 1];
    sprintf(nostring, "%s\0", NOSTRING);
    initializeReservedData(getInstDataAddress() + noData, strlen(NOSTRING) + 1, nostring);

    Vector<Base*>* allBlocks = new Vector<Base*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allBlockLineInfos = new Vector<LineInfo*>();

    std::map<uint64_t, uint32_t>* functionThreading;
    if (isThreadedMode()){
        functionThreading = threadReadyCode();
    }

    uint64_t currentLeader = 0;
    for (uint32_t i = 0; i < numberOfBlocks; i++){

        LineInfo* li = NULL;
        X86Instruction* ins = NULL;
        Function* f = NULL;
        BasicBlock* bb = NULL;

        if (isPerInstruction()){
            ins = getExposedInstruction(i);

            if (lineInfoFinder){
                li = lineInfoFinder->lookupLineInfo(ins);
            }
            f = (Function*)ins->getContainer();
            bb = f->getBasicBlockAtAddress(ins->getBaseAddress());
            ASSERT(bb && "exposed instruction should be in a basic block");

            (*allBlocks).append(ins);
            (*allBlockIds).append(i);
            (*allBlockLineInfos).append(li);

        } else {
            bb = getExposedBasicBlock(i);
            if (lineInfoFinder){
                li = lineInfoFinder->lookupLineInfo(bb);
            }
            f = bb->getFunction();
            
            (*allBlocks).append(bb);
            (*allBlockIds).append(i);
            (*allBlockLineInfos).append(li);
        }

        ASSERT(f && bb);
        if (isPerInstruction()){
            ASSERT(ins);
        }

        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Lines + sizeof(uint32_t)*i, sizeof(uint32_t), &line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
            initializeReservedPointer(filename, (uint64_t)ctrs.Files + i*sizeof(char*));
            initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());
        } else {
            temp32 = 0;
            initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Lines + sizeof(uint32_t)*i, sizeof(uint32_t), &temp32);
            initializeReservedPointer(noData, (uint64_t)ctrs.Files + i*sizeof(char*));
        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        initializeReservedPointer(funcname, (uint64_t)ctrs.Functions + i*sizeof(char*));
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        uint64_t hashValue;
        uint64_t addr;
        if (isPerInstruction()){
            HashCode* hc = ins->generateHashCode(bb);
            hashValue = hc->getValue();
            addr = ins->getProgramAddress();
            delete hc;
        } else {
            hashValue = bb->getHashCode().getValue();
            addr = bb->getProgramAddress();        
        }

        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Hashes + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Addresses + i*sizeof(uint64_t), sizeof(uint64_t), &addr);
        
        CounterTypes tmpct;
        if (isPerInstruction()){
            // only keep a bb counter for one instruction in the block (the leader). all other instructions' counters hold the ID of the active counter
            // in their block
            if (bb->getLeader()->getBaseAddress() != ins->getBaseAddress()){
                tmpct = CounterType_instruction;
                initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Types + i*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);        
                
                temp64 = currentLeader;
                initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Counters + (i * sizeof(uint64_t)), sizeof(uint64_t), &temp64);
                
                continue;
            }
        }

        currentLeader = i;

        tmpct = CounterType_basicblock;
        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Types + i*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);        

        temp64 = 0;
        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Counters + (i * sizeof(uint64_t)), sizeof(uint64_t), &temp64);

        uint64_t counterOffset = (uint64_t)ctrs.Counters + (i * sizeof(uint64_t));
        uint32_t threadReg = X86_REG_INVALID;

        if (isThreadedMode()){
            counterOffset -= (uint64_t)ctrs.Counters;
            threadReg = (*functionThreading)[f->getBaseAddress()];
        }

        InstrumentationTool::insertBlockCounter(counterOffset, bb, true, threadReg);
    }

    PRINT_INFOR("Instrumenting %d loops for counting", loopsFound.size());

    for (uint32_t i = numberOfBlocks; i < numberOfPoints; i++){
        Loop* loop = loopsFound[i - numberOfBlocks];
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
            initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Lines + sizeof(uint32_t)*i, sizeof(uint32_t), &line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
            initializeReservedPointer(filename, (uint64_t)ctrs.Files + i*sizeof(char*));
            initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());
        } else {
            temp32 = 0;
            initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Lines + sizeof(uint32_t)*i, sizeof(uint32_t), &temp32);
            initializeReservedPointer(noData, (uint64_t)ctrs.Files + i*sizeof(char*));
        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        initializeReservedPointer(funcname, (uint64_t)ctrs.Functions + i*sizeof(char*));
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        uint64_t counterOffset =  (uint64_t)ctrs.Counters + (i * sizeof(uint64_t));
        uint32_t threadReg = X86_REG_INVALID;

        if (isThreadedMode()){
            counterOffset -= (uint64_t)ctrs.Counters;
            threadReg = (*functionThreading)[f->getBaseAddress()];            
        }

        uint64_t hashValue = head->getHashCode().getValue();
        uint64_t addr = head->getProgramAddress();

        if (isPerInstruction()){
            X86Instruction* ins = head->getLeader();
            HashCode* hc = ins->generateHashCode(head);
            hashValue = hc->getValue();
            addr = ins->getProgramAddress();
            delete hc;
        } else {
            hashValue = head->getHashCode().getValue();
            addr = head->getProgramAddress();
        }

        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Hashes + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Addresses + i*sizeof(uint64_t), sizeof(uint64_t), &addr);

        CounterTypes tmpct = CounterType_loop;
        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Types + i*sizeof(CounterTypes), sizeof(CounterTypes), &tmpct);

        temp64 = 0;
        initializeReservedData(getInstDataAddress() + (uint64_t)ctrs.Counters + (i * sizeof(uint64_t)), sizeof(uint64_t), &temp64);

        //increment counter on each time we encounter the loop head
        InstrumentationTool::insertBlockCounter(counterOffset, head, true, threadReg);
        //PRINT_INFOR("Loop head at %#lx", head->getBaseAddress());

        // decrement counter each time we traverse a back edge
        for (uint32_t j = 0; j < tail->getNumberOfTargets(); j++){
            BasicBlock* target = tail->getTargetBlock(j);
            FlowGraph* fg = target->getFlowGraph();
            if (head->getHashCode().getValue() == target->getHashCode().getValue()){
                ASSERT(head->getHashCode().getValue() == target->getHashCode().getValue());

                // if control falls from tail to head, stick a decrement at the very end of the block
                if (tail->getBaseAddress() + tail->getNumberOfBytes() == target->getBaseAddress()){
                    InstrumentationTool::insertInlinedTripCounter(counterOffset, tail->getExitInstruction(), false, threadReg, InstLocation_after, NULL, 1);
                    //PRINT_INFOR("\t\tEXIT-FALLTHRU\tBLK:%#llx --> BLK:%#llx HASH %lld", tail->getBaseAddress(), target->getBaseAddress(), tail->getHashCode().getValue());
                } else {
                    BasicBlock* interposed = initInterposeBlock(fg, tail->getIndex(), target->getIndex());
                    InstrumentationTool::insertInlinedTripCounter(counterOffset, interposed->getLeader(), false, threadReg, InstLocation_prior, NULL, 1);
                    //PRINT_INFOR("\t\tEXIT-INTERPOS\tBLK:%#llx --> BLK:%#llx HASH %lld", tail->getBaseAddress(), target->getBaseAddress(), tail->getHashCode().getValue());
                }
            }
        }
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
    delete functionThreading;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
