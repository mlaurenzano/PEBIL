#ifndef _BasicBlock_h_
#define _BasicBlock_h_

#include <Base.h>

class Instruction;
class Function;

class BasicBlock : public Base {
protected:
    uint32_t index;

    Function* function;
    Instruction** instructions;
    uint32_t numberOfInstructions;

    BasicBlock** sourceBlocks;
    uint32_t numberOfSourceBlocks;
    BasicBlock** targetBlocks;
    uint32_t numberOfTargetBlocks;

#define BB_FLAGS_PADDING 0x00000001
    uint32_t flags;
public:
    BasicBlock(uint32_t idx, Function* func);
    ~BasicBlock();

    void printInstructions();
    void print();

    uint32_t setInstructions(uint32_t num, Instruction** insts);

    uint64_t findInstrumentationPoint();

    bool verify();
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    Function* getFunction() { return function; }

    uint32_t getBlockSize();
    uint32_t getIndex() { return index; }
    uint64_t getAddress();

    uint32_t getNumberOfInstructions() { return numberOfInstructions; }
    Instruction* getInstruction(uint32_t idx) { ASSERT(idx < numberOfInstructions); return instructions[idx]; }
    Instruction* getInstructionAtAddress(uint64_t addr);

    uint32_t getNumberOfSourceBlocks() { return numberOfSourceBlocks; }
    BasicBlock* getSourceBlock(uint32_t idx) { return sourceBlocks[idx]; }
    uint32_t getNumberOfTargetBlocks() { return numberOfTargetBlocks; }
    BasicBlock* getTargetBlock(uint32_t idx) { return targetBlocks[idx]; }    
    bool inRange(uint64_t addr);

    bool isFunctionPadding() { if (flags & BB_FLAGS_PADDING) return true; return false; }
    uint32_t setFunctionPadding() { flags |= BB_FLAGS_PADDING; return false; }

    uint32_t replaceInstructions(uint64_t addr, Instruction** replacements, uint32_t numberOfReplacements, Instruction*** replacedInstructions);
};


#endif /* _BasicBlock_h_ */

