#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define SIM_FUNCTION "processTrace"
#define INST_LIB_NAME "libsimulator.so"
#define INST_SUFFIX "siminst"
#define BUFFER_ENTRIES 65536

CacheSimulation::CacheSimulation(ElfFile* elf, char* inputFuncList)
    : InstrumentationTool(elf, inputFuncList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", INST_SUFFIX);

    simFunc = NULL;
}

void CacheSimulation::declare(){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    simFunc = declareFunction(SIM_FUNCTION);
    ASSERT(simFunc && "Cannot find memory print function, are you sure it was declared?");

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void CacheSimulation::instrument(){
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

    Vector<Instruction*>* allMemOps = new Vector<Instruction*>();
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
            BasicBlock* b = f->getBasicBlock(j);
            if (b->isCmpCtrlSplit()){
                PRINT_WARN(10, "Comparison/cond branch are split in block at %#llx, not instrumenting", b->getBaseAddress());
            } else {
                (*allBlocks).append(b);
                (*allLineInfos).append(lineInfoFinder->lookupLineInfo(b));
                for (uint32_t k = 0; k < b->getNumberOfInstructions(); k++){
                    Instruction* m = b->getInstruction(k);
                    if (m->isMemoryOperation()){
                        (*allMemOps).append(m);
                    }
                }
            }
        }
    }

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    uint32_t numberOfInstPoints = (*allMemOps).size();

    uint64_t bufferStore  = reserveDataOffset(BUFFER_ENTRIES * sizeof(uint64_t));
    uint64_t buffPtrStore = reserveDataOffset(sizeof(uint64_t));


    uint64_t addressStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t offsetStore  = reserveDataOffset(sizeof(uint64_t));
    uint64_t regStore     = reserveDataOffset(sizeof(uint64_t));
    uint64_t indexStore   = reserveDataOffset(sizeof(uint64_t));
    uint64_t scaleStore   = reserveDataOffset(sizeof(uint64_t));

    simFunc->addArgument(bufferStore);
    simFunc->addArgument(buffPtrStore);

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        Instruction* memop = (*allMemOps)[i];
        InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, SIZE_CONTROL_TRANSFER);

        MemoryOperand* memerand = new MemoryOperand(memop->getMemoryOperand(), this);

        Vector<Instruction*>* addressCalcInstructions = memerand->generateBufferedAddressCalculation(bufferStore, buffPtrStore, BUFFER_ENTRIES);
        ASSERT(addressCalcInstructions);
        while ((*addressCalcInstructions).size()){
            pt->addPrecursorInstruction((*addressCalcInstructions).remove(0));
        }
        delete addressCalcInstructions;
        delete memerand;

    }

    printStaticFile(allBlocks, allLineInfos);

    delete allMemOps;
    delete allBlocks;
    delete allLineInfos;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
