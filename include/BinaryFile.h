#ifndef _BinaryFile_h_
#define _BinaryFile_h_

#include <Base.h>

class BinaryInputFile {
private:
    char*        inBufferPointer;
    uint32_t     inBufferSize;
    char*        inBuffer;
public:
    BinaryInputFile() : inBufferPointer(NULL),inBufferSize(0),inBuffer(NULL) {}
    ~BinaryInputFile();

    void     readFileInMemory(char* f);

    char*    copyBytes(void* buff,uint32_t bytes);
    char*    copyBytesIterate(void* buff,uint32_t bytes);
    char*    onlyIterate(uint32_t bytes);
    char*    moreBytes();

    char*    fileOffsetToPointer(uint64_t fileOffset);
    void     setInBufferPointer(uint64_t fileOffset);

    char*    isInBuffer(char* f);
    void     setInPointer(char* f);

    char*    inPtrBase() { return inBuffer; }

    uint32_t alreadyRead() { return (uint32_t)(inBufferPointer-inBuffer); }
    uint32_t bytesLeftInBuffer();

    uint32_t getSize() { return inBufferSize; }

    uint32_t currentOffset() { return (uint32_t)(inBufferPointer-inBuffer); }
};


class BinaryOutputFile {
private:
    FILE* outFile;
    char* fileName;
public:

    BinaryOutputFile() : outFile(NULL),fileName(NULL) {}
    ~BinaryOutputFile();

    void open(char* flnm);
    bool operator!();
    void copyBytes(char* buffer,uint32_t size,uint32_t offset);
    uint32_t alreadyWritten();
    void close();
};

#endif
