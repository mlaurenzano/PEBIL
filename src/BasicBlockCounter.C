#include <BasicBlock.h>
#include <BasicBlockCounter.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define EXIT_FUNCTION "blockcounter"
#define LIB_NAME "libcounter.so"
#define NOINST_VALUE 0xffffffff
#define FILE_UNK "__FILE_UNK__"
#define INST_BUFFER_SIZE (65536*sizeof(uint32_t))

BasicBlockCounter::BasicBlockCounter(ElfFile* elf)
    : ElfFileInst(elf)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", "jbbinst");
}

void BasicBlockCounter::instrument(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(LIB_NAME);

    // declare any instrumentation functions that will be used
    InstrumentationFunction* exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    TextSection* text = getTextSection();
    TextSection* fini = getFiniSection();
    TextSection* init = getInitSection();

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    } else {
        PRINT_ERROR("This executable does not have any line information");
    }

    ASSERT(text && "Cannot find text section");

    uint64_t dataBaseAddress = getExtraDataAddress();

    Vector<BasicBlock*> allBlocks;
    Vector<LineInfo*> allLineInfos;

    PRINT_DEBUG_FUNC_RELOC("Instrumenting %d functions", exposedFunctions.size());
    for (uint32_t i = 0; i < exposedFunctions.size(); i++){
        Function* f = exposedFunctions[i];
        PRINT_DEBUG_FUNC_RELOC("\t%s", f->getName());
        ASSERT(f->hasCompleteDisassembly() && isEligibleFunction(f));
        for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
            allBlocks.append(f->getBasicBlock(j));
            allLineInfos.append(lineInfoFinder->lookupLineInfo(f->getBasicBlock(j)));
        }
    }
    ASSERT(!allLineInfos.size() || allBlocks.size() == allLineInfos.size());
    uint32_t numberOfInstPoints = allBlocks.size();

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    // we have the option of giving an initialization value to addArgument
    exitFunc->addArgument(counterArrayEntries,numberOfInstPoints);

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(numberOfInstPoints*sizeof(uint32_t));
    exitFunc->addArgument(counterArray);

    // an array for line numbers
    uint64_t lineArray = reserveDataOffset(numberOfInstPoints*sizeof(uint32_t));
    exitFunc->addArgument(lineArray);

    // an array for file name pointers
    uint64_t fileNameArray = reserveDataOffset(numberOfInstPoints*sizeof(char*));
    exitFunc->addArgument(fileNameArray);

    // an array for function name pointers
    uint64_t funcNameArray = reserveDataOffset(numberOfInstPoints*sizeof(char*));
    exitFunc->addArgument(funcNameArray);

    Vector<InstrumentationPoint*>* allPoints = new Vector<InstrumentationPoint*>();
    if (fini->findInstrumentationPoint(SIZE_CONTROL_TRANSFER, InstLocation_dont_care)){
        InstrumentationPoint* p = addInstrumentationPoint(fini,exitFunc,SIZE_CONTROL_TRANSFER);
        (*allPoints).append(p);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getApplicationName(), getInstSuffix(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

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
    fprintf(staticFD, "# buffer    = %d\n", INST_BUFFER_SIZE);
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
        BasicBlock* bb = allBlocks[i];
        LineInfo* li = allLineInfos[i];
        Function* f = bb->getFunction();

        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(dataBaseAddress+lineArray+sizeof(uint32_t)*i,sizeof(uint32_t),&line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName())+1);
            uint64_t filenameAddr = dataBaseAddress + filename;
            initializeReservedData(dataBaseAddress+fileNameArray+i*sizeof(char*),sizeof(char*),&filenameAddr);
            initializeReservedData(dataBaseAddress+filename,strlen(li->getFileName())+1,(void*)li->getFileName());

        }
        uint64_t funcname = reserveDataOffset(strlen(f->getName())+1);
        uint64_t funcnameAddr = dataBaseAddress + funcname;
        initializeReservedData(dataBaseAddress+funcNameArray+i*sizeof(char*),sizeof(char*),&funcnameAddr);
        initializeReservedData(dataBaseAddress+funcname,strlen(f->getName())+1,(void*)f->getName());
        
        InstrumentationSnippet* snip = new InstrumentationSnippet();
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));
        
        // save any registers used, this should always include the flags register
        snip->addSnippetInstruction(Instruction32::generatePushEflags());
        snip->addSnippetInstruction(Instruction32::generateStackPush(X86_REG_CX));
                
        // increment the counter for this function
        snip->addSnippetInstruction(Instruction32::generateMoveImmToReg(dataBaseAddress+counterOffset,X86_REG_CX));
        snip->addSnippetInstruction(Instruction32::generateAddByteToRegaddr(1,X86_REG_CX));
        
        // restore the registers that were saved
        snip->addSnippetInstruction(Instruction32::generateStackPop(X86_REG_CX));
        snip->addSnippetInstruction(Instruction32::generatePopEflags());
            
        // do not generate control instructions to get back to the application, this is done for
        // the snippet automatically during code generation
            
        // register the snippet we just created
        addInstrumentationSnippet(snip);
            
        // register an instrumentation point at the function that uses this snippet
        InstrumentationPoint* p = addInstrumentationPoint(bb,snip,SIZE_CONTROL_TRANSFER);

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

    delete allPoints;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
