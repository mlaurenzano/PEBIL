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

typedef struct {
    uint64_t id;
    uint64_t data;
} ThreadData;
#define ThreadHashShift (12)
#define ThreadHashMod   (0xffff)

#define MAX_DEF_USE_DIST_PRINT 1024

// returns a map of function addresses and the scratch register used to hold the thread data address
// (X86_REG_INVALID if no such register is available)
std::map<uint64_t, uint32_t>* InstrumentationTool::threadReadyCode(){
    std::map<uint64_t, uint32_t>* functionThreading = new std::map<uint64_t, uint32_t>();
    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        Function* f = getExposedFunction(i);
        (*functionThreading)[f->getBaseAddress()] = instrumentForThreading(f);
    }
    return functionThreading;
}

Vector<X86Instruction*>* InstrumentationTool::atomicIncrement(uint32_t dest, uint32_t scratch, uint32_t count, uint64_t memaddr, Vector<X86Instruction*>* insns){
    Vector<X86Instruction*>* fill = insns;
    if (fill == NULL){
        fill = new Vector<X86Instruction*>();
    }

    // mov $memops,%sr2
    fill->append(X86InstructionFactory64::emitMoveImmToReg(count, dest));
    // mov $bufstr,%sr1
    fill->append(X86InstructionFactory64::emitMoveImmToReg(memaddr, scratch));
    // [lock] xadd %sr2,%sr1
    fill->append(X86InstructionFactory64::emitExchangeAdd(dest, scratch, isThreadedMode()));

    return fill;
}

uint32_t InstrumentationTool::instrumentForThreading(Function* func){
    uint32_t d = func->getDeadGPR(0);

    uint32_t numberOfInstructions = func->getNumberOfInstructions();
    X86Instruction** allInstructions = new X86Instruction*[numberOfInstructions];
    func->getAllInstructions(allInstructions,0);

    // has a dead register throughout, so at function entry only compute the thread data addr and put
    // into that dead reg
    if (d < X86_64BIT_GPRS){
        //PRINT_INFOR("Function %s has dead reg %d", func->getName(), d);
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            X86Instruction* entry = allInstructions[i];
            if (i == 0 || entry->isCall()){
                InstLocations loc = InstLocation_after;
                if (i == 0 && !entry->isCall()){
                    loc = InstLocation_prior;
                }
                
                BitSet<uint32_t>* inv = new BitSet<uint32_t>(X86_ALU_REGS);
                for (uint32_t j = X86_64BIT_GPRS; j < X86_ALU_REGS; j++){
                    inv->insert(j);
                }
                inv->insert(X86_REG_AX);
                inv->insert(X86_REG_SP);
                inv->insert(d);
                BitSet<uint32_t>* deadRegs = entry->getDeadRegIn(inv, 1);
                
                uint32_t s;
                for (uint32_t j = 0; j < X86_64BIT_GPRS; j++){
                    if (deadRegs->contains(j)){
                        s = j;
                        break;
                    }
                }
                //PRINT_INFOR("\t\tassigning data at %#lx to reg %d via scratch %d", entry->getBaseAddress(), d, s);
                InstrumentationSnippet* snip = addInstrumentationSnippet();
                Vector<X86Instruction*>* insns;

                insns = storeThreadData(s, d);
                for (uint32_t i = 0; i < insns->size(); i++){
                    snip->addSnippetInstruction((*insns)[i]);
                }
                delete insns;
                InstrumentationPoint* p = addInstrumentationPoint(entry, snip, InstrumentationMode_inline, loc);
                p->setPriority(InstPriority_userinit);
            }
        }
    }
    // no dead register in the function. store the thread data addr on the stack
    else {
        d = X86_REG_INVALID;

        // figuring out the size of the stack frame is REALLY HARD
        return d;

        /*
        PRINT_INFOR("Function %s has no dead reg", func->getName());

        for (uint32_t i = 0; i < numberOfInstructions; i++){
            X86Instruction* entry = allInstructions[i];
            if (i == 0 || entry->isCall()){
                BasicBlock* bb;

                InstLocations loc;
                if (i == 0 && !entry->isCall()){
                    bb = func->getBasicBlockAtAddress(entry->getBaseAddress());
                    ASSERT(bb);
                    loc = InstLocation_prior;
                } else {
                    bb = func->getBasicBlockAtAddress(entry->getBaseAddress() + entry->getSizeInBytes());
                    loc = InstLocation_after;
                }
                if (bb == NULL){
                    continue;
                }

                uint32_t stackPatch = 0;
                BitSet<uint32_t>* inv = new BitSet<uint32_t>(X86_ALU_REGS);
                for (uint32_t j = X86_64BIT_GPRS; j < X86_ALU_REGS; j++){
                    inv->insert(j);
                }
                inv->insert(X86_REG_SP);
                BitSet<uint32_t>* deadRegs = entry->getDeadRegIn(inv);
                
                uint32_t s1 = -1;
                uint32_t s2 = -1;
                for (uint32_t j = 0; j < X86_64BIT_GPRS; j++){
                    if (deadRegs->contains(j)){
                        if (s1 == -1){
                            s1 = j;
                        } else {
                            s2 = j;
                            break;
                        }
                    }
                }

                if (s1 == -1){
                    s1 = X86_REG_CX;
                    s2 = X86_REG_DX;
                    stackPatch += sizeof(uint64_t);
                    stackPatch += sizeof(uint64_t);
                } else if (s2 == -1){
                    s2 = X86_REG_CX;
                    if (s1 == s2){
                        s2 = X86_REG_DX;
                    }
                    stackPatch += sizeof(uint64_t);
                }

                if (func->hasLeafOptimization() || bb->isEntry()){
                    if (stackPatch > 0){
                        stackPatch += Size__trampoline_autoinc;
                    }
                }
                if (bb->isEntry()){
                    stackPatch += func->getStackSize();
                }

                PRINT_INFOR("\t\tassigning data at %#lx to stack via scratches %d %d with patch %#x", entry->getBaseAddress(), s1, s2, stackPatch);
                InstrumentationSnippet* snip = addInstrumentationSnippet();
                Vector<X86Instruction*>* insns;

                insns = storeThreadData(s1, s2, true, stackPatch);
                for (uint32_t i = 0; i < insns->size(); i++){
                    snip->addSnippetInstruction((*insns)[i]);
                }
                delete insns;
                InstrumentationPoint* p = addInstrumentationPoint(entry, snip, InstrumentationMode_inline, loc);
                p->setPriority(InstPriority_userinit);
            }
        }
        */
    }

    delete[] allInstructions;
    return d;
}

void InstrumentationTool::assignStoragePrior(InstrumentationPoint* pt, uint32_t value, uint64_t address, uint8_t tmpreg, uint64_t regbak){
    if (getElfFile()->is64Bit()){
        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(tmpreg, regbak));
        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(value, tmpreg));
        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveRegToMem(tmpreg, address));
        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveMemToReg(regbak, tmpreg, true));
    } else {
        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(tmpreg, regbak));
        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(value, tmpreg));
        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveRegToMem(tmpreg, address));
        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveMemToReg(regbak, tmpreg));
    }
}

bool InstrumentationTool::singleArgCheck(void* arg, uint32_t mask, const char* name){
    if (arg == NULL &&
        (requiresArgs() & mask)){
        PRINT_ERROR("Argument required by %s: %s", briefName(), name);
        return false;
    }
    uint32_t allowed = requiresArgs() | allowsArgs();
    if (arg != NULL &&
        !(allowed & mask)){
        PRINT_ERROR("Argument not allowed by %s: %s", briefName(), name);
        return false;
    }
    return true;
}

bool InstrumentationTool::verifyArgs(){
    singleArgCheck((void*)phaseNo, PEBIL_OPT_PHS, "--phs");
    //singleArgCheck((void*)loopIncl, PEBIL_OPT_LPI, "--lpi");
    //singleArgCheck((void*)printDetail, PEBIL_OPT_DTL, "--dtl");
    singleArgCheck((void*)inputFile, PEBIL_OPT_INP, "--inp");
    singleArgCheck((void*)dfpFile, PEBIL_OPT_DFP, "--dfp");
    singleArgCheck((void*)trackFile, PEBIL_OPT_TRK, "--trk");
    singleArgCheck((void*)doIntro, PEBIL_OPT_DOI, "--doi");
    return true;
}

const char* InstrumentationTool::getExtension(){
    if (extension){
        return extension;
    }
    return defaultExtension();
}

void InstrumentationTool::init(char* ext){
    extension = ext;
}

void InstrumentationTool::initToolArgs(bool lpi, bool dtl, bool doi, uint32_t phase, char* inp, char* dfp, char* trk){
    loopIncl = true;
    printDetail = true;
    doIntro = doi;
    phaseNo = phase;
    inputFile = inp;
    dfpFile = dfp;
    trackFile = trk;
}

InstrumentationTool::InstrumentationTool(ElfFile* elf)
    : ElfFileInst(elf)
{}

void InstrumentationTool::declare(){
#ifdef HAVE_MPI
    initWrapperC = declareFunction(MPI_INIT_WRAPPER_CBIND);
    initWrapperF = declareFunction(MPI_INIT_WRAPPER_FBIND);
    ASSERT(initWrapperC && "Cannot find MPI_Init function, are you sure it was declared?");
    ASSERT(initWrapperF && "Cannot find MPI_Init function, are you sure it was declared?");
#endif //HAVE_MPI
}

void InstrumentationTool::instrument(){
    imageKey = reserveDataOffset(sizeof(uint64_t));
    threadHash = reserveDataOffset(sizeof(ThreadData) * (ThreadHashMod + 1));

#ifdef HAVE_MPI
    int initFound = 0;

    // wrap any call to MPI_Init
    Vector<X86Instruction*>* mpiInitCalls = findAllCalls(MPI_INIT_LIST_CBIND_PREF);
    initWrapperC->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding MPI_Init wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperC, InstrumentationMode_tramp, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;

    mpiInitCalls = findAllCalls(MPI_INIT_LIST_FBIND_PREF);
    initWrapperF->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding mpi_init_ wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperF, InstrumentationMode_tramp, InstLocation_replace);
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
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperC, InstrumentationMode_tramp, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;

    mpiInitCalls = findAllCalls(MPI_INIT_LIST_FBIND);
    initWrapperF->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding mpi_init_ wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperF, InstrumentationMode_tramp, InstLocation_replace);
        initFound++;
    }
    delete mpiInitCalls;

#endif //HAVE_MPI
}

Vector<X86Instruction*>* InstrumentationTool::storeThreadData(uint32_t scratch, uint32_t dest){
    return storeThreadData(scratch, dest, false, 0);
}

Vector<X86Instruction*>* InstrumentationTool::storeThreadData(uint32_t scratch, uint32_t dest, bool storeToStack, uint32_t stackPatch){
    ASSERT(scratch < X86_64BIT_GPRS);
    ASSERT(dest < X86_64BIT_GPRS);
    ASSERT(scratch != dest);
    Vector<X86Instruction*>* insns = new Vector<X86Instruction*>();

    // mov %fs:0x10,%d
    insns->append(X86InstructionFactory64::emitMoveTLSOffsetToReg(0x10, dest));
    // srl $12,%d
    insns->append(X86InstructionFactory64::emitShiftRightLogical(12, dest));
    // and $0xffff,%d
    insns->append(X86InstructionFactory64::emitImmAndReg(0xffff, dest));
    // mov $TData,%sr
    insns->append(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, scratch), this, getInstDataAddress() + threadHash, false));
    // sll $4,%d
    insns->append(X86InstructionFactory64::emitShiftLeftLogical(4, dest));
    // lea [$0x08+$offset](0,%d,%sr),%d
    insns->append(X86InstructionFactory64::emitLoadEffectiveAddress(scratch, dest, 0, 0x08, dest, true, true));

    if (storeToStack){
        // knowing where this info is relative to %sp is HARD
        __FUNCTION_NOT_IMPLEMENTED;
        // mov (%d),%sr
        insns->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(dest, 0, scratch));
        // mov %sr,0x200(%sp)
        insns->append(X86InstructionFactory64::emitMoveRegToRegaddrImm(scratch, X86_REG_SP, (-1)*(0x400 + stackPatch), true));
    } else {
        // mov (%d),%d
        insns->append(X86InstructionFactory64::emitMoveRegaddrImmToReg(dest, 0, dest));
    }

    return insns;
}

InstrumentationPoint* InstrumentationTool::insertInlinedTripCounter(uint64_t counterOffset, X86Instruction* bestinst, bool add, uint32_t threadReg, InstLocations loc, BitSet<uint32_t>* useRegs, uint32_t val){

    uint32_t regLimit = X86_32BIT_GPRS;
    if (getElfFile()->is64Bit()){
        regLimit = X86_64BIT_GPRS;
    }

    uint32_t sr1 = regLimit;
    uint32_t sr2 = regLimit;

    if (useRegs){
        for (uint32_t i = 0; i < regLimit; i++){
            if (useRegs->contains(i)){
                if (sr1 == regLimit){
                    sr1 = i;
                } else if (sr2 == regLimit){
                    sr2 = i;
                    break;
                }
            }
        }
    } else {
        for (uint32_t i = 0; i < regLimit; i++){
            bool useit = false;
            if (loc == InstLocation_prior && bestinst->isRegDeadIn(i)){
                useit = true;
            }
            if (loc == InstLocation_after && bestinst->isRegDeadOut(i)){
                useit = true;
            }

            if (i == X86_REG_SP || i == X86_REG_AX){
                continue;
            }
            if (useit){
                if (sr1 == regLimit){
                    sr1 = i;
                } else if (sr2 == regLimit){
                    sr2 = i;
                    break;
                }
            }
        }
    }

    // best inst point didn't have enough dead regs...
    if (sr1 == regLimit){
        sr1 = X86_REG_CX;
        sr2 = X86_REG_DX;
    } else if (sr2 == regLimit){
        sr2 = X86_REG_DX;
        if (sr1 == sr2){
            sr2 = X86_REG_CX;
        }
    }

    ASSERT(sr1 != X86_REG_SP && sr2 != X86_REG_SP);
    ASSERT(sr1 != X86_REG_AX && sr2 != X86_REG_AX);

    InstrumentationSnippet* snip = addInstrumentationSnippet();
    snip->setOverflowable(false);

    // snippet contents, in this case just increment a counter
    if (is64Bit()){
        // any threaded
        if (isThreadedMode()){
            // load thread data base addr into %sr1
            if (threadReg == X86_REG_INVALID){
                /*
                uint32_t stackPatch = 0;
                if (loc == InstLocation_prior && !bestinst->isRegDeadIn(sr1)){
                    stackPatch += sizeof(uint64_t);
                }
                if (loc == InstLocation_after && !bestinst->isRegDeadOut(sr1)){
                    stackPatch += sizeof(uint64_t);
                }

                snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrImmToReg(X86_REG_SP, (-1) * (0x400 - stackPatch), sr1));
                */
                //PRINT_INFOR("Full thread data computation for offset %lx", counterOffset);
                Vector<X86Instruction*>* loadThreadData = storeThreadData(sr2, sr1);
                for (uint32_t i = 0; i < loadThreadData->size(); i++){
                    snip->addSnippetInstruction((*loadThreadData)[i]);
                }
                delete loadThreadData;
            } else {
                sr1 = threadReg;
            }
            if (add){
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(val, sr1, counterOffset));
            } else {
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(-1 * val, sr1, counterOffset));
            }
        }
        // non-threaded executable
        else if (getElfFile()->isExecutable()){
            if (add){
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToMem(val, getInstDataAddress() + counterOffset));
            } else {
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToMem(-1 * val, getInstDataAddress() + counterOffset));
            }
        }
        // non-threaded shared library
        else {
            snip->addSnippetInstruction(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, sr1), this, getInstDataAddress() + counterOffset, false));
            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegaddrToReg(sr1, sr2));
            if (add){
                snip->addSnippetInstruction(X86InstructionFactory64::emitRegAddImm(sr2, val));
            } else {
                snip->addSnippetInstruction(X86InstructionFactory64::emitRegSubImm(sr2, val));
            }
            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddr(sr2, sr1));
        }
    } else {
        ASSERT(getElfFile()->isExecutable());
        ASSERT(!isThreadedMode());
        uint32_t v = val;
        while (v > 0x7f){
            if (add){
                snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(0x7f, getInstDataAddress() + counterOffset));
            } else {
                snip->addSnippetInstruction(X86InstructionFactory32::emitSubImmByteToMem(0x7f, getInstDataAddress() + counterOffset));
            }
            v -= 0x7f;
        }
        if (add){
            snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(v, getInstDataAddress() + counterOffset));
        } else {
            snip->addSnippetInstruction(X86InstructionFactory32::emitSubImmByteToMem(v, getInstDataAddress() + counterOffset));
        }
    }

    InstrumentationPoint* p = addInstrumentationPoint(bestinst, snip, InstrumentationMode_inline, loc);

    return p;
}

InstrumentationPoint* InstrumentationTool::insertBlockCounter(uint64_t counterOffset, Base* within){
    return insertBlockCounter(counterOffset, within, true, -1);
}

InstrumentationPoint* InstrumentationTool::insertBlockCounter(uint64_t counterOffset, Base* within, bool add, uint32_t threadReg){
    return insertBlockCounter(counterOffset, within, add, threadReg, 1);
}

InstrumentationPoint* InstrumentationTool::insertBlockCounter(uint64_t counterOffset, Base* within, bool add, uint32_t threadReg, uint32_t inc){
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

    // register an instrumentation point at the function that uses this snippet
    X86Instruction* bestinst;
    InstLocations loc;
    BitSet<uint32_t>* validRegs = new BitSet<uint32_t>(X86_ALU_REGS);
    BitSet<uint32_t>* useRegs = new BitSet<uint32_t>(X86_ALU_REGS);

    uint32_t regLimit = X86_32BIT_GPRS;
    if (getElfFile()->is64Bit()){
        regLimit = X86_64BIT_GPRS;
    }
    
    for (uint32_t i = 0; i < regLimit; i++){
        validRegs->insert(i);
    }
    validRegs->remove(X86_REG_AX);
    validRegs->remove(X86_REG_SP);

    bestinst = scope->findBestInstPoint(&loc, validRegs, useRegs, true);

    InstrumentationPoint* p = insertInlinedTripCounter(counterOffset, bestinst, add, threadReg, loc, useRegs, inc);

    delete validRegs;
    delete useRegs;

    return p;
}

void InstrumentationTool::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allBlockLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allBlockLineInfos).size() || (*allBlocks).size() == (*allBlockLineInfos).size());
    ASSERT((*allBlocks).size() == (*allBlockIds).size());

    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), getExtension(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getExtension());
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
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), getExtension(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getExtension());
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

            uint32_t loopLoc = 0;
            if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())){
                if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())->getHead()->getHashCode().getValue() == bb->getHashCode().getValue()){
                    if (bb->getLeader()->getBaseAddress() == ins->getBaseAddress()){
                        loopLoc = 1;
                    }
                } else if (bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex())->getTail()->getHashCode().getValue() == bb->getHashCode().getValue()){
                    if (bb->getExitInstruction()->getBaseAddress() == ins->getBaseAddress()){
                        loopLoc = 2;
                    }
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
                HashCode* headHash = loop->getHead()->getLeader()->generateHashCode(loop->getHead());
                HashCode* parentHash = f->getFlowGraph()->getParentLoop(loop->getIndex())->getHead()->getLeader()->generateHashCode(f->getFlowGraph()->getParentLoop(loop->getIndex())->getHead());
                loopHead = headHash->getValue();
                parentHead = parentHash->getValue();

                delete headHash;
                delete parentHash;
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

            fprintf(staticFD, "\t+dxi\t%d\t%d # %#llx\n", (uint32_t)ins->hasDefXIter(), ins->isCall(), hashValue);

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
