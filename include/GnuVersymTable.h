#ifndef _GnuVersymTable_h_
#define _GnuVersymTable_h_

#include <RawSection.h>

class GnuVersymTable : public RawSection {
private:
    uint32_t numberOfVersyms;
    uint32_t entrySize;
    uint16_t* versyms;
public:
    GnuVersymTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~GnuVersymTable();

    void print();
    uint32_t read(BinaryInputFile* b);

    uint32_t addSymbol(uint16_t val);

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    char* charStream() { return (char*)versyms; }
    const char* briefName() { return "GnuVersymTable"; }
};

#endif /* _GnuVersymTable_h_ */
