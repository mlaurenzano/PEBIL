#include <BasicBlock.h>
#include <BasicBlockCounter.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define ENTRY_FUNCTION "initcounter"
#define EXIT_FUNCTION "blockcounter"
#define INST_LIB_NAME "libcounter.so"
#define FILE_UNK "__FILE_UNK__"
#define INST_SUFFIX "jbbinst"

BasicBlockCounter::BasicBlockCounter(ElfFile* elf, char* inputFuncList)
    : ElfFileInst(elf, inputFuncList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", INST_SUFFIX);

    entryFunc = NULL;
    exitFunc = NULL;
}

void BasicBlockCounter::declare(){
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

void BasicBlockCounter::instrument(){
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
            (*allBlocks).append(f->getBasicBlock(j));
            (*allLineInfos).append(lineInfoFinder->lookupLineInfo(f->getBasicBlock(j)));
        }
    }
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    uint32_t numberOfInstPoints = (*allBlocks).size();

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(numberOfInstPoints * sizeof(uint32_t));
    uint64_t lineArray = reserveDataOffset(numberOfInstPoints * sizeof(uint32_t));
    uint64_t fileNameArray = reserveDataOffset(numberOfInstPoints * sizeof(char*));
    uint64_t funcNameArray = reserveDataOffset(numberOfInstPoints * sizeof(char*));
    uint64_t hashCodeArray = reserveDataOffset(numberOfInstPoints * sizeof(uint64_t));
    uint64_t appName = reserveDataOffset((strlen(getApplicationName()) + 1) * sizeof(char));
    initializeReservedData(dataBaseAddress + appName, strlen(getApplicationName()), getApplicationName());
    uint64_t instExt = reserveDataOffset((strlen(getInstSuffix()) + 1) * sizeof(char));
    initializeReservedData(dataBaseAddress + instExt, strlen(getInstSuffix()), getInstSuffix());

    exitFunc->addArgument(counterArray);
    exitFunc->addArgument(appName);
    exitFunc->addArgument(instExt);

    if (fini->findInstrumentationPoint(SIZE_CONTROL_TRANSFER, InstLocation_dont_care)){
        addInstrumentationPoint(fini, exitFunc, SIZE_CONTROL_TRANSFER);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    // we have the option of giving an initialization value to addArgument so it is initialized in the function wrapper
    entryFunc->addArgument(counterArrayEntries,numberOfInstPoints);
    // an array for line numbers
    entryFunc->addArgument(lineArray);
    // an array for file name pointers
    entryFunc->addArgument(fileNameArray);
    // an array for function name pointers
    entryFunc->addArgument(funcNameArray);
    // an array for hashcodes
    entryFunc->addArgument(hashCodeArray);

    BasicBlock* entryBlock = getProgramEntryBlock();
    if (entryBlock->findInstrumentationPoint(SIZE_CONTROL_TRANSFER, InstLocation_dont_care)){
        InstrumentationPoint* p = addInstrumentationPoint(entryBlock, entryFunc, SIZE_CONTROL_TRANSFER);
        p->setPriority(InstPriority_userinit);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the entry block");
    }

#ifdef DEBUG_MEMTRACK
    PRINT_DEBUG_MEMTRACK("There are %d instrumentation points", numberOfInstPoints);
#endif
    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        BasicBlock* bb = (*allBlocks)[i];
        LineInfo* li = (*allLineInfos)[i];
        Function* f = bb->getFunction();

#ifdef DEBUG_MEMTRACK
        if (i % 1000 == 0){
            PRINT_DEBUG_MEMTRACK("inst point %d", i);
            PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);            
        }
#endif
        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(dataBaseAddress + lineArray + sizeof(uint32_t)*i, sizeof(uint32_t), &line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName()) + 1);
            uint64_t filenameAddr = dataBaseAddress + filename;
            initializeReservedData(dataBaseAddress + fileNameArray + i*sizeof(char*), sizeof(char*), &filenameAddr);
            initializeReservedData(dataBaseAddress + filename, strlen(li->getFileName()) + 1, (void*)li->getFileName());

        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName()) + 1);
        uint64_t funcnameAddr = dataBaseAddress + funcname;
        initializeReservedData(dataBaseAddress + funcNameArray + i*sizeof(char*), sizeof(char*), &funcnameAddr);
        initializeReservedData(dataBaseAddress + funcname, strlen(f->getName()) + 1, (void*)f->getName());

        uint64_t hashValue = bb->getHashCode().getValue();
        initializeReservedData(dataBaseAddress + hashCodeArray + i*sizeof(uint64_t), sizeof(uint64_t), &hashValue);
        
        InstrumentationSnippet* snip = new InstrumentationSnippet();
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));

        // snippet contents, in this case just increment a counter
        if (is64Bit()){
            snip->addSnippetInstruction(InstructionGenerator64::generateAddImmByteToMem(1, dataBaseAddress + counterOffset));
        } else {
            snip->addSnippetInstruction(InstructionGenerator32::generateAddImmByteToMem(1, dataBaseAddress + counterOffset));
        }
        // do not generate control instructions to get back to the application, this is done for
        // the snippet automatically during code generation
            
        // register the snippet we just created
        addInstrumentationSnippet(snip);
            
        // register an instrumentation point at the function that uses this snippet
            InstrumentationPoint* p = addInstrumentationPoint(bb,snip,SIZE_CONTROL_TRANSFER);
        if (strcmp(f->getName(),"_start")){
        }
    }
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    printStaticFile(allBlocks, allLineInfos);

    delete allBlocks;
    delete allLineInfos;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}


void BasicBlockCounter::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getApplicationName(), getInstSuffix(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getTextSection();

    fprintf(staticFD, "# appname   = %s\n", getApplicationName());
    fprintf(staticFD, "# appsize   = %d\n", getApplicationSize());
    fprintf(staticFD, "# extension = %s\n", getInstSuffix());
    fprintf(staticFD, "# phase     = %d\n", 0);
    fprintf(staticFD, "# type      = %s\n", briefName());
    fprintf(staticFD, "# cantidate = %d\n", 0);
    fprintf(staticFD, "# blocks    = %d\n", text->getNumberOfBasicBlocks());
    fprintf(staticFD, "# memops    = %d\n", text->getNumberOfMemoryOps());
    fprintf(staticFD, "# fpops     = %d\n", text->getNumberOfFloatOps());
    fprintf(staticFD, "# insns     = %d\n", text->getNumberOfInstructions());
    fprintf(staticFD, "# buffer    = %d\n", 0);
    for (uint32_t i = 0; i < getNumberOfInstrumentationLibraries(); i++){
        fprintf(staticFD, "# library   = %s\n", getInstrumentationLibrary(i));
    }
    fprintf(staticFD, "# libTag    = %s\n", "");
    fprintf(staticFD, "# %s\n", "");
    fprintf(staticFD, "# <sequence> <block_uid> <memop> <fpop> <insn> <line> <fname> <loopcnt> <loopid> <ldepth> <hex_uid> <vaddr> <loads> <stores>\n");

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        BasicBlock* bb = (*allBlocks)[i];
        LineInfo* li = (*allLineInfos)[i];
        Function* f = bb->getFunction();

        uint32_t loopId = Invalid_UInteger_ID;
        uint32_t loopDepth = 0;
        uint32_t loopCount = bb->getFlowGraph()->getNumberOfLoops();

        for(uint32_t j = 0;j < loopCount; j++){
            Loop* currLoop = bb->getFlowGraph()->getLoop(j);
            if(currLoop->isBlockIn(bb->getIndex())){
                loopDepth++;
                loopId = currLoop->getIndex();
            }
        }

        char* fileName;
        uint32_t lineNo;
        if (li){
            fileName = li->getFileName();
            lineNo = li->GET(lr_line);
        } else {
            fileName = FILE_UNK;
            lineNo = 0;
        }
        fprintf(staticFD, "%d\t%lld\t%d\t%d\t%d\t%s:%d\t%s\t#%d\t%d\t%d\t0x%012llx\t0x%llx\t%d\t%d\n", 
                i, bb->getHashCode().getValue(), bb->getNumberOfMemoryOps(), bb->getNumberOfFloatOps(), 
                bb->getNumberOfInstructions(), fileName, lineNo, bb->getFunction()->getName(), loopCount, loopId, loopDepth, 
                bb->getHashCode().getValue(), bb->getBaseAddress(), bb->getNumberOfLoads(), bb->getNumberOfStores());
    }
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
