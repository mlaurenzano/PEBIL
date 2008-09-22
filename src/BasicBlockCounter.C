#include <BasicBlockCounter.h>
#include <TextSection.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <Function.h>
#include <BasicBlock.h>

#define EXIT_FUNCTION "blockcounter"
#define LIB_NAME "libtest.so"
#define OTHER_FUNCTION "secondtest"

BasicBlockCounter::BasicBlockCounter(ElfFile* elf)
    : ElfFileInst(elf)
{
}

BasicBlockCounter::~BasicBlockCounter(){
}

void BasicBlockCounter::declareInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 

    // declare any shared library that will contain instrumentation functions
    declareLibrary(LIB_NAME);

    // declare any instrumentation functions that will be used
    declareFunction(EXIT_FUNCTION);
    declareFunction(OTHER_FUNCTION);

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void BasicBlockCounter::reserveInstrumentation(){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    
    TextSection* text = getTextSection();
    TextSection* fini = getFiniSection();
    TextSection* init = getInitSection();

    ASSERT(text && "Cannot find text section");

    uint64_t dataBaseAddress = getExtraDataAddress();
    uint32_t instPoints = 0;
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        if (text->getTextObject(i)->isFunction()){
            Function* f = (Function*)text->getTextObject(i);
            for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
                instPoints++;
            }
        }
    }

    InstrumentationFunction* exitFunc = getInstrumentationFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");

    uint64_t counterArrayEntries = reserveDataOffset(sizeof(uint32_t));
    exitFunc->addArgument(ElfArgumentTypes_uint32_t,counterArrayEntries,instPoints);

    // in order to get a pointer we first reserve the array then a pointer to it
    uint64_t counterArray = reserveDataOffset(instPoints*sizeof(uint32_t));
    uint64_t counterArrayPtr = reserveDataOffset(sizeof(uint32_t));
    exitFunc->addArgument(ElfArgumentTypes_uint32_t_pointer,counterArray);

    if (fini->findInstrumentationPoint()){
        addInstrumentationPoint(fini,exitFunc);
    } else {
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    instPoints = 0;
    for (uint32_t i = 0; i < text->getNumberOfTextObjects(); i++){
        if (text->getTextObject(i)->isFunction()){
            Function* f = (Function*)text->getTextObject(i);
            for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){            
                BasicBlock* b = f->getBasicBlock(j);

                InstrumentationSnippet* snip = new InstrumentationSnippet();
                uint64_t counterOffset = counterArray + (instPoints * sizeof(uint32_t));
                
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
                        addInstrumentationPoint(f,snip);
                    } else {
                        PRINT_WARN("Cannot find instrumentation point for basic block %d at function %s", j, f->getName());
                    }
                }
                instPoints++;
            }
        }
    }

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
