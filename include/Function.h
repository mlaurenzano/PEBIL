#ifndef _Function_h_
#define _Function_h_

#include <Base.h>
#include <SymbolTable.h>

class Function : public Base {
protected:
    Symbol* functionSymbol;
    uint64_t functionSize;

public:
    Symbol* getFunctionSymbol() { return functionSymbol; }
    uint64_t getFunctionAddress() { ASSERT(functionSymbol && "symbol should exist"); return functionSymbol->GET(st_value); }
    uint64_t getFunctionSize() { return functionSize; }

    Function(Symbol* sym);
    ~Function() {}

    bool verify();
    void print();
    char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    const char* briefName() { return "Function"; }
};

#endif /* _Function_h_ */
