#include <BasicBlockCounter.h>
#include <TextSection.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <Function.h>
#include <BasicBlock.h>
#include <LineInformation.h>

#define EXIT_FUNCTION "blockcounter"
#define LIB_NAME "libcounter.so"
#define NOINST_VALUE 0xffffffff

BasicBlockCounter::BasicBlockCounter(ElfFile* elf)
    : ElfFileInst(elf)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", "jbbinst");
}

   /*
# appname   = a.out
# appsize   = 10370
# extension = jbbinst
# phase     = 0
# type      = BasicBlockCounter
# candidate = 91
# blocks    = 61
# memops    = 164
# fpops     = 0
# insns     = 298
# buffer    = 61
# library   = /users/sdsc/michaell/PMaCInstrumentor//lib
# libTag    = revision REVISION
# <no additional info>
# <sequence> <block_uid> <memop> <fpop> <insn> <line> <fname> <loopcnt> <loopid> <ldepth> <hex_uid> <vaddr> <loads> <stores>
   */

void BasicBlockCounter::instrument(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(LIB_NAME);

    // declare any instrumentation functions that will be used
    declareFunction(EXIT_FUNCTION);


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

    InstrumentationFunction* exitFunc = getInstrumentationFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

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


    if (fini->findInstrumentationPoint()){
        addInstrumentationPoint(fini,exitFunc);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getApplicationName(), getInstSuffix(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
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
            if (b->findInstrumentationPoint()){
                addInstrumentationPoint(b,snip);
            } else {
                PRINT_WARN(3,"BLOCK_NOT_INSTRUMENTED: %llx [%d bytes] %s", b->getAddress(), b->getBlockSize(), b->getFunction()->getName());
                uint32_t noinst_value = NOINST_VALUE;
                noInst++;
                initializeReservedData(dataBaseAddress+counterArray+sizeof(uint32_t)*i,sizeof(uint32_t),&noinst_value);
            }
        }
    }

    fclose(staticFD);

    PRINT_WARN(3,"Cannot find instrumentation points for %d/%d basic blocks in the code", noInst, numberOfInstPoints);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
