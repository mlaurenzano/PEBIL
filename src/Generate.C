#include <Base.h>
#include <Instruction.h>
#include <FileHeader.h>
#include <AOutHeader.h>
#include <SectHeader.h>
#include <RawSection.h>
#include <SymbolTable.h>
#include <StringTable.h>
#include <RelocationTable.h>
#include <LineInfoTable.h>
#include <XCoffFile.h>
#include <Iterator.h>
#include <BinaryFile.h>
#include <Generate.h>
#include <Function.h>
#include <LineInfoFinder.h>

XCoffFileGen::XCoffFileGen(XCoffFile* xcoff,char* extension,uint32_t phaseNo,char* inpFile) 
    : xcoffFile(xcoff),inputFileName(inpFile),outputFileName(NULL),fileExtension(NULL),phaseIndex(0),
      numberOfSections(0),
      instrumentedFileSize(0),
      numberOfInstPoints(0),extendedDataSize(0),dataBufferEntryCount(0),pathToInstLibraries(NULL)
{

    ASSERT(xcoffFile && "FATAL : The Gen class requires a non-null value");

    fileExtension = extension;
    phaseIndex = phaseNo;
    outputFileName = new char[__MAX_STRING_SIZE];
    if(!phaseIndex){
        sprintf(outputFileName,"%s.%s",xcoffFile->getXCoffFileName(),fileExtension);
    } else {
        sprintf(outputFileName,"%s.phase.%d.%s",xcoffFile->getXCoffFileName(),phaseIndex,fileExtension);
    }

    fileHeaderGen = new FileHeaderGen(xcoffFile->getFileHeader());
    aOutHeaderGen = new AOutHeaderGen(xcoffFile->getAOutHeader());

    numberOfSections = xcoffFile->getNumberOfSections();

    sectHeadersGen = new SectHeaderGen*[numberOfSections+1];
    rawSectionsGen = new RawSectionGen*[numberOfSections+1];
    relocationTablesGen = new RelocationTableGen*[numberOfSections+1];
    lineInfoTablesGen = new LineInfoTableGen*[numberOfSections+1];

    for(uint32_t i=1;i<=numberOfSections;i++){
        sectHeadersGen[i] = NULL;
        rawSectionsGen[i] = NULL;
        relocationTablesGen[i] = NULL;
        lineInfoTablesGen[i] = NULL;
    }

    for(uint32_t i=1;i<=numberOfSections;i++){
        sectHeadersGen[i] = new SectHeaderGen(xcoffFile->getSectHeader(i));
        rawSectionsGen[i] = new RawSectionGen(xcoffFile->getRawSection(i));
        if(xcoffFile->getRelocationTable(i)){
            relocationTablesGen[i] = new RelocationTableGen(xcoffFile->getRelocationTable(i));
        }
        if(xcoffFile->getLineInfoTable(i)){
            lineInfoTablesGen[i] = new LineInfoTableGen(xcoffFile->getLineInfoTable(i));
        }
    }

    symbolTableGen = new SymbolTableGen(xcoffFile->getSymbolTable());
    stringTableGen = new StringTableGen(xcoffFile->getStringTable());

    pathToInstLibraries = new char[2+strlen("lib")+1];
    sprintf(pathToInstLibraries,"./lib");
}

uint32_t XCoffFileGen::getNumberOfAllFunctions(){ 
    return (xcoffFile ? xcoffFile->getNumberOfFunctions() : 0); 
}
uint32_t XCoffFileGen::getNumberOfAllBlocks(){ 
    return (xcoffFile ? xcoffFile->getNumberOfBlocks() : 0); 
}
uint32_t XCoffFileGen::getNumberOfAllMemoryOps(){ 
    return (xcoffFile ? xcoffFile->getNumberOfMemoryOps() : 0); 
}
uint32_t XCoffFileGen::getNumberOfAllFloatPOps(){ 
    return (xcoffFile ? xcoffFile->getNumberOfFloatPOps() : 0); 
}
uint32_t XCoffFileGen::getNumberOfAllObjects(){
    uint32_t ret = 0;
    if(fileHeaderGen) ret++;
    if(aOutHeaderGen) ret++;
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(sectHeadersGen[i]) ret++;
        if(rawSectionsGen[i] && !rawSectionsGen[i]->hasInvalidFileOffset()) ret++;
        if(relocationTablesGen[i]) ret++;
        if(lineInfoTablesGen[i]) ret++;
    }
    if(symbolTableGen) ret++;
    if(stringTableGen) ret++;
    return ret;
}
uint32_t XCoffFileGen::getAllObjects(BaseGen** arr,uint32_t s){
    if(!arr || !s) return 0;

    uint32_t numOfAllObjects = getNumberOfAllObjects();
    if(numOfAllObjects > s){
        numOfAllObjects = s;
    }

    uint32_t idx = 0;

    if(idx == numOfAllObjects) return idx;
    if(fileHeaderGen) arr[idx++] = (BaseGen*)fileHeaderGen;

    if(idx == numOfAllObjects) return idx;
    if(aOutHeaderGen) arr[idx++] = (BaseGen*)aOutHeaderGen;

    for(uint32_t i=1;i<=numberOfSections;i++){
        if(idx == numOfAllObjects) return idx;
        if(sectHeadersGen[i]) arr[idx++] = (BaseGen*)sectHeadersGen[i];

        if(idx == numOfAllObjects) return idx;
        if(rawSectionsGen[i] && !rawSectionsGen[i]->hasInvalidFileOffset())
             arr[idx++] = (BaseGen*)rawSectionsGen[i];

        if(idx == numOfAllObjects) return idx;
        if(relocationTablesGen[i]) arr[idx++] = (BaseGen*)relocationTablesGen[i];

        if(idx == numOfAllObjects) return idx;
        if(lineInfoTablesGen[i]) arr[idx++] = (BaseGen*)lineInfoTablesGen[i];
    }
    if(idx == numOfAllObjects) return idx;
    if(symbolTableGen) arr[idx++] = (BaseGen*)symbolTableGen;

    if(idx == numOfAllObjects) return idx;
    if(stringTableGen) arr[idx++] = (BaseGen*)stringTableGen;

    return numOfAllObjects;
}
uint32_t XCoffFileGen::getSymbolTableOffsetForInst() {
    if(symbolTableGen && !symbolTableGen->hasInvalidFileOffset()){
        return symbolTableGen->getFileOffset();
    }
    return 0;
}

uint32_t XCoffFileGen::getRawSectionSizeForInst(uint32_t idx){
    if(idx <= numberOfSections){
        return rawSectionsGen[idx]->getSizeInBytes();
    }
    return 0;
}

uint32_t XCoffFileGen::getRawSectionOffsetForInst(uint32_t idx){
    if(idx <= numberOfSections){
        if(!rawSectionsGen[idx]->hasInvalidFileOffset()){
            return rawSectionsGen[idx]->getFileOffset();
        }
    }
    return 0;
}
uint32_t XCoffFileGen::getRelocOffsetForInst(uint32_t idx){
    if(idx <= numberOfSections){
        if(relocationTablesGen[idx]){
            if(!relocationTablesGen[idx]->hasInvalidFileOffset()){
                return relocationTablesGen[idx]->getFileOffset();
            }
        }
    }
    return 0;
}
uint32_t XCoffFileGen::getLineInfoOffsetForInst(uint32_t idx){
    if(idx <= numberOfSections){
        if(lineInfoTablesGen[idx]){
            if(!lineInfoTablesGen[idx]->hasInvalidFileOffset()){
                return lineInfoTablesGen[idx]->getFileOffset();
            }
        }
    }
    return 0;
}
uint64_t XCoffFileGen::getNewBSSSectionVAddressForInst(){
    uint16_t dataSectionIndex =  xcoffFile->getDataSectionIndex();
    uint64_t dataVAddr = xcoffFile->getDataSectionVAddr();
    /* uint16_t bssSectionIndex =  xcoffFile->getBSSSectionIndex(); */
    uint64_t bssVAddr = xcoffFile->getBSSSectionVAddr();

    PRINT_DEBUG("The original DATA address is %#18llx",dataVAddr);
    PRINT_DEBUG("The original BSS address is %#18llx",bssVAddr);
    if(dataVAddr < bssVAddr){
        ASSERT(getRawSectionSizeForInst(dataSectionIndex) && 
               "FATAL : The text section does not have any data");
        bssVAddr = (dataVAddr + getRawSectionSizeForInst(dataSectionIndex));
        bssVAddr = nextAlignAddressWord(bssVAddr);
    }
    PRINT_DEBUG("The new BSS address is %#18llx",bssVAddr);
    return bssVAddr;
}

void XCoffFileGen::instrument(){

    selectInstrumentationPoints(inputFileName);
    printInstrumentationPoints();
    reserveDataForInstrumentation();

    inst_step1_allocateBuffers();
    inst_step2_setFileOffsets();
    inst_step3_instrumentInBuffer();
    inst_step4_updateBSSCSectionEntries();
}

void XCoffFileGen::inst_step1_allocateBuffers(){
    fileHeaderGen->initInstrumentationBuffer(this);    
    aOutHeaderGen->initInstrumentationBuffer(this);    
    for(uint32_t i=1;i<=numberOfSections;i++){
        sectHeadersGen[i]->initInstrumentationBuffer(this);    
        rawSectionsGen[i]->initInstrumentationBuffer(this);
        if(relocationTablesGen[i]){
            relocationTablesGen[i]->initInstrumentationBuffer(this);    
        }
        if(lineInfoTablesGen[i]){
            lineInfoTablesGen[i]->initInstrumentationBuffer(this);    
        }
    }
    symbolTableGen->initInstrumentationBuffer(this);    
    stringTableGen->initInstrumentationBuffer(this);    
}

void XCoffFileGen::inst_step2_setFileOffsets(){

    instrumentedFileSize = 0;

    instrumentedFileSize = fileHeaderGen->setFileOffset(instrumentedFileSize);
    fileHeaderGen->printOffsetMapping();
    instrumentedFileSize = aOutHeaderGen->setFileOffset(instrumentedFileSize);
    aOutHeaderGen->printOffsetMapping();

    for(uint32_t i=1;i<=numberOfSections;i++){
        instrumentedFileSize = sectHeadersGen[i]->setFileOffset(instrumentedFileSize);
        sectHeadersGen[i]->printOffsetMapping();
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        instrumentedFileSize = rawSectionsGen[i]->setFileOffset(instrumentedFileSize);
        rawSectionsGen[i]->printOffsetMapping();
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(relocationTablesGen[i]){
            instrumentedFileSize = relocationTablesGen[i]->setFileOffset(instrumentedFileSize);
            relocationTablesGen[i]->printOffsetMapping();
        }
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(lineInfoTablesGen[i]){
            instrumentedFileSize = lineInfoTablesGen[i]->setFileOffset(instrumentedFileSize);
            lineInfoTablesGen[i]->printOffsetMapping();
        }
    }

    instrumentedFileSize = symbolTableGen->setFileOffset(instrumentedFileSize);
    symbolTableGen->printOffsetMapping();
    instrumentedFileSize = stringTableGen->setFileOffset(instrumentedFileSize);
    stringTableGen->printOffsetMapping();

    PRINT_DEBUG("Bytes written in the instrumented executable is %d",instrumentedFileSize);
}

void XCoffFileGen::inst_step3_instrumentInBuffer(){
    fileHeaderGen->instrument(this);    
    aOutHeaderGen->instrument(this);    
    for(uint32_t i=1;i<=numberOfSections;i++){
        sectHeadersGen[i]->instrument(this);    
        rawSectionsGen[i]->instrument(this);
        if(relocationTablesGen[i]){
            relocationTablesGen[i]->instrument(this);    
        }
        if(lineInfoTablesGen[i]){
            lineInfoTablesGen[i]->instrument(this);    
        }
    }
    symbolTableGen->instrument(this);    
    stringTableGen->instrument(this);    
}

void XCoffFileGen::inst_step4_updateBSSCSectionEntries(){

    uint32_t updatedBSSCount = 0;

//#define CHECK_FOR_SYMBOL
#ifdef CHECK_FOR_SYMBOL
    uint32_t numberOfSymbols = xcoffFile->getSymbolTable()->getNumberOfSymbols();
    Symbol** symbols = new Symbol*[numberOfSymbols];
    uint32_t bssSymCount = xcoffFile->getSymbolTable()->filterSortBSSSymbols(symbols,numberOfSymbols);
    PRINT_INFOR("%d symbols found in BSS for updateBSSCSectionEntries step4",bssSymCount);

    DEBUG(
        for(uint32_t i=0;i<bssSymCount;i++){
            xcoffFile->getSymbolTable()->printSymbol(symbols[i]);
        }
    );
#endif

    RawSectionGen* tocSectionGen = rawSectionsGen[xcoffFile->getTOCSectionIndex()];
    RawSection* tocSection = xcoffFile->getTOCSection();
    ASSERT(tocSection && "FATAL : The TOC section is missing");
    uint64_t tocSectionBeginAddress = tocSection->getSectHeader()->GET(s_vaddr);
    RawSection* bssSection = xcoffFile->getBSSSection();
    if(!bssSection)
        return;

    ASSERT(bssSection->IS_SECT_TYPE(BSS) && "FATAL : Looked for BSS but got something else");

    uint64_t oldBSSAddr = xcoffFile->getBSSSectionVAddr();
    uint64_t newBSSAddr = getNewBSSSectionVAddressForInst();

    AddressIterator ait = tocSection->getAddressIterator();
    PRINT_DEBUG("The TOC address is %#18llx in section at %#18llx",
            xcoffFile->getTOCAddress(),tocSectionBeginAddress);
    ait.skipTo(xcoffFile->getTOCAddress());
    while(ait.hasMore()){
        uint64_t address = *ait;
        uint64_t value = tocSection->readBytes(&ait);
        if(bssSection->inRange(value)){

#ifdef CHECK_FOR_SYMBOL
            /* check value whether the symbol table has it */
            void* checkRes = bsearch(&value,symbols,bssSymCount,sizeof(Symbol*),searchSymbolValue);
            if(!checkRes){
                PRINT_DEBUG("----->NOT   %#18llx -- %#18llx",address,value);
            } else {
                PRINT_DEBUG("-----<FOUND %#18llx -- %#18llx",address,value);
#endif
                updatedBSSCount++;
                uint64_t newValue = (value-oldBSSAddr) + newBSSAddr;
                PRINT_DEBUG("Updating Data Section entry %#18llx from %#18llx to %#18llx",
                            address,value,newValue);
                if(ait.isWord()){
                    tocSectionGen->writeWord(address-tocSectionBeginAddress,newValue);
                } else if(ait.isDouble()){
                    tocSectionGen->writeDouble(address-tocSectionBeginAddress,newValue);
                }
#ifdef CHECK_FOR_SYMBOL
            }
#endif
        }
        ait++;
    }

    PRINT_INFOR("%d updates are applied for BSS for updateBSSCSectionEntries step4",updatedBSSCount);

#ifdef CHECK_FOR_SYMBOL
    delete[] symbols;
#endif

    LoaderSection* ldrSection = (LoaderSection*)xcoffFile->getLoaderSection();
    uint32_t rlcCount = ldrSection->getRelocationCount();
    if(rlcCount){
        uint64_t* addrs = new uint64_t[rlcCount];
        uint32_t bssRlcCount = ldrSection->getBSSRelocations(addrs);
        PRINT_INFOR("There are %d relocations in loader section for BSS section",bssRlcCount);
        for(uint32_t i=0;i<bssRlcCount;i++){
            uint64_t address = addrs[i];
            if((address >= xcoffFile->getTOCAddress()) ||
               (address < tocSectionBeginAddress))
            {
                continue;
            }
            ait.skipTo(address);
            uint64_t value = tocSection->readBytes(&ait);
            if(bssSection->inRange(value)){
                uint64_t newValue = (value-oldBSSAddr) + newBSSAddr;
                if(ait.isWord()){
                    tocSectionGen->writeWord(address-tocSectionBeginAddress,newValue);
                } else if(ait.isDouble()){
                    tocSectionGen->writeDouble(address-tocSectionBeginAddress,newValue);
                }
                PRINT_DEBUG("Found %#llx at %#llx for BSS entry and updated with %#llx",value,address,newValue);
            }
        }
        delete[] addrs;
    }
}

uint32_t XCoffFileGen::dump(){
    binaryOutputFile.open(outputFileName);
    if(!binaryOutputFile){
        PRINT_ERROR("The output file can not be opened %s",outputFileName);
    }

    fileHeaderGen->dump(&binaryOutputFile);
    aOutHeaderGen->dump(&binaryOutputFile);

    for(uint32_t i=1;i<=numberOfSections;i++){
        sectHeadersGen[i]->dump(&binaryOutputFile);
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        rawSectionsGen[i]->dump(&binaryOutputFile);
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(relocationTablesGen[i]){
            relocationTablesGen[i]->dump(&binaryOutputFile);
        }
    }
    for(uint32_t i=1;i<=numberOfSections;i++){
        if(lineInfoTablesGen[i]){
            lineInfoTablesGen[i]->dump(&binaryOutputFile);
        }
    }

    symbolTableGen->dump(&binaryOutputFile);
    stringTableGen->dump(&binaryOutputFile);

    uint32_t ret = binaryOutputFile.alreadyWritten();

    ASSERT(binaryOutputFile.alreadyWritten() == getInstrumentedFileSize());

    binaryOutputFile.close();

    char sysCommand[__MAX_STRING_SIZE];
    sprintf(sysCommand,"chmod a+x %s",outputFileName);
    system(sysCommand);

    return ret;
}

uint32_t XCoffFileGen::generateStubForAllLibraryCalls(uint32_t genBufferOffset,BaseGen* gen){

    uint64_t textSectionStartAddr = xcoffFile->getTextSectionVAddr();

    for (uint32_t i = 0; i < getNumOfSharedLibFuncs(); i++){
        uint64_t addr = genBufferOffset + textSectionStartAddr;
        setAddrOfSharedLibFuncWrapper(i,addr);
        PRINT_INFOR("**** Function Begin addr     is %#18llx (%lld)  -- %s genoffset %#9x***",
            addr, addr-textSectionStartAddr, getSharedLibFuncName(i),genBufferOffset);
        genBufferOffset += generateSharedLibFuncWrapper(i, getSharedLibFuncAddrLocation(i), genBufferOffset, gen);
    }

    return byteCountForSharedLibFuncWrappers();
}

void XCoffFileGen::setPathToInstLib(char* libPathTop) { 
    if(libPathTop){
        if(pathToInstLibraries) delete[] pathToInstLibraries;
        pathToInstLibraries = new char[strlen(libPathTop)+strlen("lib")+2];
        sprintf(pathToInstLibraries,"%s/lib",libPathTop);
    }
    PRINT_INFOR("Top directory to shared lib is set to %s",pathToInstLibraries);
}
