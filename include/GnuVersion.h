#ifndef _GnuVersion_h_
#define _GnuVersion_h_

#include <Base.h>
#include <RawSection.h>
#include <defines/GnuVersion.d>
#include <Vector.h>

class GnuVerneed : public Base {
private:
    char* rawDataPtr;
    uint32_t index;

    void printVerneed();
    void printVernaux();

protected:
    bool isAuxiliary;
public:
    GnuVerneed(uint32_t idx) : Base(PebilClassType_GnuVerneed),
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
    Vector<GnuVerneed*> verneeds;
    uint32_t entrySize;

public:
    GnuVerneedTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~GnuVerneedTable();

    uint32_t getNumberOfVerneeds() { return verneeds.size(); }
    GnuVerneed* getVerneed(uint32_t idx) { return verneeds[idx]; }
    uint32_t findVersion(uint32_t ver);

    void print();
    uint32_t read(BinaryInputFile* b);
    bool verify();

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

class GnuVersymTable : public RawSection {
private:
    Vector<uint16_t> versyms;
    uint32_t entrySize;

public:
    GnuVersymTable(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~GnuVersymTable() {}

    void print();
    uint32_t read(BinaryInputFile* b);
    bool verify();

    uint32_t addSymbol(uint16_t val);
    uint16_t getSymbol(uint32_t idx) { return versyms[idx]; }
    void setSymbol(uint32_t idx, uint16_t val) { versyms[idx] = val; }
    uint32_t getNumberOfSymbols() { return versyms.size(); }

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

#endif /* _GnuVersion_h_ */
