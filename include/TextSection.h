#ifndef _TextSection_h_
#define _TextSection_h_

#include <Base.h>
#include <RawSection.h>
#include <BinaryFile.h>
#include <Function.h>
#include <BasicBlock.h>
#include <Disassembler.h>
#include <SymbolTable.h>

class ElfFile;
class Instruction;
class Function;

class TextSection : public RawSection {
protected:
    uint32_t index;
    Function** sortedFunctions;
    uint32_t numberOfFunctions;

    Disassembler* disassembler;
public:
    TextSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~TextSection();

    void printInstructions();
    uint32_t discoverFunctionSymbols(Symbol*** functionSymbols);

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

    uint64_t getAddress() { return elfFile->getSectionHeader(sectionIndex)->GET(sh_addr); }
    bool inRange(uint64_t addr) { return elfFile->getSectionHeader(sectionIndex)->inRange(addr); }

    uint32_t getNumberOfFunctions() { return numberOfFunctions; }
    Function* getFunction(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfFunctions && "function index is out of bounds"); return sortedFunctions[idx]; }

    uint32_t replaceInstructions(uint64_t addr, Instruction** replacements, uint32_t numberOfReplacements, Instruction*** replacedInstructions);
};


#endif /* _TextSection_h_ */

