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
#include <SymbolTable.h>
#include <TextSection.h>


uint32_t Function::getAllInstructions(Instruction** allinsts, uint32_t nexti){
    uint32_t instructionCount = 0;
    PRINT_DEBUG_ANCHOR("\tFN allinst address %lx, nexti %d", allinsts, nexti);
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        instructionCount += flowGraph->getBlock(i)->getAllInstructions(allinsts, instructionCount+nexti);
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
    return flowGraph->getBlock(idx);
}

char* Function::getName(){
    if (functionSymbol){
        return functionSymbol->getSymbolName();
    }
    return symbol_without_name;
}

void Function::printInstructions(){
    __FUNCTION_NOT_IMPLEMENTED;
}

void Function::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        flowGraph->getBlock(i)->dump(binaryOutputFile,offset+currByte);
        currByte += flowGraph->getBlock(i)->getBlockSize();
    }
    ASSERT(currByte == sizeInBytes);
}


// we assume that functions can only be entered at the beginning
uint32_t Function::generateCFG(uint32_t numberOfInstructions, Instruction** instructions){

    ASSERT(!flowGraph && "FlowGraph for this function has already been generated");

    flowGraph = new FlowGraph(this);

    // cache all addresses for this basic block
    uint64_t* addressCache = new uint64_t[numberOfInstructions];
    uint64_t* nextAddressCache = new uint64_t[numberOfInstructions];
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        addressCache[i] = instructions[i]->getBaseAddress();
        nextAddressCache[i] = instructions[i]->getNextAddress();
    }

    bool* isLeader = new bool[numberOfInstructions];
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        isLeader[i] = false;
    }

    // find leaders based on direct control flow and certain specific types
    // of indirect flow
    uint32_t numberOfBasicBlocks = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (i == 0){
            isLeader[i] = true;
            numberOfBasicBlocks++;
        }
        if (instructions[i]->isControl()){
            if (i+1 < numberOfInstructions){
                if (!isLeader[i+1]){
                    isLeader[i+1] = true;
                    numberOfBasicBlocks++;
                } 
            }
            if (inRange(nextAddressCache[i])){
                for (uint32_t j = 0; j < numberOfInstructions; j++){
                    if (addressCache[j] == nextAddressCache[i]){
                        if (!isLeader[j]){
                            isLeader[j] = true;
                            numberOfBasicBlocks++;
                        }
                        if (i+1 < numberOfInstructions){
                            if (!isLeader[i+1]){
                                isLeader[i+1] = true;
                                numberOfBasicBlocks++;
                            }
                        }
                    }
                }
            }
        }

        if (instructions[i]->isIndirectBranch()){
            
            PRINT_DEBUG_CFG("indirect branch found");
#ifdef DEBUG_CFG
                instructions[i]->print();
#endif
        }
    }


    delete[] addressCache;
    delete[] nextAddressCache;

    BasicBlock* currentBlock = NULL;
    BasicBlock* entry = NULL;
    numberOfBasicBlocks = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        if (isLeader[i]){
            if (currentBlock){
                flowGraph->addBlock(currentBlock);
                numberOfBasicBlocks++;
            }
            currentBlock = new BasicBlock(numberOfBasicBlocks,flowGraph);
            if (!entry){
                entry = currentBlock;
            }
        }
        currentBlock->addInstruction(instructions[i]);
    }
    flowGraph->addBlock(currentBlock);
    numberOfBasicBlocks++;

    flowGraph->connectGraph(entry);
    flowGraph->setImmDominatorBlocks();

#ifdef DEBUG_BASICBLOCK
    PRINT_DEBUG_BASICBLOCK("****** Printing Blocks for function %s", getName());
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        flowGraph->getBlock(i)->print();
        flowGraph->getBlock(i)->printInstructions();
    }
    PRINT_DEBUG_BASICBLOCK("Found %d instructions in %d basic blocks in function %s", numberOfInstructions, numberOfBasicBlocks, getName());
#endif

    delete[] isLeader;
    ASSERT(numberOfBasicBlocks == flowGraph->getNumberOfBasicBlocks());

    verify();

    return flowGraph->getNumberOfBasicBlocks();
}

Instruction* Function::getInstructionAtAddress(uint64_t addr){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (flowGraph->getBlock(i)->inRange(addr)){
            return flowGraph->getBlock(i)->getInstructionAtAddress(addr); 
        }
    }

    return NULL;
}

uint32_t Function::digest(){
    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint64_t instructionAddress;
    uint32_t numberOfInstructions = 0;

    Instruction* dummyInstruction = new Instruction();
    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, dummyInstruction);
    }

    delete dummyInstruction;

    /*
    PRINT_INFOR("Function %s: read %d bytes from function, %d bytes in functions", getName(), currByte, sizeInBytes);
    if (functionSymbol)
        functionSymbol->print();
    */

    Instruction** instructions = new Instruction*[numberOfInstructions];
    numberOfInstructions = 0;
    
    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setLength(MAX_X86_INSTRUCTION_LENGTH);
        instructions[numberOfInstructions]->setBaseAddress(getBaseAddress() + currByte);
        instructions[numberOfInstructions]->setProgramAddress(getBaseAddress() + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);
        instructions[numberOfInstructions]->setByteSource(ByteSource_Application_Function);
        //        instructions[numberOfInstructions]->setIndex(numberOfInstructions);
        
        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, instructions[numberOfInstructions]);
        if (!instructionLength){
            instructionLength = 1;
        }
        instructions[numberOfInstructions]->setLength(instructionLength);
        instructions[numberOfInstructions]->verify();
        //        instructions[numberOfInstructions]->print();
    }
    
    // in case the disassembler found an instruction that exceeds the function boundary, we will
    // reduce the size of the last instruction accordingly so that the extra bytes will not be
    // used
    if (currByte > sizeInBytes){
        uint32_t extraBytes = currByte-sizeInBytes;
        instructions[numberOfInstructions-1]->setLength(instructions[numberOfInstructions-1]->getLength()-extraBytes);
        currByte -= extraBytes;
        PRINT_WARN(3,"Disassembler found instructions that exceed the function boundary in %s by %d bytes", getName(), extraBytes);
    }

    ASSERT(currByte == sizeInBytes && "Number of bytes read for function does not match function size");
    generateCFG(numberOfInstructions, instructions);
    delete[] instructions;

    ASSERT(flowGraph);

    return currByte;
}

uint64_t Function::findInstrumentationPoint(uint32_t size, InstLocations loc){
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        uint64_t instAddress = flowGraph->getBlock(i)->findInstrumentationPoint(size,loc);
        if (instAddress){
            return instAddress;
        }
    }
    return 0;
}

Function::~Function(){
    if (flowGraph){
        for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
            delete flowGraph->getBlock(i);
        }
        delete flowGraph;
    }
}


Function::Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz) :
    TextObject(ElfClassTypes_Function,text,idx,sym->GET(st_value),sz)
{
    baseAddress = 0;

    textSection = text;
    functionSymbol = sym;

    flowGraph = NULL;

    hashCode = HashCode(text->getSectionIndex(),index);
    PRINT_DEBUG_HASHCODE("Function %d, section %d  Hashcode: 0x%08llx", index, text->getSectionIndex(), hashCode.getValue());

    verify();
}


bool Function::verify(){
    if (functionSymbol){
        if (!functionSymbol->isFunctionSymbol(textSection)){
            functionSymbol->print();
            PRINT_ERROR("The symbol given for this function does not appear to be a function symbol");
            return false;
        }
    }
    if (flowGraph){
        for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
            if (!flowGraph->getBlock(i)->verify()){
                return false;
            }
        }
    }
    if (!hashCode.isFunction()){
        PRINT_ERROR("Function %d HashCode is malformed", index);
        return false;
    }
    return true;
}

void Function::print(){
    PRINT_INFOR("Function %s has base address %#llx", getName(), baseAddress);
    functionSymbol->print();
}
