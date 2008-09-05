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

    addInstrumentationPoint(fini,exitFunc);

    for (uint32_t i = 0; i < text->getNumberOfFunctions(); i++){
        InstrumentationSnippet* snip = new InstrumentationSnippet();
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));

        snip->addSnippetInstruction(Instruction32::generatePushEflags());
        snip->addSnippetInstruction(Instruction32::generateStackPush(X86_REG_CX));
        snip->addSnippetInstruction(Instruction32::generateMoveImmToReg(dataBaseAddress+counterOffset,X86_REG_CX));
        snip->addSnippetInstruction(Instruction32::generateAddByteToRegaddr(1,X86_REG_CX));
        snip->addSnippetInstruction(Instruction32::generateStackPop(X86_REG_CX));
        snip->addSnippetInstruction(Instruction32::generatePopEflags());
        // do not generate control instructions to get back to the code, this is done for
        // the snippet automatically during code generation

        addInstrumentationSnippet(snip);
        if (strcmp(text->getFunction(i)->getFunctionName(),"_start")){
            addInstrumentationPoint(text->getFunction(i),snip);
        }
    }

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
