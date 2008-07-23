#include <Function.h>

Function::Function(Symbol* sym) :
    Base(ElfClassTypes_Function)
{
    functionSymbol = sym;
    functionSize = functionSymbol->GET(st_size);
    PRINT_INFOR("Function size is %lld bytes", functionSize);

    verify();
}

bool Function::verify(){
    if (functionSymbol->getSymbolType() != STT_FUNC){
        PRINT_ERROR("Function symbol should have type STT_FUNC");
    }
}

void Function::print(){
    PRINT_INFOR("Function size is %lld bytes", functionSize);
    functionSymbol->print(NULL);
}
