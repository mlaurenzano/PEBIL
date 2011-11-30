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

#include <ThrottleLoop.h>
#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <Loop.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <SymbolTable.h>

//#define LOOP_NAMING_BASIS "pfreq_throttle"
#define LOOP_NAMING_BASIS "pwr_measure"
#define PROGRAM_ENTRY LOOP_NAMING_BASIS "_init"
#define PROGRAM_EXIT LOOP_NAMING_BASIS "_fini"
#define LOOP_ENTRY LOOP_NAMING_BASIS "_lpentry"
#define LOOP_EXIT LOOP_NAMING_BASIS "_lpexit"

//#define SKIP_THROTTLE_IF_HASCALL
//#define DEBUG_INTERPOSE
#define LINE_WILDCARD_VALUE 0x12345678
#define UPDATE_SITEIDX
#define FLAGS_METHOD FlagsProtectionMethod_full

extern "C" {
    InstrumentationTool* ThrottleLoopMaker(ElfFile* elf){
        return new ThrottleLoop(elf);
    }
}

bool ThrottleLoop::checkArgs(){
    if (inputFile == NULL){
        PRINT_ERROR("argument --inp required for %s", briefName());
    }
    if (trackFile == NULL){
        PRINT_ERROR("argument --trk required for %s", briefName());
    }
    if (libraryList == NULL){
        PRINT_ERROR("argument --lnc required for %s", briefName());
    }
}

ThrottleLoop::~ThrottleLoop(){
    for (uint32_t i = 0; i < (*loopList).size(); i++){
        delete[] (*loopList)[i];
    }
    delete loopList;
    for (uint32_t i = 0; i < (*functionList).size(); i++){
        delete[] (*functionList)[i];
    }
    delete functionList;
}

ThrottleLoop::ThrottleLoop(ElfFile* elf)
    : InstrumentationTool(elf)
{
    loopEntry = NULL;
    loopExit = NULL;
    programEntry = NULL;
    programExit = NULL;

    loopList = new Vector<char*>();
}

char* ThrottleLoop::getFileName(uint32_t idx){
    ASSERT(idx < (*loopList).size());
    return (*loopList)[idx];
}

uint32_t ThrottleLoop::getLineNumber(uint32_t idx){
    ASSERT(idx < (*loopList).size());
    char* both = (*loopList)[idx];
    both += strlen(both) + 1;

    if (both[0] == '*'){
        return LINE_WILDCARD_VALUE;
    }

    uint32_t lineNo = strtol(both, NULL, 0);
    return lineNo;
}

uint32_t ThrottleLoop::loopMatch(LineInfo* li){
    for (uint32_t i = 0; i < (*loopList).size(); i++){
        bool fileMatch = false;
        bool lineMatch = false;

        if (!strcmp(getFileName(i), "*")){
            fileMatch = true;
        }
        if (getLineNumber(i) == LINE_WILDCARD_VALUE){
            lineMatch = true;
        }

        if (li){
            if (!strcmp(getFileName(i), li->getFileName())){
                fileMatch = true;
            }
            if (getLineNumber(i) == li->GET(lr_line)){
                lineMatch = true;
            }
        }

        if (fileMatch && lineMatch){
            return i;
        }
    }
    return (*loopList).size();
}

uint32_t ThrottleLoop::getThrottleLevel(uint32_t idx){
    ASSERT(idx < (*loopList).size());
    char* both = (*loopList)[idx];
    both += strlen(both) + 1;
    both += strlen(both) + 1;

    uint32_t level = strtol(both, NULL, 0);
    return level;
}

char* ThrottleLoop::getWrappedFunction(uint32_t idx){
    return (*functionList)[idx];
}
char* ThrottleLoop::getWrapperFunction(uint32_t idx){
    char* both = (*functionList)[idx];
    both += strlen(both) + 1;
    return both;
}

void ThrottleLoop::declare(){
    InstrumentationTool::declare();

    ASSERT(inputFile && loopList);
    initializeFileList(inputFile, loopList);

    functionList = new Vector<char*>();
    ASSERT(trackFile && functionList);
    initializeFileList(trackFile, functionList);

    // replace any ':' character with a '\0'
    for (uint32_t i = 0; i < (*loopList).size(); i++){
        char* both = (*loopList)[i];
        uint32_t numrepl = 0;
        uint32_t bothsz = strlen(both);
        for (uint32_t j = 0; j < bothsz; j++){
            if (both[j] == ':'){
                both[j] = '\0';
                numrepl++;
            }
        }
        if (numrepl != 2){
            PRINT_ERROR("input file %s line %d should contain two ':' tokens", inputFile, i+1);
        }
    }
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
            PRINT_ERROR("input file %s line %d should contain a single ':' token", inputFile, i+1);
        }        
    }

    for (uint32_t i = 0; i < (*functionList).size(); i++){
        functionWrappers.append(declareFunction(getWrapperFunction(i)));
        functionWrappers.back()->setSkipWrapper();
    }

    // declare any instrumentation functions that will be used
    loopEntry = declareFunction(LOOP_ENTRY);
    ASSERT(loopEntry);
    /*
    loopEntry->assumeNoFunctionFP();
    loopEntry->setSkipWrapper();
    */
    loopExit = declareFunction(LOOP_EXIT);
    ASSERT(loopExit);    
    programEntry = declareFunction(PROGRAM_ENTRY);
    ASSERT(programEntry);
    programExit = declareFunction(PROGRAM_EXIT);
    ASSERT(programExit);    
}


void ThrottleLoop::instrument(){
    InstrumentationTool::instrument();

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    } else {
        PRINT_ERROR("Loop Throttling tool requires line information: recompile  your app with -g?");
    }
    ASSERT(lineInfoFinder);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp);
    ASSERT(p);

    uint64_t siteIndex = reserveDataOffset(sizeof(uint32_t));
    programEntry->addArgument(siteIndex);

    p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp);
    ASSERT(p);


    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){

        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();

        (*allBlocks).append(bb);
        (*allBlockIds).append(i);
        (*allLineInfos).append(li);
    }

    uint32_t numCalls = 0;
    uint32_t entryPoints = 0;
    uint32_t exitPoints = 0;                    
    uint32_t numLoops = 0;
    Vector<Loop*> loopsFound;
    Vector<uint32_t> throttleLevels;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        Function* function = (Function*)bb->getFunction();
        LineInfo* li = lineInfoFinder->lookupLineInfo(bb);
        if (li && bb->isInLoop()){
            uint32_t lineMatchFound = loopMatch(li);
            if (lineMatchFound < (*loopList).size()){
                FlowGraph* fg = bb->getFlowGraph();
                Loop* outerMost = fg->getOuterMostLoopForLoop(fg->getInnermostLoopForBlock(bb->getIndex())->getIndex());
                
                bool loopAlreadyInstrumented = false;
                for (uint32_t k = 0; k < loopsFound.size(); k++){
                    if (outerMost->isIdenticalLoop(loopsFound[k])){
                        loopAlreadyInstrumented = true;
                    }
                }

#ifdef SKIP_THROTTLE_IF_HASCALL
                if (outerMost->containsCall()){
                    PRINT_WARN(10, "****** Skipping loop that contains a function call %s:%d", li->getFileName(), li->GET(lr_line));
                    continue;
                }
#endif
                if (!loopAlreadyInstrumented){
                    loopsFound.append(outerMost);
                    
                    PRINT_INFOR("Loop @ %s:%d (function %s) tagged for throttling", li->getFileName(), li->GET(lr_line), fg->getFunction()->getName());
                    //                        ASSERT(!outerMost->containsCall() && "It is weird to instrument blocks that contain calls with this tool");
#ifdef DEBUG_INTERPOSE
                    outerMost->print();                    
                    outerMost->getHead()->print();
                    outerMost->getHead()->printInstructions();
#endif
                    
                    // it is important to perform all analysis on this loop before performing any interpositions because
                    // doing the interpositions changes the CFG
                    Vector<BasicBlock*> entryInterpositions;
                    for (uint32_t k = 0; k < outerMost->getHead()->getNumberOfSources(); k++){
                        BasicBlock* source = outerMost->getHead()->getSourceBlock(k);
#ifdef DEBUG_INTERPOSE
                        PRINT_INFOR("source block %d", source->getIndex());
#endif
                        if (!outerMost->isBlockIn(source->getIndex())){
#ifdef DEBUG_INTERPOSE
                            PRINT_INFOR("\tsource not in loop");
#endif
                            if (source->getBaseAddress() + source->getNumberOfBytes() == outerMost->getHead()->getBaseAddress()){
                                // instrument somewhere in the source block
#ifdef DEBUG_INTERPOSE
                                PRINT_INFOR("\t\tsource falls through");
#endif
                                X86Instruction* bestinst = source->getLeader();
                                FlagsProtectionMethods prot = FLAGS_METHOD;
                                for (uint32_t m = 0; m < source->getNumberOfInstructions(); m++){
                                    if (source->getInstruction(m)->allFlagsDeadIn()){
                                        bestinst = source->getInstruction(m);
                                        prot = FlagsProtectionMethod_none;
                                    }
                                }
                                
                                InstrumentationPoint* pt = addInstrumentationPoint(bestinst, loopEntry, InstrumentationMode_trampinline, prot);
#ifdef UPDATE_SITEIDX
                                if (getElfFile()->is64Bit()){
                                    pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                                    pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(numLoops, X86_REG_CX));
                                    pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                                    pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
                                } else {
                                    pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                                    pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(numLoops, X86_REG_CX));
                                    pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                                    pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));
                                }
#endif
                                throttleLevels.append(getThrottleLevel(lineMatchFound));
                                PRINT_INFOR("\tENTR-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx; level %d", numLoops, source->getBaseAddress(), outerMost->getHead()->getBaseAddress(), throttleLevels.back());
                                entryPoints++;
                            } else {
                                // interpose a block between head of loop and source and instrument the interposed block
#ifdef DEBUG_INTERPOSE
                                PRINT_INFOR("\t\tsource interpose");
#endif
                            }
                        } else {
#ifdef DEBUG_INTERPOSE
                            PRINT_INFOR("\tsource in loop");
#endif
                        }
                        
                    }
                    
                    FlagsProtectionMethods prot = FLAGS_METHOD;
                    if (outerMost->getHead()->getLeader()->allFlagsDeadIn()){
                        prot = FlagsProtectionMethod_none;
                    }
                    
                    for (uint32_t k = 0; k < entryInterpositions.size(); k++){
                        bool linkFound = false;
                        for (uint32_t xyz = 0; xyz < entryInterpositions[k]->getNumberOfTargets(); xyz++){
                            if (entryInterpositions[k]->getTargetBlock(xyz)->getIndex() == outerMost->getHead()->getIndex()){
                                linkFound = true;
                            }
                        }
                        if (!linkFound){
                            entryInterpositions[k]->print();
                            outerMost->getHead()->print();
                        }
                        BasicBlock* interposed = initInterposeBlock(fg, entryInterpositions[k]->getIndex(), outerMost->getHead()->getIndex());
#ifdef DEBUG_INTERPOSE
                        interposed->print();
#endif
                        ASSERT(loopEntry);
                        InstrumentationPoint* pt = addInstrumentationPoint(interposed, loopEntry, InstrumentationMode_trampinline, prot);
#ifdef UPDATE_SITEIDX
                        if (getElfFile()->is64Bit()){
                            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(numLoops, X86_REG_CX));
                            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                            pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
                        } else {
                            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(numLoops, X86_REG_CX));
                            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                            pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));
                        }
#endif
                        throttleLevels.append(getThrottleLevel(lineMatchFound));
                        PRINT_INFOR("\tENTR-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx; level %d", numLoops, entryInterpositions[k]->getBaseAddress(), outerMost->getHead()->getBaseAddress(), throttleLevels.back());
                        entryPoints++;
                    }
                    
                    BasicBlock** allLoopBlocks = new BasicBlock*[outerMost->getNumberOfBlocks()];
                    outerMost->getAllBlocks(allLoopBlocks);
                    for (uint32_t k = 0; k < outerMost->getNumberOfBlocks(); k++){
#ifdef DEBUG_INTERPOSE
                        allLoopBlocks[k]->print();
#endif
                        
                        Vector<BasicBlock*> exitInterpositions;
                        for (uint32_t m = 0; m < allLoopBlocks[k]->getNumberOfTargets(); m++){
                            BasicBlock* target = allLoopBlocks[k]->getTargetBlock(m);
#ifdef DEBUG_INTERPOSE
                            PRINT_INFOR("target block %d", target->getIndex());
#endif
                            if (!outerMost->isBlockIn(target->getIndex())){
#ifdef DEBUG_INTERPOSE
                                PRINT_INFOR("\ttarget not in loop");
#endif
                                if (target->getBaseAddress() == allLoopBlocks[k]->getBaseAddress() + allLoopBlocks[k]->getNumberOfBytes()){
                                    // instrument somewhere in the target block
#ifdef DEBUG_INTERPOSE
                                    PRINT_INFOR("\t\ttarget falls through");
#endif
                                    X86Instruction* bestinst = target->getLeader();
                                    FlagsProtectionMethods prot = FLAGS_METHOD;
                                    for (uint32_t n = 0; n < target->getNumberOfInstructions(); n++){
                                        if (target->getInstruction(n)->allFlagsDeadIn()){
                                            bestinst = target->getInstruction(n);
                                            prot = FlagsProtectionMethod_none;
                                            break;
                                        }
                                    }
                                    InstrumentationPoint* pt = addInstrumentationPoint(bestinst, loopExit, InstrumentationMode_trampinline, prot);
#ifdef UPDATE_SITEIDX
                                    if (getElfFile()->is64Bit()){
                                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(numLoops, X86_REG_CX));
                                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
                                    } else {
                                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(numLoops, X86_REG_CX));
                                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));
                                    }
#endif
                                    PRINT_INFOR("\tEXIT-FALLTHRU(%d)\tBLK:%#llx --> BLK:%#llx", numLoops, allLoopBlocks[k]->getBaseAddress(), target->getBaseAddress());
                                    exitPoints++;
                                } else {
                                    // interpose a block between head of loop and target and instrument the interposed block
#ifdef DEBUG_INTERPOSE
                                    PRINT_INFOR("\t\ttarget interpose");
#endif
                                }
                            } else {
#ifdef DEBUG_INTERPOSE
                                PRINT_INFOR("\ttarget in loop");
#endif
                            }
                        }
                        
                        prot = FLAGS_METHOD;
                        if (allLoopBlocks[k]->getExitInstruction()->allFlagsDeadOut()){
                            prot = FlagsProtectionMethod_none;
                        }
                        
                        for (uint32_t m = 0; m < exitInterpositions.size(); m++){
                            bool linkFound = false;
                            for (uint32_t xyz = 0; xyz < allLoopBlocks[k]->getNumberOfTargets(); xyz++){
                                if (allLoopBlocks[k]->getTargetBlock(xyz)->getIndex() == exitInterpositions[m]->getIndex()){
                                    linkFound = true;
                                }
                            }
                            if (!linkFound){
                                allLoopBlocks[k]->print();
                                exitInterpositions[m]->print();
                            }
                            BasicBlock* interposed = initInterposeBlock(fg, allLoopBlocks[k]->getIndex(), exitInterpositions[m]->getIndex());
#ifdef DEBUG_INTERPOSE
                            interposed->print();
#endif
                            ASSERT(loopExit);
                            InstrumentationPoint* pt = addInstrumentationPoint(interposed, loopExit, InstrumentationMode_trampinline, prot);
#ifdef UPDATE_SITEIDX
                            if (getElfFile()->is64Bit()){
                                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(numLoops, X86_REG_CX));
                                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                                pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
                            } else {
                                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(numLoops, X86_REG_CX));
                                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                                pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));
                            }
#endif
                            PRINT_INFOR("\tEXIT-INTERPOS(%d)\tBLK:%#llx --> BLK:%#llx", numLoops, allLoopBlocks[k]->getBaseAddress(), exitInterpositions[m]->getBaseAddress());
                            exitPoints++;
                        }
                    }
                    delete[] allLoopBlocks;
                    numLoops++;
                }
            }
        }
    }

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

                    InstrumentationPoint* pt = addInstrumentationPoint(instruction, functionWrappers[funcIdx], InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
                    PRINT_INFOR("Wrapping function call for throttling: %s -> %s", getWrappedFunction(funcIdx), getWrapperFunction(funcIdx));
                    if (getElfFile()->is64Bit()){
                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(numCalls, X86_REG_CX));
                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));
                    } else {
                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(numCalls, X86_REG_CX));
                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(X86_REG_CX, getInstDataAddress() + siteIndex));
                        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));
                    }
                    numCalls++;
                }
            }
        }
    }

    uint64_t levels = reserveDataOffset(entryPoints * sizeof(uint32_t));
    programEntry->addArgument(levels);
    uint64_t levelEntries = reserveDataOffset(sizeof(uint32_t));
    uint32_t temp32 = numLoops;
    programEntry->addArgument(levelEntries);
    initializeReservedData(getInstDataAddress() + levelEntries, sizeof(uint32_t), &temp32);
    for (uint32_t i = 0; i < entryPoints; i++){
        temp32 = throttleLevels[i];
        initializeReservedData(getInstDataAddress() + levels + (i * sizeof(uint32_t)), sizeof(uint32_t), &temp32);
    }

    printStaticFile(allBlocks, allBlockIds, allLineInfos, allBlocks->size());

    delete allBlocks;
    delete allBlockIds;
    delete allLineInfos;
}

