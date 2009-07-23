#include <BasicBlock.h>
#include <PrintMemory.h>
#include <Function.h>
#include <Instrumentation.h>
#include <Instruction.h>
#include <InstructionGenerator.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define MEM_FUNCTION "printmemory"
#define INST_LIB_NAME "libcounter.so"
#define FILE_UNK "__FILE_UNK__"
#define INST_SUFFIX "siminst"

PrintMemory::PrintMemory(ElfFile* elf, char* inputFuncList)
    : ElfFileInst(elf, inputFuncList)
{
    instSuffix = new char[__MAX_STRING_SIZE];
    sprintf(instSuffix,"%s\0", INST_SUFFIX);

    memFunc = NULL;
}

void PrintMemory::declare(){
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    memFunc = declareFunction(MEM_FUNCTION);
    ASSERT(memFunc && "Cannot find memory print function, are you sure it was declared?");

    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void PrintMemory::instrument(){
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

    uint64_t addressStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t regStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t offsetStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t indexStore = reserveDataOffset(sizeof(uint64_t));
    uint64_t scaleStore = reserveDataOffset(sizeof(uint64_t));

    memFunc->addArgumentAddress(addressStore);
    memFunc->addArgumentAddress(regStore);
    memFunc->addArgumentAddress(offsetStore);
    memFunc->addArgumentAddress(indexStore);
    memFunc->addArgumentAddress(scaleStore);

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        Instruction* memop = (*allMemOps)[i];
        InstrumentationPoint* pt = addInstrumentationPoint(memop, memFunc, SIZE_CONTROL_TRANSFER);

        MemoryOperand* memerand = new MemoryOperand(memop->getMemoryOperand(), this);

        Vector<Instruction*>* addressCalcInstructions = memerand->generateAddressCalculation(addressStore, offsetStore, regStore, indexStore, scaleStore);
        ASSERT(addressCalcInstructions);
        while ((*addressCalcInstructions).size()){
            pt->addPrecursorInstruction((*addressCalcInstructions).remove(0));
        }
        delete addressCalcInstructions;

    }

    printStaticFile(allBlocks, allLineInfos);

    delete allMemOps;
    delete allBlocks;
    delete allLineInfos;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}


void PrintMemory::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos){
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
        fprintf(staticFD, "%d\t%lld\t%d\t%d\t%d\t%s:%d\t%s\t#%d\t%d\t%d\t0x%012llx\t0x%llx\t%d\t%d\t%d\t%d\n", 
                i, bb->getHashCode().getValue(), bb->getNumberOfMemoryOps(), bb->getNumberOfFloatOps(), 
                bb->getNumberOfInstructions(), fileName, lineNo, bb->getFunction()->getName(), loopCount, loopId, loopDepth, 
                bb->getHashCode().getValue(), bb->getBaseAddress(), bb->getNumberOfLoads(), bb->getNumberOfStores(),
                bb->getNumberOfIntegerOps(), bb->getNumberOfStringOps());
    }
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
