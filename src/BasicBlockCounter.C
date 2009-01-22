#include <BasicBlock.h>
#include <BasicBlockCounter.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <LineInformation.h>
#include <TextSection.h>

#define EXIT_FUNCTION "blockcounter"
#define START_FUNCTION "register_trap_handler"
#define LIB_NAME "libcounter.so"
#define NOINST_VALUE 0xffffffff
#define FILE_UNK "__FILE_UNK__"
#define INST_BUFFER_SIZE (65536*sizeof(uint32_t))
#define INSTBP_HASH_TABLE_SIZE 4

#define USE_PARTIAL_TRAP
//#define USE_FULL_TRAP

int32_t instbp_hash(int64_t key){
    return (int32_t)key;
}

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
    InstrumentationFunction* startFunc = declareFunction(START_FUNCTION);

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
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        if (text->getTextObject(i)->isFunction()){
            Function* f = (Function*)text->getTextObject(i);
            for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
                allBlocks.append(f->getBasicBlock(j));
                allLineInfos.append(lineInfoFinder->lookupLineInfo(f->getBasicBlock(j)));
            }
        }
    }
    ASSERT(allBlocks.size() == allLineInfos.size());
    uint32_t numberOfInstPoints = allBlocks.size();

    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");
    ASSERT(startFunc && "Cannot find start function, are you sure it was declared?");

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    // we have the option of giving an initialization value to addArgument
    exitFunc->addArgument(counterArrayEntries,numberOfInstPoints);

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(numberOfInstPoints*sizeof(uint32_t));
    exitFunc->addArgument(counterArray);

    // an array for basic block addresses
    uint64_t addrArray = reserveDataOffset(numberOfInstPoints*sizeof(uint64_t));
    exitFunc->addArgument(addrArray);

    // an array for line numbers
    uint64_t lineArray = reserveDataOffset(numberOfInstPoints*sizeof(uint32_t));
    exitFunc->addArgument(lineArray);

    // an array for file name pointers
    uint64_t fileNameArray = reserveDataOffset(numberOfInstPoints*sizeof(char*));
    exitFunc->addArgument(fileNameArray);

    Vector<InstrumentationPoint*>* allPoints = new Vector<InstrumentationPoint*>();
    if (fini->findInstrumentationPoint(SIZE_NEEDED_AT_INST_POINT, InstLocation_dont_care)){
        InstrumentationPoint* p = addInstrumentationPoint(fini,exitFunc,SIZE_NEEDED_AT_INST_POINT);
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
    fprintf(staticFD, "# <sequence> <block_uid> <memop> <fpop> <insn> <line> <fname> <loopcnt> <loopid> <ldepth> <hex_uid> <vaddr> <loads> <stores> <isinst?>\n");

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){
        BasicBlock* b = allBlocks[i];
        LineInfo* li = allLineInfos[i];
        Function* f = b->getFunction();

        uint64_t addr = b->getAddress();
        initializeReservedData(dataBaseAddress+addrArray+sizeof(uint64_t)*i,sizeof(uint64_t),(void*)&addr);
        
        if (li){
            uint32_t line = li->GET(lr_line);
            initializeReservedData(dataBaseAddress+lineArray+sizeof(uint32_t)*i,sizeof(uint32_t),&line);

            uint64_t filename = reserveDataOffset(strlen(li->getFileName())+1);
            uint64_t filenameAddr = dataBaseAddress + filename;
            initializeReservedData(dataBaseAddress+fileNameArray+i*sizeof(char*),sizeof(char*),&filenameAddr);
            initializeReservedData(dataBaseAddress+filename,strlen(li->getFileName())+1,(void*)li->getFileName());
        }

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
        if (strcmp(f->getName(),"_start")){
#ifdef USE_FULL_TRAP
            if (b->findInstrumentationPoint(SIZE_TRAP_INSTRUCTION, InstLocation_dont_care)){
                InstrumentationPoint* p = addInstrumentationPoint(b,snip,SIZE_TRAP_INSTRUCTION);
                (*allPoints).append(p);
                trapCount++;
            }            
#else
            if (b->findInstrumentationPoint(SIZE_CONTROL_TRANSFER, InstLocation_dont_care)){
                InstrumentationPoint* p = addInstrumentationPoint(b,snip,SIZE_CONTROL_TRANSFER);
                jumpCount++;
            }
#endif
#ifdef USE_PARTIAL_TRAP              
            else if (b->findInstrumentationPoint(SIZE_TRAP_INSTRUCTION, InstLocation_dont_care)){
                InstrumentationPoint* p = addInstrumentationPoint(b,snip,SIZE_TRAP_INSTRUCTION);
                (*allPoints).append(p);
                trapCount++;
            }
#endif
            else {
#ifdef USE_PARTIAL_TRAP
                __SHOULD_NOT_ARRIVE;
#endif
#ifdef USE_FULL_TRAP
                __SHOULD_NOT_ARRIVE;
#endif

                PRINT_WARN(3,"BLOCK_NOT_INSTRUMENTED: %llx [%d bytes] %s", b->getAddress(), b->getBlockSize(), b->getFunction()->getName());
                uint32_t noinst_value = NOINST_VALUE;
                if (!b->containsOnlyControl()){
                    noInst++;
                } else {
                    PRINT_WARN(3,"ONLY CONTROL BLOCK");
                }
                initializeReservedData(dataBaseAddress+counterArray+sizeof(uint32_t)*i,sizeof(uint32_t),&noinst_value);
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
                i, b->getHashCode().getValue(), b->getNumberOfMemoryOps(), b->getNumberOfFloatOps(), 
                b->getNumberOfInstructions(), fileName, lineNo, b->getFunction()->getName(), -1, -1, -1, 
                b->getHashCode().getValue(), b->getAddress(), b->getNumberOfLoads(), b->getNumberOfStores());
    }

    fclose(staticFD);

    // add arguments to start function
    startFunc->addArgument(counterArrayEntries,numberOfInstPoints);
    startFunc->addArgument(addrArray);
    uint64_t mapArrayEntries = reserveDataOffset(sizeof(uint32_t));
    uint32_t numMaps = (*allPoints).size();
    PRINT_DEBUG_INST("Passing map entries: %d entries at address %llx", numMaps, mapArrayEntries);
    startFunc->addArgument(mapArrayEntries,numMaps);
    uint64_t trampAddressArray = initAddressMapping(allPoints);
    startFunc->addArgument(trampAddressArray);

    delete allPoints;
    PRINT_WARN(3,"Cannot find instrumentation points for %d/%d basic blocks in the code", noInst, numberOfInstPoints);

    PRINT_INFOR("Instrumentation used %d traps and %d jumps", trapCount, jumpCount);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
