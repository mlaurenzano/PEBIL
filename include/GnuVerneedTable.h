#ifndef _GnuVerneedTable_h_
#define _GnuVerneedTable_h_

#include <defines/GnuVerneed.d>
#include <Base.h>
#include <RawSection.h>

class GnuVerneed : public Base {
private:
    char* rawDataPtr;
    uint32_t index;

    void printVerneed();
    void printVernaux();

protected:
    bool isAuxiliary;
public:
    GnuVerneed(uint32_t idx) : Base(ElfClassTypes_GnuVerneed),
        rawDataPtr(NULL), index(idx) {}
    ~GnuVerneed() {}

    GNUVERNEED_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");
    GNUVERNAUX_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    bool isAuxiliaryEntry() { return isAuxiliary; }

    void print();
    bool verify();

    virtual char* charStream() { __SHOULD_NOT_ARRIVE; }
};

class GnuVerneed32 : public GnuVerneed {
protected:
    Elf32_Verneed entry;

public:
    GNUVERNEED_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    GnuVerneed32(uint32_t idx) : GnuVerneed(idx) { isAuxiliary = false; }
    ~GnuVerneed32() {}
    char* charStream() { return (char*)&entry; }
};

class GnuVerneed64 : public GnuVerneed {
protected:
    Elf64_Verneed entry;

public:
    GNUVERNEED_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    GnuVerneed64(uint32_t idx)  : GnuVerneed(idx) { isAuxiliary = false; }
    ~GnuVerneed64() {}
    char* charStream() { return (char*)&entry; }
};

class GnuVernaux32 : public GnuVerneed {
protected:
    Elf32_Vernaux entry;

public:
    GNUVERNAUX_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    GnuVernaux32(uint32_t idx) : GnuVerneed(idx) { isAuxiliary = true; }
    ~GnuVernaux32() {}
    char* charStream() { return (char*)&entry; }
};

class GnuVernaux64 : public GnuVerneed {
protected:
    Elf64_Vernaux entry;

public:
    GNUVERNAUX_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    GnuVernaux64(uint32_t idx)  : GnuVerneed(idx) { isAuxiliary = true; }
    ~GnuVernaux64() {}
    char* charStream() { return (char*)&entry; }
};

class GnuVerneedTable : public RawSection {
private:
    uint32_t numberOfVerneeds;
    GnuVerneed** verneeds;
    uint32_t entrySize;

public:
    GnuVerneedTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~GnuVerneedTable();

    uint32_t getNumberOfVerneeds() { return numberOfVerneeds; }
    GnuVerneed* getVerneed(uint32_t idx) { ASSERT(idx < numberOfVerneeds); return verneeds[idx]; }
    uint32_t findVersion(uint32_t ver);

    void print();
    uint32_t read(BinaryInputFile* b);

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    const char* briefName() { return "GnuVerneedTable"; }
};

#endif /* _GnuVerneedTable_h_ */
