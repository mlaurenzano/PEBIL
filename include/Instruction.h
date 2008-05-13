#ifndef _Instruction_h_
#define _Instruction_h_

#include <Base.h>
#include <BinaryFile.h>

class ElfFile;

class Instruction : public Base {
protected:
    char* rawBytes;
    uint32_t instructionLength;

public:
    Instruction(uint32_t size, char* bytes) { sizeInBytes = size; }
    ~Instruction() {}

    uint32_t read(BinaryInputFile* b) {}
    void print();

    char* charStream() { return rawBytes; }
    uint64_t nextAddress();
};
#endif /* _Instruction_h_ */
