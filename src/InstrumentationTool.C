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

void InstrumentationTool::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
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
    }

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        BasicBlock* bb = (*allBlocks)[i];
        LineInfo* li = (*allLineInfos)[i];
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

            ASSERT(bb->getNumberOfLoads() + bb->getNumberOfStores() == bb->getNumberOfMemoryOps());

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
        }
    }
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
