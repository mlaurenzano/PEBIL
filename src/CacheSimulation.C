#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define SIM_FUNCTION "processTrace"
#define INST_LIB_NAME "libsimulator.so"
#define INST_SUFFIX "siminst"
#define BUFFER_ENTRIES 1

CacheSimulation::CacheSimulation(ElfFile* elf, char* inputFuncList)
    : InstrumentationTool(elf, inputFuncList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", INST_SUFFIX);

    simFunc = NULL;
    bloatType = BloatType_MemoryInstruction;
}

void CacheSimulation::declare(){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    simFunc = declareFunction(SIM_FUNCTION);
    ASSERT(simFunc && "Cannot find memory print function, are you sure it was declared?");

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void CacheSimulation::instrument(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    
    TextSection* text = getTextSection();
    TextSection* fini = getFiniSection();
    ASSERT(text && "Cannot find text section");
    ASSERT(fini && "Cannot find fini section");

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    } else {
        PRINT_ERROR("This executable does not have any line information");
    }

    uint64_t dataBaseAddress = getExtraDataAddress();

    Vector<Instruction*>* allMemOps = new Vector<Instruction*>();
    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();

    PRINT_DEBUG_FUNC_RELOC("Instrumenting %d functions", exposedFunctions.size());
    for (uint32_t i = 0; i < exposedFunctions.size(); i++){
        Function* f = exposedFunctions[i];
        PRINT_DEBUG_FUNC_RELOC("\t%s", f->getName());
        if (!f->hasCompleteDisassembly()){
            PRINT_ERROR("function %s should have complete disassembly", f->getName());
        }
        if (!isEligibleFunction(f)){
            PRINT_ERROR("function %s should be eligible", f->getName());
        }
        ASSERT(f->hasCompleteDisassembly() && isEligibleFunction(f));
        for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
            BasicBlock* b = f->getBasicBlock(j);
            if (b->isCmpCtrlSplit()){
                PRINT_WARN(10, "Comparison/cond branch are split in block at %#llx, not instrumenting", b->getBaseAddress());
            } else {
                (*allBlocks).append(b);
                (*allLineInfos).append(lineInfoFinder->lookupLineInfo(b));
                for (uint32_t k = 0; k < b->getNumberOfInstructions(); k++){
                    Instruction* m = b->getInstruction(k);
                    if (m->isMemoryOperation()){
                        (*allMemOps).append(m);
                    }
                }
            }
        }
    }

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    uint32_t numberOfInstPoints = (*allMemOps).size();

    uint64_t bufferStore  = reserveDataOffset(BUFFER_ENTRIES * sizeof(uint64_t));
    uint64_t buffPtrStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t ptIndexStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t ptSourceAddrs = reserveDataOffset(numberOfInstPoints * sizeof(uint64_t));

    simFunc->addArgument(bufferStore);
    simFunc->addArgument(buffPtrStore);
    simFunc->addArgument(ptIndexStore);
    simFunc->addArgument(ptSourceAddrs);


    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        Instruction* memop = (*allMemOps)[i];
        InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, SIZE_NEEDED_AT_INST_POINT);

        MemoryOperand* memerand = new MemoryOperand(memop->getMemoryOperand(), this);

        pt->addPrecursorInstruction(InstructionGenerator32::generateStackPush(X86_REG_AX));
        if (getElfFile()->is64Bit()){
            pt->addPrecursorInstruction(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_AX));
            pt->addPrecursorInstruction(InstructionGenerator64::generateMoveRegToMem(X86_REG_AX, getExtraDataAddress() + ptIndexStore));
        } else {
            pt->addPrecursorInstruction(InstructionGenerator32::generateMoveImmToReg(i, X86_REG_AX));
            pt->addPrecursorInstruction(InstructionGenerator32::generateMoveRegToMem(X86_REG_AX, getExtraDataAddress() + ptIndexStore));
        }
        pt->addPrecursorInstruction(InstructionGenerator32::generateStackPop(X86_REG_AX));


        Vector<Instruction*>* addressCalcInstructions = generateBufferedAddressCalculation(memerand, bufferStore, buffPtrStore, BUFFER_ENTRIES);
        ASSERT(addressCalcInstructions);
        while ((*addressCalcInstructions).size()){
            pt->addPrecursorInstruction((*addressCalcInstructions).remove(0));
        }
        delete addressCalcInstructions;
        delete memerand;

        uint64_t pointAddress = memop->getBaseAddress();
        //        PRINT_INFOR("Writing point address %#llx to data at %#llx", pointAddress, getExtraDataAddress() + ptSourceAddrs + (i * sizeof(uint64_t)));
        initializeReservedData(getExtraDataAddress() + ptSourceAddrs + i*sizeof(uint64_t), sizeof(uint64_t), &pointAddress);

    }

    printStaticFile(allBlocks, allLineInfos);

    delete allMemOps;
    delete allBlocks;
    delete allLineInfos;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

// base and index regs are saved and restored by the caller
Vector<Instruction*>* CacheSimulation::generateBufferedAddressCalculation(MemoryOperand* memerand, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t bufferSize){
    if (getElfFile()->is64Bit()){
        return generateBufferedAddressCalculation64(memerand, bufferStore, bufferPtrStore, bufferSize);
    } else {
        return generateBufferedAddressCalculation32(memerand, bufferStore, bufferPtrStore, bufferSize);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

Vector<Instruction*>* CacheSimulation::generateBufferedAddressCalculation64(MemoryOperand* memerand, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t bufferSize){
    Vector<Instruction*>* addressCalc = new Vector<Instruction*>();
    uint64_t dataAddr = getExtraDataAddress();
    Operand* operand = memerand->getOperand();

    // find 3 temp registers to use in the calculation
    BitSet<uint32_t>* availableRegs = new BitSet<uint32_t>(X86_64BIT_GPRS);
    availableRegs->insert(X86_REG_SP);
    operand->getInstruction()->touchedRegisters(availableRegs);

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
    uint64_t lValue =  operand->getValue();

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

    uint8_t indexReg = 0;
    if (operand->GET(index)){
        ASSERT(operand->GET(index) >= UD_R_RAX && operand->GET(index) <= UD_R_R15);
        indexReg = operand->GET(index) - UD_R_RAX;
    } else {
        ASSERT(!operand->GET(scale));
    }

    //operand->getInstruction()->print();
    //PRINT_INFOR("Using tmp1/tmp2/base/index/value/scale/baddr %hhd/%hhd/%hhd/%hhd/%#llx/%d/%#llx", tempReg1, tempReg2, baseReg, indexReg, lValue, operand->GET(scale), operand->getInstruction()->getProgramAddress());

    (*addressCalc).append(InstructionGenerator::generateNoop());
    
    (*addressCalc).append(InstructionGenerator64::generateMoveRegToMem(tempReg1, dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t))));
    (*addressCalc).append(InstructionGenerator64::generateMoveRegToMem(tempReg2, dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t))));
    (*addressCalc).append(InstructionGenerator64::generateMoveRegToMem(tempReg3, dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t))));
 
    if (operand->GET(base)){
        (*addressCalc).append(InstructionGenerator64::generateMoveRegToReg(baseReg, tempReg1));
    }
    if (operand->GET(index)){
        (*addressCalc).append(InstructionGenerator64::generateMoveRegToReg(indexReg, tempReg2));
    }

    if (IS_PC_REG(operand->GET(base))){
        (*addressCalc).append(InstructionGenerator64::generateLoadRipImmToReg(0, tempReg1));
    }

    if (operand->GET(base)){
        (*addressCalc).append(InstructionGenerator64::generateRegAddImm(tempReg1, (uint32_t)lValue)); 
    } else {
        (*addressCalc).append(InstructionGenerator64::generateMoveImmToReg((uint32_t)lValue, tempReg1));
    }

    if (operand->GET(index)){
        uint8_t scale = operand->GET(scale);
        if (!scale){
            scale++;
        }
        (*addressCalc).append(InstructionGenerator64::generateRegImmMultReg(tempReg2, scale, tempReg2));
        (*addressCalc).append(InstructionGenerator64::generateRegAddReg2OpForm(tempReg2, tempReg1));
    }
    
    (*addressCalc).append(InstructionGenerator64::generateMoveImmToReg(dataAddr + bufferStore, tempReg2));
    (*addressCalc).append(InstructionGenerator64::generateMoveMemToReg(dataAddr + bufferPtrStore, tempReg3));

    (*addressCalc).append(InstructionGenerator64::generateShiftLeftLogical(3, tempReg3));
    (*addressCalc).append(InstructionGenerator64::generateRegAddReg2OpForm(tempReg3, tempReg2));
    (*addressCalc).append(InstructionGenerator64::generateShiftRightLogical(3, tempReg3));
    (*addressCalc).append(InstructionGenerator64::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 0));

    (*addressCalc).append(InstructionGenerator64::generateRegAddImm(tempReg3, 1));
    (*addressCalc).append(InstructionGenerator64::generateMoveRegToMem(tempReg3, dataAddr + bufferPtrStore));
    (*addressCalc).append(InstructionGenerator64::generateCompareImmReg(bufferSize, tempReg3));
    
    (*addressCalc).append(InstructionGenerator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t)), tempReg3));
    (*addressCalc).append(InstructionGenerator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t)), tempReg2));
    (*addressCalc).append(InstructionGenerator64::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t)), tempReg1));

    (*addressCalc).append(InstructionGenerator::generateBranchJL(Size__64_bit_inst_function_call_support));

    (*addressCalc).append(InstructionGenerator::generateNoop());

    return addressCalc;
}

Vector<Instruction*>* CacheSimulation::generateBufferedAddressCalculation32(MemoryOperand* memerand, uint64_t bufferStore, uint64_t bufferPtrStore, uint32_t bufferSize){
    Vector<Instruction*>* addressCalc = new Vector<Instruction*>();
    uint64_t dataAddr = getExtraDataAddress();
    Operand* operand = memerand->getOperand();

    // find 3 temp registers
    BitSet<uint32_t>* availableRegs = new BitSet<uint32_t>(X86_64BIT_GPRS);
    availableRegs->insert(X86_REG_SP);
    operand->getInstruction()->touchedRegisters(availableRegs);

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

    uint8_t indexReg = 0;
    if (operand->GET(index)){
        ASSERT(operand->GET(index) >= UD_R_EAX && operand->GET(index) <= UD_R_EDI);
        indexReg = operand->GET(index) - UD_R_EAX;
    } else {
        ASSERT(!operand->GET(scale));
    }

    //    PRINT_INFOR("Using tmp1/tmp2/base/index/value/scale/baddr %hhd/%hhd/%hhd/%hhd/%#llx/%d/%#llx", tempReg1, tempReg2, baseReg, indexReg, operand->getValue(), operand->GET(scale), operand->getInstruction()->getProgramAddress());

    (*addressCalc).append(InstructionGenerator::generateNoop());
    
    (*addressCalc).append(InstructionGenerator32::generateMoveRegToMem(tempReg1, dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t))));
    (*addressCalc).append(InstructionGenerator32::generateMoveRegToMem(tempReg2, dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t))));
    (*addressCalc).append(InstructionGenerator32::generateMoveRegToMem(tempReg3, dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t))));
 
    if (operand->GET(base)){
        (*addressCalc).append(InstructionGenerator32::generateMoveRegToReg(baseReg, tempReg1));
    }
    if (operand->GET(index)){
        (*addressCalc).append(InstructionGenerator32::generateMoveRegToReg(indexReg, tempReg2));
    }

    if (operand->GET(base)){
        (*addressCalc).append(InstructionGenerator32::generateRegAddImm(tempReg1, (uint32_t)operand->getValue())); 
    } else {
        (*addressCalc).append(InstructionGenerator32::generateMoveImmToReg((uint32_t)operand->getValue(), tempReg1));
    }

    if (operand->GET(index)){
        uint8_t scale = operand->GET(scale);
        if (!scale){
            scale++;
        }
        (*addressCalc).append(InstructionGenerator32::generateRegImm1ByteMultReg(tempReg2, scale, tempReg2));
        (*addressCalc).append(InstructionGenerator32::generateRegAddReg2OpForm(tempReg2, tempReg1));
    }

    (*addressCalc).append(InstructionGenerator32::generateMoveImmToReg(dataAddr + bufferStore, tempReg2));
    (*addressCalc).append(InstructionGenerator32::generateMoveMemToReg(dataAddr + bufferPtrStore, tempReg3));

    (*addressCalc).append(InstructionGenerator32::generateShiftLeftLogical(3, tempReg3));
    (*addressCalc).append(InstructionGenerator32::generateRegAddReg2OpForm(tempReg3, tempReg2));
    (*addressCalc).append(InstructionGenerator32::generateShiftRightLogical(3, tempReg3));
    (*addressCalc).append(InstructionGenerator32::generateMoveRegToRegaddrImm(tempReg1, tempReg2, 0));

    (*addressCalc).append(InstructionGenerator32::generateRegAddImm(tempReg3, 1));
    (*addressCalc).append(InstructionGenerator32::generateMoveRegToMem(tempReg3, dataAddr + bufferPtrStore));
    (*addressCalc).append(InstructionGenerator32::generateCompareImmReg(bufferSize, tempReg3));

    (*addressCalc).append(InstructionGenerator32::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 4*(sizeof(uint64_t)), tempReg3));
    (*addressCalc).append(InstructionGenerator32::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 3*(sizeof(uint64_t)), tempReg2));
    (*addressCalc).append(InstructionGenerator32::generateMoveMemToReg(dataAddr + getRegStorageOffset() + 2*(sizeof(uint64_t)), tempReg1));

    (*addressCalc).append(InstructionGenerator::generateBranchJL(Size__32_bit_inst_function_call_support));

    (*addressCalc).append(InstructionGenerator::generateNoop());

    return addressCalc;
}

