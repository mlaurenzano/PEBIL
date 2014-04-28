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
#include <HybridPhiElfFile.h>

#include <algorithm>
#include <vector>

#define THREAD_EVIDENCE "clone:__clone:__clone2:pthread_.*:omp_.*"

#define MPI_INIT_WRAPPER_CBIND   "MPI_Init_pebil_wrapper"
#define MPI_INIT_LIST_CBIND_PREF "PMPI_Init"
#define MPI_INIT_LIST_CBIND      "MPI_Init"
#define MPI_INIT_WRAPPER_FBIND   "mpi_init__pebil_wrapper"
#define MPI_INIT_LIST_FBIND_PREF "pmpi_init_"
#define MPI_INIT_LIST_FBIND      "mpi_init_:MPI_INIT"

#define DYNAMIC_INST_INIT "tool_dynamic_init"

uint64_t InstrumentationTool::reserveDynamicPoints(){
    return reserveDataOffset(sizeof(DynamicInst) * dynamicPoints.size());
}

void InstrumentationTool::applyDynamicPoints(uint64_t dynArray){
    if (dynamicPoints.size() == 0){
        return;
    }


    uint64_t temp64 = dynamicPoints.size();
    initializeReservedData(getInstDataAddress() + dynamicSize, sizeof(uint64_t), (void*)&temp64);
    initializeReservedPointer(dynArray, dynamicPointArray);
    PRINT_INFOR("Initializing %d dynamic points", dynamicPoints.size());

    uint32_t dindex = 0;
    while (dynamicPoints.size()){
        DynamicInst d;
        DynamicInstInternal* di = dynamicPoints.remove(0);
        d.VirtualAddress = di->Point->getInstSourceAddress();
        initializeReservedPointer(d.VirtualAddress - getInstDataAddress(), dynArray + (dindex * sizeof(DynamicInst)) + offsetof(DynamicInst, VirtualAddress));
        d.ProgramAddress = di->Point->getSourceObject()->getProgramAddress();
        d.Key = di->Key;
        d.Flags = 0;
        if (di->Point->getInstrumentationMode() == InstrumentationMode_inline){
            d.Size = di->Point->getNumberOfBytes();
        } else {
            d.Size = Size__uncond_jump;
        }
        ASSERT(d.Size);
        d.IsEnabled = di->IsEnabled;

        ASSERT(d.Size <= DYNAMIC_POINT_SIZE_LIMIT);
        Vector<X86Instruction*>* nops = X86InstructionFactory::emitNopSeries(d.Size);
        uint32_t b = 0;
        for (uint32_t i = 0; i < nops->size(); i++){
            memcpy(&(d.OppContent[b]), (*nops)[i]->charStream(), (*nops)[i]->getSizeInBytes());
            b += (*nops)[i]->getSizeInBytes();
            delete (*nops)[i];
        }
        ASSERT(b == d.Size);
        delete nops;

        initializeReservedData(getInstDataAddress() + dynArray + (dindex * sizeof(DynamicInst)), sizeof(DynamicInst), &d);
        delete di;
        dindex++;
    }
}

void InstrumentationTool::dynamicPoint(InstrumentationPoint* pt, uint64_t key, bool enable){
    DynamicInstInternal* di = new DynamicInstInternal();
    //ASSERT(pt->getInstrumentationMode() == InstrumentationMode_inline);
    di->Point = pt;
    di->Key = key;
    di->IsEnabled = enable;
    dynamicPoints.append(di);
}

// returns a map of function addresses and the scratch register used to hold the thread data address
// (X86_REG_INVALID if no such register is available)
std::map<uint64_t, uint32_t>* InstrumentationTool::threadReadyCode(std::set<Base*>& objectsToInst){
    std::map<uint64_t, uint32_t>* functionThreading = new std::map<uint64_t, uint32_t>();

    for (std::set<Base*>::iterator it = objectsToInst.begin(); it != objectsToInst.end(); it++){
        Function* f;
        if ((*it)->getType() == PebilClassType_Function){
            f = (Function*)(*it);
        } else if ((*it)->getType() == PebilClassType_X86Instruction){
            TextObject* t = (TextObject*)(((X86Instruction*)(*it))->getContainer());
            ASSERT(t->isFunction());
            f = (Function*)t;
        } else if ((*it)->getType() == PebilClassType_BasicBlock){
            X86Instruction* l = ((BasicBlock*)(*it))->getLeader();
            TextObject* t = l->getContainer();
            ASSERT(t->isFunction());
            f = (Function*)t;
        } else {
            __SHOULD_NOT_ARRIVE;
        }

        if (functionThreading->count(f->getBaseAddress()) == 0){
            (*functionThreading)[f->getBaseAddress()] = instrumentForThreading(f);
        }
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
                delete inv;
                
                uint32_t s;
                for (uint32_t j = 0; j < X86_64BIT_GPRS; j++){
                    if (deadRegs->contains(j)){
                        s = j;
                        break;
                    }
                }
                delete deadRegs;

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
        // for now we just bail
        delete[] allInstructions;
        return d;
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

void InstrumentationTool::assignStoragePrior(InstrumentationPoint* pt, uint32_t value, uint8_t reg){
    if (getElfFile()->is64Bit()){
        pt->addPrecursorInstruction(X86InstructionFactory64::emitMoveImmToReg(value, reg));
    } else {
        pt->addPrecursorInstruction(X86InstructionFactory32::emitMoveImmToReg(value, reg));
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

bool InstrumentationTool::hasThreadEvidence(){
    Vector<X86Instruction*>* threadCalls = findAllCalls(THREAD_EVIDENCE);
    if (threadCalls->size() > 0){
        for (uint32_t i = 0; i < threadCalls->size(); i++){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol((*threadCalls)[i]->getTargetAddress());
            PRINT_WARN(20, "Found call to an apparent thread-related function (%s) at address %#lx", functionSymbol->getSymbolName(), (*threadCalls)[i]->getBaseAddress());
        }
        delete threadCalls;
        return true;
    }
    delete threadCalls;
    return false;
}

InstrumentationTool::InstrumentationTool(ElfFile* elf)
    : ElfFileInst(elf)
{
}

void InstrumentationTool::declare(){
#ifdef HAVE_MPI
    initWrapperC = declareFunction(MPI_INIT_WRAPPER_CBIND);
    initWrapperF = declareFunction(MPI_INIT_WRAPPER_FBIND);
    ASSERT(initWrapperC && "Cannot find MPI_Init function, are you sure it was declared?");
    ASSERT(initWrapperF && "Cannot find MPI_Init function, are you sure it was declared?");
#endif //HAVE_MPI
    dynamicInit = declareFunction(DYNAMIC_INST_INIT);
}


/*
* Instrumenting an embedded elf file
*
* Rewrite binary and rembed in a new section
* Find all references to __offload_target_image+xxx and point towards new image
*
*/
void InstrumentationTool::instrumentEmbeddedElf(){
    ASSERT(isHybridOffloadMode());
    PRINT_INFOR("Instrumenting an embedded elf object");

    HybridPhiElfFile* hybridElf = (HybridPhiElfFile*)elfFile;
    ElfFile* embeddedElf = hybridElf->getEmbeddedElf();
    if(embeddedElf == NULL) {
        PRINT_WARN(20, "Asked to instrument hybrid offload file, but no embedded elf image was found");
        return;
    }

    embeddedElf->dump("embedded_file", false);

    // Prepare for instrumentation
    if(embeddedElf->getProgramBaseAddress() < WEDGE_SHAMT) {
        if(!embeddedElf->isSharedLib()) {
            PRINT_WARN(20, "The base address of this binary is too small, but the binary is an executable.");
            PRINT_WARN(20, "Will attempt to shift all program addresses, which will probably fail because executables usually contain position-dependent code/data.");
        }
        PRINT_INFOR("Shifting virtual address of all program contents by %#lx", WEDGE_SHAMT);
        embeddedElf->wedge(WEDGE_SHAMT);
    }

    // Create instrumentor
    assert(maker);
    InstrumentationTool* instTool = maker(embeddedElf);

    char* inp_arg = NULL;
    instTool->init(NULL);
    instTool->initToolArgs(false, false, false, 0, inp_arg, NULL, NULL);
    ASSERT(instTool->verifyArgs());

    char* functionBlackList = NULL;
    instTool->setInputFunctions(functionBlackList);

    // TODO
    // instTool->setLibraryList(lnc_arg);
    // instTool->setAllowStatic();
    instTool->setThreadedMode();
    instTool->setMultipleImages();
    instTool->setPerInstruction();
    instTool->phasedInstrumentation();

    //instTool->print();
    //instTool->dump();

    // dump file to buffered output
    EmbeddedBinaryOutputFile outfile;
    instTool->dump(&outfile);

    delete instTool;

    IntelOffloadHeader* head = hybridElf->getIntelOffloadHeader();

    // get elf file size
    uint32_t outsize = outfile.size();
    uint32_t headsize = IntelOffloadHeader::INTEL_OFFLOAD_HEADER_SIZE;
    PRINT_INFOR("instrumented embedded elf size is 0x%llx\n", outsize);
    head->setElfSize(outsize);

    // reserve data
    uint32_t offset = reserveDataOffset(outsize + headsize);

    // initialize header
    initializeReservedData(getInstDataAddress() + offset, headsize, head->charStream());

    // initialize data
    initializeReservedData(getInstDataAddress() + offset + headsize, outsize, outfile.charStream());

    // create a dataref to the header
    DataReference* dataref = elfFile->generateDataRef(0, NULL, sizeof(uint64_t), getInstDataAddress() + offset);

    // Redirect all references to the original header to point to the new header
    Vector<AddressAnchor*>* imgAnchors = elfFile->searchAddressAnchors(head->getBaseAddress());
    for(int i = 0; i < imgAnchors->size(); ++i){
        AddressAnchor* anchor = (*imgAnchors)[i];
        anchor->updateLink(dataref);
    }
    delete imgAnchors;

    // Search for reference to the elf object itself
    imgAnchors = elfFile->searchAddressAnchors(head->getBaseAddress() + IntelOffloadHeader::INTEL_OFFLOAD_HEADER_SIZE);
    assert(imgAnchors->size() == 0);
    delete imgAnchors;
    
}

void InstrumentationTool::instrument(){
    if (!isThreadedMode()){
        if (hasThreadEvidence()){
            PRINT_ERROR("This image shows evidence of being threaded, but you ran pebil without --threaded.");
        }
    }

    if (isHybridOffloadMode()) {
        instrumentEmbeddedElf();
    }

    ASSERT(sizeof(uint64_t) == sizeof(image_key_t));
    image_key_t tmpi = (image_key_t)getElfFile()->getUniqueId();
    imageKey = reserveDataOffset(sizeof(image_key_t));
    initializeReservedData(getInstDataAddress() + imageKey, sizeof(image_key_t), &tmpi);

    threadHash = reserveDataOffset(sizeof(ThreadData) * (ThreadHashMod + 1));

    dynamicSize = reserveDataOffset(sizeof(uint64_t));
    dynamicPointArray = reserveDataOffset(sizeof(DynamicInst*));
    dynamicInit->addArgument(dynamicSize);
    dynamicInit->addArgument(dynamicPointArray);

    // ALL_FUNC_ENTER
    if (isMultiImage()){
        for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
            Function* f = getExposedFunction(i);

            InstrumentationPoint* p = addInstrumentationPoint(f, dynamicInit, InstrumentationMode_tramp, InstLocation_prior);
            ASSERT(p);
            p->setPriority(InstPriority_sysinit);
            if (!p->getInstBaseAddress()){
                PRINT_ERROR("Cannot find an instrumentation point at the entry function");
            }            

            dynamicPoint(p, getElfFile()->getUniqueId(), true);
        }
    } else {
        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), dynamicInit, InstrumentationMode_tramp);
        ASSERT(p);
        p->setPriority(InstPriority_sysinit);
        if (!p->getInstBaseAddress()){
            PRINT_ERROR("Cannot find an instrumentation point at the entry function");
        }
    }

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
    insns->append(X86InstructionFactory64::emitMoveThreadIdToReg(dest));
    // srl $12,%d
    insns->append(X86InstructionFactory64::emitShiftRightLogical(12, dest));
    // and $0xffff,%d
    insns->append(X86InstructionFactory64::emitImmAndReg(0xffff, dest));
    // mov $TData,%sr
    insns->append(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, scratch), getInstDataAddress() + threadHash, false));
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
        if (isThreadedMode() || isMultiImage()){
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
            snip->addSnippetInstruction(linkInstructionToData(X86InstructionFactory64::emitLoadRipImmReg(0, sr1), getInstDataAddress() + counterOffset, false));

            if (add){
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(val, sr1, 0));
            } else {
                snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmToRegaddrImm(-1 * val, sr1, 0));
            }
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

static bool isVectorInstruction(X86Instruction* ins) {
    X86InstructionType typ = ins->getInstructionType();
    switch(typ) {
        case X86InstructionType_simdFloat:
        case X86InstructionType_simdInt:
        case X86InstructionType_aes:
            break;
        default:
            return false;
    }
    return true;
}

void InstrumentationTool::printStaticFile(const char* extension, Vector<Base*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allBlockLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allBlockLineInfos).size() || (*allBlocks).size() == (*allBlockLineInfos).size());
    ASSERT((*allBlocks).size() == (*allBlockIds).size());

    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), extension, "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    char* debugFile = "problemInstructions";
    FILE* debugFD = fopen(debugFile, "w");

    char* vectorInstructions = "vectorInstructions";
    FILE* vectorFD = fopen(vectorInstructions, "w");

    FILE* skippedFD = fopen("nonVecInstructions", "w");


    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getExtension());
    fprintf(staticFD, "# phase     = %d\n", 0);
    fprintf(staticFD, "# type      = %s\n", briefName());
    fprintf(staticFD, "# cantidate = %d\n", getNumberOfExposedBasicBlocks());
    fprintf(staticFD, "# sha1sum   = %s\n", getElfFile()->getSHA1Sum());
    fprintf(staticFD, "# perinsn   = no\n");

    uint32_t memopcnt = 0;
    uint32_t membytcnt = 0;
    uint32_t fltopcnt = 0;
    uint32_t insncnt = 0;
    for (uint32_t i = 0; i < allBlocks->size(); i++){
        Base* b = (*allBlocks)[i];
        ASSERT(b->getType() == PebilClassType_BasicBlock);
        BasicBlock* bb = (BasicBlock*)b;

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
        fprintf(staticFD, "# +vec <#elem>x<elemSize>:<#fp>:<#int> ...\n");
    }

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    
    for (uint32_t i = 0; i < numberOfInstPoints; i++){
        Base* b = (*allBlocks)[i];
        ASSERT(b->getType() == PebilClassType_BasicBlock);
        BasicBlock* bb = (BasicBlock*)b;
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

            std::pebil_map_type<uint32_t, uint32_t> idist;
            std::pebil_map_type<uint32_t, uint32_t> fdist;
            std::vector<uint32_t> dlist;
            for (uint32_t k = 0; k < bb->getNumberOfInstructions(); k++){
                X86Instruction* x = bb->getInstruction(k);
                uint32_t d = x->getDefUseDist();
                if (d == 0){
                    continue;
                }

                if (idist.count(d) == 0){
                    idist[d] = 0;
                    fdist[d] = 0;
                    dlist.push_back(d);
                }
                if (x->isFloatPOperation()){
                    fdist[d] = fdist[d] + 1;
                } else {
                    idist[d] = idist[d] + 1;
                }
            }

            std::sort(dlist.begin(), dlist.end());
            for (std::vector<uint32_t>::iterator it = dlist.begin(); it != dlist.end(); it++){
                uint32_t d = (*it);
                fprintf(staticFD, "\t%d:%d:%d", d, idist[d], fdist[d]);
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



            // matrix to store counts elemsInVec X bytesInElem
            uint32_t fpvecs[64][16];
            uint32_t intvecs[64][16];
            bzero(fpvecs, sizeof(fpvecs));
            bzero(intvecs, sizeof(intvecs));
            for(uint32_t k = 0; k < bb->getNumberOfInstructions(); ++k){
                X86Instruction* ins = bb->getInstruction(k);

                if(!isVectorInstruction(ins)) {
                    fprintf(skippedFD, "%s\n", ud_mnemonics_str[ins->GET(mnemonic)]);
                    continue;
                }

                OperandX86* src = ins->getFirstSourceOperand();
                if(src == NULL)
                    continue;
                uint32_t bytesInReg = src->getBytesUsed();
                uint32_t bytesInElem = X86InstructionClassifier::getInstructionElemSize(ins);
                if(bytesInElem == 0) {
                    fprintf(debugFD, "%s 0 bytes In Elem\n", ud_mnemonics_str[ins->GET(mnemonic)]);
                    continue;
                }

                if(bytesInReg == 0) {
                    fprintf(debugFD, "%s 0 bytes in reg\n", ud_mnemonics_str[ins->GET(mnemonic)]);
                    continue;
                }
                uint32_t elemsInReg = bytesInReg / bytesInElem;

                if(elemsInReg > 64 || bytesInElem > 16) {
                    printf("%d elemsInReg, %d bytesInElem, %d bytesInReg, %d bytesInElem, %s\n", elemsInReg, bytesInElem, bytesInReg, bytesInElem, ud_mnemonics_str[ins->GET(mnemonic)]);
                    assert(0);
                }


                fprintf(vectorFD, "%s\t%d\t%d\n", ud_mnemonics_str[ins->GET(mnemonic)], bytesInElem, bytesInReg);

                if(ins->isFloatPOperation()) {
                    ++fpvecs[elemsInReg-1][bytesInElem-1];
                } else if(ins->isIntegerOperation()) {
                    ++intvecs[elemsInReg-1][bytesInElem-1];
                }
            }
            fprintf(staticFD, "\t+vec");
            for(uint32_t nElem = 0; nElem < 64; ++nElem) {
                for(uint32_t elemSize = 0; elemSize < 16; ++elemSize) {
                    uint32_t fpcnt = fpvecs[nElem][elemSize];
                    uint32_t intcnt = intvecs[nElem][elemSize];
                    if(fpcnt > 0 || intcnt > 0) {
                        fprintf(staticFD, "\t%dx%d:%d:%d", nElem+1, (elemSize+1)*8, fpcnt, intcnt);
                    }
                }
            }
            fprintf(staticFD, " # %#llx\n", bb->getHashCode().getValue());


        }
    }
    fclose(skippedFD);
    fclose(vectorFD);
    fclose(debugFD);
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}


void InstrumentationTool::printStaticFilePerInstruction(const char* extension, Vector<Base*>* allInstructions, Vector<uint32_t>* allInstructionIds, Vector<LineInfo*>* allInstructionLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allInstructionLineInfos).size() || (*allInstructions).size() == (*allInstructionLineInfos).size());
    ASSERT((*allInstructions).size() == (*allInstructionIds).size());

    uint32_t numberOfInstPoints = (*allInstructions).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getFullFileName(), extension, "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    char* debugFile = "problemInstructions";
    FILE* debugFD = fopen(debugFile, "w");

    char* vectorInstructions = "vectorInstructions";
    FILE* vectorFD = fopen(vectorInstructions, "w");

    FILE* skippedFD = fopen("nonVecInstructions", "w");

    TextSection* text = getDotTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getExtension());
    fprintf(staticFD, "# phase     = %d\n", 0);
    fprintf(staticFD, "# type      = %s\n", briefName());
    fprintf(staticFD, "# cantidate = %d\n", getNumberOfExposedInstructions());
    fprintf(staticFD, "# sha1sum   = %s\n", getElfFile()->getSHA1Sum());
    fprintf(staticFD, "# perinsn   = yes\n");

    uint32_t memopcnt = 0;
    uint32_t membytcnt = 0;
    uint32_t fltopcnt = 0;
    uint32_t insncnt = 0;
    for (uint32_t i = 0; i < allInstructions->size(); i++){
        Base* b = (*allInstructions)[i];
        ASSERT(b->getType() == PebilClassType_X86Instruction);
        X86Instruction* ins = (X86Instruction*)b;

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
        fprintf(staticFD, "# +vec <#elem>x<elemSize>:<#fp>:<#int> ...\n");
    }

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){
        Base* b = (*allInstructions)[i];
        ASSERT(b->getType() == PebilClassType_X86Instruction);
        X86Instruction* ins = (X86Instruction*)b;

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


            fprintf(staticFD, "\t+dud");
            uint32_t currDist = ins->getDefUseDist();
            if (currDist){
                if (ins->isFloatPOperation()){
                    fprintf(staticFD, "\t%d:%d:%d", currDist, 0, 1);
                } else {
                    fprintf(staticFD, "\t%d:%d:%d", currDist, 1, 0);
                }
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


            // <elemsXelemLen>:fp:int
            // get source operand
            // get operand length
            // get element length
            // get element type

            if(isVectorInstruction(ins) && ins->getFirstSourceOperand() != NULL) {
                OperandX86* src = ins->getFirstSourceOperand();
                uint32_t bytesInReg = src->getBytesUsed();
                uint32_t bytesInElem = X86InstructionClassifier::getInstructionElemSize(ins);

                if(bytesInElem != 0 && bytesInReg != 0) {
                    uint32_t elemsInReg = bytesInReg / bytesInElem;
                    if(elemsInReg > 64 || bytesInElem > 16) {
                        printf("%d elemsInReg, %d bytesInElem, %d bytesInReg, %d bytesInElem, %s\n", elemsInReg, bytesInElem, bytesInReg, bytesInElem, ud_mnemonics_str[ins->GET(mnemonic)]);
                        assert(0);
                    }

                    fprintf(vectorFD, "%s\t%d\t%d\n", ud_mnemonics_str[ins->GET(mnemonic)], bytesInElem, bytesInReg);

                    int fpcnt, intcnt;
                    fpcnt = intcnt = 0;

                    if(ins->isFloatPOperation()) {
                        fpcnt = 1;
                    } else if(ins->isIntegerOperation()) {
                        intcnt = 1;
                    }

                    fprintf(staticFD, "\t+vec\t%dx%d:%d:%d # %#llx\n", elemsInReg, bytesInElem << 3, fpcnt, intcnt, hashValue);

                } else {
                    fprintf(debugFD, "%s, %d, %d\n", ud_mnemonics_str[ins->GET(mnemonic)], bytesInElem, bytesInReg);
                }

            } else {
                fprintf(staticFD, "\t+vec # %#llx\n", hashValue);
                fprintf(skippedFD, "%s\n", ud_mnemonics_str[ins->GET(mnemonic)]);
            }
        }

        delete hc;
    }
    fclose(skippedFD);
    fclose(vectorFD);
    fclose(debugFD);
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
