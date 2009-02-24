#ifndef _BasicBlock_h_
#define _BasicBlock_h_

#include <Base.h>
#include <BitSet.h>
#include <FlowGraph.h>
#include <Vector.h>

class Function;
class Instruction;

class CodeBlock : public Base {
protected:
    uint32_t index;
    FlowGraph* flowGraph;
    uint64_t baseAddress;

public:
    CodeBlock(ElfClassTypes typ, uint32_t idx, FlowGraph* cfg);
    ~CodeBlock() {}

    virtual uint32_t getBlockSize() { __SHOULD_NOT_ARRIVE; return 0; }
    virtual void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { __SHOULD_NOT_ARRIVE; }
    virtual void setBaseAddress(uint64_t addr) { __SHOULD_NOT_ARRIVE; }
    virtual void print() { __SHOULD_NOT_ARRIVE; }

    uint64_t getBaseAddress() { return baseAddress; }
    uint32_t getIndex() { return index; }
    virtual void setIndex(uint32_t idx) { index = idx; }
};

class UnknownBlock : public CodeBlock {
private:
    char* rawBytes;
public:
    UnknownBlock(uint32_t idx, FlowGraph* cfg, char* byt, uint32_t sz, uint64_t addr);
    ~UnknownBlock();

    char* charStream() { return rawBytes; }
    uint64_t getBaseAddress() { return baseAddress; }
    uint32_t getBlockSize() { return getSizeInBytes(); }

    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify() { return true; }

    void setBaseAddress(uint64_t addr) { baseAddress = addr; }
    void print();
};


class BasicBlock : public CodeBlock {
private:
    const static uint32_t PaddingMask      = 0x1;
    const static uint32_t EntryMask        = 0x2;
    const static uint32_t ExitMask         = 0x4;
    const static uint32_t OnlyCtrlMask     = 0x8;
    const static uint32_t NoPathMask       = 0x10;

protected:
    Vector<Instruction*> instructions;
    Vector<BasicBlock*> sourceBlocks;
    Vector<BasicBlock*> targetBlocks;
    BasicBlock* immDominatedBy;

    HashCode hashCode;

    //    MemoryOperation** memoryOps;
    uint32_t numberOfMemoryOps;
    uint32_t numberOfFloatOps;

    uint32_t flags;
public:
    BasicBlock(uint32_t idx, FlowGraph* cfg);
    ~BasicBlock();

    uint32_t bloat(uint32_t minBlockSize);

    void setBaseAddress(uint64_t newBaseAddress);

    bool containsOnlyControl();
    bool containsCallToRange(uint64_t lowAddr, uint64_t highAddr);

    void printInstructions();
    void print();
    void printSourceBlocks();
    void printTargetBlocks();

    uint32_t getNumberOfBytes();

    uint32_t addSourceBlock(BasicBlock* srcBlock);
    uint32_t addTargetBlock(BasicBlock* tgtBlock);

    uint32_t getNumberOfSources() { return sourceBlocks.size(); }
    uint32_t getNumberOfTargets() { return targetBlocks.size(); }
    uint32_t getNumberOfMemoryOps() { return numberOfMemoryOps; }
    uint32_t getNumberOfFloatOps() { return numberOfFloatOps; }
    uint32_t getNumberOfLoads() { return 0; }
    uint32_t getNumberOfStores() { return 0; }

    uint32_t addInstruction(Instruction* inst);

    bool controlFallsThrough();
    bool findExitInstruction();

    uint64_t findInstrumentationPoint(uint32_t size, InstLocations loc);
    bool inRange(uint64_t addr);

    bool verify();
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    FlowGraph* getFlowGraph() { return flowGraph; }
    Function* getFunction() { return flowGraph->getFunction(); }

    uint32_t getBlockSize();
    uint64_t getTargetAddress();
    uint32_t getAllInstructions(Instruction** allinsts, uint32_t nexti);

    uint32_t getNumberOfInstructions() { return instructions.size(); }
    Instruction* getInstruction(uint32_t idx) { return instructions[idx]; }
    Instruction* getInstructionAtAddress(uint64_t addr);

    bool isPadding()  { return (flags & PaddingMask); }
    bool isEntry()    { return (flags & EntryMask); }
    bool isExit()     { return (flags & ExitMask); }
    bool isOnlyCtrl() { return (flags & OnlyCtrlMask); }
    bool isNoPath()   { return (flags & NoPathMask); }

    uint32_t setPadding()  { flags |= PaddingMask; return flags; }
    uint32_t setEntry()    { flags |= EntryMask; return flags; }
    uint32_t setExit()     { flags |= ExitMask; return flags; }
    uint32_t setOnlyCtrl() { flags |= OnlyCtrlMask; return flags; }
    uint32_t setNoPath()   { flags |= NoPathMask; return flags; }

    void setIndex(uint32_t idx);

    bool isUnreachable() { return isNoPath(); }
    bool isReachable() { return !isNoPath(); }

    Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements);

    HashCode getHashCode() { return hashCode; }

    bool isDominatedBy(BasicBlock* bb);
    BasicBlock* getSourceBlock(uint32_t idx) { return sourceBlocks[idx]; }
    BasicBlock* getTargetBlock(uint32_t idx) { return targetBlocks[idx]; }

    void findMemoryFloatOps();
    void setImmDominator(BasicBlock* bb) { immDominatedBy = bb; }
    BasicBlock* getImmDominator() { return immDominatedBy; }

};


#endif /* _BasicBlock_h_ */

