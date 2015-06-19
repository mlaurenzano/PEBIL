

#ifndef _HybridPhiElfFile_h_
#define _HybridPhiElfFile_h_

#include <ElfFile.h>

/*
 * 21 byte header
 * ElfFileSize     : 4 bytes
 * 00000000        : 4 bytes
 * iccout          : 6 bytes
 * ??              : 2 bytes
 * ????????        : 4 bytes
 * 00              : 1 bytes
 */
class IntelOffloadHeader {
public:
    static const uint32_t INTEL_OFFLOAD_HEADER_SIZE = 21;
    IntelOffloadHeader(char* bytes, uint64_t addr);
    char* charStream() { return bytes; }
    uint64_t getBaseAddress() { return baseAddress; }

    void setElfSize(uint32_t size);

private:
    char bytes[INTEL_OFFLOAD_HEADER_SIZE];
    uint64_t baseAddress;
};

/*
 *  HybridPhiElfFile
 *  An elf file for use on hybrid Xeon Phi systems
 *
 */
class HybridPhiElfFile : public ElfFile {
private:
    ElfFile* embeddedElf;
    IntelOffloadHeader* offloadHeader;

protected:

public:
    HybridPhiElfFile(char* f, char* a);

    // Locate embedded file
    ElfFile* getEmbeddedElf();
    IntelOffloadHeader* getIntelOffloadHeader() {return offloadHeader; }
};

#endif

