#ifndef _TextSection_h_
#define _TextSection_h_

#include <Base.h>
#include <RawSection.h>

class ElfFile;
class Instruction;
class BasicBlock;
class Function;
class BinaryInputFile;
class BinaryOutputFile;
class Disassembler;
class Symbol;
class TextSection;

class TextObject : public Base {
protected:
    TextSection* textSection;
    uint32_t index;
    uint64_t address;
public:
    TextObject(ElfClassTypes typ, TextSection* text, uint32_t idx, uint64_t addr, uint32_t sz);
    ~TextObject() {}

    uint32_t getIndex() { return index; }
    uint64_t getAddress() { return address; }
    bool inRange(uint64_t addr);
    char* charStream();
    bool isFunction();

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
    virtual char* getName() { __SHOULD_NOT_ARRIVE; }
    virtual uint32_t digest() { __SHOULD_NOT_ARRIVE; }
};

class TextUnknown : public TextObject {
protected:
    Symbol* symbol;
public:
    TextUnknown(TextSection* text, uint32_t idx, Symbol* sym, uint64_t addr, uint32_t sz);
    ~TextUnknown() {}

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    char* getName();
    uint32_t digest();
};


class FreeText : public TextObject {
protected:
    Instruction** instructions;
    uint32_t numberOfInstructions;
public:
    FreeText(TextSection* text, uint32_t idx, uint64_t addr, uint32_t sz);
    ~FreeText();

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    char* getName() { return NULL; }
    uint32_t digest();

    Instruction* getInstruction(uint32_t idx) { ASSERT(idx < numberOfInstructions); return instructions[idx]; }
    uint32_t getNumberOfInstructions() { return numberOfInstructions; }
};

class TextSection : public RawSection {
protected:
    uint32_t index;
    TextObject** sortedTextObjects;
    uint32_t numberOfTextObjects;

    Disassembler* disassembler;
public:
    TextSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~TextSection();

    void printInstructions();
    uint32_t discoverTextObjects(Symbol*** functionSymbols);

    uint32_t readNoFile();
    uint32_t getIndex() { return index; }
    uint32_t disassemble();
    uint32_t printDisassembledCode(bool instructionDetail);
    uint32_t read(BinaryInputFile* b);
    uint32_t disassemble(BinaryInputFile* b);

    uint64_t findInstrumentationPoint();
    Disassembler* getDisassembler() { return disassembler; }

    bool verify();
    const char* briefName() { return "TextSection"; }
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);

    Instruction* getInstructionAtAddress(uint64_t addr);

    uint64_t getAddress();
    bool inRange(uint64_t addr);

    uint32_t getNumberOfTextObjects() { return numberOfTextObjects; }
    TextObject* getTextObject(uint32_t idx) { ASSERT(idx < numberOfTextObjects && "function index is out of bounds"); return sortedTextObjects[idx]; }

    uint32_t replaceInstructions(uint64_t addr, Instruction** replacements, uint32_t numberOfReplacements, Instruction*** replacedInstructions);
};


#endif /* _TextSection_h_ */

