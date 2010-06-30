#include <InstrumentationTool.h>

#include <BasicBlock.h>
#include <FlowGraph.h>
#include <Function.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define MPI_INIT_WRAPPER_CBIND "MPI_Init_pebil_wrapper"
#define MPI_INIT_LIST_CBIND "PMPI_Init:MPI_Init"

#define MPI_INIT_WRAPPER_FBIND "mpi_init__pebil_wrapper"
#define MPI_INIT_LIST_FBIND "pmpi_init_:mpi_init_:MPI_INIT"

InstrumentationTool::InstrumentationTool(ElfFile* elf, char* ext, uint32_t phase, bool lpi, bool dtl)
    : ElfFileInst(elf)
{
    extension = ext;
    phaseNo = phase;
    loopIncl = lpi;
    printDetail = dtl;
}

void InstrumentationTool::declare(){
    initWrapperC = declareFunction(MPI_INIT_WRAPPER_CBIND);
    initWrapperF = declareFunction(MPI_INIT_WRAPPER_FBIND);
    ASSERT(initWrapperC && "Cannot find MPI_Init function, are you sure it was declared?");
    ASSERT(initWrapperF && "Cannot find MPI_Init function, are you sure it was declared?");
}

void InstrumentationTool::instrument(){
    // wrap any call to MPI_Init
    Vector<X86Instruction*>* mpiInitCalls = findAllCalls(MPI_INIT_LIST_CBIND);
    initWrapperC->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding MPI_Init wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperC, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
    }
    delete mpiInitCalls;

    mpiInitCalls = findAllCalls(MPI_INIT_LIST_FBIND);
    initWrapperF->setSkipWrapper();
    for (uint32_t i = 0; i < (*mpiInitCalls).size(); i++){
        ASSERT((*mpiInitCalls)[i]->isFunctionCall());
        ASSERT((*mpiInitCalls)[i]->getSizeInBytes() == Size__uncond_jump);
        PRINT_INFOR("Adding mpi_init_ wrapper @ %#llx", (*mpiInitCalls)[i]->getBaseAddress());
        InstrumentationPoint* pt = addInstrumentationPoint((*mpiInitCalls)[i], initWrapperF, InstrumentationMode_tramp, FlagsProtectionMethod_none, InstLocation_replace);
    }
    delete mpiInitCalls;
}

void InstrumentationTool::printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allLineInfos, uint32_t bufferSize){
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 

    ASSERT(!(*allLineInfos).size() || (*allBlocks).size() == (*allLineInfos).size());
    ASSERT((*allBlocks).size() == (*allBlockIds).size());

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
    fprintf(staticFD, "# cantidate = %d\n", getNumberOfExposedBasicBlocks());
    fprintf(staticFD, "# checksum  = %llx\n", getElfFile()->getCheckSum());
    uint32_t memopcnt = 0;
    uint32_t membytcnt = 0;
    uint32_t fltopcnt = 0;
    uint32_t insncnt = 0;
    for (uint32_t i = 0; i < allBlocks->size(); i++){
        BasicBlock* bb = (*allBlocks)[i];
        memopcnt += bb->getNumberOfMemoryOps();
        membytcnt += bb->getNumberOfMemoryBytes();
        fltopcnt += bb->getNumberOfFloatOps();
        insncnt += bb->getNumberOfInstructions();
    }
    fprintf(staticFD, "# blocks    = %d\n", allBlocks->size());
    fprintf(staticFD, "# memops    = %d\n", memopcnt);

    float memopavg = 0.0;
    if (memopcnt){
        memopavg = (float)membytcnt/(float)memopcnt;
    }
    fprintf(staticFD, "# memopbyte = %d ( %.5f bytes/op)\n", membytcnt, memopavg);
    fprintf(staticFD, "# fpops     = %d\n", fltopcnt);
    fprintf(staticFD, "# insns     = %d\n", insncnt);
    fprintf(staticFD, "# buffer    = %d\n", bufferSize);
    for (uint32_t i = 0; i < getNumberOfInstrumentationLibraries(); i++){
        fprintf(staticFD, "# library   = %s\n", getInstrumentationLibrary(i));
    }
    fprintf(staticFD, "# libTag    = %s\n", "revision REVISION");
    fprintf(staticFD, "# %s\n", "<no additional info>");
    fprintf(staticFD, "# <sequence> <block_unqid> <memop> <fpop> <insn> <line> <fname> # <hex_unq_id> <vaddr>\n");
    if (loopIncl){
        fprintf(staticFD, "# +lpi <loopcnt> <loopid> <ldepth>\n");
    }
    if (printDetail){
        fprintf(staticFD, "# +cnt <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op>\n");
        fprintf(staticFD, "# +mem <total_mem_op> <total_mem_bytes> <bytes/op>\n");
    }

    uint32_t noInst = 0;
    uint32_t fileNameSize = 1;
    uint32_t trapCount = 0;
    uint32_t jumpCount = 0;

    for (uint32_t i = 0; i < numberOfInstPoints; i++){

        BasicBlock* bb = (*allBlocks)[i];
        LineInfo* li = (*allLineInfos)[i];
        Function* f = bb->getFunction();

        uint32_t loopId = Invalid_UInteger_ID; 
        Loop* loop = bb->getFlowGraph()->getInnermostLoopForBlock(bb->getIndex());
        if (loop){
            loopId = loop->getIndex();
        }
        uint32_t loopDepth = bb->getFlowGraph()->getLoopDepth(bb->getIndex());
        uint32_t loopCount = bb->getFlowGraph()->getNumberOfLoops();

        char* fileName;
        uint32_t lineNo;
        if (li){
            fileName = li->getFileName();
            lineNo = li->GET(lr_line);
        } else {
            fileName = INFO_UNKNOWN;
            lineNo = 0;
        }
        fprintf(staticFD, "%d\t%lld\t%d\t%d\t%d\t%s:%d\t%s\t# %#llx\t%#llx\n", 
                (*allBlockIds)[i], bb->getHashCode().getValue(), bb->getNumberOfMemoryOps(), bb->getNumberOfFloatOps(), 
                bb->getNumberOfInstructions(), fileName, lineNo, bb->getFunction()->getName(), 
                bb->getHashCode().getValue(), bb->getLeader()->getProgramAddress());
        if (loopIncl){
            fprintf(staticFD, "\t+lpi\t%d\t%d\t%d # %#llx\n", loopCount, loopId, loopDepth, bb->getHashCode().getValue());
        }
        if (printDetail){
            fprintf(staticFD, "\t+cnt\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d # %#llx\n", 
                    bb->getNumberOfBranches(), bb->getNumberOfIntegerOps(), bb->getNumberOfLogicOps(), bb->getNumberOfShiftRotOps(),
                    bb->getNumberOfSyscalls(), bb->getNumberOfSpecialRegOps(), bb->getNumberOfStringOps(),
                    bb->getNumberOfLoads(), bb->getNumberOfStores(), bb->getNumberOfMemoryOps(), bb->getHashCode().getValue());

            ASSERT(bb->getNumberOfLoads() + bb->getNumberOfStores() == bb->getNumberOfMemoryOps());

            memopavg = 0.0;
            if (bb->getNumberOfMemoryOps()){
                memopavg = ((float)bb->getNumberOfMemoryBytes())/((float)bb->getNumberOfMemoryOps());
            }
            fprintf(staticFD, "\t+mem\t%d\t%d\t%.5f # %#llx\n", bb->getNumberOfMemoryOps(), bb->getNumberOfMemoryBytes(),
                    memopavg, bb->getHashCode().getValue());
        }
    }
    fclose(staticFD);

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}
