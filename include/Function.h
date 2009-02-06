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
protected:
    Symbol* functionSymbol;
    FlowGraph* flowGraph;

    HashCode hashCode;
    uint64_t baseAddress;

public:
    Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz);
    ~Function();

    void setBaseAddress(uint64_t newBaseAddress);

    Symbol* getFunctionSymbol() { return functionSymbol; }
    uint32_t generateCFG(uint32_t numberOfInstructions, Instruction** instructions);

    FlowGraph* getFlowGraph() { return flowGraph; }
    uint32_t getNumberOfBasicBlocks();
    BasicBlock* getBasicBlock(uint32_t idx);
    Instruction* getInstructionAtAddress(uint64_t addr);

    uint32_t getNumberOfInstructions();
    uint32_t getNumberOfBytes();

    char* getName();
    uint32_t getAllInstructions(Instruction** allinsts, uint32_t nexti);

    void printInstructions();

    uint32_t digest();
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
    void print();

    const char* briefName() { return "Function"; }
    HashCode getHashCode() { return hashCode; }

    Vector<Instruction*>* swapInstructions(uint64_t addr, Vector<Instruction*>* replacements);
    uint64_t findInstrumentationPoint(uint32_t size, InstLocations loc);
};

#endif /* _Function_h_ */
