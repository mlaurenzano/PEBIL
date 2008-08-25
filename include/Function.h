#ifndef _Function_h_
#define _Function_h_

#include <Base.h>
#include <SymbolTable.h>

class Function : public Base {
protected:
    TextSection* rawSection;

    Symbol* functionSymbol;
    uint64_t functionSize;
    Instruction** instructions;
    uint32_t numberOfInstructions;

    uint32_t index;

public:
    Symbol* getFunctionSymbol() { return functionSymbol; }
    uint64_t getFunctionAddress() { ASSERT(functionSymbol && "symbol should exist"); return functionSymbol->GET(st_value); }
    uint64_t getAddress() { return getFunctionAddress(); }
    uint64_t getFunctionSize() { return functionSize; }
    void setFunctionSize(uint64_t size);

    Instruction* getInstruction(uint32_t idx);
    uint32_t getNumberOfInstructions() { return numberOfInstructions; }
    Instruction* getInstructionAtAddress(uint64_t addr);

    uint32_t getIndex() { return index; }
    char* getFunctionName() { ASSERT(functionSymbol && "symbol should exist"); return functionSymbol->getSymbolName(); }

    Function(TextSection* rawsect, Symbol* sym, uint64_t exitAddr, uint32_t idx);
    ~Function();

    bool verify();
    void print();
    char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    const char* briefName() { return "Function"; }
};

#endif /* _Function_h_ */
