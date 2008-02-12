#ifndef _FileHeader_h_
#define _FileHeader_h_

#include <Base.h>
#include <defines/FileHeader.d>

#define ELF_HDR_MAX_MACHINES 9

static char machine_names[ELF_HDR_MAX_MACHINES][80] = {
"No Machine",
"AT&T WE 32100",
"SPARC",
"Intel 80386",
"Motorola 68000",
"Motorola 88000",
"N/A",
"Intel 80860",
"MIPS RS3000"};

#define GET_ELF_MACH_STR(__id) machine_names[__id]


class FileHeader : public Base {
protected:
    char* programHeaderTablePtr;
    char* sectionHeaderTablePtr;

protected:
    FileHeader() : Base(ElfClassTypes_file_header),programHeaderTablePtr(0),sectionHeaderTablePtr(0){}
    virtual ~FileHeader() {}
    bool verify(uint16_t targetSize);

public:

    FILEHEADER_MACROS_BASIS("For the get_X field macros check the defines directory");

    void initFilePointers(BinaryInputFile* b);
    void print() { __SHOULD_NOT_ARRIVE; }
    char* getProgramHeaderTablePtr() { return programHeaderTablePtr; }
    char* getSectionHeaderTablePtr() { return sectionHeaderTablePtr; }
    const char* briefName() { return "FileHeader"; }
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
};

class FileHeader32 : public FileHeader {
protected:
    Elf32_Ehdr entry;

public:

    FILEHEADER_MACROS_CLASS("For the get_X field macros check the defines directory");

    FileHeader32() { sizeInBytes = Size__32_bit_File_Header; }
    ~FileHeader32() {}
    uint32_t read(BinaryInputFile* b);
    void print();

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
//    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};

class FileHeader64 : public FileHeader {
protected:
    Elf64_Ehdr entry;

public:

    FILEHEADER_MACROS_CLASS("For the get_X field macros check the defines directory");

    FileHeader64() { sizeInBytes = Size__64_bit_File_Header; }
    ~FileHeader64() {}
    uint32_t read(BinaryInputFile* b);
    void print();

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
//    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};

#endif /* _FileHeader_h_ */
