#include <LineInformation.h>
#include <DwarfSection.h>
#include <BinaryFile.h>
#include <BasicBlock.h>

char* LineInfoTable::getFileName(uint32_t idx){
    ASSERT(idx < numberOfFileNames && "File names index is out of bounds");
    return fileNames[idx];
}

char* LineInfoTable::getIncludePath(uint32_t idx){
    ASSERT(idx < numberOfFileNames && "File names index is out of bounds");
    uint8_t dirIndex;
    if (fileDirIndices[idx] == 0){
        return currentDirectory;
    } else {
        dirIndex = fileDirIndices[idx]-1;
    }
    ASSERT(dirIndex < numberOfIncludePaths && "Include paths index is out of bounds");
    return includePaths[dirIndex];
}

int searchLineInfoAddress(const void* arg1,const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    LineInfo* li = *((LineInfo**)arg2);

    ASSERT(li && "LineInfo should exist");

    uint64_t val = li->GET(lr_address);

    if(key < val)
        return -1;
    if(key > val)
        return 1;
    return 0;
}


int compareLineInfoAddress(const void* arg1,const void* arg2){
    LineInfo* li1 = *((LineInfo**)arg1);
    LineInfo* li2 = *((LineInfo**)arg2);

    ASSERT(li1 && li2 && "LineInfos should exist");

    uint64_t vl1 = li1->GET(lr_address);
    uint64_t vl2 = li2->GET(lr_address);

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    return 0;
}


LineInfo* LineInfoFinder::lookupLineInfo(uint64_t addr){
    void* res = bsearch(&addr,sortedLineInfos,numberOfLineInfos,sizeof(LineInfo*),searchLineInfoAddress);
    if (res){
        uint32_t ridx = (((char*)res)-((char*)sortedLineInfos))/sizeof(LineInfo*);
        return sortedLineInfos[ridx];
    }
    return NULL;
}

LineInfo* LineInfoFinder::lookupLineInfo(BasicBlock* bb){
    if (!sortedLineInfos){
        return NULL;
    }
    ASSERT(numberOfLineInfos && "Line information entries should be initialized");
    uint64_t targetAddr = bb->getAddress();

    //    PRINT_DEBUG_LINEINFO("Need to search %d lineinfos", numberOfLineInfos);

    for (uint32_t i = 0; i < numberOfLineInfos-1; i++){
        //  PRINT_DEBUG_LINEINFO("Searching for address at idx %d -- %llx <= %llx < %llx?", i, sortedLineInfos[i]->GET(lr_address), targetAddr, sortedLineInfos[i+1]->GET(lr_address));
        if (targetAddr >= sortedLineInfos[i]->GET(lr_address) &&
            targetAddr < sortedLineInfos[i+1]->GET(lr_address)){
            return sortedLineInfos[i];
        }
    }
    return NULL;
    
    uint32_t smin = 0;
    uint32_t smax = numberOfLineInfos-1;

    while (smin < smax){
        uint32_t sidx = (smin+smax)/2;
        if (sidx == smin || sidx == smax){
            break;
        }
        if (targetAddr < sortedLineInfos[sidx]->GET(lr_address)){
            smax = sidx;
        } else if (targetAddr > sortedLineInfos[sidx]->GET(lr_address)){
            smin = sidx;
        } else {
            return sortedLineInfos[sidx];            
        }
    }
    ASSERT(smin + 1 == smax && "Either the array is not sorted or I don't understand binary search");
    return sortedLineInfos[smin];
}

bool LineInfoFinder::verify(){
    uint32_t totalLineInfos = 0;
    for (uint32_t i = 0; i < dwarfLineInfoSection->getNumberOfLineInfoTables(); i++){
        for (uint32_t j = 0; j < dwarfLineInfoSection->getLineInfoTable(i)->getNumberOfLineInfos(); j++){
            if (dwarfLineInfoSection->getLineInfoTable(i)->getLineInfo(j)->GET(lr_address)){
                totalLineInfos++;
            }
        }
    }
    if (totalLineInfos != numberOfLineInfos){
        PRINT_ERROR("The number of line informations in the finder does not match the total from the dwarf section");
        return false;
    }

    for (uint32_t i = 1; i < numberOfLineInfos; i++){
        if (sortedLineInfos[i-1]->GET(lr_address) > sortedLineInfos[i]->GET(lr_address)){
            PRINT_ERROR("Line informations in line info finder should be sorted!");
            return false;
        }
    }

    return true;
}


LineInfoFinder::LineInfoFinder(DwarfLineInfoSection* dwarf){
    dwarfLineInfoSection = dwarf;

    numberOfLineInfos = 0;

    for (uint32_t i = 0; i < dwarfLineInfoSection->getNumberOfLineInfoTables(); i++){
        for (uint32_t j = 0; j < dwarfLineInfoSection->getLineInfoTable(i)->getNumberOfLineInfos(); j++){
            if (dwarfLineInfoSection->getLineInfoTable(i)->getLineInfo(j)->GET(lr_address)){
                numberOfLineInfos++;
            }
        }
    }
    PRINT_DEBUG_LINEINFO("Using %d lineinfos", numberOfLineInfos);

    sortedLineInfos = new LineInfo*[numberOfLineInfos];
    numberOfLineInfos = 0;

    for (uint32_t i = 0; i < dwarfLineInfoSection->getNumberOfLineInfoTables(); i++){
        for (uint32_t j = 0; j < dwarfLineInfoSection->getLineInfoTable(i)->getNumberOfLineInfos(); j++){
            if (dwarfLineInfoSection->getLineInfoTable(i)->getLineInfo(j)->GET(lr_address)){
                sortedLineInfos[numberOfLineInfos++] = dwarfLineInfoSection->getLineInfoTable(i)->getLineInfo(j);
            }
        }
    }

    PRINT_DEBUG_LINEINFO("Using %d lineinfos", numberOfLineInfos);
    qsort(sortedLineInfos,numberOfLineInfos,sizeof(LineInfo*),compareLineInfoAddress);    

    verify();
}


LineInfoFinder::~LineInfoFinder(){
    if (sortedLineInfos){
        delete[] sortedLineInfos;
    }
}

uint32_t LineInfoTable::getOpcodeLength(uint32_t idx){
    ASSERT(idx < numberOfOpcodes && "Opcode index is out of bounds");
    return opcodes[idx];
}

void LineInfoTable::dump(BinaryOutputFile* b, uint32_t offset){
    __FUNCTION_NOT_IMPLEMENTED;
}

uint32_t LineInfoTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currByte = 0;

    // get the line info header
    if(!binaryInputFile->copyBytesIterate(&entry.li_length,sizeof(uint32_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_version,sizeof(uint16_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_prologue_length,sizeof(uint32_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_min_insn_length,sizeof(uint8_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_default_is_stmt,sizeof(uint8_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_line_base,sizeof(uint8_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_line_range,sizeof(uint8_t))){
        PRINT_ERROR("Line info header can not be read");
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_opcode_base,sizeof(uint8_t))){
        PRINT_ERROR("Line info header can not be read");
    }

    sizeInBytes = GET(li_length) + sizeof(uint32_t);

    registers = new LineInfo(0,NULL,this);
    registers->SET(lr_is_stmt,GET(li_default_is_stmt));

    currByte += sizeof(uint32_t) * 2 + sizeof(uint16_t) + sizeof(uint8_t) * 5;
    // get the opcode table
    numberOfOpcodes = GET(li_opcode_base)-1;
    opcodes = new uint8_t[numberOfOpcodes];
    uint8_t tmp;
    for (uint32_t i = 0; i < numberOfOpcodes; i++){
        if (!binaryInputFile->copyBytesIterate(&tmp,sizeof(uint8_t))){
            PRINT_ERROR("Line info header can not be read");
        }
        opcodes[i] = tmp;
    }
    currByte += sizeof(uint8_t) * numberOfOpcodes;

    // get the include directory table
    uint32_t numBytes = 0;
    PRINT_DEBUG_LINEINFO("Looking for directory table at byte %d", currByte);
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    do {
        ASSERT(currByte + numBytes < sizeInBytes && "Error parsing LineInfoTable include table");
        b1 = b2;
        b2 = rawDataPtr[currByte+numBytes];
        numBytes++;
    } while (b1 || b2);

    PRINT_DEBUG_LINEINFO("Number of bytes contributing to include table %d", numBytes);

    numberOfIncludePaths = 0;

    for (uint32_t i = 0; i < numBytes-1; i++){
        if (!rawDataPtr[currByte+i]){
            numberOfIncludePaths++;
        }
    }

    PRINT_DEBUG_LINEINFO("Number of include paths %d", numberOfIncludePaths);

    includePaths = new char*[numberOfIncludePaths];
    for (uint32_t i = 0; i < numberOfIncludePaths; i++){
        includePaths[i] = 0;
    }
    numberOfIncludePaths = 0;
    for (uint32_t i = 0; i < numBytes-1; i++){
        if (!includePaths[numberOfIncludePaths]){
            includePaths[numberOfIncludePaths] = rawDataPtr + currByte + i;
        }
        if (!rawDataPtr[currByte+i]){
            numberOfIncludePaths++;
        }
    }

    PRINT_DEBUG_LINEINFO("Number of include paths %d", numberOfIncludePaths);
    for (uint32_t i = 0; i < numberOfIncludePaths; i++){
        PRINT_DEBUG_LINEINFO("Include path %d = %s", i, includePaths[i]);
    }

    // get the file names table
    uint32_t len = 0;
    numberOfFileNames = 0;
    currByte += numBytes;
    numBytes = 0;
    PRINT_DEBUG_LINEINFO("Looking for directory table at byte %d", currByte);
    b1 = 0;
    b2 = 0;
    do {
        b1 = b2;
        b2 = rawDataPtr[currByte+numBytes];
        numBytes++;
        // skip the info bytes that occur after the file name
        if (!b2){
            numberOfFileNames++;
            for (uint32_t i = 0; i < 3; i++){
                dwarf2_get_leb128_unsigned(rawDataPtr+currByte+numBytes,&len);
                numBytes += len;
            }
        }
        PRINT_DEBUG_LINEINFO("Files %d, Bytes %hhx %hhx -- offset=%d", numberOfFileNames, b1, b2, currByte + numBytes);
    } while (b1 || b2);
    numberOfFileNames--;

    PRINT_DEBUG_LINEINFO("Number of bytes contributing to file name table %d", numBytes);
    PRINT_DEBUG_LINEINFO("Number of file names %d", numberOfFileNames);

    fileNames = new char*[numberOfFileNames];
    fileDirIndices = new uint8_t[numberOfFileNames];
    fileModTimes = new uint8_t[numberOfFileNames];
    fileSizes = new uint8_t[numberOfFileNames];
    for (uint32_t i = 0; i < numberOfFileNames; i++){
        fileNames[i] = 0;
        fileDirIndices[i] = 0;
        fileModTimes[i] = 0;
        fileSizes[i] = 0;
    }

    numBytes = 0;
    for (uint32_t i = 0; i < numberOfFileNames; i++){
        fileNames[i] = rawDataPtr + currByte + numBytes;
        PRINT_DEBUG_LINEINFO("file %s at offset %d",  fileNames[i], currByte + numBytes);
        numBytes += strlen(fileNames[i]) + 1;
        fileDirIndices[i] = dwarf2_get_leb128_unsigned(rawDataPtr+currByte+numBytes,&len);
        numBytes += len;
        fileModTimes[i] = dwarf2_get_leb128_unsigned(rawDataPtr+currByte+numBytes,&len);
        numBytes += len;
        fileSizes[i] = dwarf2_get_leb128_unsigned(rawDataPtr+currByte+numBytes,&len);
        numBytes += len;
    }
    numBytes++;
    currByte += numBytes;

    for (uint32_t i = 0; i < numberOfFileNames; i++){
        PRINT_DEBUG_LINEINFO("File Name %d = %s", i, fileNames[i]);
    }

    // extract the line information units
    numberOfLineInformations = sizeInBytes - currByte;
    ASSERT(numberOfLineInformations && "Line information section should contain some line number infomation");

    numBytes = 0;
    lineInformations = new LineInfo*[numberOfLineInformations];
    numberOfLineInformations = 0;
    while (currByte + numBytes < sizeInBytes){
        lineInformations[numberOfLineInformations] = new LineInfo(numberOfLineInformations,rawDataPtr+currByte+numBytes,this);
#ifdef DEBUG_LINEINFO    
        lineInformations[numberOfLineInformations]->print();
#endif
        numBytes += lineInformations[numberOfLineInformations]->getInstructionSize();
        numberOfLineInformations++;
    }
    LineInfo** dummy = new LineInfo*[numberOfLineInformations];
    for (uint32_t i = 0; i < numberOfLineInformations; i++){
        dummy[i] = lineInformations[i];
    }
    delete[] lineInformations;
    lineInformations = dummy;

    currByte += numBytes;

    PRINT_DEBUG_LINEINFO("curr %d size %d", currByte, sizeInBytes);
    ASSERT(currByte == sizeInBytes);

    verify();
    return sizeInBytes;
}

bool LineInfoTable::verify(){
    for (uint32_t i = 0; i < numberOfFileNames; i++){
        if (fileDirIndices[i] >= numberOfFileNames){
            PRINT_ERROR("Illegal directory index %d found for file %s (idx %d)", fileDirIndices[i], fileNames[i], i);
            return false;
        }
        if (fileNames[i] < rawDataPtr || fileNames[i] > rawDataPtr + sizeInBytes){
            PRINT_ERROR("File directory name table exceeds section boundaries");
            return false;
        }
    }
    for (uint32_t i = 0; i < numberOfIncludePaths; i++){
        if (includePaths[i] < rawDataPtr || includePaths[i] > rawDataPtr + sizeInBytes){
            PRINT_ERROR("Include paths table exceeds section boundaries");
            return false;
        }
    }
    for (uint32_t i = 0; i < numberOfLineInformations; i++){
        if (!lineInformations[i]->verify()){
            return false;
        }
    }
}


void LineInfoTable::print(){
    PRINT_INFOR("Line Info Table (%d):", index);

    PRINT_INFOR("Header:");
    PRINT_INFOR("\tLength                 : %d",   GET(li_length));
    PRINT_INFOR("\tVersion                : %d",  GET(li_version));
    PRINT_INFOR("\tPrologue Length        : %d",   GET(li_prologue_length));
    PRINT_INFOR("\tMin Instruction Length : %hhd", GET(li_min_insn_length));
    PRINT_INFOR("\tDefault is_stmt        : %hhd", GET(li_default_is_stmt));
    PRINT_INFOR("\tLine Base              : %hhd", GET(li_line_base));
    PRINT_INFOR("\tLine Range             : %hhd", GET(li_line_range));
    PRINT_INFOR("\tOpcode Base            : %hhd", GET(li_opcode_base));

    PRINT_INFOR("Opcodes (%hhd):", numberOfOpcodes);
    for (uint32_t i = 0; i < numberOfOpcodes; i++){
        PRINT_INFOR("\tOpcode %2d has %hhd args", i+1, opcodes[i]);
    }
    PRINT_INFOR("Include Directory Table:");
    for (uint32_t i = 0; i < numberOfIncludePaths; i++){
        PRINT_INFOR("\t%s", includePaths[i]);
    }
    PRINT_INFOR("File Name Table:");
    PRINT_INFOR("Idx\tDir\tTime\tSize\tName");
    for (uint32_t i = 0; i < numberOfFileNames; i++){
        PRINT_INFOR("\t%d\t%2hhd\t%2hhd\t%2hhd\t%s/%s", i+1, fileDirIndices[i], fileModTimes[i], fileSizes[i], getIncludePath(i), getFileName(i));
    }

    PRINT_INFOR("Line Information Table: %d entries", numberOfLineInformations);
    PRINT_INFOR("\t\t\t(Size)RawBytes\t\tAddress\t\tLine\t(Ptr)File")
    for (uint32_t i = 0; i < numberOfLineInformations; i++){
        lineInformations[i]->print();
    }
}


LineInfoTable::LineInfoTable(uint32_t idx, char* raw){
    index = idx;
    rawDataPtr = raw;
    
    opcodes = NULL;
    numberOfOpcodes = 0;

    lineInformations = NULL;
    numberOfLineInformations = 0;

    includePaths = NULL;
    numberOfIncludePaths = 0;

    fileNames = NULL;
    numberOfFileNames = 0;
    fileDirIndices = NULL;
    fileModTimes = NULL;
    fileSizes = NULL;

    registers = NULL;
}


LineInfoTable::~LineInfoTable(){
    if (lineInformations){
        for (uint32_t i = 0; i < numberOfLineInformations; i++){
            delete lineInformations[i];
        }
        delete[] lineInformations;
    }
    if (opcodes){
        delete[] opcodes;
    }
    if (includePaths){
        delete[] includePaths;
    }
    if (fileNames){
        delete[] fileNames;
    }
    if (fileDirIndices){
        delete[] fileDirIndices;
    }
    if (fileModTimes){
        delete[] fileModTimes;
    }
    if (fileSizes){
        delete[] fileSizes;
    }
    if (registers){
        delete registers;
    }
}


void LineInfo::initializeWithDefaults(){
    ASSERT(!index && "This constructor should be used only for the first line info instruction");

    SET(lr_address,0);
    SET(lr_file,1);
    SET(lr_line,1);
    SET(lr_column,0);
    SET(lr_isa,0);
    SET(lr_is_stmt,header->GET(li_default_is_stmt));
    SET(lr_basic_block,0);
    SET(lr_end_sequence,0);
    SET(lr_prologue_end,0);
    SET(lr_epilogue_begin,0);

    instructionBytes = NULL;
}

void LineInfo::updateRegsExtendedOpcode(char* instruction){
    ASSERT(instruction[0] == DW_LNS_extended_op && "This function should only be called on instructions with extended opcodes");

    PRINT_DEBUG_LINEINFO("Extended opcode %hhx, size %hhd", instruction[0], instruction[1]);

    instructionSize++;
    instructionSize += instruction[1];

    LineInfo* regs = header->getRegisters();

    switch(instruction[2]){
    case DW_LNE_end_sequence:
        // special case that modifies the regs before using them
        SET(lr_end_sequence,1);
        regs->initializeWithDefaults();
        break;
    case DW_LNE_set_address:
        uint32_t addr;
        ASSERT(instructionSize == 3+sizeof(uint32_t) && "This instruction has an unexpected size");
        memcpy((void*)&addr,(void*)(instruction+3),sizeof(uint32_t));
        regs->SET(lr_address,addr);
        break;
    case DW_LNE_define_file:
        __FUNCTION_NOT_IMPLEMENTED;
        break;
    default:
        PRINT_ERROR("This extended opcode %02hhx is not defined in the dwarf standard", instruction[2]);
        break;
    }
}


void LineInfo::updateRegsStandardOpcode(char* instruction){

    PRINT_DEBUG_LINEINFO("Standard opcode %hhx found", instruction[0]);

    // get the operands
    uint32_t numberOfOperands = header->getOpcodeLength(instruction[0]-1);

    LineInfo* regs = header->getRegisters();
    uint16_t addr16;
    uint64_t uop0;
    int64_t sop0;
    uint32_t len;

    switch(instruction[0]){
    case DW_LNS_copy:
        ASSERT(numberOfOperands == 0);
        regs->SET(lr_basic_block,0);
        regs->SET(lr_prologue_end,0);
        regs->SET(lr_epilogue_begin,0);
        break;
    case DW_LNS_advance_pc:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        instructionSize += len;
        regs->INCREMENT(lr_address,header->GET(li_min_insn_length)*uop0);
        break;
    case DW_LNS_advance_line:
        ASSERT(numberOfOperands == 1);
        sop0 = dwarf2_get_leb128_signed(instruction+1,&len);
        ASSERT(sop0 == (int32_t)sop0 && "Cannot use more than 32 bits for line value");
        instructionSize += len;
        regs->INCREMENT(lr_line,(int32_t)sop0);
        break;
    case DW_LNS_set_file:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        ASSERT(uop0 == (uint32_t)uop0 && "Cannot use more than 32 bits for file value");
        instructionSize += len;
        regs->SET(lr_file,(uint32_t)uop0);
        break;
    case DW_LNS_set_column:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        ASSERT(uop0 == (uint32_t)uop0 && "Cannot use more than 32 bits for file value");
        instructionSize += len;
        regs->SET(lr_column,(uint32_t)uop0);
        break;
    case DW_LNS_negate_stmt:
        ASSERT(numberOfOperands == 0);
        if (regs->GET(lr_is_stmt)){
            regs->SET(lr_is_stmt,0);
        } else {
            regs->SET(lr_is_stmt,1);
        }
        break;
    case DW_LNS_set_basic_block:
        ASSERT(numberOfOperands == 0);
        regs->SET(lr_basic_block,1);
        break;
    case DW_LNS_const_add_pc:
        ASSERT(numberOfOperands == 0);
        addr16 = ((255 - header->GET(li_opcode_base)) / header->GET(li_line_range)) * header->GET(li_min_insn_length);
        regs->INCREMENT(lr_address,addr16);
        break;
    case DW_LNS_fixed_advance_pc:
        ASSERT(numberOfOperands == 1);
        memcpy((void*)&addr16,(void*)(instruction+1),sizeof(uint16_t));
        instructionSize += 2;
        regs->INCREMENT(lr_address,addr16);
        break;
    case DW_LNS_set_prologue_end:
        ASSERT(numberOfOperands == 0);
        regs->SET(lr_prologue_end,1);
        break;
    case DW_LNS_set_epilogue_begin:
        ASSERT(numberOfOperands == 0);
        regs->SET(lr_epilogue_begin,1);
        break;
    case DW_LNS_set_isa:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        ASSERT(uop0 == (uint32_t)uop0 && "Cannot use more than 32 bits for isa value");
        instructionSize += len;
        regs->SET(lr_isa,(uint32_t)uop0);
        break;
    default:
        PRINT_ERROR("This standard opcode %02hhx is not defined in the dwarf standard", instruction[0]);
        break;        
    }
}


void LineInfo::updateRegsSpecialOpcode(char* instruction){
    PRINT_DEBUG_LINEINFO("Special opcode %hhx found", instruction[0]);

    uint8_t adjusted_opcode = instruction[0] - header->GET(li_opcode_base);

    int8_t addr_inc = (adjusted_opcode /  header->GET(li_line_range)) *  header->GET(li_min_insn_length);
    int8_t line_inc = header->GET(li_line_base) + (adjusted_opcode % header->GET(li_line_range));

    LineInfo* regs = header->getRegisters();

    regs->SET(lr_address,GET(lr_address)+addr_inc);
    regs->SET(lr_line,GET(lr_line)+line_inc);
    regs->SET(lr_basic_block,0);
    regs->SET(lr_prologue_end,0);
    regs->SET(lr_epilogue_begin,0);
}

void LineInfo::initializeWithInstruction(char* instruction){
    LineInfo* regs = header->getRegisters();

    memcpy((void*)this->charStream(),(void*)regs->charStream(),Size__Dwarf_LineInfo);

    uint8_t opcode = (uint8_t)instruction[0];
    PRINT_DEBUG_LINEINFO("Opcode %hhd, base %hhd", opcode, header->GET(li_opcode_base));
    if (opcode < header->GET(li_opcode_base)-1){
        if (opcode == DW_LNS_extended_op){
            updateRegsExtendedOpcode(instruction);
        } else {
            updateRegsStandardOpcode(instruction);
        }
    } else {
        updateRegsSpecialOpcode(instruction);
    }

    instructionBytes = new uint8_t[instructionSize];
    memcpy((void*)instructionBytes,(void*)instruction,instructionSize);
}

LineInfo::LineInfo(uint32_t idx, char* instruction, LineInfoTable* hdr){
    index = idx;
    instructionSize = 1;
    header = hdr;

    if (!instruction){
        initializeWithDefaults();
    } else {
        initializeWithInstruction(instruction);
    }
}

LineInfo::~LineInfo(){
    if (instructionBytes){
        delete[] instructionBytes;
    }
}

void LineInfo::print(){

    PRINT_INFO();
    PRINT_OUT("LineInfo(%d)\t(%d)", index, instructionSize);
    if (instructionBytes){
        for (uint32_t i = 0; i < instructionSize; i++){
            PRINT_OUT("%02hhx", instructionBytes[i]);
        }
        for (uint32_t i = 0; i < 8-instructionSize; i++){
            PRINT_OUT("  ");
        }
    } else {
        PRINT_OUT("n/a");
    }
    PRINT_OUT("%16llx\t%4d\t(%d)%s/%s", GET(lr_address), GET(lr_line), GET(lr_file), getFilePath(), getFileName());
    PRINT_OUT("\n");
    /*
    PRINT_INFOR("LineInfo(%d): instruction size %d", index, instructionSize);
    PRINT_INFO();
    PRINT_OUT("Raw Instruction Bytes: ");
    if (instructionBytes){
        for (uint32_t i = 0; i < instructionSize; i++){
            PRINT_OUT("%02hhx", instructionBytes[i]);
        }
    } else {
        PRINT_OUT("n/a");
    }
    PRINT_OUT("\n");
    PRINT_INFOR("\tAddress : %llx", GET(lr_address));
    PRINT_INFOR("\tFile : %d", GET(lr_file));
    PRINT_INFOR("\tLine : %d", GET(lr_line));
    PRINT_INFOR("\tColumn : %d", GET(lr_column));
    PRINT_INFOR("\tISA : %d", GET(lr_isa));
    PRINT_INFOR("\tFlags : %d %d %d %d %d", GET(lr_is_stmt), GET(lr_basic_block), GET(lr_end_sequence), GET(lr_prologue_end), GET(lr_epilogue_begin));
    */
}

char* LineInfo::getFileName(){
    ASSERT(header && "Line info table should be initialized");
    return header->getFileName(GET(lr_file)-1);
}

char* LineInfo::getFilePath(){
    ASSERT(header && "Line info table should be initialized");
    return header->getIncludePath(GET(lr_file)-1);
}

bool LineInfo::verify(){
    return true;
}
