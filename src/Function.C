#include <Function.h>

#include <BasicBlock.h>
#include <BinaryFile.h>
#include <Disassembler.h>
#include <ElfFile.h>
#include <ElfFileInst.h>
#include <FlowGraph.h>
#include <Instruction.h>
#include <LengauerTarjan.h>
#include <SectionHeader.h>
#include <Stack.h>
#include <SymbolTable.h>
#include <TextSection.h>

bool Function::hasLeafOptimization(){
    uint32_t numberOfInstructions = getNumberOfInstructions();
    Instruction** allInstructions = new Instruction*[numberOfInstructions];
    getAllInstructions(allInstructions,0);

    bool callFound = false;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (allInstructions[i]->isFunctionCall()){
            callFound = true;
        }
    }
    delete[] allInstructions;
    return !callFound;
}

void Function::printDisassembly(bool instructionDetail){
    fprintf(stdout, "%llx <func -- %s>:", getBaseAddress(), getName());
    if (getBadInstruction()){
        fprintf(stdout, "badi@ %llx", getBadInstruction());
    }
    fprintf(stdout, "\n");
    if (flowGraph){
        for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
            flowGraph->getBlock(i)->printDisassembly(instructionDetail);
        }
    }
}

uint32_t Function::bloatBasicBlocks(uint32_t minBlockSize){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        Block* block = flowGraph->getBlock(i);
        block->setBaseAddress(baseAddress+currByte);
        if (block->getType() == ElfClassTypes_BasicBlock){
            ((BasicBlock*)block)->bloat(minBlockSize);
        } 
        block->setBaseAddress(baseAddress+currByte);
        currByte += block->getNumberOfBytes();
    }
    sizeInBytes = currByte;
    return sizeInBytes;
}

bool Function::hasCompleteDisassembly(){
    // if something happened during disassembly that we dont understand
    if (getBadInstruction()){
        return false;
    }

    // if this function calls into the middle of itsself
    if (containsCallToRange(baseAddress+1,baseAddress+getNumberOfBytes())){
        return false;
    }

    // if this function references data that is inside the function body
    uint32_t numberOfInstructions = getNumberOfInstructions();
    Instruction** allInstructions = new Instruction*[numberOfInstructions];
    getAllInstructions(allInstructions,0);
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (allInstructions[i]->usesRelativeAddress() &&
            !allInstructions[i]->isControl() &&
            inRange(allInstructions[i]->getBaseAddress() + allInstructions[i]->getRelativeValue())){
            PRINT_DEBUG_FUNC_RELOC("Instruction self-data-ref inside function %s", getName());
#ifdef DEBUG_FUNC_RELOC
            allInstructions[i]->print();
#endif
            delete[] allInstructions;
            return false;
        }
    }
    delete[] allInstructions;

    // if this function calls __i686.get_pc_thunk.bx
    for (uint32_t i = 0; i < textSection->getNumberOfTextObjects(); i++){
        TextObject* tobj = textSection->getTextObject(i);
        if (tobj->getType() == ElfClassTypes_Function){
            Function* func = (Function*)tobj;
            if (!strcmp(func->getName(),"__i686.get_pc_thunk.bx")){
                if (containsCallToRange(func->getBaseAddress(),func->getBaseAddress()+func->getSizeInBytes())){
                    return false;
                }
            }
        }
    }
    return true;
}

bool Function::containsCallToRange(uint64_t lowAddr, uint64_t highAddr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->containsCallToRange(lowAddr,highAddr)){
            return true;
        }
    }
    return false;
}

uint32_t Function::getAllInstructions(Instruction** allinsts, uint32_t nexti){
    uint32_t instructionCount = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        instructionCount += flowGraph->getBasicBlock(i)->getAllInstructions(allinsts, instructionCount+nexti);
    }
    ASSERT(instructionCount == getNumberOfInstructions());
    return instructionCount;
}

Vector<Instruction*>* Function::swapInstructions(uint64_t addr, Vector<Instruction*>* replacements){
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        if (getBasicBlock(i)->inRange(addr)){
            return getBasicBlock(i)->swapInstructions(addr,replacements);
        }
    }
    PRINT_ERROR("Cannot find instructions at address 0x%llx to replace", addr);
    return 0;
}

void Function::setBaseAddress(uint64_t newBaseAddr){
    baseAddress = newBaseAddr;
    flowGraph->setBaseAddress(newBaseAddr);
}

uint32_t Function::getNumberOfBytes(){
    ASSERT(flowGraph);
    return flowGraph->getNumberOfBytes();
}

uint32_t Function::getNumberOfInstructions() { 
    ASSERT(flowGraph);
    return flowGraph->getNumberOfInstructions(); 
}


uint32_t Function::getNumberOfBasicBlocks() { 
    ASSERT(flowGraph);
    return flowGraph->getNumberOfBasicBlocks(); 
}


BasicBlock* Function::getBasicBlock(uint32_t idx){
    ASSERT(flowGraph);
    return flowGraph->getBasicBlock(idx);
}

void Function::printInstructions(){
    __FUNCTION_NOT_IMPLEMENTED;
}

void Function::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        flowGraph->getBlock(i)->dump(binaryOutputFile,offset+currByte);
        PRINT_DEBUG_CFG("\tDumping block for function %s at %#llx for %d bytes", getName(), flowGraph->getBlock(i)->getBaseAddress(), flowGraph->getBlock(i)->getNumberOfBytes());
        currByte += flowGraph->getBlock(i)->getNumberOfBytes();
    }
    if (currByte != sizeInBytes){
        PRINT_ERROR("Function %s dumped %d bytes and has %d bytes", getName(), currByte, sizeInBytes);
    }
    ASSERT(currByte == sizeInBytes);
}

uint32_t Function::digest(){
    Vector<Instruction*>* allInstructions;

    // try to use a recursive algorithm
    allInstructions = digestRecursive();

    // use a linear algorithm if recursive failed
    if (!allInstructions){
        ASSERT(getBadInstruction());
        allInstructions = digestLinear();
    } else {
        setRecursiveDisasm();
    }

    ASSERT(allInstructions);
    generateCFG(allInstructions);
    delete allInstructions;

    return sizeInBytes;
}

Vector<Instruction*>* Function::digestRecursive(){
    Vector<Instruction*>* allInstructions = new Vector<Instruction*>();
    Instruction* currentInstruction;
    uint64_t currentAddress = 0;

    PRINT_DEBUG_CFG("Recursively digesting function %s at [%#llx,%#llx)", getName(), getBaseAddress(), getBaseAddress()+getSizeInBytes());

    Stack<uint64_t> unprocessed = Stack<uint64_t>(sizeInBytes);

    // put the entry point onto stack
    unprocessed.push(baseAddress);

    // build an array of all reachable instructions
    while (!unprocessed.empty() && !getBadInstruction()){
        currentAddress = unprocessed.pop();

        void* inst = bsearch(&currentAddress,&(*allInstructions),(*allInstructions).size(),sizeof(Instruction*),searchBaseAddress);
        if (inst){
            Instruction* tgtInstruction = *(Instruction**)inst;
            ASSERT(tgtInstruction->getBaseAddress() == currentAddress && "Problem in disassembly -- found instruction that enters the middle of another instruction");
            continue;
        }
        if (!inRange(currentAddress)){
            PRINT_WARN(4,"In function %s, control flows out of function bound (%#llx)", getName(), currentAddress);
            continue;
        }

        currentInstruction = new Instruction(textSection, currentAddress,
                                             textSection->getStreamAtAddress(currentAddress), ByteSource_Application_Function, 0);
        PRINT_DEBUG_CFG("recursive cfg: address %#llx with %d bytes", currentAddress, currentInstruction->getSizeInBytes());
        uint64_t checkAddr = currentInstruction->getBaseAddress();

        if (currentInstruction->getInstructionType() == x86_insn_type_bad){
            setBadInstruction(currentInstruction->getBaseAddress());
        }

        (*allInstructions).insertSorted(currentInstruction,compareBaseAddress);

        // make sure the targets of this branch have not been processed yet
        uint64_t fallThroughAddr = currentInstruction->getBaseAddress()+currentInstruction->getSizeInBytes();
        PRINT_DEBUG_CFG("\tChecking FTaddr %#llx", fallThroughAddr);
        void* fallThrough = bsearch(&fallThroughAddr,&(*allInstructions),(*allInstructions).size(),sizeof(Instruction*),searchBaseAddress);
        if (!fallThrough){
            if (currentInstruction->controlFallsThrough()){
                PRINT_DEBUG_CFG("\t\tpushing %#llx", fallThroughAddr);
                unprocessed.push(fallThroughAddr);
            } else {
                fallThroughAddr = 0;
            }
        } else {
            Instruction* tgtInstruction = *(Instruction**)fallThrough;
            if (tgtInstruction->getBaseAddress() != fallThroughAddr){
                setBadInstruction(fallThroughAddr);
            }
        }
        
        Vector<uint64_t>* controlTargetAddrs = new Vector<uint64_t>();
        if (currentInstruction->isJumpTableBase()){
            uint64_t jumpTableBase = currentInstruction->findJumpTableBaseAddress(allInstructions);
            if (inRange(jumpTableBase) || !jumpTableBase){
                setBadInstruction(currentInstruction->getBaseAddress());
            } else {
                ASSERT(!(*controlTargetAddrs).size());
                currentInstruction->computeJumpTableTargets(jumpTableBase, this, controlTargetAddrs);
                PRINT_DEBUG_CFG("Jump table targets (%d):", (*controlTargetAddrs).size());
            }
        } else if (currentInstruction->usesControlTarget()){
            (*controlTargetAddrs).append(currentInstruction->getTargetAddress());
        }

        for (uint32_t i = 0; i < (*controlTargetAddrs).size(); i++){
            uint64_t controlTargetAddr = (*controlTargetAddrs)[i];
            PRINT_DEBUG_CFG("\tChecking CTaddr %#llx", controlTargetAddr);
            void* controlTarget = bsearch(&controlTargetAddr,&(*allInstructions),(*allInstructions).size(),sizeof(Instruction*),searchBaseAddress);
            if (!controlTarget){
                if (inRange(controlTargetAddr)                 // target address is in this functions also
                    && controlTargetAddr != fallThroughAddr){  // target and fall through are the same, meaning the address is already pushed above
                    PRINT_DEBUG_CFG("\t\tpushing %#llx", controlTargetAddr);
                    unprocessed.push(controlTargetAddr);
                }
            } else {
                Instruction* tgtInstruction = *(Instruction**)controlTarget;
                if (tgtInstruction->getBaseAddress() != controlTargetAddr){
                    setBadInstruction(controlTargetAddr);
                }              
            }
        }
        delete controlTargetAddrs;
    }

    if (getBadInstruction()){
        for (uint32_t i = 0; i < (*allInstructions).size(); i++){
            delete (*allInstructions)[i];
        }
        delete allInstructions;
        return NULL;
    }

    return allInstructions;
}

uint32_t Function::generateCFG(Vector<Instruction*>* instructions){
    BasicBlock* currentBlock = NULL;
    BasicBlock* entryBlock = NULL;
    uint32_t numberOfBasicBlocks = 0;

    ASSERT((*instructions).size());

    flowGraph = new FlowGraph(this);

    PRINT_DEBUG_CFG("Building CFG for function %s -- have %d instructions", getName(), (*instructions).size());

    // find the block leaders
    (*instructions)[0]->setLeader(true);
    Vector<uint64_t> leaderAddrs;

    for (uint32_t i = 0; i < (*instructions).size(); i++){
        Vector<uint64_t>* controlTargetAddrs = new Vector<uint64_t>();
        if ((*instructions)[i]->isJumpTableBase()){
            setJumpTable();
            uint64_t jumpTableBase = (*instructions)[i]->findJumpTableBaseAddress(instructions);
            if (!jumpTableBase){
                PRINT_WARN(6,"Cannot determine indirect jump target for instruction at %#llx", (*instructions)[i]->getBaseAddress());
                ASSERT(getBadInstruction());
            } else if (inRange(jumpTableBase)){
                ASSERT(getBadInstruction());
            } else {
                ASSERT(!(*controlTargetAddrs).size());
                (*instructions)[i]->computeJumpTableTargets(jumpTableBase, this, controlTargetAddrs);
                PRINT_DEBUG_CFG("Jump table targets (%d):", (*controlTargetAddrs).size());
            }
            (*controlTargetAddrs).append((*instructions)[i]->getBaseAddress() + (*instructions)[i]->getSizeInBytes());
        } else if ((*instructions)[i]->usesControlTarget()){
            (*controlTargetAddrs).append((*instructions)[i]->getTargetAddress());
            (*controlTargetAddrs).append((*instructions)[i]->getBaseAddress() + (*instructions)[i]->getSizeInBytes());
        } else if ((*instructions)[i]->isReturn()){
            (*controlTargetAddrs).append((*instructions)[i]->getBaseAddress() + (*instructions)[i]->getSizeInBytes());
        }
        for (uint32_t j = 0; j < (*controlTargetAddrs).size(); j++){
            leaderAddrs.append((*controlTargetAddrs)[j]);
        }
        delete controlTargetAddrs;
    }

    for (uint32_t i = 0; i < leaderAddrs.size(); i++){
        uint64_t tgtAddr = leaderAddrs[i];
        void* inst = bsearch(&tgtAddr,&(*instructions),(*instructions).size(),sizeof(Instruction*),searchBaseAddress);
        if (inst){
            Instruction* tgtInstruction = *(Instruction**)inst;
            if (!getBadInstruction()){
                if (tgtInstruction->getBaseAddress() != tgtAddr){
                    PRINT_ERROR("found instruction that enters the middle of another instruction -- function %s instruction %#llx", getName(), tgtAddr);
                }
            }
            tgtInstruction->setLeader(true);
        }
    }

    for (uint32_t i = 0; i < (*instructions).size(); i++){
        if ((*instructions)[i]->isLeader()){
            currentBlock = new BasicBlock(numberOfBasicBlocks++,flowGraph);
            currentBlock->setBaseAddress((*instructions)[i]->getBaseAddress());
            if (!flowGraph->getNumberOfBlocks()){
                entryBlock = currentBlock;
            }
            flowGraph->addBlock(currentBlock);
            PRINT_DEBUG_CFG("Starting new Block");
        }
        PRINT_DEBUG_CFG("\tAdding instruction %#llx with %d bytes", (*instructions)[i]->getBaseAddress(), (*instructions)[i]->getSizeInBytes());
        currentBlock->addInstruction((*instructions)[i]);
    }

#ifdef DEBUG_CFG
    for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
        PRINT_INFOR("Have a block at adr %#llx", flowGraph->getBlock(i)->getBaseAddress());
    }
#endif

    flowGraph->connectGraph(entryBlock);
    flowGraph->setImmDominatorBlocks();

#ifdef DEBUG_CFG
    print();
#endif

    ASSERT(flowGraph->getNumberOfBlocks() == flowGraph->getNumberOfBasicBlocks());
    ASSERT(flowGraph->getNumberOfBlocks());

    // now find any areas not covered by basic blocks and put them into RawBlocks
    Vector<RawBlock*> unknownBlocks;
    if (flowGraph->getBlock(0)->getBaseAddress() > getBaseAddress()){
        PRINT_DEBUG_CFG("\tFound RawBlock (s) at %#llx + %d bytes", getBaseAddress(), flowGraph->getBlock(0)->getBaseAddress()-getBaseAddress());
        unknownBlocks.append(new RawBlock(unknownBlocks.size(), flowGraph, textSection->getStreamAtAddress(getBaseAddress()),
                                              flowGraph->getBlock(0)->getBaseAddress()-getBaseAddress(), getBaseAddress()));
    }
    for (int32_t i = 0; i < flowGraph->getNumberOfBlocks()-1; i++){
        uint64_t blockEnds = flowGraph->getBlock(i)->getBaseAddress() + flowGraph->getBlock(i)->getNumberOfBytes();        
        uint64_t blockBegins = flowGraph->getBlock(i+1)->getBaseAddress();        
        if (blockEnds < blockBegins){
            PRINT_DEBUG_CFG("\tFound RawBlock (m) at %#llx + %d bytes", blockEnds, blockBegins-blockEnds);
            unknownBlocks.append(new RawBlock(unknownBlocks.size(), flowGraph, textSection->getStreamAtAddress(blockEnds),
                                                  blockBegins-blockEnds, blockEnds));            
        }
    }
    BasicBlock* lastBlock = (BasicBlock*)flowGraph->getBlock(flowGraph->getNumberOfBlocks()-1);
    uint64_t blockEnds = lastBlock->getBaseAddress() + lastBlock->getNumberOfBytes();
    uint64_t funcEnds = getBaseAddress() + getSizeInBytes();
    if (blockEnds < funcEnds){
        PRINT_DEBUG_CFG("\tFound RawBlock (e) at %#llx + %d bytes", blockEnds, funcEnds-blockEnds);
        unknownBlocks.append(new RawBlock(unknownBlocks.size(), flowGraph, textSection->getStreamAtAddress(blockEnds),
                                              funcEnds-blockEnds, blockEnds));
    }
    for (uint32_t i = 0; i < unknownBlocks.size(); i++){
#ifdef DEBUG_CFG
        unknownBlocks[i]->print();
#endif
        flowGraph->addBlock(unknownBlocks[i]);
    }

    verify();
    
}

Instruction* Function::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->inRange(addr)){
            return flowGraph->getBasicBlock(i)->getInstructionAtAddress(addr); 
        }
    }

    return NULL;
}

BasicBlock* Function::getBasicBlockAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBasicBlock(i)->inRange(addr)){
            return flowGraph->getBasicBlock(i); 
        }
    }

    return NULL;
}

uint64_t Function::findInstrumentationPoint(uint32_t size, InstLocations loc){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        uint64_t instAddress = flowGraph->getBasicBlock(i)->findInstrumentationPoint(size,loc);
        if (instAddress){
            return instAddress;
        }
    }
    return 0;
}

Function::~Function(){
    if (flowGraph){
        delete flowGraph;
    }
}


Function::Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz)
    : TextObject(ElfClassTypes_Function,text,idx,sym,sym->GET(st_value),sz)
{
    ASSERT(sym);

    flowGraph = NULL;
    hashCode = HashCode(text->getSectionIndex(),index);
    PRINT_DEBUG_HASHCODE("Function %d, section %d  Hashcode: 0x%08llx", index, text->getSectionIndex(), hashCode.getValue());

    badInstruction = 0;
    flags = 0;

    verify();
}


bool Function::verify(){
    if (symbol){
        if (!symbol->isFunctionSymbol(textSection)){
            symbol->print();
            PRINT_ERROR("The symbol given for this function does not appear to be a function symbol");
            return false;
        }
    }
    if (!hashCode.isFunction()){
        PRINT_ERROR("Function %d HashCode is malformed", index);
        return false;
    }
    if (flowGraph){
        flowGraph->verify();
        for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
            if (!flowGraph->getBasicBlock(i)->verify()){
                return false;
            }
        }
        if (getNumberOfBytes() > sizeInBytes){
            PRINT_ERROR("Function %s has more bytes in BBs (%d) than in its size (%d)", getName(), getNumberOfBytes(), sizeInBytes);
        }
        Instruction** allInstructions = new Instruction*[getNumberOfInstructions()];
        getAllInstructions(allInstructions,0);
        qsort(allInstructions,getNumberOfInstructions(),sizeof(Instruction*),compareBaseAddress);
        for (uint32_t i = 0; i < getNumberOfInstructions()-1; i++){
            if (allInstructions[i+1]->getBaseAddress() && allInstructions[i]->getBaseAddress() == allInstructions[i+1]->getBaseAddress()){
                allInstructions[i]->print();
                allInstructions[i+1]->print();
                PRINT_ERROR("In function %s found the same instruction twice at base address %#llx", getName(), allInstructions[i]->getBaseAddress());
                return false;
            }
        }
        delete[] allInstructions;

        uint32_t numberOfBytes = 0;
        for (uint32_t i = 0; i < flowGraph->getNumberOfBlocks(); i++){
            numberOfBytes += flowGraph->getBlock(i)->getNumberOfBytes();
        }
        if (numberOfBytes != sizeInBytes){
            PRINT_ERROR("Bytes in FlowGraph doesn't match function size");
            return false;
        }
    }

    return true;
}

void Function::print(){
    PRINT_INFOR("Function %s has base address %#llx -- flags %#llx", getName(), baseAddress, flags);
    symbol->print();
}
