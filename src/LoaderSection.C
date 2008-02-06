#include <Instruction.h>
#include <SectHeader.h>
#include <SymbolTable.h>
#include <DemangleWrapper.h>
#include <LoaderSection.h>
#include <RawSection.h>
#include <BinaryFile.h>

LSHeader* LSHeader::newHeader(LSHeader* old,bool is64Bit,
                               uint32_t l_nsyms,
                               uint32_t l_nreloc,
                               uint32_t l_istlen,
                               uint32_t l_nimpid,
                               uint32_t l_impoff,
                               uint32_t l_stlen,
                               uint32_t l_stoff,
                               uint32_t l_symoff,
                               uint32_t l_rldoff)
{
    LSHeader* ret = NULL;
    if(is64Bit){
        ret = new LSHeader64();
        LDHDR_64 entry;
        entry.l_version = old->GET(l_version);
        entry.l_nsyms = l_nsyms;
        entry.l_nreloc = l_nreloc;
        entry.l_istlen = l_istlen;
        entry.l_nimpid = l_nimpid;
        entry.l_impoff = l_impoff;
        entry.l_stlen = l_stlen;
        entry.l_stoff = l_stoff;
        entry.l_symoff = l_symoff;
        entry.l_rldoff = l_rldoff;
        memcpy(ret->charStream(),&entry,Size__64_bit_Loader_Section_Header);
    } else {
        ret = new LSHeader32();
        LDHDR entry;
        entry.l_version = old->GET(l_version);
        entry.l_nsyms = l_nsyms;
        entry.l_nreloc = l_nreloc;
        entry.l_istlen = l_istlen;
        entry.l_nimpid = l_nimpid;
        entry.l_impoff = l_impoff;
        entry.l_stlen = l_stlen;
        entry.l_stoff = l_stoff;
        memcpy(ret->charStream(),&entry,Size__32_bit_Loader_Section_Header);
    }
    return ret;
}

void LSHeader::print(){
    PRINT_INFOR("\t\t(vrs %2d)(nsy %6d)(nrl %6d)(nim %6d)",
        GET(l_version),GET(l_nsyms),GET(l_nreloc),GET(l_nimpid));
}

#define NO_NAME_OFFSET (uint32_t)(-1)

uint32_t LSSymbol32::getNameOffset(){
    uint32_t offset = NO_NAME_OFFSET;
    uint32_t zeroes = GET(l_zeroes);
    if(!zeroes){
        offset = GET(l_offset);
    }
    return offset;
}

uint32_t LSSymbol64::getNameOffset(){
    uint32_t offset = GET(l_offset);
    return offset;
}

char* LSSymbol32::getName(LSStringTable* stringTable){

    if(!stringTable)
        return NULL;

    uint32_t offset = getNameOffset();
    if(offset == NO_NAME_OFFSET){
        char tmpBuffer[9];
        strncpy(tmpBuffer,GET(l_name),8);
        tmpBuffer[8] = '\0';
        return strdup(tmpBuffer);
    } 
    return stringTable->getStringCopy(offset);
}

char* LSSymbol64::getName(LSStringTable* stringTable){
    if(!stringTable)
        return NULL;

    uint32_t offset = getNameOffset();
    return stringTable->getStringCopy(offset);
}

void LSSymbol::print(uint32_t index,LSFileNameTable* fileNameTable,LSStringTable* stringTable){
    char* name = getName(stringTable);
    char* importName = fileNameTable->getName(GET(l_ifile));
    PRINT_INFOR("\t\t[%d]\t(adr %#18llx)(scn %3d)(typ %#x)(cls %3d)(imp %9d)(prm %9d) %s [%s]",
            index,GET(l_value),GET(l_scnum),GET(l_smtype),GET(l_smclas),GET(l_ifile),GET(l_parm),
            name,importName);
    free(name);
}

LSSymbol* LSSymbol::newSymbol(bool is64Bit,uint32_t nameOffset,uint32_t fileNameId){
    LSSymbol* ret = NULL;
    uint32_t currSize = 0;
    char* ptr = NULL;
    if(is64Bit){
        ret = new LSSymbol64();
        LDSYM_64 entry;
        ptr = (char*)&entry;
        currSize = Size__64_bit_Loader_Section_Symbol;
        bzero(ptr,currSize);
        entry.l_offset = nameOffset;
        entry.l_smtype = 0x40;
        entry.l_smclas = 10;
        entry.l_ifile = fileNameId;
    } else {
        ret = new LSSymbol32();
        LDSYM entry;
        ptr = (char*)&entry;
        currSize = Size__32_bit_Loader_Section_Symbol;
        bzero(ptr,currSize);
        entry.l_offset = nameOffset;
        entry.l_smtype = 0x40;
        entry.l_smclas = 10;
        entry.l_ifile = fileNameId;
    }
    memcpy(ret->charStream(),ptr,currSize);
    return ret;
}

LSRelocation* LSRelocation::newRelocation(bool is64Bit,uint64_t addr,uint32_t idx,uint32_t sectId){
    LSRelocation* ret = NULL;
    uint32_t currSize = 0;
    char* ptr = NULL;
    if(is64Bit){
        ret = new LSRelocation64();
        LDREL_64 entry;
        ptr = (char*)&entry;
        currSize = Size__64_bit_Loader_Section_Relocation;
        bzero(ptr,currSize);
        entry.l_vaddr = addr;
        entry.l_symndx = idx;
        entry.l_rtype = 0x3f00;
        entry.l_rsecnm = sectId;
    } else {
        ret = new LSRelocation32();
        LDREL entry;
        ptr = (char*)&entry;
        currSize = Size__32_bit_Loader_Section_Relocation;
        bzero(ptr,currSize);
        entry.l_vaddr = addr;
        entry.l_symndx = idx;
        entry.l_rtype = 0x1f00;
        entry.l_rsecnm = sectId;
    }
    memcpy(ret->charStream(),ptr,currSize);
    return ret;
}

void LSRelocation::print(uint32_t index,LSSymbol** symbols,LSStringTable* stringTable) {
    char* name = NULL;
    if(symbols[GET(l_symndx)])
        name = symbols[GET(l_symndx)]->getName(stringTable);
    else
        name = strdup("");

    PRINT_INFOR("\t\t[%d]\t(adr %#18llx)(sym %9d)(typ %#x)(scn %3d) %s",
            index,GET(l_vaddr),GET(l_symndx),GET(l_rtype),GET(l_rsecnm),name);
    free(name);
}


LSFileNameTable::LSFileNameTable(LSHeader* lsHeader,char* base){
    
    fileNameTableSize = lsHeader->GET(l_istlen);
    fileNameEntryCount = lsHeader->GET(l_nimpid);
    if(fileNameTableSize){
        fileNameTablePtr = base + lsHeader->GET(l_impoff);
        fileInfos = new LSFileNameTable::FileNameEntry[fileNameEntryCount];
        char* ptr = fileNameTablePtr;
        for(uint32_t i=0;i<fileNameEntryCount;i++){
            fileInfos[i].impidpath = ptr;
            ptr += (strlen(ptr) + 1);
            fileInfos[i].impidbase = ptr;
            ptr += (strlen(ptr) + 1);
            fileInfos[i].impidmem = ptr;
            ptr += (strlen(ptr) + 1);
        }
    }
}

void LSFileNameTable::print(){
    for(uint32_t i=0;i<fileNameEntryCount;i++){
        PRINT_INFOR("\t\t[%d]\t%s %s %s",
                    i,fileInfos[i].impidpath,fileInfos[i].impidbase,fileInfos[i].impidmem);
    }
}

LSStringTable::LSStringTable(LSHeader* lsHeader,char* base){
    stringTableSize = lsHeader->GET(l_stlen);
    if(stringTableSize){
        stringTablePtr = base + lsHeader->GET(l_stoff);
    }
}

void LSStringTable::print(){
    for(uint32_t currSize = 0;currSize<stringTableSize;){
        char* ptr = stringTablePtr + currSize;
        uint16_t length = 0;
        memcpy(&length,ptr,sizeof(uint16_t));
        ptr += sizeof(uint16_t);

        char* tmpStr = new char[length+1];
        strncpy(tmpStr,ptr,length);
        tmpStr[length] = '\0';

        DemangleWrapper wrapper;
        char* demangled = wrapper.demangle_combined(tmpStr);

        PRINT_INFOR("\t\t[%d]\t%s --- %s",currSize,tmpStr,demangled);

        delete[] tmpStr;

        currSize += sizeof(uint16_t);
        currSize += length;
    }
}
char* LSStringTable::getStringCopy(uint32_t offset) { 
    ASSERT(offset < stringTableSize);
    if(!offset)
        return strdup("");

    char* ptr = stringTablePtr+offset;
    char* szptr = ptr - sizeof(uint16_t);
    uint16_t length = 0;
    memcpy(&length,szptr,sizeof(uint16_t));
    if(!length)
        return strdup("");
    char* ret = (char*)malloc(length+1);
    strncpy(ret,ptr,length);
    ret[length] = '\0';
    return ret;
}

char* LSStringTable::getString(uint32_t offset) { 
    ASSERT(offset < stringTableSize);
    if(!offset)
        return "";
    return stringTablePtr+offset;
}
