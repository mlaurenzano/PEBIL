#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>
#include <defines/RawSection.d>

class SectHeader;
class SymbolTable;
class Symbol;

class LSHeader;
class LSSymbol;
class LSRelocation;
class LSFileNameTable;
class LSStringTable;
class AddressIterator;
class Function;
class XCoffFile;
class BasicBlock;
class LineInfoFinder;
class Loop;

class RawSection : public Base {
protected:
    SectHeader* header;
    char* rawDataPtr;
    SymbolTable* xCoffSymbolTable;
    HashCode hashCode;
    XCoffFile* xCoffFile;

    ~RawSection() {}
public:

    RawSection(SectHeader* h,XCoffFile* xcoff);
    static RawSection* newRawSection(SectHeader* h,XCoffFile* xcoff);

    SECTRAW_MACROS_BASIS("For the get_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* b);
    void print();
    uint64_t readBytes(AddressIterator* ait);
    Instruction readInstruction(AddressIterator* ait);

    bool inRange(uint64_t addr) { return header->inRange(addr); }

    void setSymbolTable(SymbolTable* st) { xCoffSymbolTable = st; }
    SymbolTable*  getSymbolTable() { return xCoffSymbolTable; }

    virtual void findFunctions();
    virtual void generateCFGs();
    virtual void findMemoryFloatOps();
    virtual void displaySymbols(Symbol** symbols,uint32_t symbolCount);
    virtual char* getContentVisually(Symbol** symbols,uint32_t symbolCount,uint64_t content);

    virtual AddressIterator getAddressIterator();

    uint16_t getIndex() { return header->getIndex(); }
    SectHeader* getSectHeader() { return header; }

    XCoffFile* getXCoffFile() { return xCoffFile; }

    virtual uint32_t getNumberOfFunctions() { return 0; }
    virtual uint32_t getNumberOfBlocks()    { return 0; }
    virtual uint32_t getNumberOfMemoryOps() { return 0; }
    virtual uint32_t getNumberOfFloatPOps() { return 0; }
    virtual Function* getFunction(uint32_t idx) { return NULL; }
    virtual uint32_t getAllBlocks(BasicBlock** arr) { return 0; }

    char* getRawDataPtr() { return rawDataPtr; } 

    const char* briefName() { return "RawSection"; }

    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
    uint32_t getInstrumentationSize(XCoffFileGen* xCoffGen);

    virtual void buildLineInfoFinder() {}
    virtual LineInfoFinder* getLineInfoFinder() { return NULL; }

    virtual void buildLoops() {}

};

class DebugSection : public RawSection {
protected:

    ~DebugSection() {}

public:
    DebugSection(SectHeader* h,XCoffFile* xcoff) : RawSection(h,xcoff) {}

    char* getString(uint32_t offset);

    void print();

    const char* briefName() { return "DebugSection"; }
};

class TypeCommSection : public RawSection {
protected:
    ~TypeCommSection() {}

public:
    TypeCommSection(SectHeader* h,XCoffFile* xcoff) : RawSection(h,xcoff) {}

    void print();

    char* getString(uint32_t offset);

    const char* briefName() { return "TypeCommSection"; }
};

class Exception {
protected:
    virtual ~Exception() {}
public:
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }
    virtual void print(SymbolTable* symbolTable,uint32_t index);

    EXCEPTIONRAW_MACROS_BASIS("For the get_X field macros check the defines directory");

};

class Exception32 : public Exception {
protected:
    EXCEPTAB entry;
public:
    Exception32() {}
    ~Exception32() {}
    char* charStream() { return (char*)&entry; }

    EXCEPTIONRAW_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class Exception64 : public Exception {
protected:
    EXCEPTAB_64 entry;
public:
    Exception64() {}
    ~Exception64() {}
    char* charStream() { return (char*)&entry; }

    EXCEPTIONRAW_MACROS_CLASS("For the get_X field macros check the defines directory");
};

class ExceptionSection : public RawSection {
protected:
    Exception** exceptions;
    uint32_t numberOfExceptions;

    ~ExceptionSection() {}

public:
    ExceptionSection(SectHeader* h,XCoffFile* xcoff);

    uint32_t read(BinaryInputFile* b);
    void print();

    const char* briefName() { return "ExceptionSection"; }

    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};

class LoaderSection : public RawSection {
protected:
    LSHeader* header;

    uint32_t numberOfSymbols;
    LSSymbol** symbolTable;
    char* symbolTablePtr;

    uint32_t numberOfRelocations;
    LSRelocation** relocationTable;
    char* relocationTablePtr;

    LSFileNameTable* fileNameTable;
    LSStringTable* stringTable;

    ~LoaderSection() {}

public:
    LoaderSection(SectHeader* h,XCoffFile* xcoff) : RawSection(h,xcoff),
                      header(NULL),
                      numberOfSymbols(0),symbolTable(NULL),symbolTablePtr(NULL),
                      numberOfRelocations(0),relocationTable(NULL),relocationTablePtr(NULL),
                      fileNameTable(NULL),
                      stringTable(NULL) {}

    uint32_t read(BinaryInputFile* b);
    void print();

    const char* briefName() { return "LoaderSection"; }

    uint32_t getInstrumentationSize(XCoffFileGen* xCoffGen);
    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
    uint32_t getBSSRelocations(uint64_t* addrs);
    uint32_t getRelocationCount() { return numberOfRelocations; }
};

class TextSection : public RawSection {
protected:

    Function** functions;    /** sorted according to the base address,index is function id**/
    uint32_t numOfFunctions;
    LineInfoFinder* lineInfoFinder;

    ~TextSection() {}

public:

    TextSection(SectHeader* h,XCoffFile* xcoff) : RawSection(h,xcoff),functions(NULL),
                                        numOfFunctions(0),lineInfoFinder(NULL)  {}
    void print();
    void findFunctions();
    void generateCFGs();
    void findMemoryFloatOps();
    char* getContentVisually(Symbol** symbols,uint32_t symbolCount,uint64_t content);
    AddressIterator getAddressIterator();

    uint32_t getNumberOfFunctions();
    uint32_t getNumberOfBlocks();
    uint32_t getNumberOfMemoryOps();
    uint32_t getNumberOfFloatPOps();

    Function* getFunction(uint32_t idx) { return (idx < numOfFunctions ? functions[idx] : NULL); }

    uint32_t getAllBlocks(BasicBlock** arr);

    const char* briefName() { return "TextSection"; }

    uint32_t getInstrumentationSize(XCoffFileGen* xCoffGen);
    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);

    void buildLineInfoFinder();
    LineInfoFinder* getLineInfoFinder() { return lineInfoFinder; }

    void buildLoops();

};

class DataSection : public RawSection {
protected:

    ~DataSection() {}

public:

    DataSection(SectHeader* h,XCoffFile* xcoff) : RawSection(h,xcoff) {}
    char* getContentVisually(Symbol** symbols,uint32_t symbolCount,uint64_t content);
    AddressIterator getAddressIterator();

    const char* briefName() { return "DataSection"; }

    uint32_t getInstrumentationSize(XCoffFileGen* xCoffGen);
    uint32_t instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen);
};


class BSSSection : public RawSection {
protected:

    ~BSSSection() {}

public:

    BSSSection(SectHeader* h,XCoffFile* xcoff) : RawSection(h,xcoff) {}
    void displaySymbols(Symbol** symbols,uint32_t symbolCount);

    const char* briefName() { return "BSSSection"; }
};
#endif
