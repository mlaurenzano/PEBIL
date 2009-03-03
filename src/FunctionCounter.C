#include <BasicBlock.h>
#include <Function.h>
#include <FunctionCounter.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <LineInformation.h>
#include <TextSection.h>

#define EXIT_FUNCTION "functioncounter"
#define LIB_NAME "libcounter.so"
#define NOINST_VALUE 0xffffffff
#define FILE_UNK "__FILE_UNK__"
#define INST_BUFFER_SIZE (65536*sizeof(uint32_t))

FunctionCounter::FunctionCounter(ElfFile* elf)
    : ElfFileInst(elf)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", "fncinst");
}

void FunctionCounter::instrument(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(LIB_NAME);

    // declare any instrumentation functions that will be used
    InstrumentationFunction* exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    TextSection* text = getTextSection();
    TextSection* fini = getFiniSection();
    TextSection* init = getInitSection();

    ASSERT(text && "Cannot find text section");

    uint64_t dataBaseAddress = getExtraDataAddress();

    // find all of the functions in the .text section
    Vector<Function*> allFunctions;
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        if (text->getTextObject(i)->isFunction()){
            Function* f = (Function*)text->getTextObject(i);
            if (f->hasCompleteDisassembly() && 
                isEligibleFunction(f) && 
                strcmp(f->getName(),"_start")){
                allFunctions.append(f);
            }
        }
    }

    // the number blocks in the code
    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    // we have the option of giving an initialization value to addArgument
    exitFunc->addArgument(counterArrayEntries,allFunctions.size());

    // an array of counters. note that everything is passed by reference
    uint64_t counterArray = reserveDataOffset(allFunctions.size()*sizeof(uint32_t));
    exitFunc->addArgument(counterArray);

    // an array for function name pointers
    uint64_t funcNameArray = reserveDataOffset(allFunctions.size()*sizeof(char*));
    exitFunc->addArgument(funcNameArray);

    if (fini->findInstrumentationPoint(SIZE_CONTROL_TRANSFER, InstLocation_dont_care)){
        InstrumentationPoint* p = addInstrumentationPoint(fini,exitFunc,SIZE_CONTROL_TRANSFER);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    for (uint32_t i = 0; i < allFunctions.size(); i++){
        Function* f = allFunctions[i];

        if (f->getName()){
            uint64_t funcname = reserveDataOffset(strlen(f->getName())+1);
            uint64_t funcnameAddr = dataBaseAddress + funcname;
            initializeReservedData(dataBaseAddress+funcNameArray+i*sizeof(char*), sizeof(char*), &funcnameAddr);
            initializeReservedData(dataBaseAddress+funcname, strlen(f->getName())+1, (void*)f->getName());
        }

        InstrumentationSnippet* snip = new InstrumentationSnippet();
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));
        
        // save any registers used, this should include the flags register
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
            InstrumentationPoint* p = addInstrumentationPoint(f,snip,SIZE_CONTROL_TRANSFER);
        }
    }

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
