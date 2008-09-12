#include <FunctionCounter.h>
#include <TextSection.h>
#include <Instrumentation.h>

#define EXIT_FUNCTION "functioncounter"
#define LIB_NAME "libtest.so"
#define OTHER_FUNCTION "secondtest"

FunctionCounter::FunctionCounter(ElfFile* elf)
    : ElfFileInst(elf)
{
}

FunctionCounter::~FunctionCounter(){
}

void FunctionCounter::declareInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 

    // declare any shared library that will contain instrumentation functions
    declareLibrary(LIB_NAME);

    // declare any instrumentation functions that will be used
    declareFunction(EXIT_FUNCTION);
    declareFunction(OTHER_FUNCTION);

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void FunctionCounter::reserveInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    
    TextSection* text = getTextSection();
    TextSection* fini = getFiniSection();
    TextSection* init = getInitSection();

    ASSERT(text && "Cannot find text section");

    uint64_t dataBaseAddress = getExtraDataAddress();
    uint32_t instPoints = text->getNumberOfFunctions();

    InstrumentationFunction* exitFunc = getInstrumentationFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    exitFunc->addArgument(ElfArgumentTypes_uint32_t,counterArrayEntries,instPoints);

    // in order to get a pointer we first reserve the array then a pointer to it
    uint64_t counterArray = reserveDataOffset(instPoints*sizeof(uint32_t));
    uint64_t counterArrayPtr = reserveDataOffset(sizeof(uint32_t));
    exitFunc->addArgument(ElfArgumentTypes_uint32_t_pointer,counterArray);

    uint64_t nameArray = reserveDataOffset(sizeof(char**));
    uint64_t namePtrs = reserveDataOffset(sizeof(char*)*instPoints);
    uint64_t* names = new uint64_t[instPoints];
    exitFunc->addArgument(ElfArgumentTypes_char_pointer_pointer,nameArray);
    uint64_t tmpAddr = dataBaseAddress + namePtrs;
    initializeReservedData(dataBaseAddress+nameArray,sizeof(char**),(void*)&tmpAddr);

    if (fini->findInstrumentationPoint()){
        addInstrumentationPoint(fini,exitFunc);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }
    for (uint32_t i = 0; i < instPoints; i++){
        uint32_t nameLength = strlen(text->getFunction(i)->getFunctionName())+1;

        names[i] = reserveDataOffset(nameLength);
        tmpAddr = dataBaseAddress + names[i];
        initializeReservedData(dataBaseAddress+namePtrs+i*sizeof(char*),sizeof(char*),(void*)&tmpAddr);
        initializeReservedData(dataBaseAddress+names[i],nameLength,(void*)(text->getFunction(i)->getFunctionName()));

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
        if (strcmp(text->getFunction(i)->getFunctionName(),"_start")){
            if (text->getFunction(i)->findInstrumentationPoint()){
                addInstrumentationPoint(text->getFunction(i),snip);
            } else {
                PRINT_WARN("Cannot find instrumentation point at function %s", text->getFunction(i)->getFunctionName());
            }
        }
    }

    delete[] names;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
