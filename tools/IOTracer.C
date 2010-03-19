#include <IOTracer.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <SymbolTable.h>

#define PROGRAM_ENTRY  "inittracer"
#define PROGRAM_EXIT   "finishtracer"
#define FUNCTION_TRACE  "functiontracer"
#define INST_LIB_NAME "libtracer.so"

IOTracer::IOTracer(ElfFile* elf, char* traceFile)
    : InstrumentationTool(elf)
{
    programEntry = NULL;
    programExit = NULL;
    functionTrace = NULL;

    traceFunctions = new Vector<char*>();
    initializeFileList(traceFile, traceFunctions);
    for (uint32_t i = 0; i < (*traceFunctions).size(); i++){
        PRINT_INFOR("Trace file read from input: %s", (*traceFunctions)[i]);
    }

    //    flags = InstrumentorFlag_norelocate;
}

IOTracer::~IOTracer(){
    for (uint32_t i = 0; i < (*traceFunctions).size(); i++){
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
    Vector<Symbol*> myInstSymbols;
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();
        
        if (instruction->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(instruction->getTargetAddress());
            if (functionSymbol && searchFileList(traceFunctions, functionSymbol->getSymbolName())){
                myInstPoints.append(instruction);
                myInstSymbols.append(functionSymbol);
            }
        } 
    }
    ASSERT(myInstPoints.size() == myInstSymbols.size());

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

        char* functionName = myInstSymbols[i]->getSymbolName();
        if (!functionName){
            functionName = INFO_UNKNOWN;
        }
        uint64_t functionNameAddr = getInstDataAddress() + reserveDataOffset(strlen(functionName) + 1);
        ASSERT(sizeof(char*) == sizeof(uint64_t));
        initializeReservedData(getInstDataAddress() + funcNameArray + sizeof(char*)*i, sizeof(char*), &functionNameAddr);
        initializeReservedData(functionNameAddr, strlen(functionName) + 1, functionName);

        Vector<Instruction*> setupReg;
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_DI, getInstDataAddress() + getRegStorageOffset() + (0 * sizeof(uint64_t))));
        setupReg.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_DI));
        setupReg.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_DI, getInstDataAddress() + functionIndexAddr));
        setupReg.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + (0 * sizeof(uint64_t)), X86_REG_DI));

        while (setupReg.size()){
            pt->addPrecursorInstruction(setupReg.remove(0));
        }
    }

}

