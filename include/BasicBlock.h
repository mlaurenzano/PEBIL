#ifndef _BasicBlock_h_
#define _BasicBlock_h_

#include <Base.h>
#include <Vector.h>
#include <BitSet.h>

class Instruction;
class Function;

class BasicBlock : public Base {
protected:
    uint32_t index;

    Function* function;
    
    Vector<Instruction*> instructions;
    BitSet<BasicBlock*>* sourceBlocks;
    BitSet<BasicBlock*>* targetBlocks;
    BitSet<BasicBlock*>* dominatorBlocks;

#define BB_FLAGS_PADDING  0x00000001
#define BB_FLAGS_FNENTRY  0x00000002
    uint32_t flags;
public:
    BasicBlock(uint32_t idx, Function* func);
    ~BasicBlock();

    void printInstructions();
    void print();

    uint32_t addInstruction(Instruction* inst);

    uint64_t findInstrumentationPoint();
    bool inRange(uint64_t addr);

    bool verify();
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    Function* getFunction() { return function; }

    uint32_t getBlockSize();
    uint64_t getAddress();
    uint64_t getTargetAddress();
    uint32_t getIndex() { return index; }

    uint32_t getNumberOfInstructions() { return instructions.size(); }
    Instruction* getInstruction(uint32_t idx) { return instructions[idx]; }
    Instruction* getInstructionAtAddress(uint64_t addr);

    void giveSourceBlocks(BitSet<BasicBlock*>*);
    BitSet<BasicBlock*>* getSourceBlocks();
    void giveTargetBlocks(BitSet<BasicBlock*>*);
    BitSet<BasicBlock*>* getTargetBlocks();
    void giveDominatorBlocks(BitSet<BasicBlock*>*);
    BitSet<BasicBlock*>* getDominatorBlocks();

    bool isFunctionPadding() { if (flags & BB_FLAGS_PADDING) return true; return false; }
    uint32_t setFunctionPadding() { flags |= BB_FLAGS_PADDING; return flags; }

    bool isFunctionEntry() { if (flags & BB_FLAGS_FNENTRY) return true; return false; }
    uint32_t setFunctionEntry() { flags |= BB_FLAGS_FNENTRY; return flags; }

    Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements);
};


#endif /* _BasicBlock_h_ */

