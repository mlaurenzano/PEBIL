#include <FileHeader.h>

#include <BinaryFile.h>

bool FileHeader::verify(){
    if (!ISELFMAGIC(GET(e_ident)[EI_MAG0],GET(e_ident)[EI_MAG1],GET(e_ident)[EI_MAG2],GET(e_ident)[EI_MAG3])){
        PRINT_ERROR("Magic number incorrect");
        return false;
    }
    if (GET(e_ident)[EI_DATA] == ELFDATANONE){
        PRINT_ERROR("Elf data invalid");
        return false;
    }
    if (GET(e_ident)[EI_VERSION] != EV_CURRENT){
        PRINT_ERROR("Elf version must be current");
        return false;
    }
    if (GET(e_ident)[EI_ABIVERSION] != 0){
        PRINT_ERROR("ABI version should be 0");
        return false;
    }
    if (GET(e_type) == ET_NONE){
        PRINT_ERROR("File type unknown");
        return false;
    }
    if (GET(e_type) > ET_CORE){
        PRINT_ERROR("File type processor-specific");
        return false;
    }
    uint32_t machine_typ = GET(e_machine);
    if (machine_typ != EM_386 && 
        machine_typ != EM_860 && 
        machine_typ != EM_X86_64){
        PRINT_ERROR("Machine type must be intel or amd x86");
        return false;
    }
    if (GET(e_version) == EV_NONE){
        PRINT_ERROR("Elf header version unknown");
        return false;
    }
    if (!GET(e_phoff)){
        if (GET(e_phnum) != 0){
            PRINT_ERROR("program header entries must be zero if no program header is present");
            return false;
        }
    }
    if (!GET(e_shoff)){
        if (GET(e_shnum) != 0){
            PRINT_ERROR("section header entries must be zero if no section header is present");
            return false;
        }
        if (GET(e_shstrndx) != SHN_UNDEF){
            PRINT_ERROR("section header entries must be zero if no section header is present");
            return false;
        }
    }
    if (GET(e_ident)[EI_CLASS] == ELFCLASS64){
        if (sizeInBytes != Size__64_bit_File_Header){
            PRINT_ERROR("Program Header size is incorrect");
            return false;
        }
        if (GET(e_ehsize) != Size__64_bit_File_Header){
            PRINT_ERROR("File header size is wrong: %d != %d", GET(e_ehsize), Size__64_bit_File_Header);
            return false;
        }
        if (GET(e_phentsize) != Size__64_bit_Program_Header){
            PRINT_ERROR("Program header size is wrong: %d != %d", GET(e_phentsize), Size__64_bit_Program_Header);
            return false;           
        }
        if (GET(e_shentsize) != Size__64_bit_Section_Header){
            PRINT_ERROR("Section header size is wrong", GET(e_shentsize), Size__64_bit_Section_Header);
            return false;
        }
        
    } else if (GET(e_ident)[EI_CLASS] == ELFCLASS32){
        if (sizeInBytes != Size__32_bit_File_Header){
            PRINT_ERROR("Program Header size is incorrect");
            return false;
        }
        if (GET(e_ehsize) != Size__32_bit_File_Header){
            PRINT_ERROR("File header size is wrong");
            return false;
        }
        if (GET(e_phentsize) != Size__32_bit_Program_Header){
            PRINT_ERROR("Program header size is wrong; %d != %d", GET(e_phentsize), Size__32_bit_Program_Header);
            return false;           
        }
        if (GET(e_shentsize) != Size__32_bit_Section_Header){
            PRINT_ERROR("Section header size is wrong");
            return false;
        }
    } else if (GET(e_ident)[EI_CLASS] == ELFCLASSNONE){
        PRINT_ERROR("Elf file class defined as none");
        return false;
    } else {
        PRINT_ERROR("Elf file class invalid");
        return false;
    }

    return true;
}


const char* ETypeNames[] = { "NONE","REL","EXEC","DYN","CORE" };

#define Undef_Mach "UNDEF"
const char* EMachNames[] = { "NONE", "M32", "SPARC", "386", "68K",                                // 0
                             "88K", Undef_Mach, "860", "MIPS", "S370",                            // 5
                             "MIPS_RS3_LE", Undef_Mach, Undef_Mach, Undef_Mach, Undef_Mach,       // 10
                             "PARISC", Undef_Mach, "VPP500", "SPARC32PLUS", "960",                // 15
                             "PPC", "PPC64", "S390", Undef_Mach, Undef_Mach,                      // 20
                             Undef_Mach, Undef_Mach, Undef_Mach, Undef_Mach, Undef_Mach,          // 25
                             Undef_Mach, Undef_Mach, Undef_Mach, Undef_Mach, Undef_Mach,          // 30
                             Undef_Mach, "V800", "FR20", "RH32", "RCE",                           // 35
                             "ARM", "FAKE_ALPHA", "SH", "SPARCV9", "TRICORE",                     // 40
                             "ARC", "H8_300", "H8_300H", "H8S", "H8_500",                         // 45
                             "IA_64", "MIPS_X", "COLDFIRE", "68HC12", "MMA",                      // 50
                             "PCP", "NCPU", "NDR1", "STARCORE", "ME16",                           // 55
                             "ST100", "TINYJ", "X86_64", "PDSP", Undef_Mach,                      // 60
                             Undef_Mach, "FX66", "ST9PLUS", "ST7", "68HC16",                      // 65
                             "68HC11", "68HC08", "68HC05", "SVX", "ST19",                         // 70
                             "VAX", "CRIS", "JAVELIN", "FIREPATH", "ZSP",                         // 75
                             "MMIX", "HUANY", "PRISM", "AVR", "FR30",                             // 80
                             "D10V", "D30V", "V850", "M32R", "MN10300",                           // 85
                             "MN10200", "PJ", "OPENRISC", "ARC_A5", "XTENSA"};                    // 90
const char* EKlazNames[] = { "NONE","32-Bit","64-Bit" };
const char* EDataNames[] = { "NONE","LeastSB","MostSB" };

void FileHeader::print() { 
    PRINT_INFOR("FileHeader:");
    PRINT_INFOR("\tMagic\tClass\tData\tVersion\tPadding");
    unsigned char* ident = GET(e_ident);
    ASSERT(ident[EI_CLASS] <= ELFCLASS64);
    ASSERT(ident[EI_VERSION] == EV_CURRENT);
    PRINT_INFOR("\t%hx-%c%c%c\t%s\t%s\t%d\t%d",
        ident[EI_MAG0],ident[EI_MAG1],ident[EI_MAG2],ident[EI_MAG3],
        EKlazNames[ident[EI_CLASS]],EDataNames[ident[EI_DATA]],ident[EI_VERSION],ident[EI_PAD]);

    ASSERT(GET(e_type) < ET_LOPROC);
    PRINT_INFOR("\tehsz : %dB",GET(e_ehsize));
    PRINT_INFOR("\ttype : %s",ETypeNames[GET(e_type)]);
    PRINT_INFOR("\tmach : %s", EMachNames[GET(e_machine)]);
    ASSERT(ident[EI_VERSION] == GET(e_version));
    PRINT_INFOR("\tvers : %d",GET(e_version));
    PRINT_INFOR("\tentr : %#llx",GET(e_entry));
    if(GET(e_phoff)){
        PRINT_INFOR("\tPhdr : @%llu (%ux%uB)",GET(e_phoff),GET(e_phnum),GET(e_phentsize));
    } else {
        PRINT_INFOR("\tPhdr : none");
    }
    if(GET(e_shoff)){
        PRINT_INFOR("\tShdr : @%llu (%ux%uB)",GET(e_shoff),GET(e_shnum),GET(e_shentsize));
    } else {
        PRINT_INFOR("\tShdr : none");
    }
    if(GET(e_shstrndx) != SHN_UNDEF){
        PRINT_INFOR("\tSnam : sect%d", GET(e_shstrndx));
    } else {
        PRINT_INFOR("\tSnam : no string table for section names");
    }

}


void FileHeader::initFilePointers(BinaryInputFile* binaryInputFile){
}

uint32_t FileHeader32::read(BinaryInputFile* binaryInputFile){

    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_File_Header)){
        PRINT_ERROR("File header (32) can not be read");
    }

    initFilePointers(binaryInputFile);

    verify();

    return Size__32_bit_File_Header;
}

uint32_t FileHeader64::read(BinaryInputFile* binaryInputFile){
    setFileOffset(binaryInputFile->currentOffset());

    if(!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_File_Header)){
        PRINT_ERROR("File header (64) can not be read");
    }

    initFilePointers(binaryInputFile);

    verify();

    return Size__64_bit_File_Header;
}

void FileHeader32::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__32_bit_File_Header,offset);
}

void FileHeader64::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    binaryOutputFile->copyBytes(charStream(),Size__64_bit_File_Header,offset);
}

