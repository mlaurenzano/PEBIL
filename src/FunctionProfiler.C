#include <FunctionProfiler.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>

#define PROGRAM_ENTRY  "program_entry"
#define PROGRAM_EXIT   "program_exit"
#define FUNCTION_ENTRY "function_entry"
#define FUNCTION_EXIT  "function_exit"
#define INST_LIB_NAME "libprofiler.so"
#define INST_SUFFIX "pblprof"

FunctionProfiler::FunctionProfiler(ElfFile* elf, char* inputFuncList)
    : InstrumentationTool(elf, inputFuncList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", INST_SUFFIX);

    programEntry = NULL;
    programExit = NULL;
    functionEntry = NULL;
    functionExit = NULL;
}

void FunctionProfiler::declare(){
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    programEntry = declareFunction(PROGRAM_ENTRY);
    ASSERT(programEntry);
    programExit = declareFunction(PROGRAM_EXIT);
    ASSERT(programExit);
    functionEntry = declareFunction(FUNCTION_ENTRY);
    ASSERT(functionEntry);
    functionExit = declareFunction(FUNCTION_EXIT);
    ASSERT(functionExit);
}

void FunctionProfiler::instrument(){
    uint32_t temp32;
    uint64_t temp64;

    InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), programEntry, InstrumentationMode_tramp);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramExitBlock(), programExit, InstrumentationMode_tramp);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    uint64_t functionCountAddr = reserveDataOffset(sizeof(uint64_t));
    programEntry->addArgument(functionCountAddr);
    temp64 = getNumberOfExposedFunctions();
    initializeReservedData(getInstDataAddress() + functionCountAddr, sizeof(uint64_t), &temp64);

    uint64_t funcNameArray = reserveDataOffset(getNumberOfExposedFunctions() * sizeof(char*));
    programEntry->addArgument(funcNameArray);

    uint64_t functionIndexAddr = reserveDataOffset(sizeof(uint64_t));
    functionEntry->addArgument(functionIndexAddr);
    functionExit->addArgument(functionIndexAddr);

    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        uint64_t funcname = reserveDataOffset(strlen(getExposedFunction(i)->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(getExposedFunction(i)->getName()) + 1, (void*)getExposedFunction(i)->getName());

        if (!getExposedFunction(i)->getFlowGraph()->getEntryBlock() ||
            !getExposedFunction(i)->getFlowGraph()->getExitBlock()){
            PRINT_WARN(8, "Function %s is missing either an entry or exit block (probably exit)", getExposedFunction(i)->getName());
            continue;
        }

        BasicBlock* bb = getExposedFunction(i)->getFlowGraph()->getEntryBlock();

        Vector<Instruction*> fillIndex = Vector<Instruction*>();
        Vector<Instruction*> fillIndex2 = Vector<Instruction*>();

        fillIndex.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
        fillIndex.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_CX));
        fillIndex.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
        fillIndex.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));

        fillIndex2.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
        fillIndex2.append(InstructionGenerator64::generateMoveImmToReg(i, X86_REG_CX));
        fillIndex2.append(InstructionGenerator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
        fillIndex2.append(InstructionGenerator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX));

        p = addInstrumentationPoint(bb, functionEntry, InstrumentationMode_tramp);
        for (uint32_t j = 0; j < fillIndex.size(); j++){
            p->addPrecursorInstruction(fillIndex[j]);
        }

        bb = getExposedFunction(i)->getFlowGraph()->getExitBlock();
        p = addInstrumentationPoint(bb, functionExit, InstrumentationMode_tramp);
        for (uint32_t j = 0; j < fillIndex.size(); j++){
            p->addPrecursorInstruction(fillIndex2[j]);
        }
    }

    
    /*
    
    // the number functions in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    temp32 = getNumberOfExposedFunctions();
    initializeReservedData(getInstDataAddress() + counterArrayEntries, sizeof(uint32_t), &temp32);

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(getNumberOfExposedFunctions() * sizeof(uint32_t));
    temp32 = 0;
    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        initializeReservedData(getInstDataAddress() + counterArray + i*sizeof(uint32_t), sizeof(uint32_t), &temp32);
    }

    // the names of all the functions
    uint64_t funcNameArray = reserveDataOffset(getNumberOfExposedFunctions() * sizeof(char*));

    exitFunc->addArgument(counterArrayEntries);
    exitFunc->addArgument(counterArray);
    exitFunc->addArgument(funcNameArray);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        Function* f = getExposedFunction(i);

        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        InstrumentationSnippet* snip = new InstrumentationSnippet();
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));

        // snippet contents, in this case just increment a counter
        if (is64Bit()){
            snip->addSnippetInstruction(InstructionGenerator64::generateAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        } else {
            snip->addSnippetInstruction(InstructionGenerator32::generateAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        }
        // do not generate control instructions to get back to the application, this is done for
        // the snippet automatically during code generation
            
        // register the snippet we just created
        addInstrumentationSnippet(snip);            
        
        // register an instrumentation point at the function that uses this snippet
        InstrumentationPoint* p = addInstrumentationPoint(f, snip, InstrumentationMode_inline, FlagsProtectionMethod_light);
    }
    */
}
