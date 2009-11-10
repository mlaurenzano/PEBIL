#ifndef _Function_h_
#define _Function_h_

#include <BitSet.h>
#include <Instruction.h>
#include <TextSection.h>
#include <Vector.h>

class BasicBlock;
class BinaryInputFile;
class BinaryOutputFile;
class FlowGraph;
class Symbol;
class TextObject;

class Function : public TextObject {
private:
    const static uint32_t recursivedisasmMask     = 0x1;
    const static uint32_t instrumentationfuncMask = 0x2;
    const static uint32_t jumptableMask           = 0x4;

protected:
    FlowGraph* flowGraph;
    HashCode hashCode;
    uint64_t badInstruction;
    uint64_t flags;

    Vector<Instruction*>* digestRecursive();
public:
    Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz);
    ~Function();

    bool hasLeafOptimization();

    bool isRecursiveDisasm()          { return (flags & recursivedisasmMask); }
    bool isInstrumentationFunction()  { return (flags & instrumentationfuncMask); }
    bool isJumpTable()                { return (flags & jumptableMask); }

    void setRecursiveDisasm()         { flags |= recursivedisasmMask; }
    void setInstrumentationFunction() { flags |= instrumentationfuncMask; }
    void setJumpTable()               { flags |= jumptableMask; }

    uint64_t getBadInstruction() { return badInstruction; }
    void setBadInstruction(uint64_t addr) { badInstruction = addr; }

    bool hasCompleteDisassembly();
    bool containsCallToRange(uint64_t lowAddr, uint64_t highAddr);

    uint32_t bloatBasicBlocks(BloatTypes bloatType, uint32_t bloatAmount);

    void setBaseAddress(uint64_t newBaseAddress);

    Symbol* getFunctionSymbol() { return symbol; }
    uint32_t generateCFG(Vector<Instruction*>* instructions);

    FlowGraph* getFlowGraph() { return flowGraph; }
    uint32_t getNumberOfBasicBlocks();
    BasicBlock* getBasicBlock(uint32_t idx);
    Instruction* getInstructionAtAddress(uint64_t addr);
    BasicBlock* getBasicBlockAtAddress(uint64_t addr);

    uint32_t getNumberOfInstructions();
    uint32_t getNumberOfBytes();

    uint32_t getAllInstructions(Instruction** allinsts, uint32_t nexti);

    void printInstructions();

    uint32_t digest();
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
    void print();

    const char* briefName() { return "Function"; }
    HashCode getHashCode() { return hashCode; }

    Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements);
    uint64_t findInstrumentationPoint(uint64_t addr, uint32_t size, InstLocations loc);

    void printDisassembly(bool instructionDetail);
};

#endif /* _Function_h_ */
