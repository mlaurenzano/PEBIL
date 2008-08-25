#ifndef _TextSection_h_
#define _TextSection_h_

#include <Base.h>
#include <RawSection.h>
#include <BinaryFile.h>
#include <Function.h>

class ElfFile;
class Instruction;

class TextSection : public RawSection {
protected:
    uint32_t numberOfInstructions;
    Instruction** instructions;
    uint32_t index;
    Function** sortedFunctions;
    uint32_t numberOfFunctions;
    
public:
    TextSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
        : RawSection(ElfClassTypes_TextSection,filePtr,size,scnIdx,elf),index(idx),numberOfInstructions(0),instructions(NULL),sortedFunctions(NULL),numberOfFunctions(0) {}

    ~TextSection();

    void printInstructions();

    uint32_t readNoFile();
    uint32_t getIndex() { return index; }
    uint32_t disassemble();
    uint32_t printDisassembledCode();
    uint32_t read(BinaryInputFile* b);

    bool verify();
    const char* briefName() { return "TextSection"; }
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t getNumberOfInstruction() { return numberOfInstructions; }
    Instruction* getInstruction(uint32_t idx);
    Instruction* getInstructionAtAddress(uint64_t addr);

    uint64_t getAddress() { return elfFile->getSectionHeader(sectionIndex)->GET(sh_addr); }

    uint32_t findFunctions();
    uint32_t getNumberOfFunctions() { return numberOfFunctions; }
    Function* getFunction(uint32_t idx) { ASSERT(idx >= 0 && idx < numberOfFunctions && "function index is out of bounds"); return sortedFunctions[idx]; }

    uint32_t replaceInstructions(uint64_t addr, Instruction** replacements, uint32_t numberOfReplacements, Instruction*** replacedInstructions);
};


#endif /* _TextSection_h_ */

