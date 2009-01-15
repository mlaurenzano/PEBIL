#ifndef _Function_h_
#define _Function_h_

#include <TextSection.h>
#include <Vector.h>
#include <BitSet.h>

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

    uint32_t generateCFG(uint32_t numberOfInstructions, Instruction** instructions);

public:
    Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz);
    ~Function();

    Symbol* getFunctionSymbol() { return functionSymbol; }

    FlowGraph* getFlowGraph() { return flowGraph; }
    uint32_t getNumberOfBasicBlocks();
    BasicBlock* getBasicBlock(uint32_t idx);
    Instruction* getInstructionAtAddress(uint64_t addr);

    uint32_t getNumberOfInstructions();

    char* getName();

    uint64_t findInstrumentationPoint();
    void printInstructions();

    uint32_t digest();
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
    void print();

    const char* briefName() { return "Function"; }
    HashCode getHashCode() { return hashCode; }
};

#endif /* _Function_h_ */
