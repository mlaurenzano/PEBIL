#include <IOTracer.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <SymbolTable.h>

#define PROGRAM_ENTRY  "inittracer"
#define PROGRAM_EXIT   "finishtracer"
#define FUNCTION_TRACE  "functionentry"
#define FUNCTION_STOPTMR "functiontracer"
#define INST_LIB_NAME "libtracer.so"
#define MAX_ARG_COUNT 6

IOTracer::IOTracer(ElfFile* elf, char* traceFile)
    : InstrumentationTool(elf)
{
    programEntry = NULL;
    programExit = NULL;
    functionEntry = NULL;
    functionTrace = NULL;

    traceFunctions = new Vector<char*>();

    initializeFileList(traceFile, traceFunctions);
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
    functionEntry = declareFunction(FUNCTION_TRACE);
    ASSERT(functionEntry);
    functionTrace = declareFunction(FUNCTION_STOPTMR);
    ASSERT(functionTrace);
}


void IOTracer::instrument(){
    uint32_t temp32;
    uint64_t temp64;

    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    Vector<X86Instruction*> myInstPoints;
    Vector<Symbol*> myInstSymbols;

    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* instruction = getExposedInstruction(i);
        ASSERT(instruction->getContainer()->isFunction());
        Function* function = (Function*)instruction->getContainer();
        
        if (instruction->isFunctionCall()){
            instruction->print();
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(instruction->getTargetAddress());

            if (functionSymbol){
                uint32_t funcIdx = searchFileList(traceFunctions, functionSymbol->getSymbolName());
                if (funcIdx < (*traceFunctions).size()){
                    BasicBlock* bb = function->getBasicBlockAtAddress(instruction->getBaseAddress());
                    ASSERT(bb->containsCallToRange(0,-1));
                    //                    ASSERT(bb->searchForArgsPrep(is64Bit()) == (*argTypeLists)[funcIdx].size());
                    
                    myInstPoints.append(instruction);
                    myInstSymbols.append(functionSymbol);
                }
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

    char* fileName = getElfFile()->getFileName();
    uint64_t fileNameStorage = reserveDataOffset(strlen(fileName)+1);
    programEntry->addArgument(fileNameStorage);
    initializeReservedData(getInstDataAddress() + fileNameStorage, strlen(fileName), fileName);

    uint64_t functionIndexAddr = reserveDataOffset(sizeof(uint64_t));
    functionTrace->addArgument(functionIndexAddr);

    uint64_t funcArgumentStorage = reserveDataOffset(sizeof(uint64_t) * MAX_ARG_COUNT);
    functionTrace->addArgument(funcArgumentStorage);

    for (uint32_t i = 0; i < myInstPoints.size(); i++){

        uint32_t numArgs = MAX_ARG_COUNT;

        InstrumentationPoint* pt = addInstrumentationPoint(myInstPoints[i], functionEntry, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);

        char* functionName = myInstSymbols[i]->getSymbolName();
        if (!functionName){
            functionName = INFO_UNKNOWN;
        }
        uint64_t functionNameAddr = getInstDataAddress() + reserveDataOffset(strlen(functionName) + 1);
        ASSERT(sizeof(char*) == sizeof(uint64_t));
        initializeReservedData(getInstDataAddress() + funcNameArray + sizeof(char*)*i, sizeof(char*), &functionNameAddr);
        initializeReservedData(functionNameAddr, strlen(functionName) + 1, functionName);

        Vector<X86Instruction*> setupReg;

        ASSERT(numArgs);
        setupReg.append(X86InstructionFactory64::emitMoveRegToMem(X86_REG_DI, getInstDataAddress() + funcArgumentStorage));

        PRINT_INFOR("Wrapping call to %s (site id %d) at address %#llx", functionName, i, myInstPoints[i]->getProgramAddress());

        // already did the first one (di) so we could use it as a scratch regargumentTypeStorage
        for (uint32_t j = 1; j < numArgs; j++){
            ASSERT(j < MAX_ARG_COUNT);
            if (j < Num__64_bit_StackArgs){
                setupReg.append(X86InstructionFactory64::emitMoveRegToMem(map64BitArgToReg(j), getInstDataAddress() + funcArgumentStorage + (j * sizeof(uint64_t))));
            } else {
                PRINT_ERROR("64Bit instrumentation supports only %d args currently", Num__64_bit_StackArgs);
            }
        }

        setupReg.append(X86InstructionFactory64::emitMoveImmToReg(i, X86_REG_DI));
        setupReg.append(X86InstructionFactory64::emitMoveRegToMem(X86_REG_DI, getInstDataAddress() + functionIndexAddr));

        setupReg.append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + funcArgumentStorage, X86_REG_DI, true));

        while (setupReg.size()){
            pt->addPrecursorInstruction(setupReg.remove(0));
        }

        InstrumentationPoint* pt_after = addInstrumentationPoint(myInstPoints[i], functionTrace, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_after);
    }

}

