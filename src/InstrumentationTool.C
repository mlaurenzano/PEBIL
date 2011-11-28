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

#include <InstrumentationTool.h>

#include <BasicBlock.h>
#include <FlowGraph.h>
#include <Function.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>
#include <X86InstructionFactory.h>

#define MPI_INIT_WRAPPER_CBIND   "MPI_Init_pebil_wrapper"
#define MPI_INIT_LIST_CBIND_PREF "PMPI_Init"
#define MPI_INIT_LIST_CBIND      "MPI_Init"
#define MPI_INIT_WRAPPER_FBIND   "mpi_init__pebil_wrapper"
#define MPI_INIT_LIST_FBIND_PREF "pmpi_init_"
#define MPI_INIT_LIST_FBIND      "mpi_init_:MPI_INIT"

#define MAX_DEF_USE_DIST_PRINT 1024

InstrumentationTool::InstrumentationTool(ElfFile* elf, char* ext, uint32_t phase, bool lpi, bool dtl)
    : ElfFileInst(elf)
{
    extension = ext;
    phaseNo = phase;
    loopIncl = lpi;
    printDetail = dtl;
}

void InstrumentationTool::declare(){
#ifdef HAVE_MPI
    initWrapperC = declareFunction(MPI_INIT_WRAPPER_CBIND);
    initWrapperF = declareFunction(MPI_INIT_WRAPPER_FBIND);
    ASSERT(initWrapperC && "Cannot find MPI_Init function, are you sure it was declared?");
    ASSERT(initWrapperF && "Cannot find MPI_Init function, are you sure it was declared?");
#endif //HAVE_MPI
}

void InstrumentationTool::instrument(){
#ifdef HAVE_MPI
    int initFound = 0;

    // wrap any call to MPI_Init
    Vector<X86Instruction*>* mpiInitCalls = findAllCalls(MPI_INIT_LIST_CBIND_PREF);
    initWrapperC->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding MPI_Init wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperC, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;

    mpiInitCalls = findAllCalls(MPI_INIT_LIST_FBIND_PREF);
    initWrapperF->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding mpi_init_ wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperF, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;
    if (initFound){
        PRINT_INFOR("MPI Profile library calls found, skipping regular...");
        return;
    }

    // we just looked for PMPI calls. if none were found look for normal mpi functions
    mpiInitCalls = findAllCalls(MPI_INIT_LIST_CBIND);
    initWrapperC->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding MPI_Init wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperC, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;

    mpiInitCalls = findAllCalls(MPI_INIT_LIST_FBIND);
    initWrapperF->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding mpi_init_ wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperF, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;

#endif //HAVE_MPI
}

InstrumentationPoint* InstrumentationTool::insertInlinedTripCounter(uint64_t counterOffset, Base* within){
    BasicBlock* scope = NULL;

    if (within->getType() == PebilClassType_BasicBlock){
        scope = (BasicBlock*)within;
    } else if (within->getType() == PebilClassType_X86Instruction){
        X86Instruction* ins = (X86Instruction*)within;
        Function* f = (Function*)(ins->getContainer());
        scope = f->getBasicBlockAtAddress(ins->getBaseAddress());
    } else if (within->getType() == PebilClassType_Function){
        Function* f = (Function*)(within);
        scope = f->getBasicBlockAtAddress(f->getBaseAddress());
        ASSERT(scope->getNumberOfSources() == 0 && "Function entry block should not be a target of another block");
    } else {
        PRINT_ERROR("Cannot call InstrumentationTool::insertTripCounter for an object of type %s", within->getTypeName());
    }

    InstrumentationSnippet* snip = new InstrumentationSnippet();

    // snippet contents, in this case just increment a counter
    if (is64Bit()){
        snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmByteToMem64(1, getInstDataAddress() + counterOffset));
    } else {
        snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
    }

    // do not generate control instructions to get back to the application, this is done for
    // the snippet automatically during code generation

    // register the snippet we just created
    addInstrumentationSnippet(snip);

    // register an instrumentation point at the function that uses this snippet
    FlagsProtectionMethods prot = FlagsProtectionMethod_light;
    X86Instruction* bestinst = scope->getExitInstruction();
    InstLocations loc = InstLocation_prior;
#ifndef NO_REG_ANALYSIS
    for (int32_t j = scope->getNumberOfInstructions() - 1; j >= 0; j--){
        if (scope->getInstruction(j)->allFlagsDeadIn()){
            bestinst = scope->getInstruction(j);
            prot = FlagsProtectionMethod_none;
            break;
        }
    }
#endif
    InstrumentationPoint* p = addInstrumentationPoint(bestinst, snip, InstrumentationMode_inline, prot, loc);

    return p;
}

void InstrumentationTool::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allBlockLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allBlockLineInfos).size() || (*allBlocks).size() == (*allBlockLineInfos).size());
    ASSERT((*allBlocks).size() == (*allBlockIds).size());

    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), getInstSuffix(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getInstSuffix());
    fprintf(staticFD, "# phase     = %d\n", 0);
    fprintf(staticFD, "# type      = %s\n", briefName());
    fprintf(staticFD, "# cantidate = %d\n", getNumberOfExposedBasicBlocks());
    char* sha1sum = getElfFile()->getSHA1Sum();
    fprintf(staticFD, "# sha1sum   = %s\n", sha1sum);
    fprintf(staticFD, "# perinsn   = no\n");
    delete[] sha1sum;

    uint32_t memopcnt = 0;
    uint32_t membytcnt = 0;
    uint32_t fltopcnt = 0;
    uint32_t insncnt = 0;
    for (uint32_t i = 0; i < allBlocks->size(); i++){
        BasicBlock* bb = (*allBlocks)[i];
        memopcnt += bb->getNumberOfMemoryOps();
        membytcnt += bb->getNumberOfMemoryBytes();
        fltopcnt += bb->getNumberOfFloatOps();
        insncnt += bb->getNumberOfInstructions();
    }
    fprintf(staticFD, "# blocks    = %d\n", allBlocks->size());
    fprintf(staticFD, "# memops    = %d\n", memopcnt);

    float memopavg = 0.0;
    if (memopcnt){
        memopavg = (float)membytcnt/(float)memopcnt;
    }
    fprintf(staticFD, "# memopbyte = %d ( %.5f bytes/op)\n", membytcnt, memopavg);
    fprintf(staticFD, "# fpops     = %d\n", fltopcnt);
    fprintf(staticFD, "# insns     = %d\n", insncnt);
    fprintf(staticFD, "# buffer    = %d\n", bufferSize);
    for (uint32_t i = 0; i < getNumberOfInstrumentationLibraries(); i++){
        fprintf(staticFD, "# library   = %s\n", getInstrumentationLibrary(i));
    }
    fprintf(staticFD, "# libTag    = %s\n", "revision REVISION");
    fprintf(staticFD, "# %s\n", "<no additional info>");
    fprintf(staticFD, "# <sequence> <block_unqid> <memop> <fpop> <insn> <line> <fname> # <hex_unq_id> <vaddr>\n");

    if (printDetail){
        fprintf(staticFD, "# +lpi <loopcnt> <loopid> <ldepth> <lploc>\n");
        fprintf(staticFD, "# +cnt <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op>\n");
        fprintf(staticFD, "# +mem <total_mem_op> <total_mem_bytes> <bytes/op>\n");
        fprintf(staticFD, "# +lpc <loop_head> <parent_loop_head>\n");
        fprintf(staticFD, "# +dud <dudist1>:<duint1>:<dufp1> <dudist2>:<ducnt2>:<dufp2>...\n");
        fprintf(staticFD, "# +dxi <count_def_use_cross> <count_call>\n");
        fprintf(staticFD, "# +ipa <call_target_addr> <call_target_name>\n");
        fprintf(staticFD, "# +bin <unknown> <invalid> <cond> <uncond> <bin> <binv> <intb> <intbv> <intw> <intwv> <intd> <intdv> <intq> <intqv> <floats> <floatsv> <floatss> <floatd> <floatdv> <floatds> <move> <stack> <string> <system> <cache> <mem> <other>\n");
    }

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        BasicBlock* bb = (*allBlocks)[i];
        LineInfo* li = (*allBlockLineInfos)[i];
        Function* f = bb->getFunction();

        uint32_t loopId = Invalid_UInteger_ID; 
        Loop* loop = bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex());
        if (loop){
            loopId = loop->getIndex();
        }
        uint32_t loopDepth = bb->getFlowGraph()->getLoopDepth(bb->getIndex());
        uint32_t loopCount = bb->getFlowGraph()->getNumberOfLoops();

        char* fileName;
        uint32_t lineNo;
        if (li){
            fileName = li->getFileName();
            lineNo = li->GET(lr_line);
        } else {
            fileName = INFO_UNKNOWN;
            lineNo = 0;
        }
        fprintf(staticFD, "%d\t%lld\t%d\t%d\t%d\t%s:%d\t%s\t# %#llx\t%#llx\n", 
                (*allBlockIds)[i], bb->getHashCode().getValue(), bb->getNumberOfMemoryOps(), bb->getNumberOfFloatOps(), 
                bb->getNumberOfInstructions(), fileName, lineNo, bb->getFunction()->getName(), 
                bb->getHashCode().getValue(), bb->getLeader()->getProgramAddress());

        if (printDetail){
            uint32_t loopLoc = 0;
            if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())){
                if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())->getHead()->getHashCode().getValue() == bb->getHashCode().getValue()){
                    loopLoc = 1;
                } else if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())->getTail()->getHashCode().getValue() == bb->getHashCode().getValue()){
                    loopLoc = 2;
                }
            }
            fprintf(staticFD, "\t+lpi\t%d\t%d\t%d\t%d # %#llx\n", loopCount, loopId, loopDepth, loopLoc, bb->getHashCode().getValue());
            fprintf(staticFD, "\t+cnt\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d # %#llx\n", 
                    bb->getNumberOfBranches(), bb->getNumberOfIntegerOps(), bb->getNumberOfLogicOps(), bb->getNumberOfShiftRotOps(),
                    bb->getNumberOfSyscalls(), bb->getNumberOfSpecialRegOps(), bb->getNumberOfStringOps(),
                    bb->getNumberOfLoads(), bb->getNumberOfStores(), bb->getNumberOfMemoryOps(), bb->getHashCode().getValue());

            //            ASSERT(bb->getNumberOfLoads() + bb->getNumberOfStores() == bb->getNumberOfMemoryOps());

            memopavg = 0.0;
            if (bb->getNumberOfMemoryOps()){
                memopavg = ((float)bb->getNumberOfMemoryBytes())/((float)bb->getNumberOfMemoryOps());
            }
            fprintf(staticFD, "\t+mem\t%d\t%d\t%.5f # %#llx\n", bb->getNumberOfMemoryOps(), bb->getNumberOfMemoryBytes(),
                    memopavg, bb->getHashCode().getValue());

            uint64_t loopHead = 0;
            uint64_t parentHead = 0;
            if (loop){
                loopHead = loop->getHead()->getHashCode().getValue();
                parentHead = f->getFlowGraph()->getParentLoop(loop->getIndex())->getHead()->getHashCode().getValue();
            }
            fprintf(staticFD, "\t+lpc\t%lld\t%lld # %#llx\n", loopHead, parentHead, bb->getHashCode().getValue());

            uint32_t currINT = 0;
            uint32_t currFP = 0;
            uint32_t currDist = 1;

            fprintf(staticFD, "\t+dud");
            while (currDist < MAX_DEF_USE_DIST_PRINT){
                for (uint32_t k = 0; k < bb->getNumberOfInstructions(); k++){
                    if (bb->getInstruction(k)->getDefUseDist() == currDist){
                        if (bb->getInstruction(k)->isFloatPOperation()){
                            currFP++;
                        } else {
                            currINT++;
                        }
                    }
                }
                if (currFP > 0 || currINT > 0){
                    fprintf(staticFD, "\t%d:%d:%d", currDist, currINT, currFP);
                }
                currDist++;
                currINT = 0;
                currFP = 0;
            }
            fprintf(staticFD, " # %#llx\n", bb->getHashCode().getValue());

            fprintf(staticFD, "\t+dxi\t%d\t%d # %#llx\n", bb->getDefXIter(), bb->endsWithCall(), bb->getHashCode().getValue());

            uint64_t callTgtAddr = 0;
            char* callTgtName = INFO_UNKNOWN;
            if (bb->endsWithCall()){
                callTgtAddr = bb->getExitInstruction()->getTargetAddress();
                Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(callTgtAddr);
                if (functionSymbol && functionSymbol->getSymbolName()){
                    callTgtName = functionSymbol->getSymbolName();
                }
            }
            fprintf(staticFD, "\t+ipa\t%#llx\t%s # %#llx\n", callTgtAddr, callTgtName, bb->getHashCode().getValue());

            bb->setBins();
            fprintf(staticFD, "\t+bin\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d # %#llx\n", 
                    bb->getNumberOfBinUnknown(), bb->getNumberOfBinInvalid(), bb->getNumberOfBinCond(), bb->getNumberOfBinUncond(), 
                    bb->getNumberOfBinBin(), bb->getNumberOfBinBinv(), bb->getNumberOfBinByte(), bb->getNumberOfBinBytev(),
                    bb->getNumberOfBinWord(), bb->getNumberOfBinWordv(), bb->getNumberOfBinDword(), bb->getNumberOfBinDwordv(),
                    bb->getNumberOfBinQword(), bb->getNumberOfBinQwordv(),
                    bb->getNumberOfBinSingle(), bb->getNumberOfBinSinglev(), bb->getNumberOfBinSingles(),
                    bb->getNumberOfBinDouble(), bb->getNumberOfBinDoublev(), bb->getNumberOfBinDoubles(), bb->getNumberOfBinMove(),
                    bb->getNumberOfBinStack(), bb->getNumberOfBinString(), bb->getNumberOfBinSystem(), bb->getNumberOfBinCache(),
                    bb->getNumberOfBinMem(), bb->getNumberOfBinOther(), bb->getHashCode().getValue());

        }
    }
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

void InstrumentationTool::printStaticFilePerInstruction(Vector<X86Instruction*>* allInstructions, Vector<uint32_t>* allInstructionIds, Vector<LineInfo*>* allInstructionLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allInstructionLineInfos).size() || (*allInstructions).size() == (*allInstructionLineInfos).size());
    ASSERT((*allInstructions).size() == (*allInstructionIds).size());

    uint32_t numberOfInstPoints = (*allInstructions).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), getInstSuffix(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getInstSuffix());
    fprintf(staticFD, "# phase     = %d\n", 0);
    fprintf(staticFD, "# type      = %s\n", briefName());
    fprintf(staticFD, "# cantidate = %d\n", getNumberOfExposedInstructions());
    char* sha1sum = getElfFile()->getSHA1Sum();
    fprintf(staticFD, "# sha1sum   = %s\n", sha1sum);
    fprintf(staticFD, "# perinsn   = yes\n");
    delete[] sha1sum;

    uint32_t memopcnt = 0;
    uint32_t membytcnt = 0;
    uint32_t fltopcnt = 0;
    uint32_t insncnt = 0;
    for (uint32_t i = 0; i < allInstructions->size(); i++){
        X86Instruction* ins = (*allInstructions)[i];
        if (ins->isMemoryOperation()){
            memopcnt++;
        }
        membytcnt += ins->getNumberOfMemoryBytes();
        if (ins->isFloatPOperation()){
            fltopcnt++;
        }
        insncnt++;
    }
    fprintf(staticFD, "# blocks    = %d\n", allInstructions->size());
    fprintf(staticFD, "# memops    = %d\n", memopcnt);

    float memopavg = 0.0;
    if (memopcnt){
        memopavg = (float)membytcnt/(float)memopcnt;
    }
    fprintf(staticFD, "# memopbyte = %d ( %.5f bytes/op)\n", membytcnt, memopavg);
    fprintf(staticFD, "# fpops     = %d\n", fltopcnt);
    fprintf(staticFD, "# insns     = %d\n", insncnt);
    fprintf(staticFD, "# buffer    = %d\n", bufferSize);
    for (uint32_t i = 0; i < getNumberOfInstrumentationLibraries(); i++){
        fprintf(staticFD, "# library   = %s\n", getInstrumentationLibrary(i));
    }
    fprintf(staticFD, "# libTag    = %s\n", "revision REVISION");
    fprintf(staticFD, "# %s\n", "<no additional info>");
    fprintf(staticFD, "# <sequence> <block_unqid> <memop> <fpop> <insn> <line> <fname> # <hex_unq_id> <vaddr>\n");

    if (printDetail){
        fprintf(staticFD, "# +lpi <loopcnt> <loopid> <ldepth> <lploc>\n");
        fprintf(staticFD, "# +cnt <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op>\n");
        fprintf(staticFD, "# +mem <total_mem_op> <total_mem_bytes> <bytes/op>\n");
        fprintf(staticFD, "# +lpc <loop_head> <parent_loop_head>\n");
        fprintf(staticFD, "# +dud <dudist1>:<duint1>:<dufp1> <dudist2>:<ducnt2>:<dufp2>...\n");
        fprintf(staticFD, "# +dxi <count_def_use_cross> <count_call>\n");
        fprintf(staticFD, "# +ipa <call_target_addr> <call_target_name>\n");
    }

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        X86Instruction* ins = (*allInstructions)[i];
        Function* f = (Function*)ins->getContainer();
        BasicBlock* bb = f->getBasicBlockAtAddress(ins->getBaseAddress());
        LineInfo* li = (*allInstructionLineInfos)[i];

        HashCode* hc = ins->generateHashCode(bb);
        uint64_t hashValue = hc->getValue();

        uint32_t loopId = Invalid_UInteger_ID; 
        Loop* loop = bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex());
        if (loop){
            loopId = loop->getIndex();
        }
        uint32_t loopDepth = bb->getFlowGraph()->getLoopDepth(bb->getIndex());
        uint32_t loopCount = bb->getFlowGraph()->getNumberOfLoops();

        char* fileName;
        uint32_t lineNo;
        if (li){
            fileName = li->getFileName();
            lineNo = li->GET(lr_line);
        } else {
            fileName = INFO_UNKNOWN;
            lineNo = 0;
        }
        fprintf(staticFD, "%d\t%lld\t%d\t%d\t%d\t%s:%d\t%s\t# %#llx\t%#llx\n", 
                (*allInstructionIds)[i], hashValue, (uint32_t)ins->isMemoryOperation(), (uint32_t)ins->isFloatPOperation(), 
                1, fileName, lineNo, f->getName(),
                hashValue, ins->getProgramAddress());

        if (printDetail){

            // TODO +lpi info is per-block still
            uint32_t loopLoc = 0;
            if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())){
                if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())->getHead()->getHashCode().getValue() == bb->getHashCode().getValue()){
                    loopLoc = 1;
                } else if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())->getTail()->getHashCode().getValue() == bb->getHashCode().getValue()){
                    loopLoc = 2;
                }
            }
            fprintf(staticFD, "\t+lpi\t%d\t%d\t%d\t%d # %#llx\n", loopCount, loopId, loopDepth, loopLoc, hashValue);
            fprintf(staticFD, "\t+cnt\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d # %#llx\n", 
                    (uint32_t)ins->isBranch(), (uint32_t)ins->isIntegerOperation(), (uint32_t)ins->isLogicOp(), (uint32_t)ins->isSpecialRegOp(),
                    (uint32_t)ins->isSystemCall(), (uint32_t)ins->isSpecialRegOp(), (uint32_t)ins->isStringOperation(),
                    (uint32_t)ins->isLoad(), (uint32_t)ins->isStore(), (uint32_t)ins->isMemoryOperation(), hashValue);

            if (ins->isMemoryOperation()){
                ASSERT(ins->isLoad() || ins->isStore());
            }

            memopavg = (float)ins->getNumberOfMemoryBytes();
            fprintf(staticFD, "\t+mem\t%d\t%d\t%.5f # %#llx\n", (uint32_t)ins->isMemoryOperation(), ins->getNumberOfMemoryBytes(),
                    memopavg, hashValue);

            uint64_t loopHead = 0;
            uint64_t parentHead = 0;
            if (loop){
                loopHead = loop->getHead()->getHashCode().getValue();
                parentHead = f->getFlowGraph()->getParentLoop(loop->getIndex())->getHead()->getHashCode().getValue();
            }
            fprintf(staticFD, "\t+lpc\t%lld\t%lld # %#llx\n", loopHead, parentHead, hashValue);

            uint32_t currINT = 0;
            uint32_t currFP = 0;
            uint32_t currDist = 1;

            fprintf(staticFD, "\t+dud");
            while (currDist < MAX_DEF_USE_DIST_PRINT){
                if (ins->getDefUseDist() == currDist){
                    if (ins->isFloatPOperation()){
                        currFP++;
                    } else {
                        currINT++;
                    }
                }
                if (currFP > 0 || currINT > 0){
                    fprintf(staticFD, "\t%d:%d:%d", currDist, currINT, currFP);
                }
                currDist++;
                currINT = 0;
                currFP = 0;
            }
            fprintf(staticFD, " # %#llx\n", hashValue);

            // TODO +dxi info is per-block still
            fprintf(staticFD, "\t+dxi\t%d\t%d # %#llx\n", bb->getDefXIter(), bb->endsWithCall(), hashValue);

            uint64_t callTgtAddr = 0;
            char* callTgtName = INFO_UNKNOWN;
            if (ins->isCall()){
                callTgtAddr = ins->getTargetAddress();
                Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(callTgtAddr);
                if (functionSymbol && functionSymbol->getSymbolName()){
                    callTgtName = functionSymbol->getSymbolName();
                }
            }
            fprintf(staticFD, "\t+ipa\t%#llx\t%s # %#llx\n", callTgtAddr, callTgtName, hashValue);
        }

        delete hc;
    }
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
