#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <InstrucX86.h>
#include <InstrucX86Generator.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define ENTRY_FUNCTION "entry_function"
#define SIM_FUNCTION "MetaSim_simulFuncCall_Simu"
#define EXIT_FUNCTION "MetaSim_endFuncCall_Simu"
#define INST_LIB_NAME "libsimulator.so"
#define BUFFER_ENTRIES 0x00010000
#define Size__BufferEntry 16
#define MAX_MEMOPS_PER_BLOCK 256

void CacheSimulation::usesModifiedProgram(){
    InstrucX86* nop5Byte = InstrucX86Generator::generateNop(5);
    instpoint_info iinf;
    bzero(&iinf, sizeof(instpoint_info));
    iinf.pt_size = Size__uncond_jump;
    memcpy(iinf.pt_disable, nop5Byte->charStream(), iinf.pt_size);

    for (uint32_t i = 0; i < memInstPoints.size(); i++){
        ASSERT(memInstPoints[i]->getInstrumentationMode() != InstrumentationMode_inline);
        iinf.pt_vaddr = memInstPoints[i]->getInstSourceAddress();
        iinf.pt_blockid = memInstBlockIds[i];
        //PRINT_INFOR("mem point %d (block %d) initialized at addr %#llx", i, iinf.pt_blockid, getInstDataAddress() + instPointInfo + (i * sizeof(instpoint_info)));
        initializeReservedData(getInstDataAddress() + instPointInfo + (i * sizeof(instpoint_info)), sizeof(instpoint_info), &iinf);
    }    

    delete nop5Byte;
}

CacheSimulation::CacheSimulation(ElfFile* elf, char* inputFile)
    : InstrumentationTool(elf)
{
    simFunc = NULL;
    exitFunc = NULL;
    entryFunc = NULL;

    Vector<char*>* fileLines = new Vector<char*>();
    initializeFileList(inputFile, fileLines);
    int32_t err;
    uint64_t blockHash;
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        char* ptr = strchr((*fileLines)[i],'#');
        if(ptr) *ptr = '\0';

        if(!strlen((*fileLines)[i]) || allSpace((*fileLines)[i]))
            continue;

        err = sscanf((*fileLines)[i], "%lld", &blockHash);
        if(err <= 0){
            PRINT_ERROR("Line %d of %s has a wrong format", i, inputFile);
        }
        HashCode* hashCode = new HashCode(blockHash);
        if(!hashCode->isBlock()){
            PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block", i, inputFile);
        }
        blocksToInst.append(hashCode);
    }
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        delete[] (*fileLines)[i];
    }
    delete fileLines;

    blocksToInst.sort(compareHashCode);
}

CacheSimulation::~CacheSimulation(){
    for (uint32_t i = 0; i < blocksToInst.size(); i++){
        delete blocksToInst[i];
    }
}

void CacheSimulation::declare(){
    
    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    simFunc = declareFunction(SIM_FUNCTION);
    ASSERT(simFunc && "Cannot find memory print function, are you sure it was declared?");
    exitFunc = declareFunction(EXIT_FUNCTION);
    ASSERT(exitFunc && "Cannot find exit function, are you sure it was declared?");
    entryFunc = declareFunction(ENTRY_FUNCTION);
    ASSERT(entryFunc && "Cannot find entry function, are you sure it was declared?");
}

void CacheSimulation::instrument(){
    uint32_t temp32;
    
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    } else {
        PRINT_ERROR("This executable does not have any line information");
    }

    ASSERT(isPowerOfTwo(Size__BufferEntry));
    uint64_t bufferStore  = reserveDataOffset(BUFFER_ENTRIES * Size__BufferEntry);
    uint32_t startValue = 1;
    initializeReservedData(getInstDataAddress() + bufferStore, sizeof(uint32_t), &startValue);
    uint64_t dfpEmpty = reserveDataOffset(4*sizeof(uint64_t));

    uint64_t entryCountStore = reserveDataOffset(sizeof(uint64_t));
    startValue = Size__BufferEntry * BUFFER_ENTRIES;
    initializeReservedData(getInstDataAddress() + entryCountStore, sizeof(uint64_t), &startValue);

    uint64_t blockSizeStore = reserveDataOffset(sizeof(uint64_t));

    char* appName = getElfFile()->getFileName();
    char* extension = "siminst";
    uint32_t phaseId = 0;
    uint32_t dumpCode = 0;
    uint32_t commentSize = strlen(appName) + sizeof(uint32_t) + strlen(extension) + sizeof(uint32_t) + sizeof(uint32_t) + 4;
    uint64_t commentStore = reserveDataOffset(commentSize);
    char* comment = new char[commentSize];
    sprintf(comment, "%s %u %s %u %u", appName, phaseId, extension, getNumberOfExposedBasicBlocks(), dumpCode);
    initializeReservedData(getInstDataAddress() + commentStore, commentSize, comment);

    simFunc->addArgument(bufferStore);
    simFunc->addArgument(entryCountStore);
    simFunc->addArgument(commentStore);

    exitFunc->addArgument(bufferStore);
    exitFunc->addArgument(entryCountStore);
    exitFunc->addArgument(commentStore);

    uint64_t addrScratchSpace = reserveDataOffset(Size__BufferEntry * MAX_MEMOPS_PER_BLOCK);

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
    ASSERT(p);

    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();

    uint32_t blockId = 0;
    uint32_t memopId = 0;
    uint32_t noProtPoints = 0;
    uint32_t totalProt = 0;
    uint32_t regDefault = 0;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);

        uint64_t hashValue = bb->getHashCode().getValue();
        void* bfound = bsearch(&hashValue, &blocksToInst, blocksToInst.size(), sizeof(HashCode*), searchHashCode);

        if (bfound || !blocksToInst.size()){
            (*allBlocks).append(bb);
            (*allLineInfos).append(lineInfoFinder->lookupLineInfo(bb));
            
            uint32_t memopIdInBlock = 0;
            
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                InstrucX86* memop = bb->getInstruction(j);
                
                if (memop->isMemoryOperation()){            
                    
                    // check the buffer at the last memop
                    if (memopIdInBlock == bb->getNumberOfMemoryOps() - 1){
                        FlagsProtectionMethods prot = FlagsProtectionMethod_full;
                        totalProt++;
#ifndef NO_REG_ANALYSIS
                        if (memop->allFlagsDeadIn()){
                            noProtPoints++;
                            prot = FlagsProtectionMethod_none;
                        }
#endif
                        uint32_t tmpReg1 = X86_REG_CX;
                        uint32_t tmpReg2 = X86_REG_DX;
                        uint32_t tmpReg3 = X86_REG_AX;
                        
                        InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_trampinline, prot, InstLocation_prior);
                        pt->setPriority(InstPriority_low);
                        Vector<InstrucX86*>* bufferDumpInstructions = new Vector<InstrucX86*>();
                        
                        // save temp regs
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t))));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t))));
                        
                        // put the memory address in tmp1
                        Vector<InstrucX86*>* addrStore = InstrucX86Generator64::generateAddressComputation(memop, tmpReg1);
                        while (!(*addrStore).empty()){
                            (*bufferDumpInstructions).append((*addrStore).remove(0));
                        }
                        delete addrStore;
                        
                        // put the current buffer address in tmp2
                        // replace this with a 4-wide move
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2, false));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                        
                        // fill the buffer entry with this block's info
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveImmToRegaddrImm(memopId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
                        // update the buffer counter
                        uint32_t maxMemopsInSuccessor = MAX_MEMOPS_PER_BLOCK;
                        ASSERT(bb->getNumberOfMemoryOps() < MAX_MEMOPS_PER_BLOCK);
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateAddImmByteToMem(bb->getNumberOfMemoryOps(), getInstDataAddress() + bufferStore));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg1, false));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateCompareImmReg(BUFFER_ENTRIES - maxMemopsInSuccessor, tmpReg1));
                        
                        // restore regs
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t)), tmpReg2, true));
                        (*bufferDumpInstructions).append(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t)), tmpReg1, true));
                        
                        // jump to non-buffer-jump code
                        (*bufferDumpInstructions).append(InstrucX86Generator::generateBranchJL(Size__64_bit_inst_function_call_support));
                        
                        ASSERT(bufferDumpInstructions);
                        while ((*bufferDumpInstructions).size()){
                            pt->addPrecursorInstruction((*bufferDumpInstructions).remove(0));
                        }
                        delete bufferDumpInstructions;
                        memInstPoints.append(pt);
                    } else {
                        // TODO: get which gprs are dead at this point and use one of those 
                        InstrumentationSnippet* snip = new InstrumentationSnippet();
                        addInstrumentationSnippet(snip);
                        
                        uint32_t tmpReg1 = X86_REG_CX;
                        uint32_t tmpReg2 = X86_REG_DX;
                        bool usesLiveReg = true;
                        ASSERT(tmpReg1 < X86_64BIT_GPRS);
                        
                        // save 2 temp regs
                        if (usesLiveReg){
                            snip->addSnippetInstruction(InstrucX86Generator64::generateMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t)));
                            snip->addSnippetInstruction(InstrucX86Generator64::generateMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t)));
                        }
                        
                        // put the memory address in tmp1
                        Vector<InstrucX86*>* addrStore = InstrucX86Generator64::generateAddressComputation(memop, tmpReg1);
                        while (!(*addrStore).empty()){
                            snip->addSnippetInstruction((*addrStore).remove(0));
                        }
                        delete addrStore;
                        
                        // put the current buffer address in tmp2
                        snip->addSnippetInstruction(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2, false));
                        snip->addSnippetInstruction(InstrucX86Generator64::generateLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                        snip->addSnippetInstruction(InstrucX86Generator64::generateLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                        
                        // fill the buffer entry with this block's info
                        snip->addSnippetInstruction(InstrucX86Generator64::generateMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                        snip->addSnippetInstruction(InstrucX86Generator64::generateMoveImmToRegaddrImm(memopId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                        snip->addSnippetInstruction(InstrucX86Generator64::generateMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
                        if (usesLiveReg){
                            snip->addSnippetInstruction(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t), tmpReg2, true));
                            snip->addSnippetInstruction(InstrucX86Generator64::generateMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t), tmpReg1, true));
                        }
                        // generateAddressComputation is guaranteed to use instructions that dont def any flags, so no protection is necessary
                        InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none, InstLocation_prior);
                        memInstPoints.append(pt);
                    }
                    memInstBlockIds.append(blockId);
                    memopId++;
                    memopIdInBlock++;
                }
            }
            ASSERT(memopIdInBlock < MAX_MEMOPS_PER_BLOCK);

        }
        blockId++;

    }

    ASSERT(memInstPoints.size() && "There are no memory operations found through the filter");

    instPointInfo = reserveDataOffset(sizeof(instpoint_info) * memInstPoints.size());
    entryFunc->addArgument(instPointInfo);

    uint64_t instPointCount = reserveDataOffset(sizeof(uint32_t));
    uint64_t blockCount = reserveDataOffset(sizeof(uint32_t));
    temp32 = memInstPoints.size();
    initializeReservedData(getInstDataAddress() + instPointCount, sizeof(uint32_t), &temp32);
    temp32 = blockId;
    initializeReservedData(getInstDataAddress() + blockCount, sizeof(uint32_t), &temp32);
    entryFunc->addArgument(instPointCount);
    entryFunc->addArgument(blockCount);

#ifdef NO_REG_ANALYSIS
    PRINT_WARN(10, "Warning: register analysis disabled");
#endif
    PRINT_INFOR("Not protecting %d/%d dump points", noProtPoints, totalProt);
    PRINT_INFOR("No live scratch reg at %d/%d instrumentation points", regDefault, getNumberOfExposedMemOps());

    printStaticFile(allBlocks, allLineInfos);

    delete allBlocks;
    delete allLineInfos;
    delete[] comment;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

