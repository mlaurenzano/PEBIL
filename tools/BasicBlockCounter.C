#include <BasicBlockCounter.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define ENTRY_FUNCTION "initcounter"
#define EXIT_FUNCTION "blockcounter"
#define INST_LIB_NAME "libcounter.so"
#define NOSTRING "__pebil_no_string__"

BasicBlockCounter::BasicBlockCounter(ElfFile* elf)
    : InstrumentationTool(elf)
{
    entryFunc = NULL;
    exitFunc = NULL;
}

void BasicBlockCounter::declare()
{
    InstrumentationTool::declare();
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void BasicBlockCounter::instrument() 
{
    InstrumentationTool::instrument();
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    uint32_t temp32;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    uint64_t lineArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(uint32_t));
    uint64_t fileNameArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(char*));
    uint64_t funcNameArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(char*));
    uint64_t hashCodeArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(uint64_t));
    uint64_t appName = reserveDataOffset((strlen(getApplicationName()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + appName, strlen(getApplicationName()) + 1, getApplicationName());
    uint64_t instExt = reserveDataOffset((strlen(getInstSuffix()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + instExt, strlen(getInstSuffix()) + 1, getInstSuffix());

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(uint32_t));

    exitFunc->addArgument(counterArray);
    exitFunc->addArgument(appName);
    exitFunc->addArgument(instExt);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    temp32 = 0;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        initializeReservedData(getInstDataAddress() + counterArray + i*sizeof(uint32_t), sizeof(uint32_t), &temp32);
    }

    // the number of inst points
    entryFunc->addArgument(counterArrayEntries);
    temp32 = getNumberOfExposedBasicBlocks();
    initializeReservedData(getInstDataAddress() + counterArrayEntries, sizeof(uint32_t), &temp32);

    // an array for line numbers
    entryFunc->addArgument(lineArray);
    // an array for file name pointers
    entryFunc->addArgument(fileNameArray);
    // an array for function name pointers
    entryFunc->addArgument(funcNameArray);
    // an array for hashcodes
    entryFunc->addArgument(hashCodeArray);

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp, FlagsProtectionMethod_full, InstLocation_prior);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

    uint64_t noDataAddr = getInstDataAddress() + reserveDataOffset(strlen(NOSTRING) + 1);
    char* nostring = new char[strlen(NOSTRING) + 1];
    sprintf(nostring, "%s\0", NOSTRING);
    initializeReservedData(noDataAddr, strlen(NOSTRING) + 1, nostring);

    PRINT_DEBUG_MEMTRACK("There are %d instrumentation points", getNumberOfExposedBasicBlocks());
    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();

    uint32_t noProtPoints = 0;
    uint32_t complexSelection = 0;
    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){

        BasicBlock* bb = getExposedBasicBlock(i);
        LineInfo* li = NULL;
        if (lineInfoFinder){
            li = lineInfoFinder->lookupLineInfo(bb);
        }
        Function* f = bb->getFunction();

        (*allBlocks).append(bb);
        (*allLineInfos).append(li);

        if (i % 1000 == 0){
            PRINT_DEBUG_MEMTRACK("inst point %d", i);
            PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);            
        }

        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(getInstDataAddress() + lineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
            uint64_t filenameAddr = getInstDataAddress() + filename;
            initializeReservedData(getInstDataAddress() + fileNameArray + i*sizeof(char*), sizeof(char*), &filenameAddr);
            initializeReservedData(getInstDataAddress() + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());

        } else {
            temp32 = 0;
            initializeReservedData(getInstDataAddress() + lineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &temp32);
            initializeReservedData(getInstDataAddress() + fileNameArray + i*sizeof(char*), sizeof(char*), &noDataAddr);
        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = getInstDataAddress() + funcname;
        initializeReservedData(getInstDataAddress() + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(getInstDataAddress() + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        uint64_t hashValue = bb->getHashCode().getValue();
        initializeReservedData(getInstDataAddress() + hashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
        
        InstrumentationSnippet* snip = new InstrumentationSnippet();
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));

        // snippet contents, in this case just increment a counter
        if (is64Bit()){
            snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        } else {
            snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        }

        // do not generate control instructions to get back to the application, this is done for
        // the snippet automatically during code generation
            
        // register the snippet we just created
        addInstrumentationSnippet(snip);            
            
        // register an instrumentation point at the function that uses this snippet
        FlagsProtectionMethods prot = FlagsProtectionMethod_light;
#ifndef NO_REG_ANALYSIS
        if (bb->getLeader()->allFlagsDeadIn()){
            prot = FlagsProtectionMethod_none;
            noProtPoints++;
        }
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            if (bb->getInstruction(j)->allFlagsDeadIn() || bb->getInstruction(j)->allFlagsDeadOut()){
                complexSelection++;
                break;
            }
        }
#endif
        InstrumentationPoint* p = addInstrumentationPoint(bb, snip, InstrumentationMode_inline, prot);
    }
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
#ifdef NO_REG_ANALYSIS
    PRINT_WARN(10, "Warning: register analysis disabled");
#endif
    PRINT_INFOR("Excluding flags protection for %d/%d instrumentation points", noProtPoints, getNumberOfExposedBasicBlocks());
    //PRINT_INFOR("complex inst point selection: %d/%d instrumentation points", complexSelection, getNumberOfExposedBasicBlocks());

    printStaticFile(allBlocks, allLineInfos);

    delete[] nostring;
    delete allBlocks;
    delete allLineInfos;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
