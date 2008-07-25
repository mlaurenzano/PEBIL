#include <Function.h>
#include <TextSection.h>
#include <Instruction.h>

Function::Function(TextSection* rawsect, Symbol* sym, uint64_t exitAddr, uint32_t idx) :
    Base(ElfClassTypes_Function)
{
    rawSection = rawsect;
    functionSymbol = sym;
    index = idx;

    //    functionSize = functionSymbol->GET(st_size);
    functionSize = exitAddr - getFunctionAddress();

    numberOfInstructions = 0;
    instructions = NULL;
    // get the list of instructions in this function

    Instruction* inst = rawSection->getInstructionAtAddress(getFunctionAddress());
    PRINT_INFOR("function %d ends at %llx", index, exitAddr);
    PRINT_INFOR("function starts at %llx", getFunctionAddress());
    functionSymbol->print(NULL);

    while (inst && inst->getAddress() < exitAddr){
        ASSERT(inst && "instruction should exist");
        //        inst->print();
        inst = rawSection->getInstructionAtAddress(inst->getAddress() + inst->getLength());
        numberOfInstructions++;
    }

    PRINT_INFOR("Found %d instructions in function %d", numberOfInstructions, index);


    verify();
}

void Function::setFunctionSize(uint64_t size){
    functionSize = size;
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
