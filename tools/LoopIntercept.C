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

#include <LoopIntercept.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <Loop.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <SymbolTable.h>
#include <map>

#include <LoopTimer.hpp>


//#define DEBUG_INTERPOSE
#define FLAGS_METHOD FlagsProtectionMethod_full

extern "C" {
    InstrumentationTool* LoopInterceptMaker(ElfFile* elf){
        return new LoopIntercept(elf);
    }
}

LoopIntercept::~LoopIntercept(){
    for (uint32_t i = 0; i < (*loopList).size(); i++){
        delete[] (*loopList)[i];
    }
    delete loopList;
}

LoopIntercept::LoopIntercept(ElfFile* elf)
    : InstrumentationTool(elf)
{
    loopEntry = NULL;
    loopExit = NULL;
    programEntry = NULL;
    programExit = NULL;

    loopList = new Vector<char*>();
    discoveryMode = false;
}

uint64_t LoopIntercept::getLoopHash(uint32_t idx){
    ASSERT(idx < (*loopList).size());
    uint64_t hash = strtol((*loopList)[idx], NULL, 0);
    HashCode hashc(hash);
    uint64_t blockhash = hashc.getBlockCode();
    return blockhash;
}

void LoopIntercept::declare(){
    InstrumentationTool::declare();

    if (inputFile){
        initializeFileList(inputFile, loopList);
    }

    // declare any instrumentation functions that will be used
    loopEntry = declareFunction("loop_entry");
    ASSERT(loopEntry);
    /*
    loopEntry->assumeNoFunctionFP();
    loopEntry->setSkipWrapper();
    */
    loopExit = declareFunction("loop_exit");
    ASSERT(loopExit);    
    programEntry = declareFunction("tool_image_init");
    ASSERT(programEntry);
    programExit = declareFunction("tool_image_fini");
    ASSERT(programExit);    
}

void LoopIntercept::discoverAllLoops(){

    discoveryMode = true;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }
    
    PRINT_INFOR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    PRINT_INFOR("You passed an empty file to --inp, so this tool is running in discovery mode");
    PRINT_INFOR("Check the file %s.%s.static for a list of loop heads", getFullFileName(), getExtension());
    PRINT_INFOR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    
    if (!lineInfoFinder){
        PRINT_ERROR("Loop Intercept tool requires line information when running discovery mode (without --inp): recompile  your app with -g?");
    }
    ASSERT(lineInfoFinder);
    
    // find every loop
    std::map<uint64_t, Loop*> loops;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        Function* function = (Function*)bb->getFunction();
        FlowGraph* fg = bb->getFlowGraph();
        if (!bb->isInLoop()){
            continue;
        }
        Loop* innerMost = fg->getInnermostLoopForBlock(bb->getIndex());
        uint64_t hash = innerMost->getHead()->getHashCode().getValue();

        if (loops.count(hash) == 0){
            loops[hash] = innerMost;
        }
    }
    
    Vector<Base*>* allBlocks = new Vector<Base*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();
    
    uint32_t unq = 0;
    for (std::map<uint64_t, Loop*>::iterator ii = loops.begin(); ii != loops.end(); ii++){
        uint64_t hash = (*ii).first;
        Loop* loop = (*ii).second;
        BasicBlock* head = loop->getHead();
        allBlocks->append(head);
        allBlockIds->append(unq++);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(head);
        }
        allLineInfos->append(li);
    }
    printStaticFile(getExtension(), allBlocks, allBlockIds, allLineInfos, allBlocks->size());
    
    delete allBlocks;
    delete allBlockIds;
    delete allLineInfos;
}


void LoopIntercept::instrument(){
    InstrumentationTool::instrument();

    if (!loopList->size()){
        discoverAllLoops();
        exit(0);
    }

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    // Set indices for each loop
    std::map<uint64_t, uint32_t> loops;
    uint32_t numLoops = 0;
    for (uint32_t i = 0; i < loopList->size(); i++){
        loops[getLoopHash(i)] = numLoops++;
    }

    // pick out all the loops we want to instrument
    std::map<uint64_t, Loop*> loopsFound;
    std::map<uint64_t, Loop*> loopsRejected;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        uint64_t hash = bb->getHashCode().getValue();

        // skip blocks that aren't in our list of loop heads
        if (!(bb->isInLoop() && loops.count(hash) > 0))
            continue;

        // We should not have found this block before
        assert(loopsFound.count(hash) == 0);
        assert(loopsRejected.count(hash) == 0);

        Function* f = bb->getFunction();
        FlowGraph* fg = bb->getFlowGraph();
        ASSERT(f && fg);

        Loop* innerMost = fg->getInnermostLoopForBlock(bb->getIndex());
        ASSERT(innerMost);

        // find inner most loop whose head matches this blocks hashcode
        while (hash != innerMost->getHead()->getHashCode().getValue() && 
               innerMost->getIndex() != fg->getParentLoop(innerMost->getIndex())->getIndex()){
            innerMost = fg->getParentLoop(innerMost->getIndex());
            ASSERT(innerMost);
        }
        // Assert fail if we couldn't find the loop
        if (hash != innerMost->getHead()->getHashCode().getValue()){
            PRINT_INFOR("function %s/%s: %llx != %llx", f->getName(), innerMost->getHead()->getFunction()->getName(), hash, innerMost->getHead()->getHashCode().getValue());
        }
        ASSERT(hash == innerMost->getHead()->getHashCode().getValue());


        bool badLoop = false;

        // Check for shared loop heads
        for (uint32_t i = 0; i < fg->getNumberOfLoops(); i++){
            Loop* other = fg->getLoop(i);
            if (innerMost->hasSharedHeader(other) && !innerMost->isIdenticalLoop(other)){
                PRINT_WARN(20, "Head of loop %lld in %s is shared with other loop(s). skipping!", hash, f->getName());
                badLoop = true;
                loopsRejected[hash] = innerMost;
            }
        }

        // Check for shared loop exists
        // see if the loop has an exit point that exits a parent loop also. this is likely the result of a goto in an inner loop, which we cannot abide
        BasicBlock** allLoopBlocks = new BasicBlock*[innerMost->getNumberOfBlocks()];
        innerMost->getAllBlocks(allLoopBlocks);
        for (uint32_t k = 0; k < innerMost->getNumberOfBlocks() && !badLoop; k++){
            Vector<BasicBlock*> exitInterpositions;
            BasicBlock* bb = allLoopBlocks[k];
            for (uint32_t m = 0; m < bb->getNumberOfTargets() && !badLoop; m++){
                BasicBlock* target = bb->getTargetBlock(m);
                
                // target is outside the loop
                if (!innerMost->isBlockIn(target->getIndex())){
                    Loop* parent = innerMost;
                    while (parent != fg->getParentLoop(parent->getIndex())){
                        parent = fg->getParentLoop(parent->getIndex());
                        uint64_t phash = parent->getHead()->getHashCode().getValue();
                        if (!parent->isBlockIn(target->getIndex())){
                            if (loops.count(hash) && loops.count(phash)){
                                PRINT_WARN(20, "Loops %lld/%lld in %s have a shared exit point (eg. a goto/return statement). skipping!", hash, phash, f->getName());
                                badLoop = true;
                                loopsRejected[hash] = innerMost;
                                loopsRejected[phash] = parent;
                            }
                        }
                    }
                }
            }
        }

        // Check for indirect branches
        for (uint32_t k = 0; k < innerMost->getNumberOfBlocks() && !badLoop; k++){
            BasicBlock* bb = allLoopBlocks[k];
            if (bb->getExitInstruction()->isIndirectBranch()){
                PRINT_WARN(20, "Loop %lld is %s contains an indirect branch so we can't guarantee that all exits will be found. skipping!", hash, f->getName());
                PRINT_WARN(20, "Exit instruction:");
                bb->getExitInstruction()->print();
                badLoop = true;
                loopsRejected[hash] = innerMost;
            }
        }
        delete[] allLoopBlocks;

        // Record the loop if it passes all our tests
        if (!badLoop){
            loopsFound[hash] = innerMost;
        }
    }

    // Check for loops not found
    if (loops.size() != loopsFound.size() + loopsRejected.size()) {
        PRINT_WARN(20, "Only found %d of %d loops\n", loopsFound.size()+loopsRejected.size(), loops.size());
    }

    // Recreate indices from loopsFound
    numLoops = 0;
    for (uint32_t i = 0; i < loopsFound.size(); i++){
        loops[getLoopHash(i)] = numLoops++;
    }

    // Create input struct
    LoopTimers loopInfo;
    uint64_t loopInfoStruct = reserveDataOffset(sizeof(LoopTimers));

    loopInfo.master = getElfFile()->isExecutable();

    char* appName = getElfFile()->getAppName();
    uint64_t app = reserveDataOffset(strlen(appName)+1);
    initializeReservedPointer(app, loopInfoStruct + offsetof(LoopTimers, application));
    initializeReservedData(getInstDataAddress() + app, strlen(appName)+1, (void*)appName);

    char extName[__MAX_STRING_SIZE];
    sprintf(extName, "%s\0", getExtension());
    uint64_t ext = reserveDataOffset(strlen(extName)+1);
    initializeReservedPointer(ext, loopInfoStruct + offsetof(LoopTimers, extension));
    initializeReservedData(getInstDataAddress() + ext, strlen(extName) + 1, (void*)extName);

    loopInfo.loopCount = numLoops;

    uint64_t loopHashes = reserveDataOffset(numLoops * sizeof(uint64_t));
    initializeReservedPointer(loopHashes, loopInfoStruct + offsetof(LoopTimers, loopHashes));

    loopInfo.loopTimerAccum = NULL;
    loopInfo.loopTimerLast = NULL;

    initializeReservedData(getInstDataAddress() + loopInfoStruct, sizeof(LoopTimers), (void*)&loopInfo);

    // Add arguments to instrumentation functions
    programEntry->addArgument(loopInfoStruct);
    programEntry->addArgument(imageKey);
    programEntry->addArgument(threadHash);

    programExit->addArgument(imageKey);

    uint32_t loopEntryIndexRegister = loopEntry->addConstantArgument();
    loopEntry->addArgument(imageKey);

    uint32_t loopExitIndexRegister = loopExit->addConstantArgument();
    loopExit->addArgument(imageKey);

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


    Vector<Base*>* allBlocks = new Vector<Base*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();
    
    // instrument loops
    for (std::map<uint64_t, Loop*>::iterator ii = loopsFound.begin(); ii != loopsFound.end(); ii++){
        uint64_t hash = (*ii).first;

        Loop* loop = (*ii).second;
        BasicBlock* head = loop->getHead();
        ASSERT(head->getHashCode().getValue() == hash);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(head);
        }
        Function* f = head->getFunction();
        FlowGraph* fg = head->getFlowGraph();
        uint32_t site = loops[hash];
        initializeReservedData(getInstDataAddress() + loopHashes + (site * sizeof(uint64_t)), sizeof(uint64_t), &hash);

        allBlocks->append(head);
        allBlockIds->append(site);
        allLineInfos->append(li);
        
	if (li){
	    PRINT_INFOR("Loop %lld site %d @ %s:%d (function %s) tagged for interception has %d blocks", hash, site, li->getFileName(), li->GET(lr_line), fg->getFunction()->getName(), loop->getNumberOfBlocks());
	} else {
	    PRINT_INFOR("Loop %lld site %d @ %s:%d (function %s) tagged for interception has %d blocks", hash, site, INFO_UNKNOWN, 0, fg->getFunction()->getName(), loop->getNumberOfBlocks());
	}

        // it is important to perform all analysis on this loop before performing any interpositions because
        // inserting those interpositions changes the CFG
        Vector<BasicBlock*> entryInterpositions;
        for (uint32_t k = 0; k < head->getNumberOfSources(); k++){
            BasicBlock* source = head->getSourceBlock(k);
            if (!loop->isBlockIn(source->getIndex())){

                // source block falls through into loop
                if (source->getBaseAddress() + source->getNumberOfBytes() == head->getBaseAddress()){

                    // INSTRUMENTATION: add loop Entry for a fall-through loop entry
                    InstrumentationPoint* pt = addInstrumentationPoint(source->getExitInstruction(), loopEntry, InstrumentationMode_trampinline, InstLocation_after);
                    assignStoragePrior(pt, site, loopEntryIndexRegister);

                    PRINT_INFOR("\tENTR-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx", site, source->getBaseAddress(), head->getBaseAddress());

                // source block doesn't fall through to loop; make an interposition
                } else {
                    entryInterpositions.append(source);
                }
            }
        }

        // Find all exit points for loop
        BasicBlock** allLoopBlocks = new BasicBlock*[loop->getNumberOfBlocks()];
        loop->getAllBlocks(allLoopBlocks);
        for (uint32_t k = 0; k < loop->getNumberOfBlocks(); k++){
            Vector<BasicBlock*> exitInterpositions;
            BasicBlock* bb = allLoopBlocks[k];

            // procedure returns
            if (bb->endsWithReturn()){
                // INSTRUMENTATION
                InstrumentationPoint* pt = addInstrumentationPoint(bb->getExitInstruction(), loopExit, InstrumentationMode_trampinline, InstLocation_prior);
                assignStoragePrior(pt, site, loopExitIndexRegister);
                
                PRINT_INFOR("\tEXIT-FNRETURN(%d)\tBLK:%#llx --> ?", site, bb->getBaseAddress());
                continue;
            }

            // branches or fall-throughs
            for (uint32_t m = 0; m < bb->getNumberOfTargets(); m++){
                BasicBlock* target = bb->getTargetBlock(m);

                // Skip if the target is inside this loop
                if(loop->isBlockIn(target->getIndex()))
                    continue;

                // target is adjacent to bb 
                if (target->getBaseAddress() == bb->getBaseAddress() + bb->getNumberOfBytes()){
                   // INSTRUMENTATION: loop exit via fallthrough
                   InstrumentationPoint* pt = addInstrumentationPoint(bb->getExitInstruction(), loopExit, InstrumentationMode_trampinline, InstLocation_after);
                   assignStoragePrior(pt, site, loopExitIndexRegister);

                    PRINT_INFOR("\tEXIT-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx", site, bb->getBaseAddress(), target->getBaseAddress());

                // target needs an interposition
                } else {
                    // interpose a block between head of loop and target and instrument the interposed block
                    exitInterpositions.append(target);
                }
            }

/*
            FlagsProtectionMethods prot = FLAGS_METHOD;
            if (bb->getExitInstruction()->allFlagsDeadOut()){
                prot = FlagsProtectionMethod_none;
            }
*/
          
            // Add loop exits for all the interpositions we recorded
            for (uint32_t m = 0; m < exitInterpositions.size(); m++){
                BasicBlock* interb = exitInterpositions[m];
                bool linkFound = false;
                for (uint32_t xyz = 0; xyz < bb->getNumberOfTargets(); xyz++){
                    if (bb->getTargetBlock(xyz)->getIndex() == interb->getIndex()){
                        linkFound = true;
                    }
                }
                if (!linkFound){
                    bb->print();
                    interb->print();
                }
                BasicBlock* interposed = initInterposeBlock(fg, bb->getIndex(), interb->getIndex());
                ASSERT(loopExit);

                InstrumentationPoint* pt = addInstrumentationPoint(interposed, loopExit, InstrumentationMode_trampinline);
                assignStoragePrior(pt, site, loopExitIndexRegister);
                
                PRINT_INFOR("\tEXIT-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx", site, bb->getBaseAddress(), interb->getBaseAddress());
            }
        }
        delete[] allLoopBlocks;

        // Setup interpositions for each loop entry point
/*
        FlagsProtectionMethods prot = FLAGS_METHOD;
        if (head->getLeader()->allFlagsDeadIn()){
            prot = FlagsProtectionMethod_none;
        }
*/
        for (uint32_t j = 0; j < entryInterpositions.size(); j++){
            BasicBlock* interb = entryInterpositions[j];
            bool linkFound = false;
            for (uint32_t k = 0; k < interb->getNumberOfTargets(); k++){
                if (interb->getTargetBlock(k)->getIndex() == head->getIndex()){
                    linkFound = true;
                }
            }
            if (!linkFound){
                interb->print();
                head->print();
            }
            ASSERT(linkFound);
            BasicBlock* interposed = initInterposeBlock(fg, interb->getIndex(), head->getIndex());

            ASSERT(loopEntry);
            InstrumentationPoint* pt = addInstrumentationPoint(interposed, loopEntry, InstrumentationMode_trampinline);
            assignStoragePrior(pt, site, loopEntryIndexRegister);

            PRINT_INFOR("\tENTR-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx", site, interb->getBaseAddress(), head->getBaseAddress());
        }

    }

    printStaticFile(getExtension(), allBlocks, allBlockIds, allLineInfos, allBlocks->size());

    delete allBlocks;
    delete allBlockIds;
    delete allLineInfos;
}

