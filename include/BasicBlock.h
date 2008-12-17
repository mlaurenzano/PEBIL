#ifndef _BasicBlock_h_
#define _BasicBlock_h_

#include <Base.h>
#include <Vector.h>
#include <BitSet.h>

class Instruction;
class Function;
class FlowGraph;

class BasicBlock : public Base {
protected:
    uint32_t index;

    Function* function;
    FlowGraph* flowGraph;
    
    Vector<Instruction*> instructions;
    Vector<BasicBlock*> sourceBlocks;
    Vector<BasicBlock*> targetBlocks;
    BasicBlock* immDominatedBy;

    HashCode hashCode;

    //    MemoryOperation** memoryOps;
    uint32_t numberOfMemoryOps;
    uint32_t numberOfFloatOps;


#define BB_FLAGS_PADDING        0x00000001
#define BB_FLAGS_FNENTRY        0x00000002
#define BB_FLAGS_FNTRACE        0x00000004
#define BB_FLAGS_FNJUMPTABLE    0x00000004
#define BB_FLAGS_FNNOPATH       0x00000004
    uint32_t flags;
public:
    BasicBlock(uint32_t idx, Function* func);
    ~BasicBlock();

    void printInstructions();
    void print();

    uint32_t getNumberOfSources() { return sourceBlocks.size(); }
    uint32_t getNumberOfTargets() { return targetBlocks.size(); }
    uint32_t getNumberOfMemoryOps() { return numberOfMemoryOps; }
    uint32_t getNumberOfFloatOps() { return numberOfFloatOps; }
    
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

    bool isPadding() { return (flags & BB_FLAGS_PADDING); }
    bool isEntry() { return (flags & BB_FLAGS_FNENTRY); }
    bool isTrace() { return (flags & BB_FLAGS_FNTRACE); }
    bool isJumpTable() { return (flags & BB_FLAGS_FNJUMPTABLE); }
    bool isNoPath() { return (flags & BB_FLAGS_FNNOPATH); }

    uint32_t setPadding() { flags |= BB_FLAGS_PADDING; return flags; }
    uint32_t setEntry() { flags |= BB_FLAGS_FNENTRY; return flags; }
    uint32_t setTrace() { flags |= BB_FLAGS_FNTRACE; return flags; }
    uint32_t setJumpTable() { flags |= BB_FLAGS_FNJUMPTABLE; return flags; }
    uint32_t setNoPath() { flags |= BB_FLAGS_FNNOPATH; return flags; }

    Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements);

    HashCode getHashCode() { return hashCode; }

    bool isDominatedBy(BasicBlock* bb) { return false; }
    BasicBlock* getSourceBlock(uint32_t idx) { return sourceBlocks[idx]; }
    BasicBlock* getTargetBlock(uint32_t idx) { return targetBlocks[idx]; }
    void setIndex(uint32_t idx);

    void findMemoryFloatOps();
    void setImmDominator(BasicBlock* bb) { immDominatedBy = bb; }

};


#endif /* _BasicBlock_h_ */

