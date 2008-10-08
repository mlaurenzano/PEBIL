#ifndef _BasicBlock_h_
#define _BasicBlock_h_

#include <Base.h>
#include <Vector.h>

class Instruction;
class Function;

class BasicBlock : public Base {
protected:
    uint32_t index;

    Function* function;
    
    Vector<Instruction*> instructions;
    Vector<BasicBlock*> sourceBlocks;
    Vector<BasicBlock*> targetBlocks;

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

    uint32_t getNumberOfInstructions() { return instructions.size(); }
    Instruction* getInstruction(uint32_t idx) { return instructions[idx]; }
    Instruction* getInstructionAtAddress(uint64_t addr);

    uint32_t getNumberOfSourceBlocks() { return sourceBlocks.size(); }
    BasicBlock* getSourceBlock(uint32_t idx) { return sourceBlocks[idx]; }
    uint32_t getNumberOfTargetBlocks() { return targetBlocks.size(); }
    BasicBlock* getTargetBlock(uint32_t idx) { return targetBlocks[idx]; }    
    bool inRange(uint64_t addr);

    bool isFunctionPadding() { if (flags & BB_FLAGS_PADDING) return true; return false; }
    uint32_t setFunctionPadding() { flags |= BB_FLAGS_PADDING; return flags; }

    Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements);
};


#endif /* _BasicBlock_h_ */

