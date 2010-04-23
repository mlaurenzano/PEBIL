#include <FunctionTimer.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <InstrucX86.h>
#include <InstrucX86Generator.h>

#define PROGRAM_ENTRY  "program_entry"
#define PROGRAM_EXIT   "program_exit"
#define FUNCTION_ENTRY "function_entry"
#define FUNCTION_EXIT  "function_exit"
#define INST_LIB_NAME "libtimer.so"

FunctionTimer::FunctionTimer(ElfFile* elf)
    : InstrumentationTool(elf)
{
    programEntry = NULL;
    programExit = NULL;
    functionEntry = NULL;
    functionExit = NULL;
}

void FunctionTimer::declare(){
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

void FunctionTimer::instrument(){
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
        Function* f = getExposedFunction(i);
        
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        BasicBlock* bb = f->getFlowGraph()->getEntryBlock();
        Vector<BasicBlock*>* exitBlocks = f->getFlowGraph()->getExitBlocks();

        Vector<InstrucX86*> fillEntry = Vector<InstrucX86*>();
        fillEntry.append(InstrucX86Generator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
        fillEntry.append(InstrucX86Generator64::generateMoveImmToReg(i, X86_REG_CX));
        fillEntry.append(InstrucX86Generator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
        fillEntry.append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));

        p = addInstrumentationPoint(bb, functionEntry, InstrumentationMode_tramp);
        for (uint32_t j = 0; j < fillEntry.size(); j++){
            p->addPrecursorInstruction(fillEntry[j]);
        }

        for (uint32_t j = 0; j < (*exitBlocks).size(); j++){
            Vector<InstrucX86*> fillExit = Vector<InstrucX86*>();
            fillExit.append(InstrucX86Generator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            fillExit.append(InstrucX86Generator64::generateMoveImmToReg(i, X86_REG_CX));
            fillExit.append(InstrucX86Generator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
            fillExit.append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));

            p = addInstrumentationPoint((*exitBlocks)[j], functionExit, InstrumentationMode_tramp);
            for (uint32_t k = 0; k < fillExit.size(); k++){
                p->addPrecursorInstruction(fillExit[k]);
            }
        }
        if (!(*exitBlocks).size()){
            Vector<InstrucX86*> fillExit = Vector<InstrucX86*>();
            fillExit.append(InstrucX86Generator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + getRegStorageOffset()));
            fillExit.append(InstrucX86Generator64::generateMoveImmToReg(i, X86_REG_CX));
            fillExit.append(InstrucX86Generator64::generateMoveRegToMem(X86_REG_CX, getInstDataAddress() + functionIndexAddr));
            fillExit.append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset(), X86_REG_CX, true));

            BasicBlock* lastbb = f->getBasicBlock(f->getNumberOfBasicBlocks()-1);
            InstrucX86* lastin = lastbb->getInstruction(lastbb->getNumberOfInstructions()-1);
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
            snip->addSnippetInstruction(InstrucX86Generator64::generateAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        } else {
            snip->addSnippetInstruction(InstrucX86Generator32::generateAddImmByteToMem(1, getInstDataAddress() + counterOffset));
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
