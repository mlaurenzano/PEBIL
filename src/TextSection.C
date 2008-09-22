#include <TextSection.h>
#include <ElfFile.h>
#include <Disassembler.h>
#include <SectionHeader.h>
#include <Instruction.h>
#include <SymbolTable.h>
#include <CStructuresX86.h>
#include <Function.h>
#include <BasicBlock.h>

bool TextObject::isFunction(){
    return (getType() == ElfClassTypes_Function);
}


uint32_t TextSection::discoverTextObjects(Symbol*** functionSymbols){
    ASSERT(!*functionSymbols && "This array should be empty since it is loaded by this function");

    // count the number of symbols for this text section
    uint32_t numberOfSymbols = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->isFunctionSymbol(this)){
                    numberOfSymbols++;
                } else if (symbol->isTextObjectSymbol(this)){
                    numberOfSymbols++;
                }
            }
        }
    }

    Symbol** syms = new Symbol*[numberOfSymbols];

    // copy text symbols to local array
    numberOfSymbols = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->isFunctionSymbol(this)){
                    syms[numberOfSymbols++] = symbol;
                } else if (symbol->isTextObjectSymbol(this)){
                    syms[numberOfSymbols++] = symbol;
                }
            }
        }
    }

    // sort text symbols
    qsort(syms,numberOfSymbols,sizeof(Symbol*),compareSymbolValue);

    // mark symbol values that are duplicate, giving preference to function symbols
    uint32_t duplicates = 0;
    bool* isDuplicate = new bool[numberOfSymbols];
    for (uint32_t i = 0; i < numberOfSymbols; i++){
        isDuplicate[i] = false;
    }

    for (int32_t i = numberOfSymbols-1; i >= 0; i--){
        int32_t j = i-1;
        while (j >= 0 && syms[i]->GET(st_value) == syms[j]->GET(st_value)){
            isDuplicate[j] = true;
            duplicates++;
            j--;
            i--;
        }
    }    

    Symbol** uniqueSyms = new Symbol*[numberOfSymbols-duplicates];
    duplicates = 0;
    for (uint32_t i = 0; i < numberOfSymbols; i++){
        if (!isDuplicate[i]){
            uniqueSyms[duplicates++] = syms[i];
        }
    }
    numberOfSymbols = duplicates;
    delete[] syms;
    delete[] isDuplicate;
    *(functionSymbols) = uniqueSyms;
    return numberOfSymbols;
}

char* TextObject::charStream(){
    ASSERT(textSection);
    uint64_t functionOffset = getAddress() -
        textSection->getElfFile()->getSectionHeader(textSection->getSectionIndex())->GET(sh_addr);
    return (char*)(textSection->charStream() + functionOffset);
}

uint32_t FreeText::digest(){
    ASSERT(!instructions && !numberOfInstructions);
    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint64_t instructionAddress;

    numberOfInstructions = 0;
    Instruction* dummyInstruction = new Instruction();
    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, dummyInstruction);
    }

    delete dummyInstruction;

    instructions = new Instruction*[numberOfInstructions];
    numberOfInstructions = 0;

    for (currByte = 0; currByte < sizeInBytes; currByte += instructionLength, numberOfInstructions++){
        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);

        instructions[numberOfInstructions] = new Instruction();
        instructions[numberOfInstructions]->setLength(MAX_X86_INSTRUCTION_LENGTH);
        instructions[numberOfInstructions]->setAddress(getAddress() + currByte);
        instructions[numberOfInstructions]->setBytes(charStream() + currByte);
        instructions[numberOfInstructions]->setIndex(numberOfInstructions);

        instructionLength = textSection->getDisassembler()->print_insn(instructionAddress, instructions[numberOfInstructions]);
        if (!instructionLength){
            instructionLength = 1;
        }
        instructions[numberOfInstructions]->setLength(instructionLength);
        instructions[numberOfInstructions]->setNextAddress();
    }

    // in case the disassembler found an instruction that exceeds the function boundary, we will
    // reduce the size of the last instruction accordingly so that the extra bytes will not be
    // used
    if (currByte > sizeInBytes){
        uint32_t extraBytes = currByte-sizeInBytes;
        instructions[numberOfInstructions-1]->setLength(instructions[numberOfInstructions-1]->getLength()-extraBytes);
        currByte -= extraBytes;
        PRINT_WARN("Disassembler found instructions that exceed the function boundary in %s by %d bytes", getName(), extraBytes);
    }

    ASSERT(currByte == sizeInBytes && "Number of bytes read does not match object size");

    return currByte;
}

void FreeText::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfInstructions; i++){
        instructions[i]->dump(binaryOutputFile,offset+currByte);
        currByte += instructions[i]->getLength();
    }
    ASSERT(currByte == sizeInBytes && "Size dumped does not match object size");
}

FreeText::FreeText(TextSection* text, uint32_t idx, uint64_t addr, uint32_t sz):
    TextObject(ElfClassTypes_FreeText, text, idx, addr, sz)
{
    instructions = NULL;
    numberOfInstructions = 0;
}

FreeText::~FreeText(){
    if (instructions){
        for (uint32_t i = 0; i < numberOfInstructions; i++){
            delete instructions[i];
        }
        delete[] instructions;
    }
}

void TextUnknown::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(sizeInBytes && "This object has no bytes thus it cannot be dumped");
    binaryOutputFile->copyBytes(charStream(),sizeInBytes,offset);
}

uint32_t TextUnknown::digest(){
    return sizeInBytes;
}

TextUnknown::TextUnknown(TextSection* text, uint32_t idx, Symbol* sym, uint64_t addr, uint32_t sz)
    : TextObject(ElfClassTypes_TextUnknown,text,idx,addr,sz)
{
    symbol = sym;
}

bool TextObject::inRange(uint64_t addr){
    if (addr >= address && addr < address + sizeInBytes){
        return true;
    }
    return false;
}

TextObject::TextObject(ElfClassTypes typ, TextSection* text, uint32_t idx, uint64_t addr, uint32_t sz) :
    Base(typ)
{
    textSection = text;
    index = idx;
    address = addr;
    sizeInBytes = sz;
}

char* TextUnknown::getName(){
    if (symbol){
        return symbol->getSymbolName();
    }
    return symbol_without_name;
}

uint64_t TextSection::getAddress() { 
    return elfFile->getSectionHeader(sectionIndex)->GET(sh_addr); 
}

bool TextSection::inRange(uint64_t addr) { 
    return elfFile->getSectionHeader(sectionIndex)->inRange(addr); 
}

TextSection::TextSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : RawSection(ElfClassTypes_TextSection,filePtr,size,scnIdx,elf)
{
    index = idx;

    sortedTextObjects = NULL;
    numberOfTextObjects = 0;

    disassembler = new Disassembler(elfFile->is64Bit());
    disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
}

uint32_t TextSection::disassemble(BinaryInputFile* binaryInputFile){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    Symbol** textSymbols = NULL;
    numberOfTextObjects = discoverTextObjects(&textSymbols);
    sortedTextObjects = new TextObject*[numberOfTextObjects];

    uint32_t fCount = 0;
    uint32_t oCount = 0;

    if (numberOfTextObjects){
        for (uint32_t i = 0; i < numberOfTextObjects-1; i++){
            uint32_t size = textSymbols[i+1]->GET(st_value) - textSymbols[i]->GET(st_value);
            if (textSymbols[i]->isFunctionSymbol(this)){
                fCount++;
                sortedTextObjects[i] = new Function(this, i, textSymbols[i], size);
                ASSERT(sortedTextObjects[i]->isFunction());
            } else if (textSymbols[i]->isTextObjectSymbol(this)){
                oCount++;
                sortedTextObjects[i] = new TextUnknown(this, i, textSymbols[i], textSymbols[i]->GET(st_value), size);
                ASSERT(!sortedTextObjects[i]->isFunction());
            } else {
                PRINT_ERROR("Unknown symbol type found to be associated with text section");
            }
        }
        // the last function ends at the end of the section

        uint32_t size = sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size) - textSymbols[numberOfTextObjects-1]->GET(st_value);
        if (textSymbols[numberOfTextObjects-1]->isFunctionSymbol(this)){
            sortedTextObjects[numberOfTextObjects-1] = 
                new Function(this, numberOfTextObjects-1, textSymbols[numberOfTextObjects-1], size);
        } else {
            sortedTextObjects[numberOfTextObjects-1] =
                new TextUnknown(this, numberOfTextObjects-1, textSymbols[numberOfTextObjects-1], textSymbols[numberOfTextObjects-1]->GET(st_value), size);
        }
    }

    // this is a text section with no functions (probably the .plt section), we will put everything into a textobject
    else{
        fCount++;
        numberOfTextObjects = 1;
        if (sortedTextObjects){
            delete[] sortedTextObjects;
        }
        sortedTextObjects = new TextObject*[numberOfTextObjects];
        sortedTextObjects[0] = new FreeText(this, 0, sectionHeader->GET(sh_addr), sectionHeader->GET(sh_size));
    }

    delete[] textSymbols;

    verify();

    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        sortedTextObjects[i]->digest();
    }
}


uint32_t TextSection::read(BinaryInputFile* binaryInputFile){
    return 0;
}


uint64_t TextSection::findInstrumentationPoint(){
    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            Function* f = (Function*)sortedTextObjects[i];
            uint64_t instAddress = f->findInstrumentationPoint();
            if (instAddress){
                return instAddress;
            }
        }
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}


uint32_t TextSection::replaceInstructions(uint64_t addr, Instruction** replacements, uint32_t numberOfReplacements, Instruction*** replacedInstructions){

    ASSERT(!*(replacedInstructions) && "This array should be empty since it will be filled by this function");

    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            Function* f = (Function*)sortedTextObjects[i];
            if (f->inRange(addr)){
                for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
                    if (f->getBasicBlock(j)->inRange(addr)){
                        return f->getBasicBlock(j)->replaceInstructions(addr,replacements,numberOfReplacements,replacedInstructions);
                    }
                }
            }
        }
    }
    PRINT_ERROR("Cannot find instructions at address 0x%llx to replace", addr);
    return 0;
}


void TextSection::printInstructions(){
    PRINT_INFOR("Printing Instructions for (text) section %d", getSectionIndex());
    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            ((Function*)sortedTextObjects[i])->printInstructions();
        }
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

    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            Function* f = (Function*)sortedTextObjects[i];
            if (f->inRange(addr)){
                return f->getInstructionAtAddress(addr);
            }
        }
    }
    return NULL;
}

bool TextSection::verify(){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    if (!numberOfTextObjects){
        return true;
    }

    for (uint32_t i = 0; i < numberOfTextObjects; i++){

        uint64_t entrAddr = sortedTextObjects[i]->getAddress();
        uint64_t exitAddr = entrAddr + sortedTextObjects[i]->getSizeInBytes();
        
        // make sure each function entry resides within the bounds of this section
        if (!sectionHeader->inRange(entrAddr)){
            sectionHeader->print();
            PRINT_ERROR("The function entry address 0x%016llx is not in the range of section %d", entrAddr, sectionHeader->getIndex());
            return false;
        }
            
        // make sure each function exit resides within the bounds of this section
        if (!sectionHeader->inRange(exitAddr) && exitAddr != sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size)){
            sortedTextObjects[i]->print();
            sectionHeader->print();
            PRINT_INFOR("Section range [0x%016llx,0x%016llx]", sectionHeader->GET(sh_addr), sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size));
            PRINT_ERROR("The function exit address 0x%016llx is not in the range of section %d", exitAddr, sectionHeader->getIndex());
            return false;
        }
    }

    for (uint32_t i = 0; i < numberOfTextObjects - 1; i++){

        // make sure sortedTextObjects is actually sorted
        if (sortedTextObjects[i]->getAddress() > sortedTextObjects[i+1]->getAddress()){
            sortedTextObjects[i]->print();
            sortedTextObjects[i+1]->print();
            PRINT_ERROR("Function addresses 0x%016llx 0x%016llx are not sorted", sortedTextObjects[i]->getAddress(), sortedTextObjects[i+1]->getAddress());
            return false;
        }
    }
        
    // make sure functions span the entire section unless it is a plt section
    if (numberOfTextObjects){
        
        // check that the first function is at the section beginning
        if (sortedTextObjects[0]->getAddress() != sectionHeader->GET(sh_addr)){
            PRINT_ERROR("First function in section %d should be at the beginning of the section", getSectionIndex());
            return false;
        }
        
        // check that function boundaries are contiguous
        for (uint32_t i = 0; i < numberOfTextObjects-1; i++){
            if (sortedTextObjects[i]->getAddress() + sortedTextObjects[i]->getSizeInBytes() !=
                sortedTextObjects[i+1]->getAddress()){
                PRINT_ERROR("In section %d, boundaries on function %d and %d do not align", getSectionIndex(), i, i+1);
                return false;
            }
        }
        
        // check the the last function ends at the section end
        if (sortedTextObjects[numberOfTextObjects-1]->getAddress() + sortedTextObjects[numberOfTextObjects-1]->getSizeInBytes() !=
            sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size)){
            PRINT_ERROR("Last function in section %d should be at the end of the section", getSectionIndex());
        }
    }

    return true;
}


void TextSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < numberOfTextObjects; i++){
        ASSERT(sortedTextObjects[i] && "The functions in this text section should be initialized");
        sortedTextObjects[i]->dump(binaryOutputFile, offset + currByte);
        currByte += sortedTextObjects[i]->getSizeInBytes();
    }
}


TextSection::~TextSection(){
    if (sortedTextObjects){
        for (uint32_t i = 0; i < numberOfTextObjects; i++){
            delete sortedTextObjects[i];
        }
        delete[] sortedTextObjects;
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
