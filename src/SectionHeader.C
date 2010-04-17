#include <SectionHeader.h>

#include <BinaryFile.h>
#include <DwarfSection.h>

SectionHeader32::SectionHeader32(uint16_t idx)
    : SectionHeader()
{ 
    sizeInBytes = Size__32_bit_Section_Header; 
    index = idx; 
    bzero(&entry,sizeof(Elf32_Shdr));
}

SectionHeader64::SectionHeader64(uint16_t idx)
    : SectionHeader()
{ 
    sizeInBytes = Size__64_bit_Section_Header; 
    index = idx; 
    bzero(&entry,sizeof(Elf64_Shdr));
}

// in general we cannot use section names in this function because they will
// not be set yet
void SectionHeader::setSectionType(){
    uint32_t type = GET(sh_type);

    switch (GET(sh_type)){
    case SHT_NULL:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_PROGBITS:
        sectionType = PebilClassType_RawSection;
        if (!hasWriteBit() && !hasAllocBit() && !hasExecInstrBit()){
            sectionType = PebilClassType_DwarfSection;
        } else if (hasExecInstrBit()){
            sectionType = PebilClassType_TextSection;
        } else if (hasWriteBit()){
            sectionType = PebilClassType_DataSection;
        }
        break;
    case SHT_SYMTAB:
        sectionType = PebilClassType_SymbolTable;
        break;
    case SHT_STRTAB:
        sectionType = PebilClassType_StringTable;
        break;
    case SHT_RELA:
        sectionType = PebilClassType_RelocationTable;
        break;
    case SHT_HASH:
        sectionType = PebilClassType_SysvHashTable;
        break;
    case SHT_DYNAMIC:
        sectionType = PebilClassType_DynamicTable;
        break;
    case SHT_NOTE:
        sectionType = PebilClassType_NoteSection;
        break;
    case SHT_NOBITS:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_REL:
        sectionType = PebilClassType_RelocationTable;
        break;
    case SHT_SHLIB:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_DYNSYM:
        sectionType = PebilClassType_SymbolTable;
        break;
    case SHT_INIT_ARRAY:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_FINI_ARRAY:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_PREINIT_ARRAY:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_GROUP:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_SYMTAB_SHNDX:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_NUM:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_GNU_LIBLIST:
        sectionType = PebilClassType_no_type;
        break;
    case SHT_GNU_HASH:
        sectionType = PebilClassType_GnuHashTable;
        break;
    case SHT_GNU_verneed:
        sectionType = PebilClassType_GnuVerneedTable;
        break;
    case SHT_GNU_versym:
        sectionType = PebilClassType_GnuVersymTable;
        break;
    case SHT_GNU_verdef:
        sectionType = PebilClassType_no_type;
	break;
    default:
        sectionType = PebilClassType_RawSection;
        PRINT_ERROR("Section %d: Unknown section type %#x", getIndex(), GET(sh_type));
    }
}

bool SectionHeader::hasWriteBit(){
    return (GET(sh_flags) & SHF_WRITE);
}
bool SectionHeader::hasAllocBit(){
    return (GET(sh_flags) & SHF_ALLOC);
}
bool SectionHeader::hasExecInstrBit(){
    return (GET(sh_flags) & SHF_EXECINSTR);
}

void SectionHeader::initFilePointers(BinaryInputFile* binaryInputFile){
}

bool SectionHeader::inRange(uint64_t address){ 
    uint64_t minAddr = GET(sh_addr);
    uint64_t maxAddr = GET(sh_addr) + GET(sh_size);
    if (address >= minAddr && address < maxAddr){
        return true;
    }
    return false;
}

bool SectionHeader::verify() {
    if (index == 0){
        if (GET(sh_link) != SHN_UNDEF ||
            GET(sh_info) != 0         ||
            GET(sh_addralign) != 0    ||
            GET(sh_entsize) != 0){

            PRINT_ERROR("First section header must be unused");
            return false;
        }
    }

    if (
        GET(sh_type) != SHT_NULL &&
        GET(sh_type) != SHT_PROGBITS &&
        GET(sh_type) != SHT_SYMTAB &&
        GET(sh_type) != SHT_STRTAB &&
        GET(sh_type) != SHT_RELA &&
        GET(sh_type) != SHT_HASH &&
        GET(sh_type) != SHT_DYNAMIC &&
        GET(sh_type) != SHT_NOTE &&
        GET(sh_type) != SHT_NOBITS &&
        GET(sh_type) != SHT_REL &&
        GET(sh_type) != SHT_SHLIB &&
        GET(sh_type) != SHT_DYNSYM &&
        GET(sh_type) != SHT_INIT_ARRAY &&
        GET(sh_type) != SHT_FINI_ARRAY &&
        GET(sh_type) != SHT_PREINIT_ARRAY &&
        GET(sh_type) != SHT_GROUP &&
        GET(sh_type) != SHT_SYMTAB_SHNDX &&
        GET(sh_type) != SHT_NUM &&
        GET(sh_type) != SHT_LOOS &&
        GET(sh_type) != SHT_GNU_HASH &&
        GET(sh_type) != SHT_GNU_LIBLIST &&
        GET(sh_type) != SHT_GNU_verdef &&
        GET(sh_type) != SHT_GNU_verneed &&
        GET(sh_type) != SHT_GNU_versym
        ){
        PRINT_ERROR("Section type %d is not recognized", GET(sh_type));
        return false;
    }

    if (index >= SHN_LORESERVE){
        // currently this will always be true since SHN_HIRESERVE=0xffff
        if (index <= (uint16_t)SHN_HIRESERVE){
            PRINT_ERROR("Section header table cannot use reserved indices");
            return false;
        }
    }

    if(hasAllocBit()){
        if (GET(sh_addr) == 0){
            PRINT_ERROR("This section will reside in memory, but has no address");
            return false;
        }
    }

    if (!isPowerOfTwo(GET(sh_addralign)) && GET(sh_addralign) != 0){
        PRINT_ERROR("Section's address alignment must be 0 or a power of 2");
        return false;
    }
    if (GET(sh_addralign) != 0){
        if (GET(sh_addr) % GET(sh_addralign) != 0){
            print();
            PRINT_ERROR("Section (%d) address does not conform to alignment", index);
            return false;
        }
    }

    return true;
}

const char* STypeNames[] = { "NULL","PROGBITS","SYMTAB","STRTAB","RELA","HASH",
                             "DYNAMIC","NOTE","NOBITS","REL","SHLIB","DYNSYM" };
const char* SFlagNames[] = { "WRITE","ALLOC","EXEC","MERGE","STRINGS","INFO_LINK",
                             "LINK_ORDER","OS_NONCONFORMING","GROUP","TLS" };
void SectionHeader::print() { 

    char tmpstr[__MAX_STRING_SIZE];
    PRINT_INFOR("SHeader : %d",index);
    PRINT_INFOR("\tSnam : %s",getSectionNamePtr() ? getSectionNamePtr() : "<none>");
    if(GET(sh_type) <= SHT_DYNSYM){
        PRINT_INFOR("\tStyp : %s",STypeNames[GET(sh_type)]);
    } else {
        char* ptr = "UNK";
        switch(GET(sh_type)){
            case SHT_INIT_ARRAY: ptr="INIT_ARRAY"; break;
            case SHT_FINI_ARRAY: ptr="FINI_ARRAY"; break;
            case SHT_PREINIT_ARRAY: ptr="PREINIT_ARRAY"; break;
            case SHT_GROUP: ptr="GROUP"; break;
            case SHT_SYMTAB_SHNDX: ptr="SYMTAB_SHNDX"; break;
            case SHT_NUM: ptr="NUM"; break;
            case SHT_LOOS: ptr="LOOS"; break;
            case SHT_GNU_HASH: ptr="GNU_HASH"; break;
            case SHT_GNU_LIBLIST: ptr="GNU_LIBLIST"; break;
            case SHT_CHECKSUM: ptr="CHECKSUM"; break;
            case SHT_LOSUNW: ptr="LOSUNW"; break;
            case SHT_SUNW_COMDAT: ptr="SUNW_COMDAT"; break;
            case SHT_SUNW_syminfo: ptr="SUNW_syminfo"; break;
            case SHT_GNU_verdef: ptr="GNU_verdef"; break;
            case SHT_GNU_verneed: ptr="GNU_verneed"; break;
            case SHT_GNU_versym: ptr="GNU_versym"; break;
            case SHT_LOPROC: ptr="LOPROC"; break;
            case SHT_HIPROC: ptr="HIPROC"; break;
            case SHT_LOUSER: ptr="LOUSER"; break;
            case SHT_HIUSER: ptr="HIUSER"; break;
            default: break;
        }
        PRINT_INFOR("\tStyp : %s",ptr);
    }
    sprintf(tmpstr,"READ");
    for(uint32_t i=0;i<=10;i++){
        if(GET(sh_flags) & (0x1 << i)){
            strcat(tmpstr," + ");
            strcat(tmpstr,SFlagNames[i]);
        }
    }
    PRINT_INFOR("\tSflg : %s",tmpstr);
    PRINT_INFOR("\tSoff : @%llx with %lluB",GET(sh_offset),GET(sh_size));
    if(GET(sh_addr)){
        PRINT_INFOR("\tSvad : %#llx",GET(sh_addr));
    } else {
        PRINT_INFOR("\tSvad : <no virtual address>");
    }
    if(GET(sh_entsize)){
        PRINT_INFOR("\tSent : %lldB each",GET(sh_entsize));
    } else {
        PRINT_INFOR("\tSent : <no fixed-size entries>",GET(sh_addr));
    }
    uint64_t alignment = GET(sh_addralign);
    for(uint32_t i=0;i<64;i++){
        if((alignment >> i) & 0x1){
            alignment = i;
        }
    }
    PRINT_INFOR("\talig : 2**%llu",alignment);

    uint32_t linkValue = GET(sh_link);
    uint32_t infoValue = GET(sh_info);
    PRINT_INFOR("\tlink: %d", linkValue);
    PRINT_INFOR("\tinfo: %d", infoValue);
    switch(GET(sh_type)){
        case SHT_DYNAMIC:
        {
            sprintf(tmpstr,"string table at sect %d",linkValue);
            ASSERT(!infoValue);
            break;
        }
        case SHT_HASH:
        {
            sprintf(tmpstr,"symbol table at sect %d",linkValue);
            ASSERT(!infoValue);
            break;
        }
        case SHT_REL:
        case SHT_RELA:
            sprintf(tmpstr,"symbol table at sect %d and relocation applies to sect %d",linkValue,infoValue);
            break;
        case SHT_SYMTAB:
        case SHT_DYNSYM:
            sprintf(tmpstr,"string table at sect %d and symbol table index of last local sym %d",linkValue,infoValue-1);
            break;
        default: {
            if((linkValue != SHN_UNDEF) || infoValue){
                sprintf(tmpstr,"unknown link %d and %d",linkValue,infoValue-1);
            } else {
                sprintf(tmpstr,"<no extra info>");
                ASSERT(linkValue == SHN_UNDEF);
                ASSERT(!infoValue);
            }
        }
    }
    PRINT_INFOR("\tinfo : %s",tmpstr);
}


uint32_t SectionHeader32::read(BinaryInputFile* binaryInputFile){

    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Section_Header)){
        PRINT_ERROR("Section header (32) can not be read");
    }

    setSectionType();
    verify();
    
    return Size__32_bit_Section_Header;
}

uint32_t SectionHeader64::read(BinaryInputFile* binaryInputFile){

    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Section_Header)){
        PRINT_ERROR("Section header (64) can not be read");
    }

    setSectionType();
    verify();

    return Size__64_bit_Section_Header;
}

void SectionHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_Section_Header,offset);
}

void SectionHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_Section_Header,offset);
}

