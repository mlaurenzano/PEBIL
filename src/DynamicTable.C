#include <DynamicTable.h>
#include <BinaryFile.h>
#include <ElfFile.h>
#include <StringTable.h>
#include <SectionHeader.h>
#include <RelocationTable.h>
#include <HashTable.h>

uint32_t DynamicTable::countDynamics(uint32_t type){
    uint32_t dynCount = 0;
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (dynamics[i]->GET(d_tag) == type){
            dynCount++;
        }
    }
    return dynCount;
}

Dynamic* DynamicTable::getDynamicByType(uint32_t type, uint32_t idx){
    uint32_t dynCount = 0;
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (dynamics[i]->GET(d_tag) == type){
            if (dynCount == idx){
                return dynamics[i];
            } else {
                dynCount++;
            }
        }
    }
    return NULL;
}

uint32_t DynamicTable::findEmptyDynamic(){
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        Dynamic* dyn = getDynamic(i);
        if (dyn->GET(d_tag) == DT_NULL || dyn->GET(d_tag) == DT_INIT){
            return i;
        }
    }
    return numberOfDynamics;
}

void DynamicTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;

    for (uint32_t i = 0; i < numberOfDynamics; i++){
        binaryOutputFile->copyBytes(getDynamic(i)->charStream(),dynamicSize,offset+currByte);
        currByte += dynamicSize;
    }
}

DynamicTable::DynamicTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint16_t segmentIdx, ElfFile* elf) :
    RawSection(ElfClassTypes_DynamicTable,rawPtr, size, scnIdx, elf)
{
    sizeInBytes = size;
    segmentIndex = segmentIdx;

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


void DynamicTable::relocateStringTable(uint64_t newAddr){
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            Dynamic64* dyn = (Dynamic64*)dynamics[i];
        } else {
            Dynamic32* dyn = (Dynamic32*)dynamics[i];
            if (dyn->GET(d_tag) == DT_STRTAB){
                Elf32_Dyn dynEntry;
                memcpy(&dynEntry,dyn->charStream(),sizeof(dynEntry));
                dynEntry.d_un.d_ptr = newAddr;
                memcpy(dyn->charStream(),&dynEntry,sizeof(dynEntry));
            }
        }
    }
}


void DynamicTable::printSharedLibraries(BinaryInputFile* b){
    Dynamic* dyn;
    uint64_t strTabAddr = getDynamicByType(DT_STRTAB,0)->GET_A(d_ptr,d_un);
    StringTable* strTab = NULL;

    PRINT_INFOR("SharedLibDeps : %d",countDynamics(DT_NEEDED));

    // locate the stringTable being referred to by strTabAddr
    for (uint32_t i = 0; i < elfFile->getNumberOfStringTables(); i++){
        uint16_t scnIdx = elfFile->getStringTable(i)->getSectionIndex();
        if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == strTabAddr){
            strTab = elfFile->getStringTable(i);
        }
    }

    ASSERT(strTab && "Cannot find the string table indicated by the DT_STRTAB entry in the dynamic table");

    // look through the dynamic entries to find references to shared objects, print them using the string table we just found
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }
        if (dyn->GET(d_tag) == DT_NEEDED){
            PRINT_INFOR("\tlib%5d -- %s",i,strTab->getString(dyn->GET_A(d_ptr,d_un)));
        }
    }

}


bool DynamicTable::verify(){
    Dynamic* dyn;
    uint64_t relocDynamicAddr = 0;
    uint64_t relocDynamicSize = 0;
    uint64_t relocDynamicEnt = 0;
    uint64_t relocAddendDynamicAddr = 0;
    uint64_t relocAddendDynamicSize = 0;
    uint64_t relocAddendDynamicEnt = 0;
    uint64_t hashTableAddress = 0;
    uint64_t stringTableAddress = 0;
    uint64_t symbolTableAddress = 0;
    uint64_t pltgotAddress = 0;
    uint64_t finiFunctionAddress = 0;
    uint64_t initFunctionAddress = 0;
    uint64_t verneedAddress = 0;
    uint64_t versymAddress = 0;


    uint32_t entryCounts[DT_JMPREL+1];
    
    for (uint32_t i = 0; i < DT_JMPREL+1; i++){
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

        if (dyn->GET(d_tag) <= DT_JMPREL){
            entryCounts[dyn->GET(d_tag)]++;
        } 
        // special case: we will count DT_GNU_HASH as DT_HASH
        if (dyn->GET(d_tag) == DT_GNU_HASH){
            entryCounts[DT_HASH]++;
        }

        if (dyn->GET(d_tag) == DT_PLTREL){
            if (dyn->GET_A(d_val,d_un) != DT_REL && dyn->GET_A(d_val,d_un) != DT_RELA){
                PRINT_ERROR("Dynamic Table entry with type DT_PLTREL contains an illegal value");
            }
        }

        if (dyn->GET(d_tag) == DT_RELAENT){
            uint64_t correctRelSize;
            if (elfFile->is64Bit()){
                correctRelSize = Size__64_bit_Relocation_Addend;
            } else {
                correctRelSize = Size__32_bit_Relocation_Addend;
            }
            if (dyn->GET_A(d_val,d_un) != correctRelSize){
                PRINT_ERROR("Relocation addend size found in dynamic table is not correct");
            }
            relocAddendDynamicEnt = dyn->GET_A(d_val,d_un);
        }
        if (dyn->GET(d_tag) == DT_RELENT){
            uint64_t correctRelSize;
            if (elfFile->is64Bit()){
                correctRelSize = Size__64_bit_Relocation;
            } else {
                correctRelSize = Size__32_bit_Relocation;
            }
            if (dyn->GET_A(d_val,d_un) != correctRelSize){
                PRINT_ERROR("Relocation size found in dynamic table is not correct");
            }
            relocDynamicEnt = dyn->GET_A(d_val,d_un);
        }

        if (dyn->GET(d_tag) == DT_RELA){
            relocAddendDynamicAddr = dyn->GET_A(d_ptr,d_un);
        }
        if (dyn->GET(d_tag) == DT_REL){
            relocDynamicAddr = dyn->GET_A(d_ptr,d_un);
        }

        if (dyn->GET(d_tag) == DT_RELASZ){
            relocAddendDynamicSize = dyn->GET_A(d_ptr,d_un);
        }
        if (dyn->GET(d_tag) == DT_RELSZ){
            relocDynamicSize = dyn->GET_A(d_ptr,d_un);
        }

        if (dyn->GET(d_tag) == DT_PLTGOT){
            pltgotAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_HASH || dyn->GET(d_tag) == DT_GNU_HASH){
            hashTableAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_INIT){
            initFunctionAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_FINI){
            finiFunctionAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_STRTAB){
            stringTableAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_SYMTAB){
            symbolTableAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_VERNEED){
            verneedAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_VERSYM){
            versymAddress = dyn->GET_A(d_ptr,d_un);
        }
    }

    // enforce an order on the addresses of certain sections
    if (hashTableAddress >= symbolTableAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
    }
    if (symbolTableAddress >= stringTableAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
    }
    if (stringTableAddress >= versymAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
    }
    if (versymAddress >= verneedAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
    }
    uint64_t textAddress = 0;
    if (initFunctionAddress){
        if (verneedAddress >= initFunctionAddress){
            PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect: %llx <= %llx", initFunctionAddress, verneedAddress);
        }
        if (initFunctionAddress > textAddress){
            textAddress = initFunctionAddress;
        }
    }
    if (finiFunctionAddress){
        if (verneedAddress >= finiFunctionAddress){
            PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        }
        if (finiFunctionAddress > textAddress){
            textAddress = finiFunctionAddress;
        }
    }
    if (textAddress >= pltgotAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
    }

    if (entryCounts[DT_HASH] == 1){
        uint16_t scnIdx = elfFile->getHashTable()->getSectionIndex();
        if (hashTableAddress != elfFile->getSectionHeader(scnIdx)->GET(sh_addr)){
            PRINT_ERROR("Hash table address in the dynamic table is inconsistent with the hash table address found in the section header");
        }
    } 
    // must have a DT_HASH entry only if the executable participates in dynamic linking
    /*
    else {
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_HASH, %d found", entryCounts[DT_HASH]);
    }
    */
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

    if (entryCounts[DT_RELA] > 1){
        PRINT_ERROR("There must be no more than one entry of type DT_RELA");
    }
    if (entryCounts[DT_REL] > 1){
        PRINT_ERROR("There must be no more than one entry of type DT_REL");
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

    // make sure the relocation addend entries found in the dynamic table match up to some relocation table in the executable
    if (relocAddendDynamicAddr){
        if (elfFile->is64Bit()){
            if (relocAddendDynamicEnt != Size__64_bit_Relocation_Addend){
                PRINT_ERROR("Relocation addend 64 entry size found in dynamic table is not correct");
            }
        } else {
            if (relocAddendDynamicEnt != Size__32_bit_Relocation_Addend){
                PRINT_ERROR("Relocation addend 32 entry size found in dynamic table is not correct");
            }
        }
        for (uint32_t i = 0; i < elfFile->getNumberOfRelocationTables(); i++){
            uint16_t scnIdx = elfFile->getRelocationTable(i)->getSectionIndex();
            if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == relocAddendDynamicAddr){
                relocAddendDynamicAddr = 0;
                if (elfFile->getSectionHeader(scnIdx)->GET(sh_size) != relocAddendDynamicSize){
                    PRINT_ERROR("Size of section containing the relocation addend table does not match the size given in the dynamic table");
                }
            }
        }
    }
    if (relocAddendDynamicAddr){
        PRINT_ERROR("Did not find a relocation table matching the address indicated by a DT_RELA entry in the dynamic table");
    }

    // make sure the relocation entries found in the dynamic table match up to some relocation table in the executable
    if (relocDynamicAddr){
        if (elfFile->is64Bit()){
            if (relocDynamicEnt != Size__64_bit_Relocation){
                PRINT_ERROR("Relocation 64 entry size found in dynamic table is not correct");
            }
        } else {
            if (relocDynamicEnt != Size__32_bit_Relocation){
                PRINT_ERROR("Relocation 32 entry size found in dynamic table is not correct");
            }
        }
        for (uint32_t i = 0; i < elfFile->getNumberOfRelocationTables(); i++){
            uint16_t scnIdx = elfFile->getRelocationTable(i)->getSectionIndex();
            if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == relocDynamicAddr){
                relocDynamicAddr = 0;
                if (elfFile->getSectionHeader(scnIdx)->GET(sh_size) != relocDynamicSize){
                    PRINT_ERROR("Size of section containing the relocation addend table does not match the size given in the dynamic table");
                }
            }
        }
    }
    if (relocDynamicAddr){
        PRINT_ERROR("Did not find a relocation table matching the address indicated by a DT_REL entry in the dynamic table");
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
    PRINT_INFOR("DynamicTable : with %d",numberOfDynamics);
    PRINT_INFOR("\tsect : %d",sectionIndex);
    PRINT_INFOR("\tphdr : %d",segmentIndex);

    for (uint32_t i = 0; i < numberOfDynamics; i++){
        char* namestr = NULL;
        Symbol* foundsymbols[3];
        getElfFile()->findSymbol4Addr(dynamics[i]->GET_A(d_ptr,d_un),foundsymbols,3,&namestr);
        dynamics[i]->print(namestr);
        delete[] namestr;
    }

}

uint32_t DynamicTable::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t totalBytesRead = 0;

    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dynamics[i] = new Dynamic64(getFilePointer() + (i * Size__64_bit_Dynamic_Entry), i);
        } else {
            dynamics[i] = new Dynamic32(getFilePointer() + (i * Size__32_bit_Dynamic_Entry), i);
        }
        totalBytesRead += dynamics[i]->read(binaryInputFile);
    }

    ASSERT(sizeInBytes == totalBytesRead && "size read from file does not match theorietical size of Dynamic Table");

    verify();
    return sizeInBytes;
}

Dynamic* DynamicTable::getDynamic(uint32_t idx){
    ASSERT(dynamics && "Dynamic table should be initialized");
    ASSERT(idx < numberOfDynamics && "Dynamic Table index out of bounds");
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


Dynamic::Dynamic(char* dPtr, uint32_t idx) :
    Base(ElfClassTypes_Dynamic)
{
    dynPtr = dPtr;
    index = idx;
}

const char* DTagNames[] = { "NULL", "NEEDED", "PLTRELSZ", "PLTGOT", "HASH", "STRTAB", "SYMTAB", 
                            "RELA", "RELASZ", "RELAENT", "STRSZ", "SYMENT", "INIT", "FINI", "SONAME", 
                            "RPATH", "SYMBOLIC", "REL", "RELSZ", "RELENT", "PLTREL", "DEBUG", "TEXTREL",
                            "JMPREL", "BIND_NOW", "INIT_ARRAY", "FINI_ARRAY", "INIT_ARRAYSZ", "FINI_ARRAYSZ", 
                            "RUNPATH", "FLAGS", "UNK31", "ENCODING", "PREINIT_ARRAYSZ", "NUM" };

void Dynamic::print(char* str){
    uint64_t tag = GET(d_tag);
    uint8_t treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;

    char tmpstr[__MAX_STRING_SIZE];
    sprintf(tmpstr,"Th%llx",tag);
    if(tag <= DT_NUM){
        sprintf(tmpstr,"%s",DTagNames[tag]);
    }

    // check if d_un is to be interpreted as d_val or d_ptr
    switch(GET(d_tag)){
    case DT_NULL:
    case DT_TEXTREL:
    case DT_SYMBOLIC:
        treatDun = DYNAMIC_ENT_D_UN_IGNORED;
        break;
    case DT_RELASZ:
    case DT_RELAENT:
    case DT_STRSZ:
    case DT_SYMENT:
    case DT_NEEDED:
    case DT_PLTRELSZ:
    case DT_SONAME:
    case DT_RPATH:
    case DT_RELSZ:
    case DT_RELENT:
    case DT_PLTREL:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_VAL;
        break;
    case DT_PLTGOT:
    case DT_HASH:
    case DT_STRTAB:
    case DT_SYMTAB:
    case DT_RELA:
    case DT_INIT:
    case DT_FINI:
    case DT_REL:
    case DT_DEBUG:
    case DT_JMPREL:
        treatDun = DYNAMIC_ENT_D_UN_IS_D_PTR;
        break;
    default:
        break;
    }


    if (treatDun == DYNAMIC_ENT_D_UN_IGNORED){
        PRINT_INFOR("\tdyn%5d -- typ:%11s",index,tmpstr);
    } else if (treatDun == DYNAMIC_ENT_D_UN_IS_D_VAL){
        PRINT_INFOR("\tdyn%5d -- typ:%11s val:%lld",index,tmpstr,GET_A(d_val,d_un));        
    } else if (treatDun == DYNAMIC_ENT_D_UN_IS_D_PTR){
        PRINT_INFOR("\tdyn%5d -- typ:%11s ptr:%#llx -- %s",index,tmpstr,GET_A(d_ptr,d_un),str);        
    } else {
        PRINT_INFOR("\tdyn%5d -- typ:%#llx v?p:%#llx -- %s",index,GET(d_tag),GET_A(d_ptr,d_un),str);        
    }
}

