#include <TextSection.h>

#include <BasicBlock.h>
#include <CStructuresX86.h>
#include <Disassembler.h>
#include <ElfFile.h>
#include <Function.h>
#include <Instruction.h>
#include <SectionHeader.h>
#include <SymbolTable.h>

uint32_t FreeText::getNumberOfInstructions(){
    uint32_t numberOfInstructions = 0;
    for (uint32_t i = 0; i < blocks.size(); i++){
        if (blocks[i]->getType() == ElfClassTypes_BasicBlock){
            numberOfInstructions += ((BasicBlock*)blocks[i])->getNumberOfInstructions();
        }
    }
    return numberOfInstructions;
}

char* TextObject::getName(){
    if (symbol){
        return symbol->getSymbolName();
    }
    return symbol_without_name;
}

uint32_t TextSection::printDisassembly(bool instructionDetail){
    ASSERT(elfFile && "Text section should be linked to its corresponding ElfFile object");

    Base::disassembler->setPrintFunction((fprintf_ftype)fprintf,stdout);

    fprintf(stdout, "Disassembly of section %s\n\n", getSectionHeader()->getSectionNamePtr());

    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        sortedTextObjects[i]->printDisassembly(instructionDetail);
        fprintf(stdout, "\n");
    }

    Base::disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);
}

void FreeText::printDisassembly(bool instructionDetail){
    fprintf(stdout, "%llx <free -- %s>:\n", getBaseAddress(), getName());

    for (uint32_t i = 0; i < blocks.size(); i++){
        blocks[i]->printDisassembly(instructionDetail);
    }
}

uint32_t FreeText::getAllInstructions(Instruction** allinsts, uint32_t nexti){
    uint32_t instructionCount = 0;

    for (uint32_t i = 0; i < blocks.size(); i++){
        if (blocks[i]->getType() == ElfClassTypes_BasicBlock){
            BasicBlock* bb = (BasicBlock*)blocks[i];
            bb->getAllInstructions(allinsts,nexti+instructionCount);
            instructionCount += bb->getNumberOfInstructions();
        }
    }
    return instructionCount;
}

uint32_t TextSection::getAllInstructions(Instruction** allinsts, uint32_t nexti){
    uint32_t instructionCount = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        instructionCount += sortedTextObjects[i]->getAllInstructions(allinsts, instructionCount+nexti);
    }
    ASSERT(instructionCount == getNumberOfInstructions());
    return instructionCount;
}

Function* TextSection::replaceFunction(uint32_t idx, Function* replacementFunction){
    ASSERT(idx < sortedTextObjects.size() && "function index out of bounds");
    ASSERT(sortedTextObjects[idx]->isFunction() && "function index is not a function");

    Function* toReplace = (Function*)sortedTextObjects[idx];
    ASSERT(toReplace->getNumberOfBytes() == replacementFunction->getNumberOfBytes());

    sortedTextObjects.assign(replacementFunction,idx);
    return toReplace;
}

uint32_t TextSection::getNumberOfBasicBlocks(){
    uint32_t numberOfBasicBlocks = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->isFunction()){
            numberOfBasicBlocks += ((Function*)sortedTextObjects[i])->getFlowGraph()->getNumberOfBasicBlocks();
        }
    }
    return numberOfBasicBlocks;
}

uint32_t TextSection::getNumberOfInstructions(){
    uint32_t numberOfInstructions = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        numberOfInstructions += sortedTextObjects[i]->getNumberOfInstructions();
    }
    return numberOfInstructions;
}

uint32_t TextSection::getNumberOfMemoryOps(){
    uint32_t numberOfMemoryOps = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->isFunction()){
            numberOfMemoryOps += ((Function*)sortedTextObjects[i])->getFlowGraph()->getNumberOfMemoryOps();
        }
    }
    return numberOfMemoryOps;
}

uint32_t TextSection::getNumberOfFloatOps(){
    uint32_t numberOfFloatOps = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->isFunction()){
            numberOfFloatOps += ((Function*)sortedTextObjects[i])->getFlowGraph()->getNumberOfFloatOps();
        }
    }
    return numberOfFloatOps;
}

ByteSources TextSection::setByteSource(ByteSources src){
    source = src;
    return source;
}

ByteSources TextSection::getByteSource(){
    return source;
}

uint32_t TextSection::buildLoops(){
    uint32_t numberOfLoops = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->isFunction()){
            numberOfLoops += ((Function*)sortedTextObjects[i])->getFlowGraph()->buildLoops();
        }
    }
    return numberOfLoops;
}

void FreeText::print(){
    PRINT_INFOR("Free Text area at address %#llx", baseAddress);
}

void TextSection::printLoops(){
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->isFunction()){
            ((Function*)sortedTextObjects[i])->getFlowGraph()->printLoops();
        }
    }
}

bool TextObject::isFunction(){
    return (getType() == ElfClassTypes_Function);
}


Vector<Symbol*> TextSection::discoverTextObjects(){
    Vector<Symbol*> functionSymbols;

    ASSERT(!functionSymbols.size() && "This array should be empty since it is loaded by this function");

    // count the number of symbols for this text section
    uint32_t numberOfSymbols = 0;
    for (uint32_t i = 0; i < elfFile->getNumberOfSymbolTables(); i++){
        SymbolTable* symbolTable = elfFile->getSymbolTable(i);
        if (!symbolTable->isDynamic()){
            for (uint32_t j = 0; j < symbolTable->getNumberOfSymbols(); j++){
                Symbol* symbol = symbolTable->getSymbol(j);
                if (symbol->isFunctionSymbol(this)){
                    functionSymbols.append(symbol);
                } else if (symbol->isTextObjectSymbol(this)){
                    functionSymbols.append(symbol);
                }
            }
        }
    }

    // sort text symbols in into decreasing order
    qsort(&functionSymbols,functionSymbols.size(),sizeof(Symbol*),compareSymbolValue);

    // delete symbol values that have duplicate values
    functionSymbols.reverse();
    if (functionSymbols.size()){
        for (uint32_t i = 0; i < functionSymbols.size()-1; i++){
            while (functionSymbols.size() > i+1 && functionSymbols[i+1]->GET(st_value) == functionSymbols[i]->GET(st_value)){
                functionSymbols.remove(i+1);
            }
        }
    }
    functionSymbols.reverse();

    return functionSymbols;
}

char* TextObject::charStream(){
    ASSERT(textSection);
    uint64_t functionOffset = getBaseAddress() -
        textSection->getElfFile()->getSectionHeader(textSection->getSectionIndex())->GET(sh_addr);
    return (char*)(textSection->charStream() + functionOffset);
}

Vector<Instruction*>* TextObject::digestLinear(){
    Vector<Instruction*>* allInstructions = new Vector<Instruction*>();

    uint32_t currByte = 0;
    uint32_t instructionLength = 0;
    uint64_t instructionAddress;

    PRINT_DEBUG_CFG("Digesting textobject linearly");

    uint32_t numberOfInstructions = 0;
    while (currByte < sizeInBytes){

        instructionAddress = (uint64_t)((uint64_t)charStream() + currByte);
        Instruction* newInstruction = new Instruction(textSection, getBaseAddress() + currByte, 
                                                      charStream() + currByte, ByteSource_Application_FreeText, numberOfInstructions++);
        PRINT_DEBUG_CFG("linear cfg: instruction at %#llx with %d bytes", newInstruction->getBaseAddress(), newInstruction->getSizeInBytes());

        (*allInstructions).append(newInstruction);
        currByte += newInstruction->getSizeInBytes();
    }

    // in case the disassembler found an instruction that exceeds the function boundary, we will
    // reduce the size of the last instruction accordingly so that the extra bytes will not be
    // used
    if (currByte > sizeInBytes){
        uint32_t extraBytes = currByte-sizeInBytes;
        (*allInstructions).back()->setSizeInBytes((*allInstructions).back()->getSizeInBytes()-extraBytes);
        currByte -= extraBytes;

        char oType[9];
        if (getType() == ElfClassTypes_FreeText){
            sprintf(oType, "%s", "FreeText\0");
        } else if (getType() == ElfClassTypes_Function){
            sprintf(oType, "%s", "Function\0");
        }

        PRINT_WARN(3,"Found instructions that exceed the %s boundary in %.24s by %d bytes", oType, getName(), extraBytes);
    }

    ASSERT(currByte == sizeInBytes && "Number of bytes read does not match object size");

    return allInstructions;   
}

uint32_t FreeText::digest(){
    ASSERT(!blocks.size());
    if (usesInstructions){
        PRINT_DEBUG_CFG("\tdigesting freetext instructions at %#llx", getBaseAddress());
        Vector<Instruction*>* allInstructions = digestLinear();
        ASSERT(allInstructions);
        (*allInstructions).sort(compareBaseAddress);

        CodeBlock* cb = new CodeBlock(0, NULL);
        cb->setBaseAddress(getBaseAddress());
        for (uint32_t i = 0; i < (*allInstructions).size(); i++){
            cb->addInstruction((*allInstructions)[i]);
        }
        blocks.append(cb);
        delete allInstructions;
    } else {
        PRINT_DEBUG_CFG("\tdigesting freetext unknown area at %#llx", getBaseAddress());
        RawBlock* ub = new RawBlock(0, NULL, textSection->getStreamAtAddress(getBaseAddress()),
                                            sizeInBytes, getBaseAddress());
        blocks.append(ub);
    }
    return sizeInBytes;
}

void FreeText::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < blocks.size(); i++){
        blocks[i]->dump(binaryOutputFile,offset+currByte);
        currByte += blocks[i]->getNumberOfBytes();
    }
    ASSERT(currByte == sizeInBytes && "Size dumped does not match object size");
}

FreeText::FreeText(TextSection* text, uint32_t idx, Symbol* sym, uint64_t addr, uint32_t sz, bool usesI)
    : TextObject(ElfClassTypes_FreeText, text, idx, sym, addr, sz)
{
    usesInstructions = usesI;
}

FreeText::~FreeText(){
    for (uint32_t i = 0; i < blocks.size(); i++){
        delete blocks[i];
    }
}

bool TextObject::inRange(uint64_t addr){
    if (addr >= baseAddress && addr < baseAddress + sizeInBytes){
        return true;
    }
    return false;
}

TextObject::TextObject(ElfClassTypes typ, TextSection* text, uint32_t idx, Symbol* sym, uint64_t addr, uint32_t sz) :
    Base(typ)
{
    symbol = sym;
    textSection = text;
    index = idx;
    baseAddress = addr;
    sizeInBytes = sz;
}


uint64_t TextSection::getBaseAddress() { 
    return elfFile->getSectionHeader(sectionIndex)->GET(sh_addr); 
}

bool TextSection::inRange(uint64_t addr) { 
    return elfFile->getSectionHeader(sectionIndex)->inRange(addr); 
}

TextSection::TextSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf, ByteSources src) :
    RawSection(ElfClassTypes_TextSection,filePtr,size,scnIdx,elf)
{
    index = idx;

    Base::disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf,stdout);

    source = src;
}

uint32_t TextSection::disassemble(BinaryInputFile* binaryInputFile){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());

    Vector<Symbol*> textSymbols = discoverTextObjects();

    if (textSymbols.size()){
        uint32_t i;
        for (i = 0; i < textSymbols.size()-1; i++){
            uint32_t size = textSymbols[i+1]->GET(st_value) - textSymbols[i]->GET(st_value);
            if (textSymbols[i]->isFunctionSymbol(this)){
                sortedTextObjects.append(new Function(this, i, textSymbols[i], size));
                ASSERT(sortedTextObjects.back()->isFunction());
            } else if (textSymbols[i]->isTextObjectSymbol(this)){
                sortedTextObjects.append(new FreeText(this, i, textSymbols[i], textSymbols[i]->GET(st_value), size, false));
                ASSERT(!sortedTextObjects.back()->isFunction());
            } else {
                PRINT_ERROR("Unknown symbol type found to be associated with text section");
            }
        }

        // the last function ends at the end of the section
        uint32_t size = sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size) - textSymbols.back()->GET(st_value);
        if (textSymbols.back()->isFunctionSymbol(this)){
            sortedTextObjects.append(new Function(this, i, textSymbols.back(), size));
        } else {
            sortedTextObjects.append(new FreeText(this, i, textSymbols.back(), textSymbols.back()->GET(st_value), size, false));
        }
    }

    // this is a text section with no functions (probably the .plt section), so we will put everything into a single textobject
    else{
        sortedTextObjects.append(new FreeText(this, 0, NULL, sectionHeader->GET(sh_addr), sectionHeader->GET(sh_size), true));
    }

    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
#ifdef DEBUG_CFG
        if (sortedTextObjects[i]->isFunction()){
            PRINT_DEBUG_CFG("Digesting function object at %#llx", sortedTextObjects[i]->getBaseAddress());
        } else {
            PRINT_DEBUG_CFG("Digesting gentext object at %#llx", sortedTextObjects[i]->getBaseAddress());
        }
#endif
        sortedTextObjects[i]->digest();
    }

    verify();

    return sortedTextObjects.size();
}


uint32_t TextSection::read(BinaryInputFile* binaryInputFile){
    return 0;
}


uint64_t TextSection::findInstrumentationPoint(uint32_t size, InstLocations loc){
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            Function* f = (Function*)sortedTextObjects[i];
            uint64_t instAddress = f->findInstrumentationPoint(size,loc);
            if (instAddress){
                return instAddress;
            }
        }
    }
    PRINT_ERROR("There should be an instrumentation point in text section %d", getSectionIndex());
    __SHOULD_NOT_ARRIVE;
    return 0;
}


Vector<Instruction*>* TextSection::swapInstructions(uint64_t addr, Vector<Instruction*>* replacements){
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            Function* f = (Function*)sortedTextObjects[i];
            if (f->inRange(addr)){
                return f->swapInstructions(addr, replacements);
                for (uint32_t j = 0; j < f->getNumberOfBasicBlocks(); j++){
                    if (f->getBasicBlock(j)->inRange(addr)){
                        return f->getBasicBlock(j)->swapInstructions(addr,replacements);
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
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        if (sortedTextObjects[i]->getType() == ElfClassTypes_Function){
            ((Function*)sortedTextObjects[i])->printInstructions();
        }
    }
}


Instruction* TextSection::getInstructionAtAddress(uint64_t addr){
    SectionHeader* sectionHeader = elfFile->getSectionHeader(getSectionIndex());
    if (!sectionHeader->inRange(addr)){
        return NULL;
    }

    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
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

    if (sortedTextObjects.size()){

        for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
            
            uint64_t entrAddr = sortedTextObjects[i]->getBaseAddress();
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

        for (uint32_t i = 0; i < sortedTextObjects.size() - 1; i++){
            
            // make sure sortedTextObjects is actually sorted
            if (sortedTextObjects[i]->getBaseAddress() > sortedTextObjects[i+1]->getBaseAddress()){
                sortedTextObjects[i]->print();
                sortedTextObjects[i+1]->print();
                PRINT_ERROR("Function addresses 0x%016llx 0x%016llx are not sorted", sortedTextObjects[i]->getBaseAddress(), sortedTextObjects[i+1]->getBaseAddress());
                return false;
            }
        }
        
        // make sure functions span the entire section unless it is a plt section
        if (sortedTextObjects.size()){
            
            // check that the first function is at the section beginning
            if (sortedTextObjects[0]->getBaseAddress() != sectionHeader->GET(sh_addr)){
                PRINT_ERROR("First function in section %d should be at the beginning of the section", getSectionIndex());
                return false;
            }
            
            // check that function boundaries are contiguous
            for (uint32_t i = 0; i < sortedTextObjects.size()-1; i++){
                if (sortedTextObjects[i]->getBaseAddress() + sortedTextObjects[i]->getSizeInBytes() !=
                    sortedTextObjects[i+1]->getBaseAddress()){
                    PRINT_ERROR("In section %d, boundaries on function %d and %d do not align", getSectionIndex(), i, i+1);
                    return false;
                }
            }
            
            // check the the last function ends at the section end
            if (sortedTextObjects[sortedTextObjects.size()-1]->getBaseAddress() + sortedTextObjects[sortedTextObjects.size()-1]->getSizeInBytes() !=
                sectionHeader->GET(sh_addr) + sectionHeader->GET(sh_size)){
                PRINT_ERROR("Last function in section %d should be at the end of the section", getSectionIndex());
                return false;
            }
        }
    }

    return true;
}


void TextSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        ASSERT(sortedTextObjects[i] && "The functions in this text section should be initialized");
        sortedTextObjects[i]->dump(binaryOutputFile, offset + currByte);
        currByte += sortedTextObjects[i]->getSizeInBytes();
    }
}


TextSection::~TextSection(){
    for (uint32_t i = 0; i < sortedTextObjects.size(); i++){
        delete sortedTextObjects[i];
    }
}

