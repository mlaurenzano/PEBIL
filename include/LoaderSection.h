#ifndef _LoaderSection_h_
#define _LoaderSection_h

#include <Base.h>
#include <defines/LoaderSection.d>

#define IMPLICIT_SYM_COUNT 3

class LSFileNameTable;
class LSStringTable;

class LSHeader {
protected:
    LSHeader() {}
public:

    virtual void print();
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    LOADERHEADER_MACROS_BASIS("For the get_X field macros check the defines directory");
    virtual ~LSHeader() {}

    static LSHeader* newHeader(LSHeader* old,bool is64Bit,
                               uint32_t l_nsyms,
                               uint32_t l_nreloc,
                               uint32_t l_istlen,
                               uint32_t l_nimpid,
                               uint32_t l_impoff,
                               uint32_t l_stlen,
                               uint32_t l_stoff,
                               uint32_t l_symoff,
                               uint32_t l_rldoff);
};

class LSHeader32 : public LSHeader {
protected:
    LDHDR entry;
public:
    LSHeader32() {}
    ~LSHeader32() {}
    char* charStream() { return (char*)&entry; }
    
    LOADERHEADER_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class LSHeader64 : public LSHeader {
protected:
    LDHDR_64 entry;
public:
    LSHeader64() {}
    ~LSHeader64() {}
    char* charStream() { return (char*)&entry; }

    LOADERHEADER_MACROS_CLASS("For the get_X field macros check the defines directory");
    GET_FIELD_CLASS(uint64_t,l_symoff);
    GET_FIELD_CLASS(uint64_t,l_rldoff);

};

class LSSymbol {
protected:
    LSSymbol() {}
public:
    virtual void print(uint32_t index,LSFileNameTable* ft,LSStringTable* st);
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }
    virtual char* getName(LSStringTable* st) { __SHOULD_NOT_ARRIVE; return NULL; }
    virtual uint32_t getNameOffset() { __SHOULD_NOT_ARRIVE; return 0; }

    LOADERSYMBOL_MACROS_BASIS("For the get_X field macros check the defines directory");

    static LSSymbol* newSymbol(bool is64Bit,uint32_t nameOffset,uint32_t fileNameId);
    virtual ~LSSymbol() {}
};

class LSSymbol32 : public LSSymbol {
protected:
    LDSYM entry;
public:
    LSSymbol32() {}
    ~LSSymbol32() {}
    char* charStream() { return (char*)&entry; }
    char* getName(LSStringTable* st);
    uint32_t getNameOffset();

    LOADERSYMBOL_MACROS_CLASS("For the get_X field macros check the defines directory");
    GET_FIELD_CLASS(char*,l_name); \
    GET_FIELD_CLASS(uint32_t,l_zeroes); \
};

class LSSymbol64 : public LSSymbol {
protected:
    LDSYM_64 entry;
public:
    LSSymbol64() {}
    ~LSSymbol64() {}
    char* charStream() { return (char*)&entry; }
    char* getName(LSStringTable* st);
    uint32_t getNameOffset();

    LOADERSYMBOL_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class LSRelocation {
protected:
    LSRelocation() {}
public:
    void print(uint32_t index,LSSymbol** syms,LSStringTable* st);
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    LOADERRELOC_MACROS_BASIS("For the get_X field macros check the defines directory");

    static LSRelocation* newRelocation(bool is64Bit,uint64_t addr,uint32_t idx,uint32_t sectId);
    virtual ~LSRelocation() {}

};

class LSRelocation32 : public LSRelocation {
protected:
    LDREL entry;
public:
    LSRelocation32() {}
    ~LSRelocation32() {}
    char* charStream() { return (char*)&entry; }

    LOADERRELOC_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class LSRelocation64 : public LSRelocation {
protected:
    LDREL_64 entry;
public:
    LSRelocation64() {}
    ~LSRelocation64() {}
    char* charStream() { return (char*)&entry; }

    LOADERRELOC_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class LSFileNameTable {
protected:

    typedef struct {
        char* impidpath;
        char* impidbase;
        char* impidmem;
    } FileNameEntry;

    uint32_t fileNameTableSize;
    uint32_t fileNameEntryCount;
    char* fileNameTablePtr;
    FileNameEntry* fileInfos;

    LSFileNameTable() : fileNameTableSize(0),fileNameEntryCount(0),fileNameTablePtr(NULL),fileInfos(NULL) {}
    ~LSFileNameTable() {}
public:
    LSFileNameTable(LSHeader* lsHeader,char* base);
    void print();
    char* getName(uint32_t index) { ASSERT(index < fileNameEntryCount); return fileInfos[index].impidbase; }
    uint32_t getFileNameTableSize() { return fileNameTableSize; }
    char* getFileNameTablePtr() { return fileNameTablePtr; }
    uint32_t getFileNameEntryCount() { return fileNameEntryCount; }
};

class LSStringTable {
protected:
    uint32_t stringTableSize;
    char* stringTablePtr;

    LSStringTable() : stringTableSize(0),stringTablePtr(NULL) {}
    ~LSStringTable() {}
public:
    LSStringTable(LSHeader* lsHeader,char* base);
    void print();
    char* getStringCopy(uint32_t offset);
    char* getString(uint32_t offset);
    uint32_t getStringTableSize() { return stringTableSize; }
    char* getStringTablePtr() { return stringTablePtr; }
};

#endif
