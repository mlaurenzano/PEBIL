#include <CacheSimulation.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>
#include <DFPattern.h>
#include <SimpleHash.h>

#define ENTRY_FUNCTION "entry_function"
#define SIM_FUNCTION "MetaSim_simulFuncCall_Simu"
#define EXIT_FUNCTION "MetaSim_endFuncCall_Simu"
#define INST_LIB_NAME "libsimulator.so"
#define BUFFER_ENTRIES 0x00010000
#define Size__BufferEntry 16
#define MAX_MEMOPS_PER_BLOCK 2048

//#define DISABLE_BLOCK_COUNT

void CacheSimulation::usesModifiedProgram(){
    X86Instruction* nop5Byte = X86InstructionFactory::emitNop(Size__uncond_jump);
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

DFPatternType convertDFPattenType(char* patternString){
    if(!strcmp(patternString,"dfTypePattern_Gather")){
        return dfTypePattern_Gather;
    } else if(!strcmp(patternString,"dfTypePattern_Scatter")){
        return dfTypePattern_Scatter;
    } else if(!strcmp(patternString,"dfTypePattern_FunctionCallGS")){
        return dfTypePattern_FunctionCallGS;
    }
    return dfTypePattern_undefined;
}

void CacheSimulation::filterBBs(){
    Vector<char*>* fileLines = new Vector<char*>();
    initializeFileList(bbFile, fileLines);

    int32_t err;
    uint64_t blockHash;
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        char* ptr = strchr((*fileLines)[i],'#');
        if(ptr) *ptr = '\0';

        if(!strlen((*fileLines)[i]) || allSpace((*fileLines)[i]))
            continue;

        err = sscanf((*fileLines)[i], "%lld", &blockHash);
        if(err <= 0){
            PRINT_ERROR("Line %d of %s has a wrong format", i+1, bbFile);
        }
        HashCode* hashCode = new HashCode(blockHash);
        if(!hashCode->isBlock()){
            PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block", i+1, bbFile);
        }
        BasicBlock* bb = findExposedBasicBlock(*hashCode);
        delete hashCode;

        ASSERT(bb && "cannot find basic block for hash code found in input file");
        blocksToInst.insert(blockHash, bb);
    }
    for (uint32_t i = 0; i < (*fileLines).size(); i++){
        delete[] (*fileLines)[i];
    }
    delete fileLines;

    if (!blocksToInst.size()){
        for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
            BasicBlock* bb = getExposedBasicBlock(i);
            blocksToInst.insert(bb->getHashCode().getValue(), bb);
        }
    }

    // if any loop contains blocks that are in our list, include all blocks from those loops
    if (loopIncl){
        for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
            BasicBlock* bb = getExposedBasicBlock(i);
            uint64_t hashValue = bb->getHashCode().getValue();

            if (blocksToInst.get(hashValue)){
                if (bb->isInLoop()){
                    FlowGraph* fg = bb->getFlowGraph();
                    for (uint32_t j = 0; j < fg->getNumberOfLoops(); j++){
                        Loop* lp = fg->getLoop(j);
                        if (lp->isBlockIn(bb->getIndex())){
                            BasicBlock** allBlocks = new BasicBlock*[lp->getNumberOfBlocks()];
                            lp->getAllBlocks(allBlocks);
                            for (uint32_t k = 0; k < lp->getNumberOfBlocks(); k++){
                                uint64_t code = allBlocks[k]->getHashCode().getValue();
                                blocksToInst.insert(code, allBlocks[k]);
                            }
                            delete[] allBlocks;
                        }
                    }
                }
            }
        }
    }

    BasicBlock** bbs = blocksToInst.values();
    qsort(bbs, blocksToInst.size(), sizeof(BasicBlock*), compareBaseAddress);
    
    if (dfPatternFile){

        Vector<char*>* dfpFileLines = new Vector<char*>();
        initializeFileList(dfPatternFile, dfpFileLines);

        ASSERT(!dfpSet.size());

        for (uint32_t i = 0; i < dfpFileLines->size(); i++){
            PRINT_INFOR("dfp line %d: %s", i, (*dfpFileLines)[i]);

            uint64_t id = 0;
            char patternString[__MAX_STRING_SIZE];
            int32_t err = sscanf((*dfpFileLines)[i], "%lld %s", &id, patternString);
            if(err <= 0){
                PRINT_ERROR("Line %d of %s has a wrong format", i+1, dfPatternFile);
            }
            DFPatternType dfpType = convertDFPattenType(patternString);
            if(dfpType == dfTypePattern_undefined){
                PRINT_ERROR("Line %d of %s is a wrong pattern type [%s]", i+1, dfPatternFile, patternString);
            } else {
                PRINT_INFOR("found valid pattern %s -> %d", patternString, dfpType);
            }
            HashCode hashCode(id);
            if(!hashCode.isBlock()){
                PRINT_ERROR("Line %d of %s is a wrong unique id for a basic block", i+1, dfPatternFile);
            }

            // if the bb is not in the list already but is a valid block, include it!
            BasicBlock* bb = findExposedBasicBlock(hashCode);
            if(!bb || bb->getHashCode().getValue() != id){
                PRINT_ERROR("Line %d of %s is not a valid basic block id", i+1, dfPatternFile);
                continue;
            }
            blocksToInst.insert(hashCode.getValue(), bb);

            if (dfpType != dfTypePattern_None){
                dfpSet.insert(hashCode.getValue(), dfpType);
            }
        }

        for (uint32_t i = 0; i < dfpFileLines->size(); i++){
            delete[] (*dfpFileLines)[i];
        }

        delete dfpFileLines;

        PRINT_INFOR("**** Number of basic blocks tagged for DFPattern %d (out of %d) ******",
                    dfpSet.size(), blocksToInst.size());
    }

    ASSERT(!dfpBlocks.size() || dfpBlocks.size() == blocksToInst.size());
    delete[] bbs;
}


CacheSimulation::CacheSimulation(ElfFile* elf, char* inputFile, char* ext, uint32_t phase, bool lpi, bool dtl, char* dfpFile)
    : InstrumentationTool(elf, ext, phase, lpi, dtl)
{
    simFunc = NULL;
    exitFunc = NULL;
    entryFunc = NULL;

    bbFile = inputFile;
    dfPatternFile = dfpFile;
}


CacheSimulation::~CacheSimulation(){
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
    filterBBs();

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

    uint64_t dfPatternStore = reserveDataOffset(sizeof(DFPatternSpec) * (getNumberOfExposedBasicBlocks() + 1));
    if(dfpSet.size()){
        DFPatternSpec dfInfo;
        dfInfo.memopCnt = 0;
        dfInfo.type = DFPattern_Active;

        initializeReservedData(getInstDataAddress() + dfPatternStore, sizeof(DFPatternSpec), (void*)&dfInfo);
    }

    uint64_t entryCountStore = reserveDataOffset(sizeof(uint64_t));
    uint32_t startValue = BUFFER_ENTRIES;

    initializeReservedData(getInstDataAddress() + entryCountStore, sizeof(uint64_t), &startValue);

    uint64_t blockSizeStore = reserveDataOffset(sizeof(uint64_t));

    char* appName = getElfFile()->getAppName();
    char* ext = extension;
    uint32_t phaseId = phaseNo;
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

    uint64_t counterArray = reserveDataOffset(getNumberOfExposedBasicBlocks() * sizeof(uint64_t));

    InstrumentationPoint* p = addInstrumentationPoint(getProgramExitBlock(), exitFunc, InstrumentationMode_tramp);
    ASSERT(p);
    p->setPriority(InstPriority_userinit);
    if (!p->getInstBaseAddress()){
        PRINT_ERROR("Cannot find an instrumentation point at the exit function");
    }

    p = addInstrumentationPoint(getProgramEntryBlock(), entryFunc, InstrumentationMode_tramp);
    ASSERT(p);

    Vector<BasicBlock*>* allBlocks = new Vector<BasicBlock*>();
    Vector<uint32_t>* allBlockIds = new Vector<uint32_t>();
    Vector<LineInfo*>* allLineInfos = new Vector<LineInfo*>();

    uint32_t blockId = 0;
    uint32_t memopId = 0;
    uint32_t regDefault = 0;

    for (uint32_t i = 0; i < getNumberOfExposedBasicBlocks(); i++){
        BasicBlock* bb = getExposedBasicBlock(i);

        DFPatternType dfpType = dfTypePattern_None;
        DFPatternSpec spec;
        if (dfpSet.get(bb->getHashCode().getValue(), &dfpType)){
            PRINT_INFOR("found dfpattern for block %d (hash %#lld)", i, bb->getHashCode().getValue());
        }
        spec.type = dfpType;
        spec.memopCnt = bb->getNumberOfMemoryOps();
        initializeReservedData(getInstDataAddress() + dfPatternStore + (i+1)*sizeof(DFPatternSpec), sizeof(DFPatternSpec), &spec);

        if (blocksToInst.get(bb->getHashCode().getValue())){

            (*allBlocks).append(bb);
            (*allBlockIds).append(blockId);
            if (lineInfoFinder){
                (*allLineInfos).append(lineInfoFinder->lookupLineInfo(bb));
            } else {
                (*allLineInfos).append(NULL);
            }
            uint32_t memopIdInBlock = 0;
            
            for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
                X86Instruction* memop = bb->getInstruction(j);
                
                if (memop->isMemoryOperation()){            
                    //PRINT_INFOR("The following instruction has %d membytes", memop->getNumberOfMemoryBytes());
                    //memop->print();
                    
                    if (getElfFile()->is64Bit()){
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
                            
                            // generateAddressComputation doesn't def any flags, so no protection is necessary
                            InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none, InstLocation_prior);
                            memInstPoints.append(pt);
                        } 
                    } else {
                        
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
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t))));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t))));
                            
                            // put the memory address in tmp1
                            Vector<X86Instruction*>* addrStore = X86InstructionFactory32::emitAddressComputation(memop, tmpReg1);
                            while (!(*addrStore).empty()){
                                (*bufferDumpInstructions).append((*addrStore).remove(0));
                            }
                            delete addrStore;
                            
                            // put the current buffer address in tmp2
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                            
                            // fill the buffer entry with this block's info
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
                            // update the buffer counter
                            uint32_t maxMemopsInSuccessor = MAX_MEMOPS_PER_BLOCK;
                            ASSERT(bb->getNumberOfMemoryOps() < MAX_MEMOPS_PER_BLOCK);
                            uint32_t memcnt = bb->getNumberOfMemoryOps();
                            while (memcnt > 0x7f){
                                (*bufferDumpInstructions).append(X86InstructionFactory32::emitAddImmByteToMem(0x7f, getInstDataAddress() + bufferStore));
                                memcnt -= 0x7f;
                            }
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitAddImmByteToMem(memcnt, getInstDataAddress() + bufferStore));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg1));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitCompareImmReg(BUFFER_ENTRIES - maxMemopsInSuccessor, tmpReg1));
                            
                            // restore regs
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*(sizeof(uint64_t)), tmpReg2));
                            (*bufferDumpInstructions).append(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*(sizeof(uint64_t)), tmpReg1));
                            
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
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToMem(tmpReg1, getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t)));
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToMem(tmpReg2, getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t)));
                            }
                            
                            // put the memory address in tmp1
                            Vector<X86Instruction*>* addrStore = X86InstructionFactory32::emitAddressComputation(memop, tmpReg1);
                            while (!(*addrStore).empty()){
                                snip->addSnippetInstruction((*addrStore).remove(0));
                            }
                            delete addrStore;
                            
                            // put the current buffer address in tmp2
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + bufferStore, tmpReg2));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, 0, tmpReg2, false, true));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitLoadEffectiveAddress(0, tmpReg2, 4, getInstDataAddress() + bufferStore, tmpReg2, false, true));
                            
                            // fill the buffer entry with this block's info
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveRegToRegaddrImm(tmpReg1, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 8));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveImmToRegaddrImm(memopIdInBlock, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 4));
                            snip->addSnippetInstruction(X86InstructionFactory32::emitMoveImmToRegaddrImm(blockId, tmpReg2, (memopIdInBlock * Size__BufferEntry) + 0));
                            if (usesLiveReg){
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 2*sizeof(uint64_t), tmpReg2));
                                snip->addSnippetInstruction(X86InstructionFactory32::emitMoveMemToReg(getInstDataAddress() + getRegStorageOffset() + 1*sizeof(uint64_t), tmpReg1));
                            }
                            
                            // generateAddressComputation doesn't def any flags, so no protection is necessary
                            InstrumentationPoint* pt = addInstrumentationPoint(memop, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none, InstLocation_prior);
                            memInstPoints.append(pt);
                        }
                    }
                    memInstBlockIds.append(blockId);
                    memopId++;
                    memopIdInBlock++;
                }
                ASSERT(memopIdInBlock < MAX_MEMOPS_PER_BLOCK && "Too many memory ops in some basic block... try increasing MAX_MEMOPS_PER_BLOCK");
            }

        }
#ifndef DISABLE_BLOCK_COUNT
        InstrumentationSnippet* snip = new InstrumentationSnippet();
        addInstrumentationSnippet(snip);
        
        uint64_t counterOffset = counterArray + (i * sizeof(uint64_t));
        ASSERT(i == blockId);
        if (is64Bit()){
            snip->addSnippetInstruction(X86InstructionFactory64::emitAddImmByteToMem64(1, getInstDataAddress() + counterOffset));
        } else {
            snip->addSnippetInstruction(X86InstructionFactory32::emitAddImmByteToMem(1, getInstDataAddress() + counterOffset));
        }
        
        FlagsProtectionMethods prot = FlagsProtectionMethod_light;
        X86Instruction* bestinst = bb->getExitInstruction();
        for (int32_t j = bb->getNumberOfInstructions() - 1; j >= 0; j--){
            if (bb->getInstruction(j)->allFlagsDeadIn()){
                bestinst = bb->getInstruction(j);
                prot = FlagsProtectionMethod_none;
                break;
            }
        }
        
        InstrumentationPoint* p = addInstrumentationPoint(bestinst, snip, InstrumentationMode_inline, prot, InstLocation_prior);
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

    printStaticFile(allBlocks, allBlockIds, allLineInfos, BUFFER_ENTRIES);

    delete allBlocks;
    delete allBlockIds;
    delete allLineInfos;
    delete[] comment;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}

