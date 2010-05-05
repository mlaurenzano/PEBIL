#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
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

//#define DISABLE_BLOCK_COUNT

void CacheSimulation::usesModifiedProgram(){
    X86Instruction* nop5Byte = X86InstructionFactory::emitNop(5);
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
    InstrumentationTool::declare();
    
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
    InstrumentationTool::instrument();

    uint32_t temp32;
    
    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    ASSERT(isPowerOfTwo(Size__BufferEntry));
    uint64_t bufferStore  = reserveDataOffset(BUFFER_ENTRIES * Size__BufferEntry);
    char* emptyBuff = new char[BUFFER_ENTRIES * Size__BufferEntry];
    bzero(emptyBuff, BUFFER_ENTRIES * Size__BufferEntry);
    initializeReservedData(getInstDataAddress() + bufferStore, BUFFER_ENTRIES * Size__BufferEntry, emptyBuff);
    delete[] emptyBuff;

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

    uint64_t counterArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(uint32_t));

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
    uint32_t regDefault = 0;

    if (!getElfFile()->is64Bit()){
        __FUNCTION_NOT_IMPLEMENTED;
    }

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);

        uint64_t hashValue = bb->getHashCode().getValue();
        void* bfound = bsearch(&hashValue, &blocksToInst, blocksToInst.size(), sizeof(HashCode*), searchHashCode);

        if (bfound || !blocksToInst.size()){
            (*allBlocks).append(bb);
            if (lineInfoFinder){
                (*allLineInfos).append(lineInfoFinder->lookupLineInfo(bb));
            } else {
                (*allLineInfos).append(NULL);
            }
            uint32_t memopIdInBlock = 0;
            
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);
                
                if (memop->isMemoryOperation()){            
                    
                    // check the buffer at the last memop
                    if (memopIdInBlock == bb->getNumberOfMemoryOps() - 1){
                        FlagsProtectionMethods prot = FlagsProtectionMethod_full;
#ifndef NO_REG_ANALYSIS
                        if (memop->allFlagsDeadIn()){
                            prot = FlagsProtectionMethod_none;
                        }
#endif
                        uint32_t tmpReg1 = X86_REG_CX;
                        uint32_t tmpReg2 = X86_REG_DX;
                        uint32_t tmpReg3 = X86_REG_AX;
                        
                        InstrumentationPoint* pt = addInstrumentationPoint(memop, simFunc, InstrumentationMode_trampinline, prot, InstLocation_prior);
                        pt->setPriority(InstPriority_low);
                        Vector<X86Instruction*>* bufferDumpInstructions = new Vector<X86Instruction*>();
                        
                        // save temp regs
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t))));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t))));
                        
                        // put the memory address in tmp1
                        Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, tmpReg1);
                        while (!(*addrStore).empty()){
                            (*bufferDumpInstructions).append((*addrStore).remove(0));
                        }
                        delete addrStore;
                        
                        // put the current buffer address in tmp2
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2, false));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                        
                        // fill the buffer entry with this block's info
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
                        // update the buffer counter
                        uint32_t maxMemopsInSuccessor = MAX_MEMOPS_PER_BLOCK;
                        ASSERT(bb->getNumberOfMemoryOps() < MAX_MEMOPS_PER_BLOCK);
                        uint32_t memcnt = bb->getNumberOfMemoryOps();
                        while (memcnt > 0x7f){
                            (*bufferDumpInstructions).append(X86InstructionFactory64::emitAddImmByteToMem(0x7f, getInstDataAddress() + bufferStore));
                            memcnt -= 0x7f;
                        }
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitAddImmByteToMem(memcnt, getInstDataAddress() + bufferStore));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg1, false));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitCompareImmReg(BUFFER_ENTRIES - maxMemopsInSuccessor, tmpReg1));
                        
                        // restore regs
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t)), tmpReg2, true));
                        (*bufferDumpInstructions).append(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t)), tmpReg1, true));
                        
                        // jump to non-buffer-jump code
                        (*bufferDumpInstructions).append(X86InstructionFactory::emitBranchJL(Size__64_bit_inst_function_call_support));
                        
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
                            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t)));
                            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t)));
                        }
                        
                        // put the memory address in tmp1
                        Vector<X86Instruction*>* addrStore = X86InstructionFactory64::emitAddressComputation(memop, tmpReg1);
                        while (!(*addrStore).empty()){
                            snip->addSnippetInstruction((*addrStore).remove(0));
                        }
                        delete addrStore;
                        
                        // put the current buffer address in tmp2
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2, false));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                        
                        // fill the buffer entry with this block's info
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8, true));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                        snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
                        if (usesLiveReg){
                            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t), tmpReg2, true));
                            snip->addSnippetInstruction(X86InstructionFactory64::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t), tmpReg1, true));
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

#ifndef DISABLE_BLOCK_COUNT
        InstrumentationSnippet* snip = new InstrumentationSnippet();
        addInstrumentationSnippet(snip);
        
        uint64_t counterOffset = counterArray + (i * sizeof(uint32_t));
        ASSERT(i == blockId);
        if (is64Bit()){
            snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        } else {
            snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        }
        FlagsProtectionMethods prot = FlagsProtectionMethod_light;
        if (bb->getLeader()->allFlagsDeadIn()){
            prot = FlagsProtectionMethod_none;
        }
        InstrumentationPoint* p = addInstrumentationPoint(bb->getLeader(), snip, InstrumentationMode_inline, prot, InstLocation_prior);
#endif
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
    entryFunc->addArgument(counterArray);

#ifdef NO_REG_ANALYSIS
    PRINT_WARN(10, "Warning: register analysis disabled");
#endif

    printStaticFile(allBlocks, allLineInfos);

    delete allBlocks;
    delete allLineInfos;
    delete[] comment;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

