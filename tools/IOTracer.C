#include <IOTracer.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>

#define PROGRAM_ENTRY  "inittracer"
#define PROGRAM_EXIT   "finishtracer"
#define FUNCTION_TRACE  "functiontracer"
#define INST_LIB_NAME "libtracer.so"
#define INST_SUFFIX "iotinst"

IOTracer::IOTracer(ElfFile* elf, char* inputFuncList, char* inputFileList, char* traceFile)
    : InstrumentationTool(elf, inputFuncList, inputFileList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", INST_SUFFIX);

    programEntry = NULL;
    programExit = NULL;
    functionTrace = NULL;

    traceFunctions = new Vector<char*>();
    initializeFileList(traceFile, traceFunctions);
    for (uint32_t i = 0; i < (*traceFunctions).size(); i++){
        PRINT_INFOR("tf %d = %s", i, (*traceFunctions)[i]);
    }

    //    flags = InstrumentorFlag_norelocate;
}

IOTracer::~IOTracer(){
    PRINT_INFOR("deleting tracer");
    for (uint32_t i = 0; i < (*traceFunctions).size(); i++){
        PRINT_INFOR("elt %d", i);
        delete[] (*traceFunctions)[i];
    }
    delete traceFunctions;
}

void IOTracer::declare(){
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    programEntry = declareFunction(PROGRAM_ENTRY);
    ASSERT(programEntry);
    programExit = declareFunction(PROGRAM_EXIT);
    ASSERT(programExit);
    functionTrace = declareFunction(FUNCTION_TRACE);
    ASSERT(functionTrace);
}

void IOTracer::instrument(){
    uint32_t temp32;
    uint64_t temp64;

    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_dont_care);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_dont_care);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    Vector<Instruction*> myInstPoints;
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();
        
        if (instruction->isFunctionCall() && searchFileList(traceFunctions, function->getName())){
            myInstPoints.append(instruction);
        } 
    }

    uint64_t callargsOffset = reserveDataOffset(sizeof(uint64_t) * 8);

    uint64_t callCountAddr = reserveDataOffset(sizeof(uint64_t));
    programEntry->addArgument(callCountAddr);
    temp64 = myInstPoints.size();
    initializeReservedData(getInstDataAddress() + callCountAddr, sizeof(uint64_t), &temp64);

    uint64_t funcNameArray = reserveDataOffset(myInstPoints.size() * sizeof(char*));
    programEntry->addArgument(funcNameArray);

    uint64_t functionIndexAddr = reserveDataOffset(sizeof(uint64_t));
    functionTrace->addArgument(functionIndexAddr);

    for (uint32_t i = 0; i < myInstPoints.size(); i++){

        InstrumentationPoint* pt = addInstrumentationPoint(myInstPoints[i], functionTrace, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);

        Vector<Instruction*> setupReg;
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_DI, getInstDataAddress() + callargsOffset + (0 * sizeof(uint64_t))));
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_SI, getInstDataAddress() + callargsOffset + (1 * sizeof(uint64_t))));
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_DX, getInstDataAddress() + callargsOffset + (2 * sizeof(uint64_t))));
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + callargsOffset + (3 * sizeof(uint64_t))));

        // already have DI saved so let's use that as the scratch reg
        setupReg.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_DI));
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_DI, getInstDataAddress() + functionIndexAddr));
        setupReg.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + callargsOffset + (0 * sizeof(uint64_t)), X86_REG_DI));

        while (setupReg.size()){
            pt->addPrecursorInstruction(setupReg.remove(0));
        }

        /*        
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        BasicBlock* bb = f->getFlowGraph()->getEntryBlock();
        Vector<BasicBlock*>* exitBlocks = f->getFlowGraph()->getExitBlocks();

        Vector<Instruction*> fillEntry = Vector<Instruction*>();
        fillEntry.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
        fillEntry.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_CX));
        fillEntry.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
        fillEntry.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));

        p = addInstrumentationPoint(bb, functionEntry, InstrumentationMode_tramp);
        for (uint32_t j = 0; j < fillEntry.size(); j++){
            p->addPrecursorInstruction(fillEntry[j]);
        }

        for (uint32_t j = 0; j < (*exitBlocks).size(); j++){
            Vector<Instruction*> fillExit = Vector<Instruction*>();
            fillExit.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            fillExit.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_CX));
            fillExit.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
            fillExit.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));

            p = addInstrumentationPoint((*exitBlocks)[j], functionExit, InstrumentationMode_tramp);
            for (uint32_t k = 0; k < fillExit.size(); k++){
                p->addPrecursorInstruction(fillExit[k]);
            }
        }
        if (!(*exitBlocks).size()){
            Vector<Instruction*> fillExit = Vector<Instruction*>();
            fillExit.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            fillExit.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_CX));
            fillExit.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
            fillExit.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));

            BasicBlock* lastbb = f->getBasicBlock(f->getNumberOfBasicBlocks()-1);
            Instruction* lastin = lastbb->getInstruction(lastbb->getNumberOfInstructions()-1);
            if (lastin){
                p = addInstrumentationPoint(lastin, functionExit, InstrumentationMode_tramp);
                for (uint32_t k = 0; k < fillExit.size(); k++){
                    p->addPrecursorInstruction(fillExit[k]);
                }
            } else {
                PRINT_WARN(10, "No exit from function %s", f->getName());
            }
        }

        delete exitBlocks;
        */
    }

}

