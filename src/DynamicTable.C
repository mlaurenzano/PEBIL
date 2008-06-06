#include <DynamicTable.h>
#include <ElfFile.h>
#include <StringTable.h>
#include <SectionHeader.h>

DynamicTable::DynamicTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint16_t segmentIdx, ElfFile* elf) :
    RawSection(ElfClassTypes_dynamic_table,rawPtr, size, scnIdx, elf)
{
    sizeInBytes = size;
    segmentIndex = segmentIdx;

    uint32_t dynamicSize;
    ASSERT(elfFile && "elfFile should be initialized");

    if (elfFile->is64Bit()){
        dynamicSize = Size__64_bit_Dynamic_Entry;
    } else {
        dynamicSize = Size__32_bit_Dynamic_Entry; 
    }

    ASSERT(sizeInBytes % dynamicSize == 0 && "Dynamic Table must have size n*dynEntrySize");
    numberOfDynamics = sizeInBytes / dynamicSize;
    dynamics = new Dynamic*[numberOfDynamics];
    
}

void DynamicTable::printSharedLibraries(BinaryInputFile* b){
    Dynamic* dyn;
    uint64_t strTabAddr = 0;
    StringTable* strTab;

    PRINT_INFOR("Looking for shared library deps in the Dynamic Table");

    // find the string table that contains the shared library names
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }
        
        if (dyn->GET(d_tag) == DT_STRTAB){
            if (strTabAddr){
                PRINT_ERROR("Cannot have multiple entries in the Dynamic Table with type DT_STRTAB");
            }
            strTabAddr = dyn->GET_A(d_ptr,d_un);
        }
    }

    // locate the stringTable being referred to by strTabAddr
    for (uint32_t i = 0; i < elfFile->getNumberOfStringTables(); i++){
        uint16_t scnIdx = elfFile->getStringTable(i)->getSectionIndex();
        if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == strTabAddr){
            strTab = elfFile->getStringTable(i);
        }
    }

    // look through the dynamic entries to find references to shared objects, print them using the string table we just found
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }
        if (dyn->GET(d_tag) == DT_NEEDED){
            PRINT_INFOR("\t\t%s", strTab->getString(dyn->GET_A(d_ptr,d_un)));
        }
    }

}

uint32_t DynamicTable::getNumberOfSharedLibraries(){
    uint32_t numberOfSharedLibs = 0;
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }
        if (dyn->GET(d_tag) == DT_NEEDED){
            numberOfSharedLibs++;
        }
    }
    return numberOfSharedLibs;
}

bool DynamicTable::verify(){
    Dynamic* dyn;

    uint32_t entryCounts[DT_JMPREL];
    for (uint32_t i = 0; i < DT_JMPREL; i++){
        entryCounts[i] = 0;
    }

    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }

        // since DT_NULL marks the end of the table, we exit the loop so we don't look at anything after the DT_NULL entry
        if (dyn->GET(d_tag) == DT_NULL){
            i = numberOfDynamics;    
        }

        if (dyn->GET(d_tag) < DT_JMPREL){
            entryCounts[dyn->GET(d_tag)]++;
        } 

    }

    if (entryCounts[DT_HASH] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_HASH, %d found", entryCounts[DT_HASH]);
    }
    if (entryCounts[DT_STRTAB] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_STRTAB, %d found", entryCounts[DT_STRTAB]);
    }
    if (entryCounts[DT_SYMTAB] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_SYMTAB, %d found", entryCounts[DT_SYMTAB]);
    }

    // some type of relocation table must be present (and it's entries will have either implicit or explicit addends)
    if (entryCounts[DT_RELA] + entryCounts[DT_REL] < 1){
        PRINT_ERROR("There must be at least one Dynamic Table entry of type DT_RELA/DT_REL, %d found", entryCounts[DT_RELA] + entryCounts[DT_REL]);
    }

    // if relocations with explicit addends are present
    if (entryCounts[DT_RELA]){
        if (entryCounts[DT_RELASZ] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELASZ, %d found", entryCounts[DT_RELASZ]);
        }
        if (entryCounts[DT_RELAENT] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELAENT, %d found", entryCounts[DT_RELAENT]);
        }
    }

    // if relocations with implicit addends are present
    if (entryCounts[DT_REL]){
        if (entryCounts[DT_RELSZ] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELSZ, %d found", entryCounts[DT_RELSZ]);
        }
        if (entryCounts[DT_RELENT] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELENT, %d found", entryCounts[DT_RELENT]);
        }
    }

    if (entryCounts[DT_STRSZ] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_STRSZ, %d found", entryCounts[DT_STRSZ]);
    }
    if (entryCounts[DT_SYMENT] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_SYMENT, %d found", entryCounts[DT_SYMENT]);
    }

    if (entryCounts[DT_JMPREL]){
        if (!entryCounts[DT_PLTRELSZ]){
            PRINT_ERROR("If a DT_JMPREL entry is present in the Dynamic Table, a DT_PLTRELSZ entry must also be present");
        } 
        if (!entryCounts[DT_PLTREL]){
            PRINT_ERROR("If a DT_JMPREL entry is present in the Dynamic Table, a DT_PLTREL entry must also be present");
        } 
    }
}

DynamicTable::~DynamicTable(){
    if (dynamics){
        for (uint32_t i = 0; i < numberOfDynamics; i++){
            if (dynamics[i]){
                delete dynamics[i];
            }
        }
        delete[] dynamics;
    }
}

void DynamicTable::print(){
    PRINT_INFOR("Dynamic Table: section %d, segment %d, %d entries", sectionIndex, segmentIndex, numberOfDynamics);
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        dynamics[i]->print();
    }

}

uint32_t DynamicTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t totalBytesRead = 0;

    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
        } else {
            dynamics[i] = new Dynamic32(getFilePointer() + (i * Size__32_bit_Dynamic_Entry), i);
        }
        totalBytesRead += dynamics[i]->read(binaryInputFile);
    }

    verify();

    ASSERT(sizeInBytes == totalBytesRead && "size read from file does not match theorietical size of Dynamic Table");
    return sizeInBytes;
}

Dynamic* DynamicTable::getDynamic(uint32_t idx){
    ASSERT(dynamics && "Dynamic table should be initialized");
    ASSERT(idx >= 0 && idx < numberOfDynamics && "Dynamic Table index out of bounds");
    return dynamics[idx];
}

uint32_t Dynamic32::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(dynPtr);
    setFileOffset(binaryInputFile->currentOffset());
    
    if (!binaryInputFile->copyBytesIterate(&entry,Size__32_bit_Dynamic_Entry)){
        PRINT_ERROR("Dynamic Entry (32) cannot be read");
    }

    return Size__32_bit_Dynamic_Entry;
}

uint32_t Dynamic64::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(dynPtr);
    setFileOffset(binaryInputFile->currentOffset());
    
    if (!binaryInputFile->copyBytesIterate(&entry,Size__64_bit_Dynamic_Entry)){
        PRINT_ERROR("Dynamic Entry (64) cannot be read");
    }
    
    return Size__64_bit_Dynamic_Entry;
}


Dynamic::Dynamic(char* dPtr, uint32_t idx){
    dynPtr = dPtr;
    index = idx;
}

void Dynamic::print(){
    uint64_t tag = GET(d_tag);
    uint8_t treatDun = DYNAMIC_ENT_D_UN_IGNORED;

    // check if d_un is to be interpreted as d_val or d_ptr
    switch(GET(d_tag)){
    case DT_NULL:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    case DT_NEEDED:
    case DT_PLTRELSZ:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_VAL;
        break;
    case DT_PLTGOT:
    case DT_HASH:
    case DT_STRTAB:
    case DT_SYMTAB:
    case DT_RELA:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    case DT_RELASZ:
    case DT_RELAENT:
    case DT_STRSZ:
    case DT_SYMENT:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_VAL;
        break;
    case DT_INIT:
    case DT_FINI:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    case DT_SONAME:
    case DT_RPATH:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_VAL;
        break;
    case DT_SYMBOLIC:
        treatDun = DYNAMIC_ENT_D_UN_IGNORED;
        break;
    case DT_REL:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    case DT_RELSZ:
    case DT_RELENT:
    case DT_PLTREL:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_VAL;
        break;
    case DT_DEBUG:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    case DT_TEXTREL:
        treatDun = DYNAMIC_ENT_D_UN_IGNORED;
        break;
    case DT_JMPREL:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    default:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_VAL;
    }

    if (treatDun == DYNAMIC_ENT_D_UN_IGNORED){
        PRINT_INFOR("DYN[%d]\tTAG 0x%016llx", index, GET(d_tag));
    } else if (treatDun == DYNAMIC_ENT_D_UN_IS_D_VAL){
        PRINT_INFOR("DYN[%d]\tTAG 0x%016llx\tVAL 0x%016llx", index, GET(d_tag), GET_A(d_val,d_un));        
    } else if (treatDun == DYNAMIC_ENT_D_UN_IS_D_PTR){
        PRINT_INFOR("DYN[%d]\tTAG 0x%016llx\tPTR 0x%016llx", index, GET(d_tag), GET_A(d_ptr,d_un));        
    } else {
        PRINT_ERROR("Dynamic tag type was not interpreted correctly: %d", treatDun);
    }
}

