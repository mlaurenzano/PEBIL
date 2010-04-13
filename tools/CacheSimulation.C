#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <InstrucX86.h>
#include <InstrucX86Generator.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define ENTRY_FUNCTION "entry_function" // unused for now, but will use this to initialize stuff when optimizing instrumentation
#define SIM_FUNCTION "MetaSim_simulFuncCall_Simu"
#define EXIT_FUNCTION "MetaSim_endFuncCall_Simu"
#define INST_LIB_NAME "libsimulator.so"
#define BUFFER_ENTRIES 0x00010000
#define Size__BufferEntry 16
#define MAX_MEMOPS_PER_BLOCK 1024

CacheSimulation::CacheSimulation(ElfFile* elf)
    : InstrumentationTool(elf)
{
    simFunc = NULL;
    exitFunc = NULL;
    entryFunc = NULL;
}

void CacheSimulation::declare(){
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    simFunc = declareFunction(SIM_FUNCTION);
    ASSERT(simFunc && "Cannot find memory print function, are you sure it was declared?");
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");
    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");
}

void CacheSimulation::instrument(){
    uint32_t temp32;
    
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    } else {
        PRINT_ERROR("This executable does not have any line information");
    }

    ASSERT(isPowerOfTwo(Size__BufferEntry));
    uint64_t bufferStore  = reserveDataOffset(BUFFER_ENTRIES * Size__BufferEntry);
    uint32_t startValue = 1;
    initializeReservedData(getInstDataAddress() + bufferStore, sizeof(uint32_t), &startValue);
    uint64_t buffPtrStore = reserveDataOffset(sizeof(uint64_t));


    char* appName = getElfFile()->getFileName();
    char* extension = "siminst";
    uint32_t phaseId = 0;
    uint32_t dumpCode = 0;
    uint32_t commentSize = strlen(appName) + sizeof(uint32_t) + strlen(extension) + sizeof(uint32_t) + sizeof(uint32_t) + 4;
    uint64_t commentStore = reserveDataOffset(commentSize);
    char* comment = new char[commentSize];
    sprintf(comment, "%s %u %s %u %u", appName, phaseId, extension, getNumberOfExposedBasicBlocks(), dumpCode);
    initializeReservedData(getInstDataAddress() + commentStore, commentSize, comment);

    simFunc->addArgument(bufferStore);
    simFunc->addArgument(buffPtrStore);
    simFunc->addArgument(commentStore);

    exitFunc->addArgument(bufferStore);
    exitFunc->addArgument(buffPtrStore);
    exitFunc->addArgument(commentStore);

    uint64_t addrScratchSpace = reserveDataOffset(sizeof(uint64_t) * MAX_MEMOPS_PER_BLOCK);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    /*
    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry function");
    }
    */
    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();

    uint32_t blockId = 0;
    uint32_t memopId = 0;
    uint32_t noProtPoints = 0;
    uint32_t regDefault = 0;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);
        (*allBlocks).append(bb);
        (*allLineInfos).append(lineInfoFinder->lookupLineInfo(bb));

        uint32_t memopIdInBlock = 0;
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            InstrucX86* memop = bb->getInstruction(j);
            if (memop->isMemoryOperation()){            
                //InstrumentationSnippet* snip = new InstrumentationSnippet();
                //InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none);

                // check the buffer on the last memop in the block
                if (memopIdInBlock == bb->getNumberOfMemoryOps()){
                    __SHOULD_NOT_ARRIVE;
                    FlagsProtectionMethods prot = FlagsProtectionMethod_full;
#ifndef NO_REG_ANALYSIS
                    if (memop->allFlagsDeadIn()){
                        noProtPoints++;
                        prot = FlagsProtectionMethod_none;
                    }
#endif
                    
                    InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_trampinline, prot, InstLocation_prior);
                    Vector<InstrucX86*>* addressCalcInstructions = generateBufferedAddressCalculation(memop, bufferStore, buffPtrStore, blockId, memopId, BUFFER_ENTRIES, prot);
                    ASSERT(addressCalcInstructions);
                    while ((*addressCalcInstructions).size()){
                        //snip->addSnippetInstruction((*addressCalcInstructions).remove(0));
                        pt->addPrecursorInstruction((*addressCalcInstructions).remove(0));
                    }
                    delete addressCalcInstructions;
                // for every memop just quickly buffer the address
                } else {
                    // TODO: get which gprs are dead at this point and use one of those 
                    InstrumentationSnippet* snip = new InstrumentationSnippet();
                    
                    uint32_t tmpReg1 = X86_REG_AX;
                    bool usesLiveReg = true;
                    ASSERT(tmpReg1 < X86_64BIT_GPRS);
                    if (usesLiveReg){
                        snip->addSnippetInstruction(InstrucX86Generator64::generateMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t)));
                    }
                    Vector<InstrucX86*>* addrStore = InstrucX86Generator64::generateAddressComputation(memop, tmpReg1);
                    (*addrStore).append(InstrucX86Generator64::generateMoveRegToMem(tmpReg1, getInstDataAddress() + addrScratchSpace + (memopIdInBlock * sizeof(uint64_t))));
                    while (!(*addrStore).empty()){
                        snip->addSnippetInstruction((*addrStore).remove(0));
                    }
                    delete addrStore;
                    if (usesLiveReg){
                        snip->addSnippetInstruction(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t), tmpReg1));
                    }
                    InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_inline, FlagsProtectionMethod_none, InstLocation_prior);
                }
                memopId++;
                memopIdInBlock++;
            }
        }
        ASSERT(memopIdInBlock < MAX_MEMOPS_PER_BLOCK);
        blockId++;
    }
    ASSERT(memopId == getNumberOfExposedMemOps());
#ifdef NO_REG_ANALYSIS
    PRINT_WARN(10, "Warning: register analysis disabled");
#endif
    PRINT_INFOR("Not protecting %d/%d instrumentation points", noProtPoints, getNumberOfExposedMemOps());
    PRINT_INFOR("No live scratch reg at %d/%d instrumentation points", regDefault, getNumberOfExposedMemOps());

    printStaticFile(allBlocks, allLineInfos);

    delete allBlocks;
    delete allLineInfos;
    delete[] comment;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

// base and index regs are saved and restored by the caller
Vector<InstrucX86*>* CacheSimulation::generateBufferedAddressCalculation(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method){
    if (getElfFile()->is64Bit()){
        return tmp_generateBufferedAddressCalculation64(instruction, bufferStore, bufferPtrStore, blockId, memopId, bufferSize, method);
    } else {
        return generateBufferedAddressCalculation32(instruction, bufferStore, bufferPtrStore, blockId, memopId, bufferSize, method);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

Vector<InstrucX86*>* CacheSimulation::tmp_generateBufferedAddressCalculation64(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method){
    Vector<InstrucX86*>* addressCalc = new Vector<InstrucX86*>();
    uint64_t dataAddr = getInstDataAddress();

    OperandX86* operand = NULL;
    if (instruction->isExplicitMemoryOperation()){
        operand = instruction->getMemoryOperand();
    }


    // find 3 temp registers to use in the calculation
    BitSet<uint32_t>* availableRegs = new BitSet<uint32_t>(X86_64BIT_GPRS);
    uint32_t tempReg1 = X86_64BIT_GPRS;
    uint32_t tempReg2 = X86_64BIT_GPRS;
    uint32_t tempReg3 = X86_64BIT_GPRS;

    availableRegs->insert(X86_REG_SP);
    if (method == FlagsProtectionMethod_light){
        availableRegs->insert(X86_REG_AX);
    }
    if (operand){
        operand->getInstruction()->touchedRegisters(availableRegs);
    }

    ~(*availableRegs);

    for (int32_t i = 0; i < availableRegs->size(); i++){
        uint32_t idx = X86_64BIT_GPRS - i;
        if (availableRegs->contains(idx)){
            if (tempReg1 == X86_64BIT_GPRS){
                tempReg1 = idx;
            } else if (tempReg2 == X86_64BIT_GPRS){
                tempReg2 = idx;
            } else if (tempReg3 == X86_64BIT_GPRS){
                tempReg3 = idx;
            }
        }
    }
    ASSERT(tempReg1 != X86_64BIT_GPRS && tempReg2 != X86_64BIT_GPRS && tempReg3 != X86_64BIT_GPRS);

    delete availableRegs;


    //operand->getInstruction()->print();
    //PRINT_INFOR("Using tmp1/tmp2/base/index/value/scale/baddr %hhd/%hhd/%hhd/%hhd/%#llx/%d/%#llx", tempReg1, tempReg2, baseReg, indexReg, lValue, operand->GET(scale), operand->getInstruction()->getProgramAddress());

    // save a few temp regs
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg1, dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t))));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg2, dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t))));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg3, dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t))));

    Vector<InstrucX86*>* addrComputation = InstrucX86Generator64::generateAddressComputation(instruction, tempReg1);
    while (!(*addrComputation).empty()){
        (*addressCalc).append((*addrComputation).remove(0));
    }
    delete addrComputation;

    (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(dataAddr + bufferStore, tempReg2));
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + bufferPtrStore, tempReg3));

    // compute the address of the buffer entry
    (*addressCalc).append(InstrucX86Generator64::generateShiftLeftLogical(logBase2(Size__BufferEntry), tempReg3));
    (*addressCalc).append(InstrucX86Generator64::generateRegAddReg2OpForm(tempReg3, tempReg2));
    (*addressCalc).append(InstrucX86Generator64::generateShiftRightLogical(logBase2(Size__BufferEntry), tempReg3));

    // fill the buffer entry
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 2*sizeof(uint32_t), true));
    (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(blockId, tempReg1));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 0, false));
    (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(memopId, tempReg1));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, sizeof(uint32_t), false));

    // inc the buffer pointer and see if the buffer is full
    (*addressCalc).append(InstrucX86Generator64::generateRegAddImm(tempReg3, 1));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg3, dataAddr + bufferPtrStore));
    (*addressCalc).append(InstrucX86Generator64::generateCompareImmReg(bufferSize, tempReg3));
    
    // restore regs
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t)), tempReg3));
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t)), tempReg2));
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t)), tempReg1));

    (*addressCalc).append(InstrucX86Generator::generateBranchJL(Size__64_bit_inst_function_call_support));
    //    (*addressCalc).append(InstrucX86Generator::generateJumpRelative(0,5 + Size__64_bit_inst_function_call_support));

    return addressCalc;
}

Vector<InstrucX86*>* CacheSimulation::generateBufferedAddressCalculation64(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method){
    Vector<InstrucX86*>* addressCalc = new Vector<InstrucX86*>();
    uint64_t dataAddr = getInstDataAddress();

    OperandX86* operand = NULL;
    if (instruction->isExplicitMemoryOperation()){
        operand = instruction->getMemoryOperand();
    }


    // find 3 temp registers to use in the calculation
    BitSet<uint32_t>* availableRegs = new BitSet<uint32_t>(X86_64BIT_GPRS);
    availableRegs->insert(X86_REG_SP);
    if (operand){
        operand->getInstruction()->touchedRegisters(availableRegs);
    }

    ~(*availableRegs);

    uint32_t tempReg1 = X86_64BIT_GPRS;
    uint32_t tempReg2 = X86_64BIT_GPRS;
    uint32_t tempReg3 = X86_64BIT_GPRS;

    for (int32_t i = 0; i < availableRegs->size(); i++){
        uint32_t idx = X86_64BIT_GPRS - i;
        if (availableRegs->contains(idx)){
            if (tempReg1 == X86_64BIT_GPRS){
                tempReg1 = idx;
            } else if (tempReg2 == X86_64BIT_GPRS){
                tempReg2 = idx;
            } else if (tempReg3 == X86_64BIT_GPRS){
                tempReg3 = idx;
            }
        }
    }
    ASSERT(tempReg1 != X86_64BIT_GPRS && tempReg2 != X86_64BIT_GPRS && tempReg3 != X86_64BIT_GPRS);
    delete availableRegs;


    uint8_t baseReg = 0;
    uint64_t lValue = 0;

    if (operand){
        lValue = operand->getValue();
        if (operand->GET(base)){
            if (!IS_64BIT_GPR(operand->GET(base)) && !IS_PC_REG(operand->GET(base))){
                PRINT_ERROR("bad operand value %d -- %s", operand->GET(base), ud_reg_tab[operand->GET(base)-1]);
            }
            if (IS_64BIT_GPR(operand->GET(base))){
                baseReg = operand->GET(base) - UD_R_RAX;
            } else {
                baseReg = UD_R_RAX - UD_R_RAX;
            }
        } else {
            if (!lValue && !operand->GET(index)){
                PRINT_WARN(3, "Operand requesting memory address 0?");
            }
            if (!operand->GET(index)){
                if (lValue < MIN_CONST_MEMADDR){
                    PRINT_WARN(6, "Const memory address probably isn't valid %#llx, zeroing", lValue);
                    lValue = 0;
                }
            }
        }
    }

    uint8_t indexReg = 0;
    if (operand){
        if (operand->GET(index)){
            ASSERT(operand->GET(index) >= UD_R_RAX && operand->GET(index) <= UD_R_R15);
            indexReg = operand->GET(index) - UD_R_RAX;
        } else {
            ASSERT(!operand->GET(scale));
        }
    }

    //operand->getInstruction()->print();
    //PRINT_INFOR("Using tmp1/tmp2/base/index/value/scale/baddr %hhd/%hhd/%hhd/%hhd/%#llx/%d/%#llx", tempReg1, tempReg2, baseReg, indexReg, lValue, operand->GET(scale), operand->getInstruction()->getProgramAddress());

    // save a few temp regs
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg1, dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t))));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg2, dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t))));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg3, dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t))));
 
    if (operand){
        if (operand->GET(base)){
            (*addressCalc).append(InstrucX86Generator64::generateMoveRegToReg(baseReg, tempReg1));
#ifndef NO_LAHF_SAHF
            // AX contains the flags values and the legitimate value of AX is in regStorage when LAHF/SAHF are in place
            if (baseReg == X86_REG_AX && method == FlagsProtectionMethod_light){
                (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset(), tempReg1));
            }
#endif
        }
    } else {
        (*addressCalc).append(InstrucX86Generator64::generateMoveRegToReg(X86_REG_SP, tempReg1));
    }

    if (operand){
        if (operand->GET(index)){
            (*addressCalc).append(InstrucX86Generator64::generateMoveRegToReg(indexReg, tempReg2));
#ifndef NO_LAHF_SAHF
            // AX contains the flags values and the legitimate value of AX is in regStorage when LAHF/SAHF are in place
            if (indexReg == X86_REG_AX && method == FlagsProtectionMethod_light){
                (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset(), tempReg2));
            }
#endif
        }

        if (IS_PC_REG(operand->GET(base))){
            (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(instruction->getProgramAddress(), tempReg1));
            (*addressCalc).append(InstrucX86Generator64::generateRegAddImm(tempReg1, instruction->getSizeInBytes()));
        }
        
        if (operand->GET(base)){
            (*addressCalc).append(InstrucX86Generator64::generateRegAddImm(tempReg1, lValue)); 
        } else {
            (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(lValue, tempReg1));
        }

        if (operand->GET(index)){
            uint8_t scale = operand->GET(scale);
            if (!scale){
                scale++;
            }
            (*addressCalc).append(InstrucX86Generator64::generateRegImmMultReg(tempReg2, scale, tempReg2));
            (*addressCalc).append(InstrucX86Generator64::generateRegAddReg2OpForm(tempReg2, tempReg1));
        }
    }

    (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(dataAddr + bufferStore, tempReg2));
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + bufferPtrStore, tempReg3));

    // compute the address of the buffer entry
    (*addressCalc).append(InstrucX86Generator64::generateShiftLeftLogical(logBase2(Size__BufferEntry), tempReg3));
    (*addressCalc).append(InstrucX86Generator64::generateRegAddReg2OpForm(tempReg3, tempReg2));
    (*addressCalc).append(InstrucX86Generator64::generateShiftRightLogical(logBase2(Size__BufferEntry), tempReg3));

    // fill the buffer entry
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 2*sizeof(uint32_t), true));
    (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(blockId, tempReg1));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 0, false));
    (*addressCalc).append(InstrucX86Generator64::generateMoveImmToReg(memopId, tempReg1));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, sizeof(uint32_t), false));

    // inc the buffer pointer and see if the buffer is full
    (*addressCalc).append(InstrucX86Generator64::generateRegAddImm(tempReg3, 1));
    (*addressCalc).append(InstrucX86Generator64::generateMoveRegToMem(tempReg3, dataAddr + bufferPtrStore));
    (*addressCalc).append(InstrucX86Generator64::generateCompareImmReg(bufferSize, tempReg3));
    
    // restore regs
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t)), tempReg3));
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t)), tempReg2));
    (*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t)), tempReg1));

    (*addressCalc).append(InstrucX86Generator::generateBranchJL(Size__64_bit_inst_function_call_support));

    return addressCalc;
}

Vector<InstrucX86*>* CacheSimulation::generateBufferedAddressCalculation32(InstrucX86* instruction, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t blockId, uint32_t memopId, uint32_t bufferSize, FlagsProtectionMethods method){
    PRINT_ERROR("implement the lea 32bit addr computations you lazy");

    Vector<InstrucX86*>* addressCalc = new Vector<InstrucX86*>();
    uint64_t dataAddr = getInstDataAddress();

    OperandX86* operand = NULL;
    if (instruction->isExplicitMemoryOperation()){
        operand = instruction->getMemoryOperand();
    }

    // find 3 temp registers
    BitSet<uint32_t>* availableRegs = new BitSet<uint32_t>(X86_64BIT_GPRS);
    availableRegs->insert(X86_REG_SP);
    if (operand){
        operand->getInstruction()->touchedRegisters(availableRegs);
    }

    ~(*availableRegs);

    uint32_t tempReg1 = X86_32BIT_GPRS;
    uint32_t tempReg2 = X86_32BIT_GPRS;
    uint32_t tempReg3 = X86_32BIT_GPRS;

    for (int32_t i = 0; i < availableRegs->size(); i++){
        uint32_t idx = X86_32BIT_GPRS - i;
        if (availableRegs->contains(idx)){
            if (tempReg1 == X86_32BIT_GPRS){
                tempReg1 = idx;
            } else if (tempReg2 == X86_32BIT_GPRS){
                tempReg2 = idx;
            } else if (tempReg3 == X86_32BIT_GPRS){
                tempReg3 = idx;
            }
        }
    }
    ASSERT(tempReg1 != X86_32BIT_GPRS && tempReg2 != X86_32BIT_GPRS && tempReg3 != X86_32BIT_GPRS);
    delete availableRegs;

    uint8_t baseReg = 0;
    if (operand){
        if (operand->GET(base)){
            if (!IS_32BIT_GPR(operand->GET(base))){
                PRINT_ERROR("bad operand value %d -- %s", operand->GET(base), ud_reg_tab[operand->GET(base)-1]);
            }
            if (IS_32BIT_GPR(operand->GET(base))){
                baseReg = operand->GET(base) - UD_R_EAX;
            } else {
                baseReg = UD_R_EAX - UD_R_EAX;
            }
        } else {
            ASSERT(operand->getValue() || operand->GET(index));
        }
    }

    uint8_t indexReg = 0;
    if (operand){
        if (operand->GET(index)){
            ASSERT(operand->GET(index) >= UD_R_EAX && operand->GET(index) <= UD_R_EDI);
            indexReg = operand->GET(index) - UD_R_EAX;
        } else {
            ASSERT(!operand->GET(scale));
        }
    }

    //    PRINT_INFOR("Using tmp1/tmp2/base/index/value/scale/baddr %hhd/%hhd/%hhd/%hhd/%#llx/%d/%#llx", tempReg1, tempReg2, baseReg, indexReg, operand->getValue(), operand->GET(scale), operand->getInstruction()->getProgramAddress());
    
    (*addressCalc).append(InstrucX86Generator32::generateMoveRegToMem(tempReg1, dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t))));
    (*addressCalc).append(InstrucX86Generator32::generateMoveRegToMem(tempReg2, dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t))));
    (*addressCalc).append(InstrucX86Generator32::generateMoveRegToMem(tempReg3, dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t))));
 
    if (operand){
        if (operand->GET(base)){
            (*addressCalc).append(InstrucX86Generator32::generateMoveRegToReg(baseReg, tempReg1));
#ifndef NO_LAHF_SAHF
            // AX contains the flags values and the legitimate value of AX is in regStorage when LAHF/SAHF are in place
            if (baseReg == X86_REG_AX && method == FlagsProtectionMethod_light){
                //(*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset(), tempReg1));
            }
#endif
        }
        if (operand->GET(index)){
            (*addressCalc).append(InstrucX86Generator32::generateMoveRegToReg(indexReg, tempReg2));
#ifndef NO_LAHF_SAHF
            // AX contains the flags values and the legitimate value of AX is in regStorage when LAHF/SAHF are in place
            if (indexReg == X86_REG_AX && method == FlagsProtectionMethod_light){
                //(*addressCalc).append(InstrucX86Generator64::generateMoveMemToReg(dataAddr + getRegStorageOffset(), tempReg2));
            }
#endif
        }

        if (operand->GET(base)){
            (*addressCalc).append(InstrucX86Generator32::generateRegAddImm(tempReg1, operand->getValue())); 
        } else {
            (*addressCalc).append(InstrucX86Generator32::generateMoveImmToReg(operand->getValue(), tempReg1));
        }

        if (operand->GET(index)){
            uint8_t scale = operand->GET(scale);
            if (!scale){
                scale++;
            }
            (*addressCalc).append(InstrucX86Generator32::generateRegImm1ByteMultReg(tempReg2, scale, tempReg2));
            (*addressCalc).append(InstrucX86Generator32::generateRegAddReg2OpForm(tempReg2, tempReg1));
        }
    }

    (*addressCalc).append(InstrucX86Generator32::generateMoveImmToReg(dataAddr + bufferStore, tempReg2));
    (*addressCalc).append(InstrucX86Generator32::generateMoveMemToReg(dataAddr + bufferPtrStore, tempReg3));

    (*addressCalc).append(InstrucX86Generator32::generateShiftLeftLogical(logBase2(Size__BufferEntry), tempReg3));
    (*addressCalc).append(InstrucX86Generator32::generateRegAddReg2OpForm(tempReg3, tempReg2));
    (*addressCalc).append(InstrucX86Generator32::generateShiftRightLogical(logBase2(Size__BufferEntry), tempReg3));
    (*addressCalc).append(InstrucX86Generator32::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 0));

    (*addressCalc).append(InstrucX86Generator32::generateRegAddImm(tempReg3, 1));
    (*addressCalc).append(InstrucX86Generator32::generateMoveRegToMem(tempReg3, dataAddr + bufferPtrStore));
    (*addressCalc).append(InstrucX86Generator32::generateCompareImmReg(bufferSize, tempReg3));

    (*addressCalc).append(InstrucX86Generator32::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t)), tempReg3));
    (*addressCalc).append(InstrucX86Generator32::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t)), tempReg2));
    (*addressCalc).append(InstrucX86Generator32::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t)), tempReg1));

    (*addressCalc).append(InstrucX86Generator::generateBranchJL(Size__32_bit_inst_function_call_support));
    return addressCalc;
}

