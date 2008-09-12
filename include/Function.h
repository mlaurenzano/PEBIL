#ifndef _Function_h_
#define _Function_h_

#include <Base.h>
#include <SymbolTable.h>
#include <BasicBlock.h>
#include <BinaryFile.h>
#include <TextSection.h>

class BasicBlock;

class Function : public Base {
protected:
    TextSection* rawSection;

    Symbol* functionSymbol;
    uint64_t functionAddress;
    uint64_t functionSize;
    BasicBlock** basicBlocks;
    uint32_t numberOfBasicBlocks;

    uint32_t index;

    uint32_t findBasicBlocks(uint32_t numberOfInstructions, Instruction** instructions);

public:
    Function(TextSection* rawsect, Symbol* sym, uint64_t exitAddr, uint32_t idx);
    Function(TextSection* rawsect, uint64_t addr, uint64_t exitAddr, uint32_t idx);
    ~Function();

    Symbol* getFunctionSymbol() { return functionSymbol; }
    uint64_t getFunctionAddress() { return functionAddress; }
    uint64_t getAddress() { return getFunctionAddress(); }
    uint64_t getFunctionSize() { return functionSize; }
    void setFunctionSize(uint64_t size);

    BasicBlock* getBasicBlock(uint32_t idx) { ASSERT(basicBlocks && idx < numberOfBasicBlocks); return basicBlocks[idx]; }
    uint32_t getNumberOfBasicBlocks() { return numberOfBasicBlocks; }
    Instruction* getInstructionAtAddress(uint64_t addr);

    bool inRange(uint64_t addr);
    uint32_t getIndex() { return index; }
    char* getFunctionName();

    uint64_t findInstrumentationPoint();
    void printInstructions();

    uint32_t read(BinaryInputFile* binaryInputFile);
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
    void print();
    char* charStream();

    const char* briefName() { return "Function"; }
};

#endif /* _Function_h_ */
