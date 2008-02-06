#include <Instruction.h>
#include <SectHeader.h>
#include <SymbolTable.h>
#include <DemangleWrapper.h>
#include <LoaderSection.h>
#include <RawSection.h>
#include <Iterator.h>
#include <Function.h>
#include <Instruction.h>
#include <XCoffFile.h>
#include <BinaryFile.h>
#include <LineInfoFinder.h>
#include <Loop.h>


void TextSection::buildLoops(){
    PRINT_INFOR("Building Loops for section %d", header->getIndex());

    uint32_t idx = header->getIndex();

    if (!getNumberOfFunctions()){
        PRINT_INFOR("Cannot build loops for section %d -- no Functions found", idx); 
        return;
    }

    for (uint32_t i = 0; i < getNumberOfFunctions(); i++){
        getFunction(i)->buildLoops();
    }
}

RawSection::RawSection(SectHeader* h,XCoffFile* xcoff) 
    : Base(XCoffClassTypes_sect_rawdata),header(h),xCoffSymbolTable(NULL),xCoffFile(xcoff)
{
    rawDataPtr = header->getRawDataPtr();
    sizeInBytes = (uint32_t)header->getRawDataSize();
    hashCode = HashCode((uint32_t)header->getIndex());
    ASSERT(hashCode.isSection() && "FATAL : the has code for the section is not right");
}

uint32_t RawSection::read(BinaryInputFile* binaryInputFile) { 
    
    if(rawDataPtr){
        binaryInputFile->setInPointer(rawDataPtr);
        setFileOffset(binaryInputFile->currentOffset());
    }
    return sizeInBytes; 
}

void RawSection::print(){
    if(!rawDataPtr)
        return;

    PRINT_INFOR("SECTRAWDATA %s %d",header->getTypeName(),header->getIndex());
    PRINT_INFOR("\tBase  : %#llx",header->GET(s_scnptr));
    PRINT_INFOR("\tSize  : %d",sizeInBytes);
}

RawSection* RawSection::newRawSection(SectHeader* h,XCoffFile* xcoff){
    RawSection* ret = NULL;
    if(h->IS_SECT_TYPE(DEBUG)){
        ret = new DebugSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(TYPCHK) ||
              h->IS_SECT_TYPE(INFO)){
        ret = new TypeCommSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(EXCEPT)){
        ret = new ExceptionSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(LOADER)){
        ret = new LoaderSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(TEXT)){
        ret = new TextSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(DATA)){ 
        ret = new DataSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(BSS)){
        ret = new BSSSection(h,xcoff);
    } else if(h->IS_SECT_TYPE(OVRFLO) || 
              h->IS_SECT_TYPE(PAD))
    {
        ret = new RawSection(h,xcoff);
    }
    ASSERT(ret && "There is a section type which we do not know");
    return ret;
}

void DebugSection::print(){

    RawSection::print();

    DEBUG (
    PRINT_INFOR("\tDEBUGSTRINGS:");
    PRINT_INFOR("\tStrs :");

    for(uint32_t currByte=0;currByte<sizeInBytes;){

        uint16_t length32 = 0;
        uint32_t length64 = 0;
        uint32_t length = 0;

        char* ptr = (char*)(rawDataPtr+currByte);
        if(getXCoffFile()->is64Bit()){
            memcpy(&length64,ptr,sizeof(uint32_t));
            length = length64;
            currByte += sizeof(uint32_t);
            ptr += sizeof(uint32_t);
        } else {
            memcpy(&length32,ptr,sizeof(uint16_t));
            length = length32;
            currByte += sizeof(uint16_t);
            ptr += sizeof(uint16_t);
        }

        DemangleWrapper wrapper;
        char* demangled = wrapper.demangle_combined(ptr);
        ASSERT(demangled && "FATAL : demangling should always return non-null pointer");

        PRINT_INFOR("\tDBG %9d %s --- %s",currByte,ptr,demangled);

        ASSERT(((length - 1) == strlen(ptr)) && "FATAL : Somehow the string size does not match in debug");
        currByte += length;
    }
    );
}

char* DebugSection::getString(uint32_t offset){
    ASSERT(offset < sizeInBytes);
    if(!offset)
        return "";
    return rawDataPtr+offset;
}


void TypeCommSection::print(){

    RawSection::print();

    PRINT_INFOR("\tStrs :");

    for(uint32_t currByte=0;currByte<sizeInBytes;){

        char* ptr = (char*)(rawDataPtr+currByte);

        uint16_t length16 = 0;
        uint32_t length32 = 0;
        uint32_t length = 0;
        if(IS_SECT_TYPE(TYPCHK)){
            memcpy(&length16,ptr,sizeof(uint16_t));
            length = length16;
            currByte += sizeof(uint16_t);
            ptr += sizeof(uint16_t);
        } else if(IS_SECT_TYPE(INFO)) {
            memcpy(&length32,ptr,sizeof(uint32_t));
            length = length32;
            currByte += sizeof(uint32_t);
            ptr += sizeof(uint32_t);
        } else {
            ASSERT(0 && "FATAL: The section to call this can only be typecheck or comment (info)");
        }

        char* tmpStr = new char[length+1];
        strncpy(tmpStr,ptr,length);
        tmpStr[length] = '\0';
        PRINT_INFOR("%9d %s %d %d",currByte,tmpStr,length,length-(uint32_t)strlen(tmpStr));
        delete[] tmpStr;

        currByte += length;
    }
}

char* TypeCommSection::getString(uint32_t offset){
    ASSERT(offset < sizeInBytes);
    if(!offset)
        return "";
    return rawDataPtr+offset;
}

ExceptionSection::ExceptionSection(SectHeader* h,XCoffFile* xcoff) 
    : RawSection(h,xcoff),exceptions(NULL) 
{
    if(getXCoffFile()->is64Bit()){
        numberOfExceptions = sizeInBytes/Size__64_bit_ExceptionTable_Entry;
    } else {
        numberOfExceptions = sizeInBytes/Size__32_bit_ExceptionTable_Entry;
    }
}

uint32_t ExceptionSection::read(BinaryInputFile* binaryInputFile){

    exceptions = new Exception*[numberOfExceptions];

    PRINT_DEBUG("Reading the EXCEPTION table\n");

    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currSize = Size__32_bit_ExceptionTable_Entry;
    if(getXCoffFile()->is64Bit()){
        currSize = Size__64_bit_ExceptionTable_Entry;
    }

    uint32_t ret = 0;
    for(uint32_t i = 0;i<numberOfExceptions;i++){
        if(getXCoffFile()->is64Bit()){
            exceptions[i] = new Exception64();
        } else {
            exceptions[i] = new Exception32();
        }
        binaryInputFile->copyBytesIterate(exceptions[i]->charStream(),currSize);
        ret += currSize;
    }

    ASSERT((sizeInBytes == ret) && "FATAL : Somehow the number of read does not match");

    return sizeInBytes;
}

void Exception::print(SymbolTable* symbolTable,uint32_t index){
    uint8_t reason = GET(e_reason);
    uint8_t lang = GET(e_lang);

    if(!reason){
        uint32_t symbolTableIndex = GET(e_symndx);
        PRINT_INFOR("\tEXP_S [%3d] (rsn %3d) (lng %3d) (sym %9d)",index,reason,lang,symbolTableIndex);
        if(symbolTable){
            symbolTable->printSymbol(symbolTableIndex);
        }
    } else {
        uint64_t trapAddress = GET(e_paddr);
        PRINT_INFOR("\tEXP_A [%3d] (rsn %3d) (lng %3d) (adr %#llx)",index,reason,lang,trapAddress);
    }
}

void ExceptionSection::print(){
    if(!xCoffSymbolTable || !exceptions){
        RawSection::print();
        return;
    }

    PRINT_INFOR("EXCEPTIONTABLE");
    PRINT_INFOR("\tSize  : %d",sizeInBytes);
    PRINT_INFOR("\tCoun  : %d",numberOfExceptions);

    PRINT_INFOR("\tExps  :");
    for(uint32_t i=0;i<numberOfExceptions;i++){
        exceptions[i]->print(xCoffSymbolTable,i);
    }
}

uint32_t LoaderSection::read(BinaryInputFile* binaryInputFile){
    
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currSize = 0;
    if(getXCoffFile()->is64Bit()){
        header = new LSHeader64();
        currSize = Size__64_bit_Loader_Section_Header;
    } else {
        header = new LSHeader32();
        currSize = Size__32_bit_Loader_Section_Header;
    }

    binaryInputFile->copyBytesIterate(header->charStream(),currSize);

    numberOfSymbols = header->GET(l_nsyms);
    if(numberOfSymbols){
        symbolTable = new LSSymbol*[numberOfSymbols+IMPLICIT_SYM_COUNT];
        for(uint32_t i=0;i<IMPLICIT_SYM_COUNT;i++)
            symbolTable[i] = NULL;
    }

    numberOfRelocations = header->GET(l_nreloc);
    if(numberOfRelocations){
        relocationTable = new LSRelocation*[numberOfRelocations];
    }

    if(getXCoffFile()->is64Bit()){
        symbolTablePtr = rawDataPtr + header->GET(l_symoff);
        relocationTablePtr = rawDataPtr + header->GET(l_rldoff);
    } else {
        symbolTablePtr = rawDataPtr + currSize;
        relocationTablePtr = symbolTablePtr + (numberOfSymbols * Size__32_bit_Loader_Section_Symbol);
    }

    binaryInputFile->setInPointer(symbolTablePtr);
    for(uint32_t i=0;i<numberOfSymbols;i++){
        if(getXCoffFile()->is64Bit()){
            symbolTable[i+IMPLICIT_SYM_COUNT] = new LSSymbol64();
            currSize = Size__64_bit_Loader_Section_Symbol;
        } else {
            symbolTable[i+IMPLICIT_SYM_COUNT] = new LSSymbol32();
            currSize = Size__32_bit_Loader_Section_Symbol;
        }
        binaryInputFile->copyBytesIterate(symbolTable[i+IMPLICIT_SYM_COUNT]->charStream(),currSize);
    }

    binaryInputFile->setInPointer(relocationTablePtr);
    for(uint32_t i=0;i<numberOfRelocations;i++){
        if(getXCoffFile()->is64Bit()){
            relocationTable[i] = new LSRelocation64();
            currSize = Size__64_bit_Loader_Section_Relocation;
        } else {
            relocationTable[i] = new LSRelocation32();
            currSize = Size__32_bit_Loader_Section_Relocation;
        }
        binaryInputFile->copyBytesIterate(relocationTable[i]->charStream(),currSize);
    }

    fileNameTable = new LSFileNameTable(header,rawDataPtr);
    stringTable = new LSStringTable(header,rawDataPtr);

    return sizeInBytes;
}

void LoaderSection::print(){
    RawSection::print();

    PRINT_INFOR("\tLSHeader");
    header->print();

    PRINT_INFOR("\tLSSymbols");
    for(uint32_t i=0;i<numberOfSymbols;i++){
        symbolTable[i+IMPLICIT_SYM_COUNT]->print(i+IMPLICIT_SYM_COUNT,fileNameTable,stringTable);
    }

    PRINT_INFOR("\tLSRelocations");
    for(uint32_t i=0;i<numberOfRelocations;i++){
        relocationTable[i]->print(i,symbolTable,stringTable);
    }

    PRINT_INFOR("\tLSFileNameTable");
    fileNameTable->print();

    
    PRINT_INFOR("\tLSStringTable");
    stringTable->print();
}

uint32_t LoaderSection::getBSSRelocations(uint64_t* addrs){
#define BSS_SYMBOL_IN_LOADER 2
    uint32_t rlcReturn = 0;
    for(uint32_t i=0;i<numberOfRelocations;i++){
        LSRelocation* rlc = relocationTable[i];
        uint16_t secno = rlc->GET(l_symndx);
        if(secno == BSS_SYMBOL_IN_LOADER){
            addrs[rlcReturn] = rlc->GET(l_vaddr);
            rlcReturn++;
        }
    }
    return rlcReturn;
}

uint32_t TextSection::getNumberOfFunctions(){
    return numOfFunctions;
}

uint32_t TextSection::getNumberOfBlocks(){
    uint32_t ret = 0;
    for(uint32_t i=0;i<numOfFunctions;i++){
        Function* f = functions[i];
        FlowGraph* cfg = f->getFlowGraph();
        ret += (cfg ? cfg->getNumOfBasicBlocks() : 0);
    }
    return ret;
}

uint32_t TextSection::getNumberOfMemoryOps(){
    uint32_t ret = 0;
    for(uint32_t i=0;i<numOfFunctions;i++){
        Function* f = functions[i];
        FlowGraph* cfg = f->getFlowGraph();
        ret += (cfg ? cfg->getNumOfMemoryOps() : 0);
    }
    return ret;
}

uint32_t TextSection::getNumberOfFloatPOps(){
    uint32_t ret = 0;
    for(uint32_t i=0;i<numOfFunctions;i++){
        Function* f = functions[i];
        FlowGraph* cfg = f->getFlowGraph();
        ret += (cfg ? cfg->getNumOfFloatPOps() : 0);
    }
    return ret;
}

void TextSection::buildLineInfoFinder(){
    PRINT_INFOR("Building LineInfoFinder for section %d", header->getIndex());

    uint32_t idx = header->getIndex();

    if (!xCoffFile->getLineInfoTable(idx)){
        PRINT_INFOR("Cannot build LineInfoFinder for section %d -- no LineInfoTable present", idx);
        lineInfoFinder = NULL;
        return;
    }
    if (!xCoffFile->getSymbolTable()){
        PRINT_INFOR("Cannot build LineInfoFinder for section %d -- no SymbolTable present", idx);
        lineInfoFinder = NULL;
        return;
    }
    if (!xCoffFile->getStringTable()){
        PRINT_INFOR("Cannot build LineInfoFinder for section %d -- no StringTable present", idx);
        lineInfoFinder = NULL;
        return;
    }

    if(!lineInfoFinder){
        lineInfoFinder = new LineInfoFinder(header->getIndex(), xCoffFile);
    }

#ifdef LINE_INFO_TEST
    lineInfoFinder->commandLineTest(); /** only testing purposes otherwise comment it out **/
    lineInfoFinder->testLineInfoFinder();
#endif

}

void TextSection::print(){
    RawSection::print();
    for(uint32_t i=0;i<numOfFunctions;i++){
        functions[i]->print();
    }
}

uint64_t RawSection::readBytes(AddressIterator* ait){

    ASSERT(rawDataPtr && "FATAL : The section does not contain any valid data");
        
    uint64_t address = *(*ait);
    ASSERT(inRange(address) && "FATAL: the section does not conatin the address");

    ASSERT(header->GET(s_vaddr) && "FATAL : the section does not have any valid virtual address");

    uint64_t ret = 0;
    uint64_t baseAddr = header->GET(s_vaddr);
    uint64_t offset = address - baseAddr;

    char* ptr = rawDataPtr + offset;
    ret = ait->readBytes(ptr);

    return ret;
}

Instruction RawSection::readInstruction(AddressIterator* ait){
    uint32_t bits = (uint32_t)readBytes(ait);
    return Instruction(bits);
}

char* RawSection::getContentVisually(Symbol** symbols,uint32_t symbolCount,uint64_t content){
    /***** print_insn_powerpc ****/
    return NULL;
}

char* TextSection::getContentVisually(Symbol** symbols,uint32_t symbolCount,uint64_t content){
    return strdup("instruction");
}

char* DataSection::getContentVisually(Symbol** symbols,uint32_t symbolCount,uint64_t content){
    char ret[1024+16];
    ret[1023] = '\0';
    strcpy(ret,"");

    if(content){

        Symbol* targetSym = Symbol::findSymbol(symbols,symbolCount,content);
        if(targetSym){
            char* symName = xCoffSymbolTable->getSymbolName(targetSym);

            DemangleWrapper wrapper;
            char* demangled = wrapper.demangle_combined(symName);
            if(strlen(demangled) > 1023)
                strncpy(ret,demangled,1023);
            else
                strcpy(ret,demangled);

            uint64_t symVal = targetSym->GET(n_value);
            if(content != symVal){
                sprintf(ret+strlen(ret)," + %lld ",(content-symVal));
            }

            free(symName);
        }
    }
    return strdup(ret);
}

AddressIterator RawSection::getAddressIterator(){
    ASSERT(NULL && "FATAL : No Section other than Text and Data can have an address iterator");
    return AddressIterator::invalidIterator();
}

AddressIterator TextSection::getAddressIterator(){
     return AddressIterator::newAddressIteratorWord(header->GET(s_vaddr),header->GET(s_size));
}

AddressIterator DataSection::getAddressIterator(){
     if(getXCoffFile()->is64Bit())
         return AddressIterator::newAddressIteratorDouble(header->GET(s_vaddr),header->GET(s_size));
     return AddressIterator::newAddressIteratorWord(header->GET(s_vaddr),header->GET(s_size));
}

void RawSection::displaySymbols(Symbol** symbols,uint32_t symbolCount){

    header->print();

    ASSERT(Symbol::isSorted(symbols,symbolCount));

    int64_t beginIndex = -1;
    for(uint32_t i=0;i<symbolCount;i++){
        Symbol* sym = symbols[i];
        if(sym->GET(n_scnum) == header->getIndex()){
            beginIndex = i;
            break;
        }
    }
    if((beginIndex < 0) || (beginIndex >= symbolCount))
        return;

    AddressIterator ait = getAddressIterator();
    if(ait.isInvalid())
        return;

    while(ait.hasMore() && (beginIndex < symbolCount)){

        Symbol* sym = symbols[beginIndex];
        if(sym->GET(n_scnum) != header->getIndex())
            break;

        bool printOnlyFirst = false;
        uint64_t symVal = sym->GET(n_value);
        while(*ait < symVal){
            uint64_t content = readBytes(&ait);    
            if(!printOnlyFirst){
                char* contentVisual = getContentVisually(symbols,symbolCount,content);
                PRINT_INFOR("\t%#18llx:  %#18llx\t%s",*ait,content,contentVisual);
                free(contentVisual);
                printOnlyFirst = true;
            }
            ++ait;
        }

        if(*ait == symVal){
            while(*ait == symVal){
                uint64_t symLength = xCoffSymbolTable->getSymbolLength(sym);
                char* symName = xCoffSymbolTable->getSymbolName(sym);

                DemangleWrapper wrapper;
                char* demangled = wrapper.demangle_combined(symName);

                if(symLength){
                    PRINT_INFOR("<%s>: %lld",
                        demangled,symLength);
                } else {
                    PRINT_INFOR("<%s>:",demangled);
                }
                free(symName);

                beginIndex++;
                if(beginIndex >= symbolCount)
                    break;

                sym = symbols[beginIndex];
                symVal = sym->GET(n_value);
            }

            --beginIndex;

        } else {
            uint64_t symLength = xCoffSymbolTable->getSymbolLength(sym);
            char* symName = xCoffSymbolTable->getSymbolName(sym);

            DemangleWrapper wrapper;
            char* demangled = wrapper.demangle_combined(symName);

            if(symLength){
                PRINT_INFOR("\t\t<%s>: %#18llx (UN_ALGN) %lld ",
                            demangled,symVal,symLength);
            } else {
                PRINT_INFOR("\t\t<%s>: %#18llx (UN_ALGN)",demangled,symVal);
            }
            free(symName);
        }

        beginIndex++;
    }

    bool printOnlyFirst = false;
    while(ait.hasMore()){
        uint64_t content = readBytes(&ait);    
        if(!printOnlyFirst){
            char* contentVisual = getContentVisually(symbols,symbolCount,content);
            PRINT_INFOR("\t%#18llx:  %#18llx\t%s",*ait,content,contentVisual);
            free(contentVisual);
            printOnlyFirst = true;
        }
        ++ait;
    }
}

void BSSSection::displaySymbols(Symbol** symbols,uint32_t symbolCount){

    header->print();

    ASSERT(Symbol::isSorted(symbols,symbolCount));

    int64_t beginIndex = -1;
    for(uint32_t i=0;i<symbolCount;i++){
        Symbol* sym = symbols[i];
        if(sym->GET(n_scnum) == header->getIndex()){
            beginIndex = i;
            break;
        }
    }
    if((beginIndex < 0) || (beginIndex >= symbolCount))
        return;


    while(beginIndex < symbolCount){

        Symbol* sym = symbols[beginIndex];
        if(sym->GET(n_scnum) != header->getIndex())
            break;

        uint64_t symVal = sym->GET(n_value);
        uint64_t currSymVal = symVal;

        while(currSymVal == symVal){
            uint64_t symLength = xCoffSymbolTable->getSymbolLength(sym);
            char* symName = xCoffSymbolTable->getSymbolName(sym);

            DemangleWrapper wrapper;
            char* demangled = wrapper.demangle_combined(symName);

            if(symLength){
                PRINT_INFOR("<%s>: %lld",demangled,symLength);
            } else {
                PRINT_INFOR("<%s>:",demangled);
            }
            free(symName);
            beginIndex++;
            if(beginIndex >= symbolCount)
                break;

            sym = symbols[beginIndex];
            symVal = sym->GET(n_value);
        }
        PRINT_INFOR("\t%#18llx:",currSymVal);
    }
}

void RawSection::findFunctions(){
    PRINT_DEBUG("For section %d there is no need to find functions", header->getIndex());
}
void RawSection::generateCFGs(){
    PRINT_DEBUG("For section %d there is no need to generate CFGs", header->getIndex());
}

void RawSection::findMemoryFloatOps(){
    PRINT_DEBUG("For section %d there is no need to find memory operations", header->getIndex());
}

void TextSection::findFunctions(){

    PRINT_INFOR("For section %d finding the functions", header->getIndex());

    uint32_t numberOfSymbols = xCoffSymbolTable->getNumberOfSymbols();
    Symbol** symbols = new Symbol*[numberOfSymbols];
    numberOfSymbols = xCoffSymbolTable->filterSortFuncSymbols(symbols,numberOfSymbols,header);

    ASSERT(Symbol::isSorted(symbols,numberOfSymbols));

    if(!numberOfSymbols)
        return;

    uint32_t functionIndex = 0;
    Function** candidateFunctions = new Function*[numberOfSymbols];

    Function* candidateFunction = NULL;

    uint64_t prevBeginAddress = header->GET(s_vaddr);
    uint64_t prevEndAddress = header->GET(s_vaddr);

    for(uint32_t lastSymIndex = 0;lastSymIndex<numberOfSymbols;lastSymIndex++){
        
        Symbol* candidateSym = symbols[lastSymIndex];
        uint64_t candidateAddr = candidateSym->GET(n_value);

        ASSERT(candidateSym->GET(n_scnum) == header->getIndex());

        prevEndAddress = candidateAddr;

        if(candidateFunction){
            candidateFunction->updateSize(prevEndAddress-prevBeginAddress);
        } 

        uint32_t howManySameAddr = 0;
        for(uint32_t i=(lastSymIndex+1);i<numberOfSymbols;i++){
            Symbol* nextSym = symbols[i];
            if(candidateAddr == nextSym->GET(n_value)){
                howManySameAddr++;
            } else {
                break;
            }
        }

        candidateFunction = new Function(functionIndex,howManySameAddr+1,symbols+lastSymIndex,(RawSection*)this);

        lastSymIndex += howManySameAddr;

        prevBeginAddress = prevEndAddress;
        
        uint8_t candidateStorageMapping = xCoffSymbolTable->getStorageMapping(candidateSym);
        if((candidateStorageMapping != XMC_RO) && (candidateStorageMapping != XMC_DB)) {
            candidateFunctions[functionIndex++] = candidateFunction;
        } else {
            delete candidateFunction;
            candidateFunction = NULL;
        }
    }

    prevEndAddress = header->GET(s_vaddr) +  header->GET(s_size);

    if(candidateFunction){
        candidateFunction->updateSize(prevEndAddress-prevBeginAddress);
    }

    PRINT_INFOR("Total number of functions is %d",functionIndex);
    
    numOfFunctions = functionIndex;
    functions = new Function*[functionIndex];
    for(uint32_t i=0;i<functionIndex;i++){
        Function* function = candidateFunctions[i];
        function->updateInstructionSize();
        functions[i] = function;
    }

    delete[] candidateFunctions;
    delete[] symbols;
}

void TextSection::generateCFGs(){
    PRINT_INFOR("For section %d generating the CFGs", header->getIndex());
    for(uint32_t i=0;i<numOfFunctions;i++){
        Function* function = functions[i];
        function->generateCFG();
    }
}

void TextSection::findMemoryFloatOps(){
    PRINT_INFOR("For section %d finding the memory operations", header->getIndex());
    for(uint32_t i=0;i<numOfFunctions;i++){
        Function* function = functions[i];
        function->findMemoryFloatOps();
    }
}   

uint32_t TextSection::getAllBlocks(BasicBlock** arr){
    uint32_t ret = 0;
    for(uint32_t i=0;i<numOfFunctions;i++){
        Function* function = functions[i];
        FlowGraph* cfg = function->getFlowGraph();
        uint32_t n = cfg->getAllBlocks(arr);
        arr += n;
        ret += n;
    }
    return ret;
}   
