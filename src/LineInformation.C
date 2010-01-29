#include <LineInformation.h>

#include <Base.h>
#include <BasicBlock.h>
#include <BinaryFile.h>
#include <DwarfSection.h>
#include <ElfFile.h>

char* LineInfoTable::getFileName(uint32_t idx){
    return fileNames[idx].fn_name;
}

char* LineInfoTable::getIncludePath(uint32_t idx){
    uint8_t dirIndex;
    if (idx >= fileNames.size() || fileNames[idx].fn_dir_index == 0){
        return currentDirectory;
    } else {
        dirIndex = fileNames[idx].fn_dir_index-1;
    }
    ASSERT(dirIndex < includePaths.size() && "Include paths index is out of bounds");
    return includePaths[dirIndex];
}

int searchLineInfoAddressExact(const void* arg1,const void* arg2){
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

int searchLineInfoAddress(const void* arg1,const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    LineInfo* li = *((LineInfo**)arg2);

    ASSERT(li && "LineInfo should exist");

    uint64_t val = li->GET(lr_address);

    if(key < val)
        return -1;
    if(key >= val + li->getAddressSpan())
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
    void* res = bsearch(&addr,&sortedLineInfos,sortedLineInfos.size(),sizeof(LineInfo*),searchLineInfoAddress);
    if (res){
        uint32_t ridx = (((char*)res)-((char*)&sortedLineInfos))/sizeof(LineInfo*);
        return sortedLineInfos[ridx];
    }
    return NULL;
}

LineInfo* LineInfoFinder::lookupLineInfo(BasicBlock* bb){
    if (!sortedLineInfos.size()){
        return NULL;
    }
    return lookupLineInfo(bb->getProgramAddress());
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
    if (totalLineInfos != sortedLineInfos.size()){
        PRINT_ERROR("The number of line informations in the finder does not match the total from the dwarf section");
        return false;
    }

    for (uint32_t i = 1; i < sortedLineInfos.size(); i++){
        if (sortedLineInfos[i-1]->GET(lr_address) > sortedLineInfos[i]->GET(lr_address)){
            PRINT_ERROR("Line informations in line info finder should be sorted!");
            return false;
        }
    }

    return true;
}


LineInfoFinder::LineInfoFinder(DwarfLineInfoSection* dwarf){
    dwarfLineInfoSection = dwarf;
    ASSERT(dwarfLineInfoSection);

    for (uint32_t i = 0; i < dwarfLineInfoSection->getNumberOfLineInfoTables(); i++){
        for (uint32_t j = 0; j < dwarfLineInfoSection->getLineInfoTable(i)->getNumberOfLineInfos(); j++){
            if (dwarfLineInfoSection->getLineInfoTable(i)->getLineInfo(j)->GET(lr_address)){
                sortedLineInfos.append(dwarfLineInfoSection->getLineInfoTable(i)->getLineInfo(j));
            }
        }
    }

    PRINT_DEBUG_LINEINFO("Using %d lineinfos", sortedLineInfos.size());
    qsort(&sortedLineInfos,sortedLineInfos.size(),sizeof(LineInfo*),compareLineInfoAddress);    

    for (uint32_t i = 0; i < sortedLineInfos.size()-1; i++){
        sortedLineInfos[i]->setAddressSpan(sortedLineInfos[i+1]->GET(lr_address)-sortedLineInfos[i]->GET(lr_address));
    }

    verify();
}


LineInfoFinder::~LineInfoFinder(){
}

uint32_t LineInfoTable::getOpcodeLength(uint32_t idx){
    return opcodes[idx];
}

void LineInfoTable::dump(BinaryOutputFile* b, uint32_t offset){
    __FUNCTION_NOT_IMPLEMENTED;
}

uint32_t LineInfoTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currByte = 0;

    uint32_t firstWord = 0;
    if (!binaryInputFile->copyBytesIterate(&firstWord,sizeof(uint32_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint32_t);
    if (firstWord >= DWARF2_FIRSTBYTE_LO){
        if (firstWord == DWARF2_FIRSTBYTE_64BIT_FORMAT){
            format = DebugFormat_DWARF2_64bit;
        } else {
            PRINT_ERROR("The first word %d of this debug section is not an understood DWARF2 format", firstWord);
        }
    } else {
        format = DebugFormat_DWARF2_32bit;
    }

    ASSERT(format && "The format of this debug section is unknown");
    PRINT_DEBUG_LINEINFO("The format of the debug section is %d", format);

    // get the line info header
    if (format == DebugFormat_DWARF2_64bit){
        if(!binaryInputFile->copyBytesIterate(&entry.li_length,sizeof(uint64_t))){
            PRINT_ERROR("Line info section header cannot be read");
        }
        currByte += sizeof(uint64_t);
    } else {
        entry.li_length = firstWord;
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_version,sizeof(uint16_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint16_t);
    if (format == DebugFormat_DWARF2_64bit){
        if(!binaryInputFile->copyBytesIterate(&entry.li_prologue_length,sizeof(uint64_t))){
            PRINT_ERROR("Line info section header cannot be read");
        }
        currByte += sizeof(uint64_t);
    } else {
        if(!binaryInputFile->copyBytesIterate(&firstWord,sizeof(uint32_t))){
            PRINT_ERROR("Line info section header cannot be read");
        }
        currByte += sizeof(uint32_t);
        entry.li_prologue_length = firstWord;
    }
    if(!binaryInputFile->copyBytesIterate(&entry.li_min_insn_length,sizeof(uint8_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint8_t);
    if(!binaryInputFile->copyBytesIterate(&entry.li_default_is_stmt,sizeof(uint8_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint8_t);
    if(!binaryInputFile->copyBytesIterate(&entry.li_line_base,sizeof(uint8_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint8_t);
    if(!binaryInputFile->copyBytesIterate(&entry.li_line_range,sizeof(uint8_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint8_t);
    if(!binaryInputFile->copyBytesIterate(&entry.li_opcode_base,sizeof(uint8_t))){
        PRINT_ERROR("Line info section header cannot be read");
    }
    currByte += sizeof(uint8_t);

    sizeInBytes = GET(li_length) + sizeof(uint32_t);
    if (format == DebugFormat_DWARF2_64bit){
        sizeInBytes += 8;
    }

    DEBUG_LINEINFO(dwarfLineInfoSection->printBytes(0,0,0);)

    registers = new LineInfo(0,NULL,this);
    registers->SET(lr_is_stmt,GET(li_default_is_stmt));

    // get the opcode table
    uint8_t tmp;
    for (uint32_t i = 0; i < GET(li_opcode_base)-1; i++){
        if (!binaryInputFile->copyBytesIterate(&tmp,sizeof(uint8_t))){
            PRINT_ERROR("Line info section opcode table cannot be read");
        }
        opcodes.append(tmp);
        currByte += sizeof(uint8_t);
    }


    // get the include directory table
    PRINT_DEBUG_LINEINFO("Looking for directory table at byte %d", currByte);
    while (rawDataPtr[currByte]){
        includePaths.append(rawDataPtr+currByte);
        currByte += strlen(includePaths.back())+1;
    }
    currByte++;
    PRINT_DEBUG_LINEINFO("Number of include paths %d", includePaths.size());
    for (uint32_t i = 0; i < includePaths.size(); i++){
        PRINT_DEBUG_LINEINFO("Include path %d = %s", i, includePaths[i]);
    }

    // get the file names table
    PRINT_DEBUG_LINEINFO("Looking for file name table at byte %d", currByte);
    DWARF2_FileName currentFile;
    uint32_t len = 0;
    while (rawDataPtr[currByte]){
        currentFile.fn_name = rawDataPtr+currByte;
        currByte += strlen(currentFile.fn_name)+1;
        currentFile.fn_dir_index = dwarf2_get_leb128_unsigned(rawDataPtr+currByte,&len);
        currByte += len;
        currentFile.fn_mod_time = dwarf2_get_leb128_unsigned(rawDataPtr+currByte,&len);
        currByte += len;
        currentFile.fn_size = dwarf2_get_leb128_unsigned(rawDataPtr+currByte,&len);
        currByte += len;
        fileNames.append(currentFile);
    }
    currByte++;
    PRINT_DEBUG_LINEINFO("Number of file name entries %d", fileNames.size());
    for (uint32_t i = 0; i < fileNames.size(); i++){
        PRINT_DEBUG_LINEINFO("File Name %d = %s", i, fileNames[i].fn_name);
    }

    // extract the line information units
    PRINT_DEBUG_LINEINFO("Looking for line info program instructions at byte %d", currByte);
    uint32_t liIndex = 0;
    while (currByte < sizeInBytes){
        lineInformations.append(new LineInfo(liIndex++,rawDataPtr+currByte,this));
        DEBUG_LINEINFO(lineInformations.back()->print();)
        currByte += lineInformations.back()->getInstructionSize();
    }

    PRINT_DEBUG_LINEINFO("Found %d lineinfo program instructions", lineInformations.size());

    ASSERT(currByte == sizeInBytes);

    verify();
    return sizeInBytes;
}

bool LineInfoTable::verify(){
    for (uint32_t i = 0; i < fileNames.size(); i++){
        if (fileNames[i].fn_dir_index > fileNames.size()){
            PRINT_ERROR("Illegal directory index %d found for file %s (idx %d)", fileNames[i].fn_dir_index, fileNames[i].fn_name, i);
            return false;
        }
        if (fileNames[i].fn_name < rawDataPtr || fileNames[i].fn_name > rawDataPtr + sizeInBytes){
            PRINT_ERROR("File directory name table exceeds section boundaries");
            return false;
        }
    }
    for (uint32_t i = 0; i < includePaths.size(); i++){
        if (includePaths[i] < rawDataPtr || includePaths[i] > rawDataPtr + sizeInBytes){
            PRINT_ERROR("Include paths table exceeds section boundaries");
            return false;
        }
    }
    for (uint32_t i = 0; i < lineInformations.size(); i++){
        if (!lineInformations[i]->verify()){
            return false;
        }
    }
    return true;
}


void LineInfoTable::print(){
    PRINT_INFOR("Line Info Table (%d):", index);

    PRINT_INFOR("Header:");
    PRINT_INFOR("\tLength                 : %lld",   GET(li_length));
    PRINT_INFOR("\tVersion                : %d",  GET(li_version));
    PRINT_INFOR("\tPrologue Length        : %lld",   GET(li_prologue_length));
    PRINT_INFOR("\tMin Instruction Length : %hhd", GET(li_min_insn_length));
    PRINT_INFOR("\tDefault is_stmt        : %hhd", GET(li_default_is_stmt));
    PRINT_INFOR("\tLine Base              : %hhd", GET(li_line_base));
    PRINT_INFOR("\tLine Range             : %hhd", GET(li_line_range));
    PRINT_INFOR("\tOpcode Base            : %hhd", GET(li_opcode_base));

    PRINT_INFOR("Opcodes (%hhd):", opcodes.size());
    for (uint32_t i = 0; i < opcodes.size(); i++){
        PRINT_INFOR("\tOpcode %2d has %hhd args", i+1, opcodes[i]);
    }
    PRINT_INFOR("Include Directory Table:");
    for (uint32_t i = 0; i < includePaths.size(); i++){
        PRINT_INFOR("\t%s", includePaths[i]);
    }
    PRINT_INFOR("File Name Table:");
    PRINT_INFOR("Idx\tDir\tTime\tSize\tName");
    for (uint32_t i = 0; i < fileNames.size(); i++){
        PRINT_INFOR("\t%d\t%2lld\t%2lld\t%2lld\t%s/%s", i+1, fileNames[i].fn_dir_index, fileNames[i].fn_mod_time, fileNames[i].fn_size, getIncludePath(fileNames[i].fn_dir_index), fileNames[i].fn_name);
    }

    PRINT_INFOR("Line Information Table: %d entries", lineInformations.size());
    PRINT_INFOR("%6s\t%6s\t%16s%8s\t%10s%16s\t%4s\t%s", "Index", "Size", "Raw Bytes       ", "Type", "Opcode", "Address", "Line", "File");
    for (uint32_t i = 0; i < lineInformations.size(); i++){
        lineInformations[i]->print();
    }
}


LineInfoTable::LineInfoTable(uint32_t idx, char* raw, DwarfLineInfoSection* dwarf){
    index = idx;
    rawDataPtr = raw;
    dwarfLineInfoSection = dwarf;
    format = DebugFormat_undefined;

    registers = NULL;
}


LineInfoTable::~LineInfoTable(){
    for (uint32_t i = 0; i < lineInformations.size(); i++){
        delete lineInformations[i];
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
}

uint32_t LineInfoTable::getAddressSize(){
    if (dwarfLineInfoSection->getElfFile()->is64Bit()){
        return sizeof(uint64_t);
    }
    return sizeof(uint32_t);
}

void LineInfo::updateRegsExtendedOpcode(char* instruction){
    ASSERT(instruction[0] == DW_LNS_extended_op && "This function should only be called on instructions with extended opcodes");

    uint32_t addressSize;

    uint8_t size = instruction[1];
    uint8_t opcode = instruction[2];

    PRINT_DEBUG_LINEINFO("Extended opcode %hhx, size %hhd", opcode, size);

    instructionBytes.append(instruction[1]);
    instructionBytes.append(instruction[2]);

    LineInfo* regs = header->getRegisters();

    switch(opcode){
    case DW_LNE_end_sequence:
        // special case that modifies the regs before using them
        SET(lr_end_sequence,1);
        regs->initializeWithDefaults();
        break;
    case DW_LNE_set_address:
        addressSize = header->getAddressSize();
        for (uint32_t i = 0; i < addressSize; i++){
            instructionBytes.append(instruction[3+i]);
        }
        ASSERT(instructionBytes.size() == 3+addressSize && "This instruction has an unexpected size");

        if (addressSize == sizeof(uint32_t)){
            uint32_t addr = getUInt32(instruction+3);
            regs->SET(lr_address,addr);
        } else {
            ASSERT(addressSize == sizeof(uint64_t) && "addressSize has an unexpected value");   
            uint64_t addr = getUInt64(instruction+3);
            regs->SET(lr_address,addr);
        }
        break;
    case DW_LNE_define_file:
        __FUNCTION_NOT_IMPLEMENTED;
        break;
    default:
        PRINT_ERROR("This extended opcode %02hhx is not defined in the dwarf standard", opcode);
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

    switch(instructionBytes[0]){
    case DW_LNS_copy:
        ASSERT(numberOfOperands == 0);
        regs->SET(lr_basic_block,0);
        regs->SET(lr_prologue_end,0);
        regs->SET(lr_epilogue_begin,0);
        break;
    case DW_LNS_advance_pc:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        for (uint32_t i = 0; i < len; i++){
            instructionBytes.append(instruction[1+i]);
        }
        regs->INCREMENT(lr_address,header->GET(li_min_insn_length)*uop0);
        break;
    case DW_LNS_advance_line:
        ASSERT(numberOfOperands == 1);
        sop0 = dwarf2_get_leb128_signed(instruction+1,&len);
        ASSERT(sop0 == (int32_t)sop0 && "Cannot use more than 32 bits for line value");
        for (uint32_t i = 0; i < len; i++){
            instructionBytes.append(instruction[1+i]);
        }
        regs->INCREMENT(lr_line,(int32_t)sop0);
        break;
    case DW_LNS_set_file:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        ASSERT(uop0 == (uint32_t)uop0 && "Cannot use more than 32 bits for file value");
        for (uint32_t i = 0; i < len; i++){
            instructionBytes.append(instruction[1+i]);
        }
        regs->SET(lr_file,(uint32_t)uop0);
        break;
    case DW_LNS_set_column:
        ASSERT(numberOfOperands == 1);
        uop0 = dwarf2_get_leb128_unsigned(instruction+1,&len);
        ASSERT(uop0 == (uint32_t)uop0 && "Cannot use more than 32 bits for file value");
        for (uint32_t i = 0; i < len; i++){
            instructionBytes.append(instruction[1+i]);
        }
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
        addr16 = getUInt16(instruction+1);
        for (uint32_t i = 0; i < 2; i++){
            instructionBytes.append(instruction[1+i]);
        }
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
        for (uint32_t i = 0; i < len; i++){
            instructionBytes.append(instruction[1+i]);
        }
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

    ASSERT(header->GET(li_line_range) && "A divide by zero error is about to occur");
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
    instructionBytes.append(instruction[0]);

    uint8_t opcode = instruction[0];
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
}

LineInfo::LineInfo(uint32_t idx, char* instruction, LineInfoTable* hdr){
    index = idx;
    header = hdr;

    if (!instruction){
        initializeWithDefaults();
    } else {
        initializeWithInstruction(instruction);
    }

    addressSpan = 0;
}

LineInfo::~LineInfo(){
}

void LineInfo::print(){

    PRINT_INFO();
    PRINT_OUT("%6d\t%6d\t", index, instructionBytes.size());
    if (instructionBytes.size() <= 8){
        for (uint32_t i = 0; i < instructionBytes.size(); i++){
            PRINT_OUT("%02hhx", instructionBytes[i]);
        }
        for (uint32_t i = 0; i < 8-instructionBytes.size(); i++){
            PRINT_OUT("  ");
        }
    } else {
        for (uint32_t i = 0; i < 8-2; i++){
            PRINT_OUT("%02hhx", instructionBytes[i]);
        }
        PRINT_OUT("... ");
    }

    char* typestr = "UNKNOWN";
    char* opstr = "UNKNOWN";
    switch(instructionBytes[0]){
    case DW_LNS_extended_op: typestr = "EXTENDED"; 
        switch (instructionBytes[2]){
        case DW_LNE_end_sequence: opstr = "ENDSEQNCE" ; break;
        case DW_LNE_set_address:  opstr = "SETADDRESS"; break;
        case DW_LNE_define_file:  opstr = "DEFINEFILE"; break;
        }
        break;
    case DW_LNS_copy:                 typestr = "STANDARD"; opstr = "COPY"      ; break;
    case DW_LNS_advance_pc:           typestr = "STANDARD"; opstr = "ADVANCEPC" ; break;
    case DW_LNS_advance_line:         typestr = "STANDARD"; opstr = "ADVNCELINE"; break;
    case DW_LNS_set_file:             typestr = "STANDARD"; opstr = "SETFILE"   ; break;
    case DW_LNS_set_column:           typestr = "STANDARD"; opstr = "SETCOLUMN" ; break;
    case DW_LNS_negate_stmt:          typestr = "STANDARD"; opstr = "NEGATESTMT"; break;
    case DW_LNS_set_basic_block:      typestr = "STANDARD"; opstr = "SETBSCBLCK"; break;
    case DW_LNS_const_add_pc:         typestr = "STANDARD"; opstr = "CONSTADDPC"; break;
    case DW_LNS_fixed_advance_pc:     typestr = "STANDARD"; opstr = "FXDADVNCPC"; break;
    case DW_LNS_set_prologue_end:     typestr = "STANDARD"; opstr = "SETPRLGEND"; break;
    case DW_LNS_set_epilogue_begin:   typestr = "STANDARD"; opstr = "SETEPLGBEG"; break;
    case DW_LNS_set_isa:              typestr = "STANDARD"; opstr = "SETISA"    ; break;
    default:                          typestr = "SPECIAL" ; break;
    }

    PRINT_OUT("%8s\t%10s%16llx\t%4d\t%s/%s", typestr, opstr, GET(lr_address), GET(lr_line), getFilePath(), getFileName());
    PRINT_OUT("\n");
    /*
    PRINT_INFOR("LineInfo(%d): instruction size %d", index, instructionBytes.size());
    PRINT_INFO();
    PRINT_OUT("Raw Instruction Bytes: ");
    if (instructionBytes){
        for (uint32_t i = 0; i < instructionBytes.size(); i++){
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
