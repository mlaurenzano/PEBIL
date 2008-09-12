#include <TextSection.h>
#include <ElfFile.h>
#include <Disassembler.h>
#include <SectionHeader.h>
#include <Instruction.h>
#include <SymbolTable.h>
#include <CStructuresX86.h>

TextSection::TextSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : RawSection(ElfClassTypes_TextSection,filePtr,size,scnIdx,elf)
{
    index = idx;

    sortedFunctions = NULL;
    numberOfFunctions = 0;

    disassembler = new Disassembler(elfFile->is64Bit());
    disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
}

uint32_t TextSection::disassemble(BinaryInputFile* binaryInputFile){
    PRINT_INFOR("Reading (text) section %d", getSectionIndex());

    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    numberOfFunctions = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->getSymbolType() == STT_FUNC && symbol->GET(st_shndx) == getSectionIndex()){
                    numberOfFunctions++;
                }
            }
        }
    }

    sortedFunctions = new Function*[numberOfFunctions];
    Symbol** functionSymbols = new Symbol*[numberOfFunctions];

    numberOfFunctions = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->getSymbolType() == STT_FUNC && symbol->GET(st_shndx) == getSectionIndex()){
                    functionSymbols[numberOfFunctions++] = symbol;
                }
            }
        }
    }

    qsort(functionSymbols,numberOfFunctions,sizeof(Symbol*),compareSymbolValue);

    if (numberOfFunctions){
        for (uint32_t i = 0; i < numberOfFunctions-1; i++){
            sortedFunctions[i] = new Function(this, functionSymbols[i], functionSymbols[i+1]->GET(st_value), i);
        }
        // the last function ends at the end of the section
        sortedFunctions[numberOfFunctions-1] = new Function(this, functionSymbols[numberOfFunctions-1], 
                                                            sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size), numberOfFunctions-1);
    }

    // this is a text section with no functions (probably the .plt section), we wil put everything in a function
    else{
        numberOfFunctions = 1;
        if (sortedFunctions){
            delete[] sortedFunctions;
        }
        sortedFunctions = new Function*[numberOfFunctions];
        sortedFunctions[0] = new Function(this, sectionHeader->GET(sh_addr), sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size), 0);
    }

    delete[] functionSymbols;

    verify();

    PRINT_INFOR("Found %d functions in secton %d", numberOfFunctions, getSectionIndex());
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        sortedFunctions[i]->read(binaryInputFile);
    }
}


uint32_t TextSection::read(BinaryInputFile* binaryInputFile){
    return 0;
}


uint64_t TextSection::findInstrumentationPoint(){
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        uint64_t instAddress = sortedFunctions[i]->findInstrumentationPoint();
        if (instAddress){
            return instAddress;
        }
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}


uint32_t TextSection::replaceInstructions(uint64_t addr, Instruction** replacements, uint32_t numberOfReplacements, Instruction*** replacedInstructions){

    ASSERT(!*(replacedInstructions) && "This array should be empty since it will be filled by this function");

    for (uint32_t i = 0; i < numberOfFunctions; i++){
        Function* f = sortedFunctions[i];
        if (f->inRange(addr)){
            for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
                if (f->getBasicBlock(j)->inRange(addr)){
                    return f->getBasicBlock(j)->replaceInstructions(addr,replacements,numberOfReplacements,replacedInstructions);
                }
            }
        }
    }
    PRINT_ERROR("Cannot find instructions at address 0x%llx to replace", addr);
    return 0;
}


void TextSection::printInstructions(){
    PRINT_INFOR("Printing Instructions for (text) section %d", getSectionIndex());
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        sortedFunctions[i]->printInstructions();
    }
}


int searchInstructionAddress(const void* arg1, const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    Instruction* inst = *((Instruction**)arg2);

    ASSERT(inst && "Instruction should exist");

    uint64_t val = inst->getAddress();

    if (key < val)
        return -1;
    if (key > val)
        return 1;
    return 0;
}


Instruction* TextSection::getInstructionAtAddress(uint64_t addr){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());
    if (!sectionHeader->inRange(addr)){
        return NULL;
    }

    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (sortedFunctions[i]->inRange(addr)){
            return sortedFunctions[i]->getInstructionAtAddress(addr);
        }
    }
    return NULL;
}

bool TextSection::verify(){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    if (!numberOfFunctions){
        return true;
    }

    for (uint32_t i = 0; i < numberOfFunctions; i++){

        uint64_t entrAddr = sortedFunctions[i]->getFunctionAddress();
        uint64_t exitAddr = entrAddr + sortedFunctions[i]->getFunctionSize();

        // make sure each function entry resides within the bounds of this section
        if (!sectionHeader->inRange(entrAddr)){
            sectionHeader->print();
            PRINT_ERROR("The function entry address 0x%016llx is not in the range of section %d", entrAddr, sectionHeader->getIndex());
            return false;
        }

        // make sure each function exit resides within the bounds of this section
        if (!sectionHeader->inRange(exitAddr) && exitAddr != sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size)){
            sortedFunctions[i]->print();
            sectionHeader->print();
            PRINT_INFOR("Section range [0x%016llx,0x%016llx]", sectionHeader->GET(sh_addr), sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size));
            PRINT_ERROR("The function exit address 0x%016llx is not in the range of section %d", exitAddr, sectionHeader->getIndex());
            return false;
        }
    }

    for (uint32_t i = 0; i < numberOfFunctions - 1; i++){

        // make sure sortedFunctions is actually sorted
        if (sortedFunctions[i]->getFunctionAddress() > sortedFunctions[i+1]->getFunctionAddress()){
            sortedFunctions[i]->print();
            sortedFunctions[i+1]->print();
            PRINT_ERROR("Function addresses 0x%016llx 0x%016llx are not sorted", sortedFunctions[i]->getFunctionAddress(), sortedFunctions[i+1]->getFunctionAddress());
            return false;
        }
    }

    // make sure functions span the entire section unless it is a plt section
    if (numberOfFunctions){

        // check that the first function is at the section beginning
        if (sortedFunctions[0]->getFunctionAddress() != sectionHeader->GET(sh_addr)){
            PRINT_ERROR("First function in section %d should be at the beginning of the section", getSectionIndex());
            return false;
        }

        // check that function boundaries are contiguous
        for (uint32_t i = 0; i < numberOfFunctions-1; i++){
            if (sortedFunctions[i]->getFunctionAddress() + sortedFunctions[i]->getFunctionSize() !=
                sortedFunctions[i+1]->getFunctionAddress()){
                PRINT_ERROR("In section %d, boundaries on function %d and %d do not align", getSectionIndex(), i, i+1);
                return false;
            }
        }

        // check the the last function ends at the section end
        if (sortedFunctions[numberOfFunctions-1]->getFunctionAddress() + sortedFunctions[numberOfFunctions-1]->getFunctionSize() !=
            sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size)){
            PRINT_ERROR("Last function in section %d should be at the end of the section", getSectionIndex());
        }

    }

    return true;
}


void TextSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        ASSERT(sortedFunctions[i] && "The functions in this text section should be initialized");
        sortedFunctions[i]->dump(binaryOutputFile, offset + currByte);
        currByte += sortedFunctions[i]->getFunctionSize();
    }
}


TextSection::~TextSection(){
    if (sortedFunctions){
        for (uint32_t i = 0; i < numberOfFunctions; i++){
            delete sortedFunctions[i];
        }
        delete[] sortedFunctions;
    }
    if (disassembler){
        delete disassembler;
    }
}

uint32_t TextSection::printDisassembledCode(bool instructionDetail){
    ASSERT(elfFile && "Text section should be linked to its corresponding ElfFile object");

    ASSERT(disassembler && "Disassembler should be initialized before calling disassemble");
    disassembler->setPrintFunction((fprintf_ftype)fprintf,stdout);

    SectionHeader* sHdr = elfFile->getSectionHeader(sectionIndex);
    ASSERT(sHdr && "Invalid sectionIndex set on text section");

    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint32_t instructionCount = 0;
    uint64_t instructionAddress;
    Instruction* dummyInstruction = new Instruction();

    PRINT_INFOR("Disassembly output of Section %s(%d)", sHdr->getSectionNamePtr(), sectionIndex);

    for (currByte = 0; currByte < sHdr->GET(sh_size); currByte += instructionLength, instructionCount++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        //fprintf(stdout, "(0x%llx) 0x%llx:\t", (uint64_t)(charStream() + currByte), (uint64_t)(sHdr->GET(sh_addr) + currByte));
        fprintf(stdout, "0x%llx:\t", (uint64_t)(sHdr->GET(sh_addr) + currByte));

        instructionLength = disassembler->print_insn(instructionAddress, dummyInstruction);
        
        fprintf(stdout, "\t(bytes -- ");
        uint8_t* bytePtr;
        for (uint32_t j = 0; j < instructionLength; j++){
            bytePtr = (uint8_t*)charStream() + currByte + j;
            fprintf(stdout, "%2.2lx ", *bytePtr);
        }
        fprintf(stdout, ")\n");
        
        if (instructionDetail){
            dummyInstruction->print();
        }
    }
    PRINT_INFOR("Found %d instructions (%d bytes) in section %d", instructionCount, currByte, sectionIndex);
    disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
}
