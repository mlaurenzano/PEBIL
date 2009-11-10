#ifndef _FileHeader_h_
#define _FileHeader_h_

#include <Base.h>
#include <defines/FileHeader.d>

class FileHeader : public Base {
protected:

public:

    FileHeader() : Base(PebilClassType_FileHeader){}
    bool verify();

    virtual ~FileHeader() {}
    FILEHEADER_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    void initFilePointers(BinaryInputFile* b);
    void print();
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
};

class FileHeader32 : public FileHeader {
protected:
    Elf32_Ehdr entry;

public:

    FILEHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    FileHeader32() { sizeInBytes = Size__32_bit_File_Header; }
    ~FileHeader32() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

class FileHeader64 : public FileHeader {
protected:
    Elf64_Ehdr entry;

public:

    FILEHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    FileHeader64() { sizeInBytes = Size__64_bit_File_Header; }
    ~FileHeader64() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

#endif /* _FileHeader_h_ */
