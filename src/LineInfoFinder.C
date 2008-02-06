#include <LineInfoFinder.h>
#include <SymbolTable.h>
#include <XCoffFile.h>
#include <BinaryFile.h>
#include <Auxilary.h>
#include <SectHeader.h>
#include <StringTable.h>

#define TEST_ALL
#ifdef TEST_ALL
#define TEST_ALL_ADDRESS
//#define TEST_ALL_ADDRESS_OUT
//#define TEST_GET_FUNCTION_NAME
//#define TEST_GET_FILE_NAME
//#define TEST_GET_LINE_NUMBER_IN_FUNCTION
//#define TEST_GET_LINE_NUMBER_IN_FILE
//#define TEST_GET_ADDRESS_BY_FUNCTION
//#define TEST_GET_ADDRESS_BY_FILE
#endif


uint32_t LineInfoFinder::getLineInfoIndexOfPreviousAddr(uint64_t addr){
    uint32_t first = 0;
    uint32_t mid;
    uint32_t last = numberOfLineAddresses - 1;


        /* check to see if addr can be found in sortedAddresses */
    if (addr < sortedAddresses[first].address){
        return numberOfLineInfos;
    } else if (addr > sortedAddresses[last].address){
        return sortedAddresses[last].index;
    }

        /* do a binary searh on sortedAddresses to find the address */
    while (first <= last){
        mid = (first + last) / 2;
        if (addr > sortedAddresses[mid].address){
            first = mid + 1;
        } else if (addr < sortedAddresses[mid].address){
            last = mid - 1;
        } else {
            return sortedAddresses[mid].index;
        }
    }
    while (sortedAddresses[last].address > addr){
        last--;
    }
    return sortedAddresses[last].index;
}



    /* returns some index in the lineInfoTable whose address is addr */
uint32_t LineInfoFinder::getLineInfoIndexOfAddr(uint64_t addr){
    uint32_t first = 0;
    uint32_t mid;
    uint32_t last = numberOfLineAddresses - 1;

        /* do a binary searh on sortedAddresses to find the address */
    while (first <= last){
        mid = (first + last) / 2;
        if (addr > sortedAddresses[mid].address){
            first = mid + 1;
        } else if (addr < sortedAddresses[mid].address){
            last = mid - 1;
        } else {
            return sortedAddresses[mid].index;
        }
    }
    return numberOfLineInfos;
}


void LineInfoFinder::commandLineTest(){
    uint32_t choice = 1;
    uint32_t idx;
    uint64_t address;

    printf("**************************************************\n");
    printf("Command Line Test of LineInfoFinder\n");
    printf("\t0: quit\n");
    printf("\t1: print number of line infos\n");
    printf("\t2: print an address\n");
    printf("\t3: get the nearest address\n");
    printf("\t4: print info for an address\n");
    printf("\t5: print lineInfoTable\n");
    printf("\t6: print symbolTable\n");
    printf("**************************************************\n\n");

    while(choice != 0) {
        printf("Enter choice > ");
        fscanf(stdin, "%d", &choice);
        if (choice == 0){
        } else if (choice == 1){
            printf("There are %d lineInfos\n", lineInfoTable->getNumberOfLineInfos());
        } else if (choice == 2){
            printf("Enter an index > ");
            fscanf(stdin, "%d", &idx);
            if (idx < lineInfoTable->getNumberOfLineInfos() && lineInfoTable->getLineInfo(idx)->GET(l_lnno)){
                printf("lineInfoTable[%d] has address %#llx\n", idx, lineInfoTable->getLineInfo(idx)->GET(l_paddr));
            } else {
                printf("lineInfoTable[%d] has no address\n", idx);
            }
        } else if (choice == 3){
            printf("Enter an address > ");
            fscanf(stdin, "%llx", &address);
            idx = getLineInfoIndexOfPreviousAddr(address);
            printf("idx is %d\n", idx);
            printf("The nearest address to %#llx is %#llx at index %d\n", address, lineInfoTable->getLineInfo(idx)->GET(l_paddr), idx);
        } else if (choice == 4){
            printf("Enter an address > ");
            fscanf(stdin, "%llx", &address);
            printf("address %#llx is at line %d in function %s, line %d in file %s\n", 
                address, getLineNumberInFunction(address), getFunctionName(address), getLineNumberInFile(address), 
                getFileName(address));        
       
        } else if (choice == 5){
            printLineInfoTable();
        } else if (choice == 6){
            printSymbolTable();
        } else {
        }
    }
}


    /* converts a file offset into an index into lineInfoTable */
uint32_t LineInfoFinder::getLineInfoIndex(uint64_t fileOffset){
    uint64_t lineInfoSize;
    if (is64Bit){
        lineInfoSize = (uint64_t)LINESZ_64;
    } else {
        lineInfoSize = (uint64_t)LINESZ;
    }
    ASSERT(((fileOffset - lineInfoPointer) % lineInfoSize) == 0);
    return ((fileOffset - lineInfoPointer) / lineInfoSize);
}


LineInfoFinder::LineInfoFinder(uint32_t idx, XCoffFile* xCoffFile) {
    uint32_t numberOfLocalFunctions;
    uint32_t currentSourceFile;
    SymbolBase* tmpSymbol;
    uint32_t* parentIndices;
    uint32_t* fileIndices;
    uint32_t symbolClass;

    lineInfoTable = xCoffFile->getLineInfoTable(idx);
    symbolTable = xCoffFile->getSymbolTable();
    stringTable = xCoffFile->getStringTable();
    lineInfoPointer = lineInfoTable->getXCoffFile()->getSectHeader(idx)->GET(s_lnnoptr);
    is64Bit = xCoffFile->is64Bit();

    numberOfLineInfos = lineInfoTable->getNumberOfLineInfos();
    lineInfos = new LineInfo*[numberOfLineInfos];
    lineInfoLineNumbers = new uint32_t[numberOfLineInfos];
    lineInfoAddresses = new uint64_t[numberOfLineInfos];
    numberOfLineAddresses = 0;
    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        lineInfos[i] = lineInfoTable->getLineInfo(i);
        lineInfoLineNumbers[i] = lineInfos[i]->GET(l_lnno);
        lineInfoAddresses[i] = lineInfos[i]->GET(l_paddr);
        if (lineInfoLineNumbers[i]){
            numberOfLineAddresses++;
        } else {
            numberOfLineInfoFunctions++;
        }
    }

    sortedAddresses = new struct idx_addr[numberOfLineAddresses];
    lineInfoFunctions = new struct idx_symndx[numberOfLineInfoFunctions];
    numberOfLineAddresses = 0;
    numberOfLineInfoFunctions = 0;

    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (lineInfoLineNumbers[i]){
            sortedAddresses[numberOfLineAddresses].index = i;
            sortedAddresses[numberOfLineAddresses].address = lineInfoAddresses[i];
            numberOfLineAddresses++;
        } else {
            lineInfoFunctions[numberOfLineInfoFunctions].index = i;
            lineInfoFunctions[numberOfLineInfoFunctions].symndx = lineInfoTable->getLineInfo(i)->GET(l_symndx);
            numberOfLineInfoFunctions++;
        }
    }

    qsort(&sortedAddresses[0], numberOfLineAddresses, sizeof(struct idx_addr), compare_idx_addr);

    numberOfFiles = 0;
    numberOfFunctions = 0;

        /* count the number of files and functions in the symbol table */
    for (uint32_t i = 0; i < symbolTable->getNumberOfSymbols(); i++){
        tmpSymbol = symbolTable->getSymbol(i);
        if (!tmpSymbol->isAuxilary()){
            symbolClass = ((Symbol*)tmpSymbol)->GET(n_sclass);
            if (symbolClass == C_FILE){
                numberOfFiles++;
            }
            else if (symbolClass == C_BINCL){
                numberOfFiles++;
            }
            else if (symbolClass == C_FUN){
                numberOfFunctions++;
            }
        }
    }

    files = new FileFinder*[numberOfFiles];
    functions = new FunctionFinder*[numberOfFunctions];
    numberOfFiles = 0;
    numberOfFunctions = 0;

        /* initialize each entry of files[] and functions[] */
    for (uint32_t i = 0; i < symbolTable->getNumberOfSymbols(); i++){
        tmpSymbol = symbolTable->getSymbol(i);
        if (!tmpSymbol->isAuxilary()){
            symbolClass = ((Symbol*)tmpSymbol)->GET(n_sclass);
            if (symbolClass == C_FILE){
                files[numberOfFiles] = new SourceFileFinder(symbolTable, lineInfoTable, stringTable, i);
                currentSourceFile = numberOfFiles;
                numberOfFiles++;
            }
            else if (symbolClass == C_BINCL){
                files[numberOfFiles] = new IncludeFileFinder(symbolTable, lineInfoTable, stringTable, i,
                        getLineInfoIndex(((Symbol*)tmpSymbol)->GET(n_value)),
                        getLineInfoIndex(((Symbol*)symbolTable->getSymbol(i+1))->GET(n_value)));
                numberOfFiles++;
            }
            else if (symbolClass == C_FUN){
                functions[numberOfFunctions] = new FunctionFinder(symbolTable, lineInfoTable, stringTable, i, lineInfoFunctions, numberOfLineInfoFunctions);
                numberOfFunctions++;
            }
        }
    }

        /* find out which files and addresses are from include files */
    isIncludeFile = new uint16_t[(numberOfFiles/sizeof(uint16_t))+1];
    isIncludeFile2 = new bool[numberOfFiles];
    isAddressFromIncludeFile = new uint16_t[(numberOfLineInfos/sizeof(uint16_t))+1];
    isAddressFromIncludeFile2 = new bool[numberOfLineInfos];
    for (uint32_t i = 0; i < numberOfFiles; i++){
        if (files[i]->isIncludeFile()){
            SET_IS_INCL_FILE_BIT(i);
            isIncludeFile2[i] = true;
            for (uint32_t j = ((IncludeFileFinder*)files[i])->getLineInfoIndex(); j <= ((IncludeFileFinder*)files[i])->getEndLineInfoIndex(); j++){
                SET_IS_INCL_ADDR_BIT(j);
                isAddressFromIncludeFile2[j] = true;
            }
        } else {
        }
    }


        /* now that all entries of files[] are initialized, find parent files for each function */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        functions[i]->setParent(files, numberOfFiles);
    }

        /* collect symbol indices for all files and all function parents */
    parentIndices = new uint32_t[numberOfFunctions];
    fileIndices = new uint32_t[numberOfFiles];
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        parentIndices[i] = functions[i]->getParentFile()->getSymbolIndex();
    }
    for (uint32_t i = 0; i < numberOfFiles; i++){
        fileIndices[i] = files[i]->getSymbolIndex();
    }

        /* now that all entries of functions[] have parent files, find member functions for each file */
    for (uint32_t i = 0; i < numberOfFiles; i++){
        numberOfLocalFunctions = 0;

        for (uint32_t j = 0; j < numberOfFunctions; j++){
            if (parentIndices[j] == fileIndices[i]){
                numberOfLocalFunctions++;
            } 
        }

        files[i]->setNumberOfFunctions(numberOfLocalFunctions);
        numberOfLocalFunctions = 0;

        for (uint32_t j = 0; j < numberOfFunctions; j++){
            if (parentIndices[j] == fileIndices[i]){
                files[i]->addFunction(functions[j],numberOfLocalFunctions);
                numberOfLocalFunctions++;
            }
        }
    }
}


LineInfoFinder::~LineInfoFinder(){
    delete lineInfos;
    delete lineInfoLineNumbers;
    delete lineInfoAddresses;
    delete isIncludeFile;
    delete isAddressFromIncludeFile;
    delete sortedAddresses;
    delete files;
    delete functions;
}


void LineInfoFinder::printFiles(){
    PRINT_INFOR("***********");
    PRINT_INFOR("There are %d source files in the symbol table", numberOfFiles);
    PRINT_INFOR("***********");
    for (uint32_t i = 0; i < numberOfFiles; i++){
        files[i]->print();
    }
}


void LineInfoFinder::printFunctions(){
    PRINT_INFOR("***********");
    PRINT_INFOR("There are %d functions in the symbol table", numberOfFunctions);
    PRINT_INFOR("***********");
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        functions[i]->print();
    }
}


bool LineInfoFinder::testLineInfoFinder(){
    uint64_t address;
    char* fName;

#ifdef TEST_ALL_ADDRESS
//    printLineInfoTable();
//    printSymbolTable();

    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing  getLineNumberInFunction, getFunctionName, getLineNumberInFile, getFileName");
    PRINT_INFOR("***********");

    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (lineInfoLineNumbers[i]){
            address = lineInfoAddresses[i];
            ASSERT(isAddressInLineInfoTable(address));
/*
            PRINT_INFOR("LineInfoTable[%d]: address %#llx is at line %d in function %s, include %d", i, address, getLineNumberInFunction(address), 
                getFunctionName(address), GET_IS_INCL_ADDR_BIT(i));
*/
            PRINT_INFOR("LineInfoTable[%d]: address %#llx is at line %d in function %s, line %d in file %s, include %d", i, address, getLineNumberInFunction(address), 
                getFunctionName(address), getLineNumberInFile(address), getFileName(address), GET_IS_INCL_ADDR_BIT(i));

#ifdef TEST_ALL_ADDRESS_OUT
            if (getLineInfoIndexOfPreviousAddr(address-4) < numberOfLineInfos){
                PRINT_INFOR("\taddress %#llx -> %#llx is at line %d in function %s, line %d in file %s", 
                    address-4, lineInfoTable->getLineInfo(getLineInfoIndexOfPreviousAddr(address-4))->GET(l_paddr), 
                    getLineNumberInFunction(address-4), getFunctionName(address-4), getLineNumberInFile(address-4), 
                    getFileName(address-4));
            }
            if (getLineInfoIndexOfPreviousAddr(address+4) < numberOfLineInfos){
                PRINT_INFOR("\taddress %#llx -> %#llx is at line %d in function %s, line %d in file %s", 
                    address+4, lineInfoTable->getLineInfo(getLineInfoIndexOfPreviousAddr(address+4))->GET(l_paddr), 
                    getLineNumberInFunction(address+4), getFunctionName(address+4), getLineNumberInFile(address+4), 
                    getFileName(address+4));
            }
#endif
        }
    }
#endif

#ifdef TEST_GET_FUNCTION_NAME
    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing getFunctionName");
    PRINT_INFOR("***********");

    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (lineInfoLineNumbers[i])){
            address = lineInfoAddresses[i];
            ASSERT(isAddressInLineInfoTable(address));
            fName = getFunctionName(address);
            PRINT_INFOR("Address %#llx is in function %s", address, fName);
        }
    }
#endif

#ifdef TEST_GET_FILE_NAME
    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing getFileName");
    PRINT_INFOR("***********");

    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (lineInfoLineNumbers[i])){
            address = lineInfoAddresses[i];
            ASSERT(isAddressInLineInfoTable(address));
            fName = getFileName(address);
            PRINT_INFOR("Address %#llx is in file %s", address, fName);
        }
    }
#endif

#ifdef TEST_GET_LINE_NUMBER_IN_FUNCTION
    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing getLineNumberInFunction");
    PRINT_INFOR("***********");

    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (lineInfoLineNumbers[i])){
            address = lineInfoAddresses[i];
            ASSERT(isAddressInLineInfoTable(address));
            PRINT_INFOR("Address %#llx has line %d", address, getLineNumberInFunction(address));
        }
    }
#endif

#ifdef TEST_GET_LINE_NUMBER_IN_FILE
    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing getLineNumberInFile");
    PRINT_INFOR("***********");

    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (lineInfoLineNumbers[i])){
            address = lineInfoAddresses[i];
            ASSERT(isAddressInLineInfoTable(address));
            PRINT_INFOR("Address %#llx has line %d", address, getLineNumberInFile(address));
        }
    }
#endif


#ifdef TEST_GET_ADDRESS_BY_FUNCTION
    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing getAddressByFunction");
    PRINT_INFOR("***********");

    char* fcnName;
    bool isOldFcnName;
    char** uniqueFcnNames = new char*[numberOfFunctions];
    uint32_t nextFcn = 0;

    for (uint32_t i = 0; i < numberOfFunctions; i++){
        PRINT_INFOR("About to examine function %d", i);
        fcnName = functions[i]->getName();
        PRINT_INFOR("Examining function %s", fcnName);
        isOldFcnName = false;
        for (uint32_t j = 0; j < numberOfFunctions; j++){
            if (!strcmp(uniqueFcnNames[j],fcnName)){
                isOldFcnName = true;
            }
        }
        if (!isOldFcnName){
            uniqueFcnNames[nextFcn] = fcnName;
            nextFcn++;
            for (uint32_t j = 0; j < MAX_LINE_TEST; j++){
                ASSERT(isFunctionNameInSymbolTable(functions[i]->getName()));
                if (isLineInFunction(j, functions[i]->getName())){
                    address = getAddressByFunction(j,functions[i]->getName());
                    PRINT_INFOR("\tFunctionFinder %s at line %d in file %s, line %d -- address %#llx", functions[i]->getName(), functions[i]->getFirstLine(),
                            functions[i]->getParentFile()->getName(), j, address);
                    address = getAddressByFunction(j,functions[i]->getName(),address);
                    while (address != 0){
                        PRINT_INFOR("\t\taddress %#llx", address);
                        address = getAddressByFunction(j,functions[i]->getName(),address);
                    }
                }
            }
        }
    }
#endif
#ifdef TEST_GET_ADDRESS_BY_FILE
    PRINT_INFOR("***********");
    PRINT_INFOR("*********** Testing getAddressByFile");
    PRINT_INFOR("***********");

    char* fileName;
    bool isOldFileName;
    char** uniqueFileNames = new char*[numberOfFiles];
    uint32_t nextFile = 0;

    for (uint32_t i = 0; i < numberOfFiles; i++){
        fileName = files[i]->getName();
        isOldFileName = false;
        for (uint32_t j = 0; j < numberOfFiles; j++){
            if (!strcmp(uniqueFileNames[j],fileName)){
                isOldFileName = true;
            }
        }
        if (!isOldFileName){
            uniqueFileNames[nextFile] = fileName;
            nextFile++;
            for (uint32_t j = 0; j < MAX_LINE_TEST; j++){
                ASSERT(isFileNameInSymbolTable(files[i]->getName()));
                if (isLineInFile(j, files[i]->getName())){
                    address = getAddressByFile(j,files[i]->getName());
                    PRINT_INFOR("\tFile %s, line %d -- address %#llx", files[i]->getName(), j, address);
                    address = getAddressByFile(j,files[i]->getName(),address);
                    while (address != 0){
                        PRINT_INFOR("\t\taddress %#llx", address);
                        address = getAddressByFile(j,files[i]->getName(),address);
                    }
                }
            }
        }
    }
#endif

    return true;
}


char* LineInfoFinder::getFileName(uint64_t addr) { 

        /* search every include file for this address */
    for (uint32_t i = 0; i < numberOfFiles; i++){
        if (GET_IS_INCL_FILE_BIT(i)){
            if (((IncludeFileFinder*)files[i])->containsAddress(addr)){
                return files[i]->getName();
            }
        }
    }
        /* address is not part of an include file, so use the header in
           lineInfoTable to look up the function */
    return getFileNameOfFunction(getFunctionName(addr)); 
}


void LineInfoFinder::printFunctionSymbols(){
    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if (!lineInfoLineNumbers[i]){
            symbolTable->printSymbol(lineInfos[i]->GET(l_symndx)); 
        }
    }
    return;
}


void LineInfoFinder::printFileSymbols(){
    for (uint32_t i = 0; i < symbolTable->getNumberOfSymbols(); i++){
        if (!symbolTable->getSymbol(i)->isAuxilary() && ((Symbol*)symbolTable->getSymbol(i))->GET(n_sclass) == C_FILE){
            symbolTable->printSymbol(i);
        }
    }
    return;
}


void LineInfoFinder::printSymbolTable(){
    for (uint32_t i = 0; i < symbolTable->getNumberOfSymbols(); i++){
        if (!symbolTable->getSymbol(i)->isAuxilary()){
            symbolTable->printSymbol(i);
        }
    }
    return;
}


void LineInfoFinder::printLineInfoTable(){
    for (uint32_t i = 0; i < numberOfLineInfos; i++){
        if(lineInfoLineNumbers[i]){
            PRINT_INFOR("\tLNN [%3d] (lnn %9d)(adr %#llx)",i,lineInfoLineNumbers[i],lineInfoAddresses[i]);
        } else {
            PRINT_INFOR("\tLNN [%3d] (fcn bgn)(sym %9d)",i,lineInfos[i]->GET(l_symndx));
        }
    }
}


char* LineInfoFinder::getFileNameOfFunction(char* fcnName){

        /* look in the list of functions for this fcnName, then return
           the name of the parent file of that function */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if(!strcmp(fcnName,functions[i]->getName())){
            return functions[i]->getParentFile()->getName();
        }
    }
    return NULL;
}


char* LineInfoFinder::getFunctionName(uint64_t addr){
    uint32_t previousFunctionIndex;
    uint32_t addrIndex;

        /* find the address, then go backwards in the lineInfoTable until a function entry
           is found. when this occurs, follow the pointer to the symbol table to get the
           function name */
    addrIndex = getLineInfoIndexOfPreviousAddr(addr);
    if (addrIndex == numberOfLineInfos){
        return NULL;
    }
    previousFunctionIndex = addrIndex - 1;
    for (; previousFunctionIndex >= 0; previousFunctionIndex--){
        if (!lineInfoLineNumbers[previousFunctionIndex]){
            return symbolTable->getSymbolName(lineInfos[previousFunctionIndex]->GET(l_symndx));
        }
    }
    return NULL;
}


bool LineInfoFinder::isFunctionNameInSymbolTable(char* fcnName){

        /* scan function list to see if this fcnName is found */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getName(),fcnName)){
            return true;
        }
    }
    return false;
}


bool LineInfoFinder::isFileNameInSymbolTable(char* fileName){

        /* scan file list to see if this fileName is found */
    for (uint32_t i = 0; i < numberOfFiles; i++){
        if (!strcmp(files[i]->getName(),fileName)){
            return true;
        }
    }
    return false;
}


bool LineInfoFinder::isLineInFunction(uint32_t lineno, char* fcnName) {

        /* scan function list to find this fcnName, then see if that function
           has the given lineno */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getName(),fcnName)){
            if (functions[i]->containsLineNumber(lineno)){
                return true;
            }
        }
    }
    return false;
}


bool LineInfoFinder::isLineInFile(uint32_t lineno, char* fileName) {
    uint32_t effectiveLine;

        /* scan function list to see if some function's parent file is named
           fileName, then see if that file contains the given lineno */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getParentFile()->getName(),fileName)){
            effectiveLine = lineno - functions[i]->getFirstLine() + 1;
            if (functions[i]->containsLineNumber(effectiveLine)){
                return true;
            }
        }
    }

    return false;
}


uint64_t LineInfoFinder::getAddressByFunction(uint32_t lineno, char* fcnName) {

        /* scan function list to find this fcnName, see if that function
           has the given lineno, then return the first address associated with 
           that lineno */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getName(),fcnName)){
            if (functions[i]->containsLineNumber(lineno)){
                return functions[i]->getAddressByLineNumber(lineno);
            }
        }
    }
    return 0;
}


uint64_t LineInfoFinder::getAddressByFunction(uint32_t lineno, char* fcnName, uint64_t addr) {

        /* scan function list to find this fcnName, see if that function
           has the given lineno, then return the first address greater than 
           addr associated with that lineno */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getName(),fcnName)){
            if (functions[i]->containsLineNumber(lineno)){
                return functions[i]->getAddressByLineNumber(lineno, addr);
            }
        }
    }
    return 0;
}


uint64_t LineInfoFinder::getAddressByFile(uint32_t lineno, char* fileName) {
    uint32_t effectiveLine;

        /* scan function list to see if some function's parent file is named
           fileName, see if that file contains the given lineno, then return
           the first address associated with that lineno */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getParentFile()->getName(),fileName)){
            effectiveLine = lineno - functions[i]->getFirstLine() + 1;
            if (functions[i]->containsLineNumber(effectiveLine)){
                return functions[i]->getAddressByLineNumber(effectiveLine);
            }
        }
    }

    return 0;
}


uint64_t LineInfoFinder::getAddressByFile(uint32_t lineno, char* fileName, uint64_t addr) {
    uint32_t effectiveLine;
    uint64_t address;

        /* scan function list to see if some function's parent file is named
           fileName, see if that file contains the given lineno, then return
           the first address greater than addr associated with that lineno */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (!strcmp(functions[i]->getParentFile()->getName(),fileName)){
            effectiveLine = lineno - functions[i]->getFirstLine() + 1;
            if (functions[i]->containsLineNumber(effectiveLine)){
                address = functions[i]->getAddressByLineNumber(effectiveLine, addr);
                if (address != 0){
                    return address;
                }
            }
        }
    }

    return 0;
}


bool LineInfoFinder::isAddressInLineInfoTable(uint64_t addr){
    uint32_t addrIndex = getLineInfoIndexOfAddr(addr);
    if (addrIndex == numberOfLineInfos){
        return false;
    }
    return true;
}


uint32_t LineInfoFinder::getLineNumberInFile(uint64_t addr){

        /* scan function list to find the function that contains addr, then
           return the appropriately modified line number for addr */
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        if (functionContainsAddress(i, addr)){
            if (GET_IS_INCL_ADDR_BIT(getLineInfoIndexOfPreviousAddr(addr))){
                return getLineNumberInFunction(addr);
            } else {
                return functions[i]->getFirstLine() + getLineNumberInFunction(addr) - 1;
            }
        }
    }
    return 0;
}


uint32_t LineInfoFinder::getLineNumberInFunction(uint64_t addr){
    uint32_t addrIndex = getLineInfoIndexOfPreviousAddr(addr);
    if (addrIndex >= numberOfLineInfos){
        return 0;
    }
    return lineInfoLineNumbers[addrIndex];
}


bool LineInfoFinder::functionContainsAddress(uint32_t idx, uint64_t addr){
    uint32_t lineIndex = getLineInfoIndexOfPreviousAddr(addr);
    if (lineIndex > functions[idx]->getBeginLineInfoIndex() && lineIndex < functions[idx]->getEndLineInfoIndex()){
        return true;
    }
    return false;
}


FileFinder* FunctionFinder::setParent(FileFinder** files, uint32_t numberOfFiles){

        /* scan the file list to see if this function belongs to an include file */
    for (uint32_t i = 0; i < numberOfFiles; i++){
        if (files[i]->isIncludeFile()){
            if (((IncludeFileFinder*)files[i])->getLineInfoIndex() == beginLineInfoIndex){
                parentFile = files[i];
                return parentFile;
            }
        }
    }

        /* scan the file list to see which source file this function belongs to */
    for (uint32_t i = 0; i < numberOfFiles-1; i++){
        if (files[i]->isSourceFile()){

                /* find the next source file in the symbol table */
            uint32_t j = i+1;
            while (!files[j]->isSourceFile()){
                j++;
            }

                /* see if the next source file occurs after this function in the symbol table */
            if (files[j]->getSymbolIndex() > lineInfoTable->getLineInfo(beginLineInfoIndex)->GET(l_symndx)){
                parentFile = files[i];
                return parentFile;
            }
        }
    }
    return 0;
}


bool FunctionFinder::containsLineNumber(uint32_t lineno){

        /* scan this function's section of the lineInfoTable to see if lineno
           can be found */
    for (uint32_t i = beginLineInfoIndex + 1; i < endLineInfoIndex; i++){
        if (lineInfoTable->getLineInfo(i)->GET(l_lnno)){
            if (lineInfoTable->getLineInfo(i)->GET(l_lnno) == lineno){
                return true;
            }
        }
    }
    return false;
}


uint64_t FunctionFinder::getAddressByLineNumber(uint32_t lineno){

        /* scan this function's section of the lineInfoTable to see if lineno
           can be found. if so, retturn the first address associated with that lineno */
    for (uint32_t i = beginLineInfoIndex; i < endLineInfoIndex; i++){
        if (lineInfoTable->getLineInfo(i)->GET(l_lnno) == lineno){
            return lineInfoTable->getLineInfo(i)->GET(l_paddr);
        }
    }
    return 0;
}


uint64_t FunctionFinder::getAddressByLineNumber(uint32_t lineno, uint64_t addr){

        /* scan this function's section of the lineInfoTable to see if lineno
           can be found. if so, return the first address greater than addr 
           associated with that lineno */
    for (uint32_t i = beginLineInfoIndex; i < endLineInfoIndex; i++){
        if (lineInfoTable->getLineInfo(i)->GET(l_lnno) == lineno){
            if (lineInfoTable->getLineInfo(i)->GET(l_paddr) > addr){
                return lineInfoTable->getLineInfo(i)->GET(l_paddr);
            }
        }
    }
    return 0;
}


void FunctionFinder::print(){
    if (fromIncludeFile){
    } else {
        PRINT_INFOR("FunctionFinder %s, LineInfoTable range [%d, %d), firstline %d, parent file %s", 
                functionName, beginLineInfoIndex, endLineInfoIndex, firstLine, parentFile->getName());
    }
}


FunctionFinder::FunctionFinder(SymbolTable* symTable, LineInfoTable* linTable, StringTable* strTable, uint32_t idx,
struct idx_symndx* lineInfoFunctions, uint32_t numberOfLineInfoFunctions){
    uint32_t symbolClass;
    uint32_t numberOfLineInfos = linTable->getNumberOfLineInfos();
    uint32_t extSymbolIndex;
    uint32_t functionSymbolIndex;
    uint32_t lastLine;
    uint32_t beginFcnSymbolIndex;
    uint32_t endFcnSymbolIndex;
    char* fName;

    symbolTable = symTable;
    lineInfoTable = linTable;
    stringTable = strTable;

    functionSymbolIndex = idx;

        /* go backwards in the symbol table to look for the EXT symbol for this function */
    for (int32_t i = idx; i > 0; i--){
        if (!symbolTable->getSymbol(i)->isAuxilary()){
            symbolClass = ((Symbol*)symbolTable->getSymbol(i))->GET(n_sclass);
            if (symbolClass == C_EXT || symbolClass == C_HIDEXT || symbolClass == C_WEAKEXT){
                extSymbolIndex = i;
                i = 0;
            }
        }
    }

    functionName = symbolTable->getSymbolName(extSymbolIndex);


        /* go forwards in the symbol table to look for the begin FCN and end FCN symbols
           for this function */

    for (uint32_t i = functionSymbolIndex; i < symbolTable->getNumberOfSymbols(); i++){
        if (!symbolTable->getSymbol(i)->isAuxilary() && ((Symbol*)symbolTable->getSymbol(i))->GET(n_sclass) == C_FCN){
            fName = symbolTable->getSymbolName(i);
            if (!strcmp(fName,C_FCN_BEGIN_NAME)){
                beginFcnSymbolIndex = i;
                firstLine = ((Auxilary*)symbolTable->getSymbol(i+1))->GET_A(x_lnno,x_misc);
            }
            if (!strcmp(fName,C_FCN_END_NAME)){
                endFcnSymbolIndex = i;
                lastLine = ((Auxilary*)symbolTable->getSymbol(i+1))->GET_A(x_lnno,x_misc);
                i = symbolTable->getNumberOfSymbols();
            }
            free(fName);
        }                
    }

    for (uint32_t i = 0; i < numberOfLineInfoFunctions; i++){
        if (lineInfoFunctions[i].symndx == extSymbolIndex){
            beginLineInfoIndex = lineInfoFunctions[i].index;
            if (i == numberOfLineInfoFunctions - 1){
                endLineInfoIndex = numberOfLineInfos;
            } else {
                endLineInfoIndex = lineInfoFunctions[i+1].index;
            }
            i = numberOfLineInfoFunctions;
        }
    }
}


FunctionFinder::~FunctionFinder(){
    free(functionName);
}




uint32_t FileFinder::setNumberOfFunctions(uint32_t numberFunctions){
    numberOfFunctions = numberFunctions;
    memberFunctions = new FunctionFinder*[numberOfFunctions];
    return numberOfFunctions;
}


uint32_t FileFinder::addFunction(FunctionFinder* child, uint32_t idx){
    memberFunctions[idx] = child;
    return idx;
}


SourceFileFinder::SourceFileFinder(SymbolTable* symTable, LineInfoTable* linTable, StringTable* strTable, uint32_t idx){
    uint32_t auxNameLen;
    symbolTable = symTable;
    lineInfoTable = linTable;
    stringTable = strTable;
    includeFile = false;
    fileType = FILE_TYPE_SOURCE;
    
    char* fName = symbolTable->getSymbolName(idx);

        /* see if the symbol for this file in a generic fashion. if so, the next
           symbol contains the name for this file */
    if (!strcmp(fName, CPP_GENERIC_FILE_SYM_NAME)){
        cppNameScheme = true;
        nameIndex = idx + CPP_FILE_FORWARD_OFFSET_TO_NAME;
        free(fName);
        Auxilary* auxEntry = (Auxilary*)symbolTable->getSymbol(nameIndex);
        if (auxEntry->GET_A(x_zeroes, x_file) == 0){
            auxNameLen = strlen(stringTable->getString(auxEntry->GET_A(x_offset, x_file))); 
            fileName = new char[auxNameLen];            
            strcpy(fileName, stringTable->getString(auxEntry->GET_A(x_offset, x_file)));
        } else {
            auxNameLen = strlen(auxEntry->GET_A(x_fname, x_file));
            fileName = new char[auxNameLen];
            strcpy(fileName, auxEntry->GET_A(x_fname, x_file));
        }
    } else {
        cppNameScheme = false;
        nameIndex = idx;
        fileName = fName;
    }
    symbolIndex = idx;
}


SourceFileFinder::~SourceFileFinder(){
    free(fileName);
}


void SourceFileFinder::print(){
    PRINT_INFOR("Source File %s, index %d, cppNameScheme %d, nameIndex %d", fileName, symbolIndex, cppNameScheme, nameIndex);
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        PRINT_INFOR("\tMember function %s", memberFunctions[i]->getName());
    }
}


bool IncludeFileFinder::containsAddress(uint64_t addr){

        /* scan this include file's section of the lineInfoTable to see
           if addr can be found */
    for (uint32_t i = lineInfoIndex; i < endLineInfoIndex + 1; i++){
        if (lineInfoTable->getLineInfo(i)->GET(l_paddr)){
            if (lineInfoTable->getLineInfo(i)->GET(l_paddr) == addr){
                return true;
            }
        }
    }
    return false;
}


void IncludeFileFinder::print(){
    PRINT_INFOR("Include File %s, symbol index %d, line info indices [%d,%d]", fileName, symbolIndex, lineInfoIndex, endLineInfoIndex);
    for (uint32_t i = 0; i < numberOfFunctions; i++){
        PRINT_INFOR("\tMember function %s", memberFunctions[i]->getName());
    }
}


IncludeFileFinder::IncludeFileFinder(SymbolTable* symTable, LineInfoTable* linTable, StringTable* strTable, uint32_t idx, uint32_t linIndex, uint32_t
endLinIndex){
    symbolIndex = idx;
    lineInfoIndex = linIndex;
    endLineInfoIndex = endLinIndex;
    symbolTable = symTable;
    lineInfoTable = linTable;
    stringTable = strTable;
    includeFile = true;
    fileName = symbolTable->getSymbolName(idx);
    fileType = FILE_TYPE_INCLUDE;
}


IncludeFileFinder::~IncludeFileFinder(){
    free(fileName);
}

int32_t compare_idx_addr(const void* a, const void* b){
    return ( (*(struct idx_addr*)a).address - (*(struct idx_addr*)b).address);
}


