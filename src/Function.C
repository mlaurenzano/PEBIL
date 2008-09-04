#include <Function.h>
#include <TextSection.h>
#include <Instruction.h>
#include <ElfFileInst.h>

uint64_t Function::findInstrumentationPoint(){
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i]->print();
        uint32_t j = i;
        uint32_t instBytes = 0;
        PRINT_INFOR("Examining function %s", getFunctionName());
        while (j < numberOfInstructions && instructions[j]->isRelocatable()){
            PRINT_INFOR("Examining instruction %d", j);
            instBytes += instructions[j]->getLength();
            j++;
        }
        if (instBytes >= SIZE_NEEDED_AT_INST_POINT){
            return instructions[i]->getAddress();
        }
    }
    return 0;
}

Function::~Function(){
    if (instructions){
        delete[] instructions;
    }
}


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

    PRINT_INFOR("Initializing functions for section %d", sym->GET(st_shndx));
    PRINT_INFOR("function %d [%llx,%llx] = %d bytes", index, getFunctionAddress(), exitAddr, functionSize);

    // get the list of instructions in this function
    Instruction* inst = rawSection->getInstructionAtAddress(getFunctionAddress());
    //    rawSection->printInstructions();

    while (inst && inst->getAddress() < exitAddr){
        ASSERT(inst && "instruction should exist");
        inst = rawSection->getInstructionAtAddress(inst->getAddress() + inst->getLength());
        numberOfInstructions++;
    }

    PRINT_INFOR("Found %d instructions in function %d", numberOfInstructions, index);

    instructions = new Instruction*[numberOfInstructions];
    numberOfInstructions = 0;
    inst = rawSection->getInstructionAtAddress(getFunctionAddress());
    while (inst && inst->getAddress() < exitAddr){
        ASSERT(inst && "instruction should exist");
        //        PRINT_INFOR("Instruction ptr at address %x", inst);
        //PRINT_INFOR("Getting instruction %d at address %llx for this function", numberOfInstructions, inst->getAddress());
        instructions[numberOfInstructions++] = inst;
        inst = rawSection->getInstructionAtAddress(inst->getAddress() + inst->getLength());
    }

    verify();
}

void Function::setFunctionSize(uint64_t size){
    functionSize = size;
}

bool Function::verify(){
    if (functionSymbol->getSymbolType() != STT_FUNC){
        PRINT_ERROR("Function symbol should have type STT_FUNC");
    }

    // make sure the instruction addresses span the function
    if (numberOfInstructions){
        if (instructions[0]->getAddress() != getFunctionAddress()){
            instructions[0]->print();
            PRINT_ERROR("First instruction in function %d should be at the beginning of the function(%llx)", index, getFunctionAddress());
            return false;
        }
        for (uint32_t i = 0; i < numberOfInstructions-1; i++){
            if (instructions[i]->getAddress() + instructions[i]->getLength() != instructions[i+1]->getAddress()){
                instructions[i]->print();
                instructions[i+1]->print();
                PRINT_ERROR("In function %d instructions %d and %d boundaries should touch", index, i, i+1);
                return false;
            }
        }   

        // ideally these would align exactly, but the GNU disassembler is flawed and sometimes does detect the ends correctly
        if (instructions[numberOfInstructions-1]->getAddress() + instructions[numberOfInstructions-1]->getLength() <
            getFunctionAddress() + getFunctionSize()){
            instructions[numberOfInstructions-1]->print();
            functionSymbol->print();
            PRINT_ERROR("Last instruction in function %d should be at the end of the function (%llx)", index, getFunctionAddress() + getFunctionSize());
            return false;
        }
        
    }

    return true;

}

void Function::print(){
    PRINT_INFOR("Function size is %lld bytes", functionSize);
    functionSymbol->print();
}
