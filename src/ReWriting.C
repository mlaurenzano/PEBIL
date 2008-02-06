#include <Instruction.h>
#include <FileHeader.h>
#include <AOutHeader.h>
#include <SectHeader.h>
#include <LoaderSection.h>
#include <RawSection.h>
#include <SymbolTable.h>
#include <StringTable.h>
#include <RelocationTable.h>
#include <LineInfoTable.h>
#include <XCoffFile.h>
#include <Iterator.h>
#include <BinaryFile.h>
#include <Function.h>
#include <Generate.h>
#include <Auxilary.h>

/**** ----------------------------------------------------------------- ****/
uint32_t FileHeader32::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("FileHeader  Instrumentation ");

    FILHDR newEntry = entry;
    newEntry.f_symptr = xCoffGen->getSymbolTableOffsetForInst();

    memcpy(buffer,&newEntry,sizeInBytes); 
    return sizeInBytes;
}
uint32_t FileHeader64::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("FileHeader  Instrumentation ");

    FILHDR_64 newEntry = entry;
    newEntry.f_symptr = xCoffGen->getSymbolTableOffsetForInst();

    memcpy(buffer,&newEntry,sizeInBytes); 
    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/**** ----------------------------------------------------------------- ****/
uint32_t AOutHeader32::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("AOutHeader  Instrumentation ");

    AOUTHDR newEntry = entry;
    newEntry.o_tsize = xCoffGen->getRawSectionSizeForInst(GET(o_sntext));
    newEntry.o_dsize = xCoffGen->getRawSectionSizeForInst(GET(o_sndata));

    memcpy(buffer,&newEntry,sizeInBytes); 
    return sizeInBytes;
}
uint32_t AOutHeader64::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("AOutHeader  Instrumentation ");

    AOUTHDR_64 newEntry = entry;
    newEntry.o_tsize = xCoffGen->getRawSectionSizeForInst(GET(o_sntext));
    newEntry.o_dsize = xCoffGen->getRawSectionSizeForInst(GET(o_sndata));

    memcpy(buffer,&newEntry,sizeInBytes); 
    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/**** ----------------------------------------------------------------- ****/
uint32_t SectHeader32::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("SectHeader  Instrumentation ");

    SCNHDR newEntry = entry;
    if(!IS_SECT_TYPE(OVRFLO)){
        newEntry.s_size = xCoffGen->getRawSectionSizeForInst(getIndex());
        newEntry.s_scnptr = xCoffGen->getRawSectionOffsetForInst(getIndex());
        newEntry.s_relptr = xCoffGen->getRelocOffsetForInst(getIndex());
        newEntry.s_lnnoptr = xCoffGen->getLineInfoOffsetForInst(getIndex());
    } else {
        newEntry.s_relptr = xCoffGen->getRelocOffsetForInst(GET(s_nreloc));
        newEntry.s_lnnoptr = xCoffGen->getLineInfoOffsetForInst(GET(s_nreloc));
    }

    if(IS_SECT_TYPE(BSS)){
        newEntry.s_size = GET(s_size);
        newEntry.s_paddr = xCoffGen->getNewBSSSectionVAddressForInst();
        newEntry.s_vaddr = newEntry.s_paddr;
    }

    if(IS_SECT_TYPE(OVRFLO)){
        ASSERT(!newEntry.s_size && "Fatal: Size of OVRFLO has to be 0");
        ASSERT(!newEntry.s_scnptr && "Fatal: Sect pointer of OVRFLO has to be 0");
    }
    memcpy(buffer,&newEntry,sizeInBytes); 

    return sizeInBytes;
}
uint32_t SectHeader64::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("SectHeader  Instrumentation ");

    SCNHDR_64 newEntry = entry;
    if(!IS_SECT_TYPE(OVRFLO)){
        newEntry.s_size = xCoffGen->getRawSectionSizeForInst(getIndex());
        newEntry.s_scnptr = xCoffGen->getRawSectionOffsetForInst(getIndex());
        newEntry.s_relptr = xCoffGen->getRelocOffsetForInst(getIndex());
        newEntry.s_lnnoptr = xCoffGen->getLineInfoOffsetForInst(getIndex());
    } else {
        newEntry.s_relptr = xCoffGen->getRelocOffsetForInst(GET(s_nreloc));
        newEntry.s_lnnoptr = xCoffGen->getLineInfoOffsetForInst(GET(s_nreloc));
    }

    if(IS_SECT_TYPE(BSS)){
        newEntry.s_size = GET(s_size);
        newEntry.s_paddr = xCoffGen->getNewBSSSectionVAddressForInst();
        newEntry.s_vaddr = newEntry.s_paddr;
    }

    if(IS_SECT_TYPE(OVRFLO)){
        ASSERT(!newEntry.s_size && "Fatal: Size of OVRFLO has to be 0");
        ASSERT(!newEntry.s_scnptr && "Fatal: Sect pointer of OVRFLO has to be 0");
    }
    memcpy(buffer,&newEntry,sizeInBytes); 

    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/**** ----------------------------------------------------------------- ****/
uint32_t LineInfoTable::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("LineInfoTable  Instrumentation ");

    uint32_t totalBytesWritten = 0;

    uint32_t currSize = Size__32_bit_LineInfoTable_Entry;
    if(getXCoffFile()->is64Bit()){
        currSize = Size__64_bit_LineInfoTable_Entry;
    }

    char* currPtr = buffer;
    for(uint32_t i=0;i<numOfLineInfos;i++){
        memcpy(currPtr,lineInfos[i]->charStream(),currSize);
        currPtr += currSize;
        totalBytesWritten += currSize;
    }

    ASSERT((sizeInBytes == totalBytesWritten) && 
           "FATAL: The bytes written should match the size of the linenumbers");

    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/**** ----------------------------------------------------------------- ****/
uint32_t RelocationTable::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("RelocationTable  Instrumentation ");

    uint32_t totalBytesWritten = 0;

    uint32_t currSize = Size__32_bit_RelocationTable_Entry;
    if(getXCoffFile()->is64Bit()){
        currSize = Size__64_bit_RelocationTable_Entry;
    }

    char* currPtr = buffer;
    for(uint32_t i=0;i<numOfRelocations;i++){
        memcpy(currPtr,relocations[i]->charStream(),currSize);
        currPtr += currSize;
        totalBytesWritten += currSize;
    }

    ASSERT((sizeInBytes == totalBytesWritten) && 
           "FATAL: The bytes written should match the size of the relocs");

    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/**** ----------------------------------------------------------------- ****/
int compareOriginalFileOffsets(const void* arg1,const void* arg2){
    BaseGen* gen1 = *((BaseGen**)arg1);
    BaseGen* gen2 = *((BaseGen**)arg2);
    Base* base1 = gen1->getParsedBase();
    Base* base2 = gen2->getParsedBase();
    uint32_t vl1 = base1->getFileOffset();
    uint32_t vl2 = base2->getFileOffset();

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    return 0;
}

BaseGen* binarySearch(BaseGen** objects,uint32_t objectCount,uint32_t value){

    if(!objectCount)
        return NULL;

    BaseGen* ret = NULL;

    uint32_t beginIndex = 0;
    uint32_t endIndex = objectCount;
   
    uint32_t midIndex = 0;
    BaseGen* midObject = NULL;
    uint32_t midVal = 0;

    while(beginIndex < endIndex){
        midIndex = (beginIndex+endIndex) / 2;
        midObject = objects[midIndex];
        midVal = midObject->getParsedBase()->getFileOffset();

        if(midVal > value){
            endIndex = midIndex;
        } else if(midVal < value){
            beginIndex = midIndex+1;
        } else {
            ret = midObject;
            break;
        }
    }

    if(!ret){
        if(value < midVal){
            if(midIndex) {
                midIndex--;
                ret = objects[midIndex];
            }
        } else {
            if(midIndex < (objectCount-1)){
                ret = midObject;
            }
        }
    }

    return ret;
}

uint32_t SymbolTable::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {

    uint16_t bssIndex = getXCoffFile()->getBSSSectionIndex();
    uint64_t oldBssVAddr = getXCoffFile()->getBSSSectionVAddr();
    uint64_t newBssVAddr = xCoffGen->getNewBSSSectionVAddressForInst();
    PRINT_DEBUG("BSS section index is %d",bssIndex);

    uint32_t numberOfAllObjects = xCoffGen->getNumberOfAllObjects();
    BaseGen** allObjects = new BaseGen*[numberOfAllObjects];
    numberOfAllObjects = xCoffGen->getAllObjects(allObjects,numberOfAllObjects);
    qsort(allObjects,numberOfAllObjects,sizeof(BaseGen*),compareOriginalFileOffsets);

    PRINT_DEBUG("SymbolTable  Instrumentation ");

    uint32_t totalBytesWritten = 0;

    uint32_t currSize = Size__NN_bit_SymbolTable_Entry;
    SYMENT newEntry;

    char* currPtr = buffer;
    for(uint32_t i=0;i<numberOfSymbols;i++){
        char* ptr = symbols[i]->charStream();

        SymbolBase* symBase = symbols[i];
        if(!symBase->isAuxilary()){

            Symbol* sym = (Symbol*)symBase;  
            if(sym->IS_SYMB_TYPE(BINCL) ||
               sym->IS_SYMB_TYPE(EINCL)){

                PRINT_DEBUG("To update C_BINCL or C_EINCL");

                uint64_t value = sym->GET(n_value);
                if(value){
                    BaseGen* gen = binarySearch(allObjects,numberOfAllObjects,value);
                    ASSERT(gen && gen->getParsedBase()->includesFileOffset(value));
                    value = gen->convertFileOffset(value);
                }

                sym->changeValueCopy(value,(char*)&newEntry);
                ptr = (char*)&newEntry;
            } else if((sym->GET(n_scnum) == bssIndex) &&
                     (sym->IS_SYMB_TYPE(EXT) ||
                      sym->IS_SYMB_TYPE(HIDEXT) ||
                      sym->IS_SYMB_TYPE(WEAKEXT))){

                PRINT_DEBUG("To update C_EXT or C_HIDEXT or C_WEAKEXT");

                uint64_t value = sym->GET(n_value);
                if(value){
                    PRINT_DEBUG("Changing %#llx with %#llx",value,(value-oldBssVAddr) + newBssVAddr);
                    value = (value-oldBssVAddr) + newBssVAddr;
                }
                sym->changeValueCopy(value,(char*)&newEntry);
                ptr = (char*)&newEntry;
            }

        } else if(symBase->isAuxilary()){
            Auxilary* aux = (Auxilary*)symBase;
            if(aux->getAuxilaryType() == Type__Auxilary_Symbol_Function){

                PRINT_DEBUG("To update AUX Function");

                uint64_t lnnoptr = aux->GET_A(x_lnnoptr,x_fcn);
                if(lnnoptr){
                    BaseGen* gen = binarySearch(allObjects,numberOfAllObjects,lnnoptr);
                    ASSERT(gen && gen->getParsedBase()->includesFileOffset(lnnoptr));
                    lnnoptr = gen->convertFileOffset(lnnoptr);
                }

                uint32_t exptr = 0;
                if(!getXCoffFile()->is64Bit()){
                    exptr = aux->GET_A(x_exptr,x_fcn);
                    if(exptr){
                        gen = binarySearch(allObjects,numberOfAllObjects,exptr);
                        ASSERT(gen && gen->getParsedBase()->includesFileOffset(exptr));
                        exptr = gen->convertFileOffset(exptr);
                    }
                }

                AuxilaryFunction* auxFun = (AuxilaryFunction*)aux;
                auxFun->changeExptrLnnoptrCopy(exptr,lnnoptr,(char*)&newEntry);
                ptr = (char*)&newEntry;

            } else if(aux->getAuxilaryType() == Type__Auxilary_Symbol_Exception){

                PRINT_DEBUG("To update AUX EXCEPTION");

                uint64_t exptr = aux->GET_A(x_exptr,x_except);
                if(exptr){
                    BaseGen* gen = binarySearch(allObjects,numberOfAllObjects,exptr);
                    ASSERT(gen && gen->getParsedBase()->includesFileOffset(exptr));
                    exptr = gen->convertFileOffset(exptr);
                }

                AuxilaryException* auxExp = (AuxilaryException*)aux;
                auxExp->changeExptrCopy(exptr,(char*)&newEntry);
                ptr = (char*)&newEntry;
            }
        }

        memcpy(currPtr,ptr,currSize);
        currPtr += currSize;
        totalBytesWritten += currSize;
    }

    ASSERT((sizeInBytes == totalBytesWritten) &&
           "FATAL: The bytes written should match the size of the linenumbers");

    delete[] allObjects;
    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/**** ----------------------------------------------------------------- ****/
uint32_t StringTable::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("StringTable  Instrumentation ");
    memcpy(buffer,stringTablePtr,sizeInBytes);
    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/

/**** ----------------------------------------------------------------- ****/
uint32_t RawSection::getInstrumentationSize(XCoffFileGen* xCoffGen){
    if(getRawDataPtr())
        return sizeInBytes;
    return 0;
}
uint32_t RawSection::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    if(getRawDataPtr()){
        PRINT_DEBUG("RawSection  Instrumentation ");
        memcpy(buffer,getRawDataPtr(),sizeInBytes); 
        return sizeInBytes;
    }
    PRINT_DEBUG("RawSection  Instrumentation  DO NOTHING");
    return 0;
}
uint32_t ExceptionSection::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("ExceptionSection  Instrumentation ");

    uint32_t totalBytesWritten = 0;

    uint32_t currSize = Size__32_bit_ExceptionTable_Entry;
    if(getXCoffFile()->is64Bit()){
        currSize = Size__64_bit_ExceptionTable_Entry;
    }

    char* currPtr = buffer;
    for(uint32_t i=0;i<numberOfExceptions;i++){
        memcpy(currPtr,exceptions[i]->charStream(),currSize);
        currPtr += currSize;
        totalBytesWritten += currSize;
    }

    ASSERT((sizeInBytes == totalBytesWritten) &&
           "FATAL: The bytes written should match the size of the exceptions");

    return sizeInBytes;
}
/**** ----------------------------------------------------------------- ****/
/*********************************************************************************/
uint32_t LoaderSection::getInstrumentationSize(XCoffFileGen* xCoffGen){

    uint32_t ret = sizeInBytes;

    uint32_t numOfSharedLibs = xCoffGen->getNumOfSharedLibFuncs();
    if(numOfSharedLibs){
        uint32_t oneSymbolSize = Size__32_bit_Loader_Section_Symbol;
        uint32_t oneRelocSize  = Size__32_bit_Loader_Section_Relocation;
        if(getXCoffFile()->is64Bit()){
            oneSymbolSize = Size__64_bit_Loader_Section_Symbol;
            oneRelocSize  = Size__64_bit_Loader_Section_Relocation;
        }

        ret += (oneSymbolSize * numOfSharedLibs);
        ret += (oneRelocSize * numOfSharedLibs);
        char* impidpath = NULL;
        char* impidbase = NULL;
        char* impidmem = NULL;
        xCoffGen->getSharedLibraryPathAndObj(&impidpath,&impidbase,&impidmem);
        ret += (strlen(impidpath) + strlen(impidbase) + strlen(impidmem) + 3);
        for(uint32_t i=0;i<numOfSharedLibs;i++){
            char* newSymName = xCoffGen->getSharedLibFuncName(i);
            ret += (sizeof(uint16_t) + strlen(newSymName) + 1);
        }
    }

    return ret;
}
uint32_t LoaderSection::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen){
    PRINT_DEBUG("LoaderSection Frequency Instrumentation");

    uint32_t numOfSharedLibs = xCoffGen->getNumOfSharedLibFuncs();

    uint32_t oneHeaderSize = Size__32_bit_Loader_Section_Header;
    uint32_t oneSymbolSize = Size__32_bit_Loader_Section_Symbol;
    uint32_t oneRelocSize  = Size__32_bit_Loader_Section_Relocation;
    if (getXCoffFile()->is64Bit()){
        oneHeaderSize = Size__64_bit_Loader_Section_Header;
        oneSymbolSize = Size__64_bit_Loader_Section_Symbol;
        oneRelocSize  = Size__64_bit_Loader_Section_Relocation;
    }

    uint32_t totalBytesWritten = 0;
    char* currPtr = buffer;

    currPtr += oneHeaderSize;
    totalBytesWritten += oneHeaderSize;

    uint32_t newSymTableOffset = totalBytesWritten;
    uint32_t newSymTableSize = 0;

    for (uint32_t i = 0; i < numberOfSymbols; i++){
        memcpy(currPtr,symbolTable[i+IMPLICIT_SYM_COUNT]->charStream(),oneSymbolSize);
        currPtr += oneSymbolSize;
        totalBytesWritten += oneSymbolSize;
        newSymTableSize += oneSymbolSize;
    }

    for (uint32_t i = 0; i < numOfSharedLibs; i++){
        uint32_t newSymOff = stringTable->getStringTableSize();
        for(uint32_t j=0;j<i;j++){
            char* newSymName = xCoffGen->getSharedLibFuncName(j);
            newSymOff += (sizeof(uint16_t) + strlen(newSymName) + 1);
        }
        newSymOff += sizeof(uint16_t);

        LSSymbol* newSymbol = LSSymbol::newSymbol(getXCoffFile()->is64Bit(),
                                                   newSymOff,fileNameTable->getFileNameEntryCount());
        memcpy(currPtr,newSymbol->charStream(),oneSymbolSize);
        delete newSymbol;

        currPtr += oneSymbolSize;
        totalBytesWritten += oneSymbolSize;
        newSymTableSize += oneSymbolSize;
    }

    uint32_t newRelocTableOffset = totalBytesWritten;
    uint32_t newRelocTableSize = 0;

    for (uint32_t i = 0; i < numberOfRelocations; i++){
        memcpy(currPtr,relocationTable[i]->charStream(),oneRelocSize);
        currPtr += oneRelocSize;
        totalBytesWritten += oneRelocSize;
        newRelocTableSize += oneRelocSize;
    }
    for (uint32_t i = 0; i < numOfSharedLibs; i++){
        uint64_t newSymAddr = xCoffGen->getSharedLibFuncAddrLocation(i);
        LSRelocation* newReloc = LSRelocation::newRelocation(getXCoffFile()->is64Bit(),
                                                    newSymAddr,
                                                    numberOfSymbols + IMPLICIT_SYM_COUNT + i,
                                                    getXCoffFile()->getDataSectionIndex());
        memcpy(currPtr,newReloc->charStream(),oneRelocSize);
        delete newReloc;
        currPtr += oneRelocSize;
        totalBytesWritten += oneRelocSize;
        newRelocTableSize += oneRelocSize;
    }

    uint32_t newFileNameOffset = totalBytesWritten;
    uint32_t newFileNameSize = 0;

    uint32_t currSize = fileNameTable->getFileNameTableSize();
    memcpy(currPtr,fileNameTable->getFileNameTablePtr(),currSize);
    currPtr += currSize;
    totalBytesWritten += currSize;
    newFileNameSize += currSize;
    if (numOfSharedLibs){
        char* impidpath = NULL;
        char* impidbase = NULL;
        char* impidmem = NULL;
        xCoffGen->getSharedLibraryPathAndObj(&impidpath,&impidbase,&impidmem);
        ASSERT(impidpath && impidbase && impidmem);
        
        uint32_t ns = 0;
        memcpy(currPtr,impidpath,strlen(impidpath)+1);
        ns += strlen(impidpath) + 1;
        memcpy(currPtr + ns,impidbase,strlen(impidbase)+1);
        ns += strlen(impidbase) + 1;
        memcpy(currPtr + ns ,impidmem,strlen(impidmem)+1);
        ns += strlen(impidmem) + 1;

        currPtr += ns;
        totalBytesWritten += ns;
        newFileNameSize += ns;
    }

    uint32_t newStringTableOffset = totalBytesWritten;
    uint32_t newStringTableSize = 0;

    currSize = stringTable->getStringTableSize();
    memcpy(currPtr,stringTable->getStringTablePtr(),currSize);
    currPtr += currSize;
    totalBytesWritten += currSize;
    newStringTableSize += currSize;

    if (numOfSharedLibs){
        for(uint32_t i=0;i<numOfSharedLibs;i++){
            char* newSymName = xCoffGen->getSharedLibFuncName(i);
            uint32_t ns = 0;
            uint16_t symsz = strlen(newSymName) + 1;
            memcpy(currPtr,&symsz,sizeof(uint16_t));
            ns += sizeof(uint16_t);
            memcpy(currPtr+ns,newSymName,symsz);
            ns += symsz;
            currPtr += ns;
            totalBytesWritten += ns;
            newStringTableSize += ns;
        }
    }

    LSHeader* newHeader = LSHeader::newHeader(header,getXCoffFile()->is64Bit(),
                                              numberOfSymbols+numOfSharedLibs,
                                              numberOfRelocations + numOfSharedLibs,
                                              newFileNameSize,
                                              fileNameTable->getFileNameEntryCount() + (numOfSharedLibs ? 1 : 0),
                                              newFileNameOffset,newStringTableSize,newStringTableOffset,
                                              newSymTableOffset,newRelocTableOffset);
    memcpy(buffer,newHeader->charStream(),oneHeaderSize);
    delete newHeader;

    return totalBytesWritten;
}

uint32_t TextSection::getInstrumentationSize(XCoffFileGen* xCoffGen){

    uint32_t bufferWordPtr = sizeInBytes;

    if(xCoffGen->getNumberOfInstPoints(this) || xCoffGen->byteCountForSharedLibFuncWrappers()){

        bufferWordPtr = nextAlignAddressWord(bufferWordPtr);

        bufferWordPtr += xCoffGen->byteCountForSharedLibFuncWrappers();

        uint32_t instCount = xCoffGen->getNumberOfInstPoints(this);

        uint64_t textSectionStartAddr = getSectHeader()->GET(s_vaddr);
        for(uint32_t i = 0;i<instCount;i++){
            bufferWordPtr += xCoffGen->byteCountForInst(i,textSectionStartAddr + bufferWordPtr,this);
        }
    }
    return bufferWordPtr;
}

uint32_t TextSection::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen){

    uint32_t bufferWordPtr = sizeInBytes;

    PRINT_DEBUG("TextSection Instrumentation");
    memcpy(buffer,getRawDataPtr(),bufferWordPtr);

    if (xCoffGen->getNumberOfInstPoints(this) || xCoffGen->byteCountForSharedLibFuncWrappers()){
        bufferWordPtr = nextAlignAddressWord(bufferWordPtr);

        xCoffGen->generateStubForAllLibraryCalls(bufferWordPtr,gen);
        bufferWordPtr += xCoffGen->byteCountForSharedLibFuncWrappers();

        uint32_t instCount = xCoffGen->getNumberOfInstPoints(this);

        uint64_t textSectionStartAddr = getSectHeader()->GET(s_vaddr);
        for (uint32_t i = 0; i < instCount; i++){
            bufferWordPtr += xCoffGen->generateCodeForInst(i,textSectionStartAddr + bufferWordPtr,this,
                                                           gen,bufferWordPtr);
        }
    }

    DEBUG_MORE(
        Instruction::print(buffer,getSectHeader()->GET(s_vaddr),bufferWordPtr,true);
    );

    PRINT_DEBUG("Using bufferwordptr %d gensize %d", bufferWordPtr, gen->getSizeInBytes());
    ASSERT(bufferWordPtr == gen->getSizeInBytes());

    return bufferWordPtr;
}

uint32_t DataSection::getInstrumentationSize(XCoffFileGen* xCoffGen){
    return (sizeInBytes + xCoffGen->getExtendedDataSize());
}
uint32_t DataSection::instrument(char* buffer,XCoffFileGen* xCoffGen,BaseGen* gen) {
    PRINT_DEBUG("DataSection  Instrumentation ");
    memcpy(buffer,getRawDataPtr(),sizeInBytes);
    xCoffGen->initializeReservedData(this,gen);
    return getInstrumentationSize(xCoffGen);
}
/*********************************************************************************/
