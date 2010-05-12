#include <InstrumentationTool.h>

#include <BasicBlock.h>
#include <FlowGraph.h>
#include <Function.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define MPI_INIT_WRAPPER "MPI_Init_pebil_wrapper"
#define MPI_INIT_LIST "PMPI_Init:MPI_Init"

InstrumentationTool::InstrumentationTool(ElfFile* elf)
    : ElfFileInst(elf)
{
}

void InstrumentationTool::declare(){
    initWrapper = declareFunction("MPI_Init_pebil_wrapper");
    ASSERT(initWrapper && "Cannot find MPI_Init function, are you sure it was declared?");
}

void InstrumentationTool::instrument(){
    // wrap any call to MPI_Init
    Vector<X86Instruction*>* mpiInitCalls = findAllCalls("PMPI_Init:MPI_Init");
    initWrapper->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding MPI_Init wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapper, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
    }

}

void InstrumentationTool::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    uint32_t numberOfInstPoints = (*allBlocks).size();

    char* staticFile = new char[__MAX_STRING_SIZE];
    sprintf(staticFile,"%s.%s.%s", getApplicationName(), getInstSuffix(), "static");
    FILE* staticFD = fopen(staticFile, "w");
    delete[] staticFile;

    TextSection* text = getDotTextSection();

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
            fileName = INFO_UNKNOWN;
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
