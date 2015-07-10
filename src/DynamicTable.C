/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <DynamicTable.h>

#include <BinaryFile.h>
#include <ElfFile.h>
#include <HashTable.h>
#include <RelocationTable.h>
#include <SectionHeader.h>
#include <StringTable.h>

void DynamicTable::wedge(uint32_t shamt){
    for (uint32_t i = 0; i < dynamics.size(); i++){
        if (dynamics[i]->getValueType() == DynamicValueType_pointer && elfFile->isWedgeAddress(dynamics[i]->GET_A(d_ptr, d_un))){
            dynamics[i]->INCREMENT_A(d_ptr, d_un, shamt);
        }
    }
}

uint32_t DynamicTable::prependEntry(uint32_t type, uint32_t strOffset){

    // find empty table slot
    uint32_t emptyDynamicIdx = findEmptyDynamic();
    ASSERT(emptyDynamicIdx < getNumberOfDynamics() && "No free entries found in the dynamic table");

    // if any DT_RUNPATH entries are present we must use DT_RUNPATH since DT_RPATH entries will be overrun by DT_RUNPATH entries
    // if no DT_RUNPATH are present, we must not use DT_RPATH since using DT_RUNPATH would overrun the DT_RPATH entries
    if (type == DT_RPATH && countDynamics(DT_RUNPATH)){
        type = DT_RUNPATH;
    }

    dynamics[emptyDynamicIdx]->SET(d_tag, type);

    // shuffle all other DT_R[UN]PATH entries down 1 slot
    int32_t prev = -1;
    for (int32_t i = emptyDynamicIdx; i >= 0; i--){
        if (dynamics[i]->GET(d_tag) == type){
            if (prev >= 0){
                dynamics[prev]->SET(d_tag, dynamics[i]->GET(d_tag));
                dynamics[prev]->SET_A(d_ptr, d_un, dynamics[i]->GET_A(d_ptr, d_un));
            }
            prev = i;
        }
    }
    ASSERT(prev >= 0);

    // insert new entry into first slot
    dynamics[prev]->SET(d_tag, type);
    dynamics[prev]->SET_A(d_ptr, d_un, strOffset);

    return prev;
}


uint32_t DynamicTable::extendTable(uint32_t num){
    uint32_t extraSize = 0;
    char* emptyDyn = new char[dynamicSize];
    bzero(emptyDyn, dynamicSize);

    //    PRINT_INFOR("extending dynamic table by %d entries", num);
    for (uint32_t i = 0; i < num; i++){
        if (elfFile->is64Bit()){
            dynamics.append(new Dynamic64(NULL, dynamics.size()));
            dynamics.back()->clear();
        } else {
            dynamics.append(new Dynamic64(NULL, dynamics.size()));
            dynamics.back()->clear();
        }
        extraSize += dynamicSize;
    }
    sizeInBytes += extraSize;
    
    delete[] emptyDyn;
    verify();
    return extraSize;
}

uint32_t DynamicTable::countDynamics(uint32_t type){
    uint32_t dynCount = 0;
    for (uint32_t i = 0; i < dynamics.size(); i++){
        if (dynamics[i]->GET(d_tag) == type){
            dynCount++;
        }
    }
    return dynCount;
}

Dynamic* DynamicTable::getDynamicByType(uint32_t type, uint32_t idx){
    uint32_t dynCount = 0;
    for (uint32_t i = 0; i < dynamics.size(); i++){
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
    for (uint32_t i = 0; i < dynamics.size(); i++){
        Dynamic* dyn = getDynamic(i);
        // DT_INIT is used for shared libraries!
        //if (dyn->GET(d_tag) == DT_NULL || dyn->GET(d_tag) == DT_INIT){
        if (dyn->GET(d_tag) == DT_NULL){
            return i;
        }
    }
    return dynamics.size();
}

void DynamicTable::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;

    for (uint32_t i = 0; i < dynamics.size(); i++){
        binaryOutputFile->copyBytes(getDynamic(i)->charStream(),dynamicSize,offset+currByte);
        currByte += dynamicSize;
    }
}

DynamicTable::DynamicTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint16_t segmentIdx, ElfFile* elf) :
    RawSection(PebilClassType_DynamicTable,rawPtr, size, scnIdx, elf)
{
    sizeInBytes = size;
    segmentIndex = segmentIdx;

    ASSERT(elfFile && "elfFile should be initialized");

    if (elfFile->is64Bit()){
        dynamicSize = Size__64_bit_Dynamic_Entry;
    } else {
        dynamicSize = Size__32_bit_Dynamic_Entry; 
    }

}


void DynamicTable::relocateStringTable(uint64_t newAddr){
    for (uint32_t i = 0; i < dynamics.size(); i++){
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


void DynamicTable::printSharedLibraries(){
    Dynamic* dyn;
    uint64_t strTabAddr = getDynamicByType(DT_STRTAB,0)->GET_A(d_ptr,d_un);
    StringTable* strTab = NULL;

    PRINT_INFOR("%d Shared Library Dependencies", countDynamics(DT_NEEDED));

    // locate the stringTable being referred to by strTabAddr
    for (uint32_t i = 0; i < elfFile->getNumberOfStringTables(); i++){
        uint16_t scnIdx = elfFile->getStringTable(i)->getSectionIndex();
        if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == strTabAddr){
            strTab = elfFile->getStringTable(i);
        }
    }

    ASSERT(strTab && "Cannot find the string table indicated by the DT_STRTAB entry in the dynamic table");

    // look through the dynamic entries to find references to shared objects, print them using the string table we just found
    for (uint32_t i = 0; i < dynamics.size(); i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }
        if (dyn->GET(d_tag) == DT_NEEDED){
            PRINT_INFOR("\t\t[%5d] %s", i, strTab->getString(dyn->GET_A(d_ptr,d_un)));
        }
    }

}


bool DynamicTable::verify(){

    if (sizeInBytes % dynamicSize != 0){
        PRINT_ERROR("Dynamic Table must have size n * %d", dynamicSize);
    }
    uint32_t numberOfDynamics = sizeInBytes / dynamicSize;
    if (numberOfDynamics != dynamics.size()){
        PRINT_ERROR("Size of dynamic table (%d) does not match expeced size (%d)",  dynamics.size(), numberOfDynamics);
    }

    Dynamic* dyn;
    uint64_t relocDynamicAddr = 0;
    uint64_t relocDynamicSize = 0;
    uint64_t relocDynamicEnt = 0;
    uint64_t relocAddendDynamicAddr = 0;
    uint64_t relocAddendDynamicSize = 0;
    uint64_t relocAddendDynamicEnt = 0;
    uint64_t gnuHashTableAddress = 0;
    uint64_t sysvHashTableAddress = 0;
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

    for (uint32_t i = 0; i < dynamics.size(); i++){
        if (elfFile->is64Bit()){
            dyn = (Dynamic64*)dynamics[i];
        } else {
            dyn = (Dynamic32*)dynamics[i];
        }

        // since DT_NULL marks the end of the table, we exit the loop so we don't look at anything after the DT_NULL entry
        if (dyn->GET(d_tag) == DT_NULL){
            i = dynamics.size();    
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
                return false;
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
                return false;
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
                return false;
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
        } else if (dyn->GET(d_tag) == DT_HASH){
            sysvHashTableAddress = dyn->GET_A(d_ptr,d_un);
        } else if (dyn->GET(d_tag) == DT_GNU_HASH){
            gnuHashTableAddress = dyn->GET_A(d_ptr,d_un);
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
    if (gnuHashTableAddress >= symbolTableAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        return false;
    }
    /* libpthread.so fails this
    if (sysvHashTableAddress >= symbolTableAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        return false;
    }
    */
    if (symbolTableAddress >= stringTableAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        return false;
    }
    if (stringTableAddress >= versymAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        return false;
    }
    if (versymAddress >= verneedAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        return false;
    }
    uint64_t textAddress = 0;
    if (initFunctionAddress){
        if (verneedAddress >= initFunctionAddress){
            PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect: %llx <= %llx", initFunctionAddress, verneedAddress);
            return false;
        }
        if (initFunctionAddress > textAddress){
            textAddress = initFunctionAddress;
        }
    }
    if (finiFunctionAddress){
        if (verneedAddress >= finiFunctionAddress){
            PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
            return false;
        }
        if (finiFunctionAddress > textAddress){
            textAddress = finiFunctionAddress;
        }
    }
    if (textAddress >= pltgotAddress){
        PRINT_ERROR("The dynamic table indicates that sections are in a different order than we expect");
        return false;
    }

    for (uint32_t i = 0; i < elfFile->getNumberOfHashTables(); i++){
        if (elfFile->getHashTable(i)->getSectionHeader()->GET(sh_type) == SHT_HASH){
            uint16_t scnIdx = elfFile->getHashTable(i)->getSectionIndex();
            if (sysvHashTableAddress != elfFile->getSectionHeader(scnIdx)->GET(sh_addr)){
                PRINT_ERROR("(Sysv) Hash table address in the dynamic table is inconsistent with the hash table address found in the section header");
                return false;
            }
        }

        if (elfFile->getHashTable(i)->getSectionHeader()->GET(sh_type) == SHT_GNU_HASH){
            uint16_t scnIdx = elfFile->getHashTable(i)->getSectionIndex();
            if (gnuHashTableAddress != elfFile->getSectionHeader(scnIdx)->GET(sh_addr)){
                PRINT_ERROR("(Gnu) Hash table address in the dynamic table is inconsistent with the hash table address found in the section header");
                return false;
            }
        }

    } 
    if (entryCounts[DT_HASH] == 0){
        if (sysvHashTableAddress || gnuHashTableAddress){
            PRINT_ERROR("Unexpected hash section(s) found");
            return false;
        }
    } else if (entryCounts[DT_HASH] == 1){
        //        PRINT_INFOR("sysv addr %#llx, gnu addr %#llx", sysvHashTableAddress, gnuHashTableAddress);
        if (sysvHashTableAddress & gnuHashTableAddress){
            PRINT_ERROR("Unexpected hash section(s) found");
            return false;
        }
    } else if (entryCounts[DT_HASH] == 2){
        if (!sysvHashTableAddress || !gnuHashTableAddress){
            PRINT_ERROR("Unexpected hash section(s) found");
            return false;
        }
    } else {
        PRINT_ERROR("Too many hash tables found");
        return false;
    }

    // must have a DT_HASH entry only if the executable participates in dynamic linking
    /*
    else {
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_HASH, %d found", entryCounts[DT_HASH]);
        return false;
    }
    */
    if (entryCounts[DT_STRTAB] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_STRTAB, %d found", entryCounts[DT_STRTAB]);
        return false;
    }
    if (entryCounts[DT_SYMTAB] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_SYMTAB, %d found", entryCounts[DT_SYMTAB]);
        return false;
    }


    // some type of relocation table must be present (and it's entries will have either implicit or explicit addends)
    if (entryCounts[DT_RELA] + entryCounts[DT_REL] < 1){
        PRINT_ERROR("There must be at least one Dynamic Table entry of type DT_RELA/DT_REL, %d found", entryCounts[DT_RELA] + entryCounts[DT_REL]);
        return false;
    }

    if (entryCounts[DT_RELA] > 1){
        PRINT_ERROR("There must be no more than one entry of type DT_RELA");
        return false;
    }
    if (entryCounts[DT_REL] > 1){
        PRINT_ERROR("There must be no more than one entry of type DT_REL");
        return false;
    }

    // if relocations with explicit addends are present
    if (entryCounts[DT_RELA]){
        if (entryCounts[DT_RELASZ] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELASZ, %d found", entryCounts[DT_RELASZ]);
            return false;
        }
        if (entryCounts[DT_RELAENT] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELAENT, %d found", entryCounts[DT_RELAENT]);
            return false;
        }
    }

    // if relocations with implicit addends are present
    if (entryCounts[DT_REL]){
        if (entryCounts[DT_RELSZ] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELSZ, %d found", entryCounts[DT_RELSZ]);
            return false;
        }
        if (entryCounts[DT_RELENT] != 1){
            PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_RELENT, %d found", entryCounts[DT_RELENT]);
            return false;
        }
    }

    // make sure the relocation addend entries found in the dynamic table match up to some relocation table in the executable
    if (relocAddendDynamicAddr){
        if (elfFile->is64Bit()){
            if (relocAddendDynamicEnt != Size__64_bit_Relocation_Addend){
                PRINT_ERROR("Relocation addend 64 entry size found in dynamic table is not correct");
                return false;
            }
        } else {
            if (relocAddendDynamicEnt != Size__32_bit_Relocation_Addend){
                PRINT_ERROR("Relocation addend 32 entry size found in dynamic table is not correct");
                return false;
            }
        }
        for (uint32_t i = 0; i < elfFile->getNumberOfRelocationTables(); i++){
            uint16_t scnIdx = elfFile->getRelocationTable(i)->getSectionIndex();
            if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == relocAddendDynamicAddr){
                relocAddendDynamicAddr = 0;
                if (elfFile->getSectionHeader(scnIdx)->GET(sh_size) != relocAddendDynamicSize){
                    PRINT_ERROR("Size of section containing the relocation addend table does not match the size given in the dynamic table");
                    return false;
                }
            }
        }
    }
    if (relocAddendDynamicAddr){
        PRINT_ERROR("Did not find a relocation table matching the address indicated by a DT_RELA entry in the dynamic table");
        return false;
    }

    // make sure the relocation entries found in the dynamic table match up to some relocation table in the executable
    if (relocDynamicAddr){
        if (elfFile->is64Bit()){
            if (relocDynamicEnt != Size__64_bit_Relocation){
                PRINT_ERROR("Relocation 64 entry size found in dynamic table is not correct");
                return false;
            }
        } else {
            if (relocDynamicEnt != Size__32_bit_Relocation){
                PRINT_ERROR("Relocation 32 entry size found in dynamic table is not correct");
                return false;
            }
        }
        for (uint32_t i = 0; i < elfFile->getNumberOfRelocationTables(); i++){
            uint16_t scnIdx = elfFile->getRelocationTable(i)->getSectionIndex();
            if (elfFile->getSectionHeader(scnIdx)->GET(sh_addr) == relocDynamicAddr){
                relocDynamicAddr = 0;
                if (elfFile->getSectionHeader(scnIdx)->GET(sh_size) != relocDynamicSize){
                    PRINT_ERROR("Size of section containing the relocation addend table does not match the size given in the dynamic table");
                    return false;
                }
            }
        }
    }
    if (relocDynamicAddr){
        PRINT_ERROR("Did not find a relocation table matching the address indicated by a DT_REL entry in the dynamic table");
        return false;
    }

    if (entryCounts[DT_STRSZ] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_STRSZ, %d found", entryCounts[DT_STRSZ]);
        return false;
    }
    if (entryCounts[DT_SYMENT] != 1){
        PRINT_ERROR("There must be exactly one Dynamic Table entry of type DT_SYMENT, %d found", entryCounts[DT_SYMENT]);
        return false;
    }

    if (entryCounts[DT_JMPREL]){
        if (!entryCounts[DT_PLTRELSZ]){
            PRINT_ERROR("If a DT_JMPREL entry is present in the Dynamic Table, a DT_PLTRELSZ entry must also be present");
            return false;
        } 
        if (!entryCounts[DT_PLTREL]){
            PRINT_ERROR("If a DT_JMPREL entry is present in the Dynamic Table, a DT_PLTREL entry must also be present");
            return false;
        } 
    }
    return true;
}

DynamicTable::~DynamicTable(){
    for (uint32_t i = 0; i < dynamics.size(); i++){
        if (dynamics[i]){
            delete dynamics[i];
        }
    }
}

void DynamicTable::print(){
    PRINT_INFOR("DynamicTable : with %d",dynamics.size());
    PRINT_INFOR("\tsect : %d",sectionIndex);
    PRINT_INFOR("\tphdr : %d",segmentIndex);

    for (uint32_t i = 0; i < dynamics.size(); i++){
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

    uint32_t numberOfDynamics = sizeInBytes / dynamicSize;
    for (uint32_t i = 0; i < numberOfDynamics; i++){
        if (elfFile->is64Bit()){
            dynamics.append(new Dynamic64(getFilePointer() + (i * Size__64_bit_Dynamic_Entry), i));
        } else {
            dynamics.append(new Dynamic32(getFilePointer() + (i * Size__32_bit_Dynamic_Entry), i));
        }
        totalBytesRead += dynamics[i]->read(binaryInputFile);
    }
    ASSERT(sizeInBytes == totalBytesRead && "size read from file does not match theorietical size of Dynamic Table");

    verify();
    return sizeInBytes;
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
    Base(PebilClassType_Dynamic)
{
    dynPtr = dPtr;
    index = idx;
}

const char* DTagNames[] = { "NULL", "NEEDED", "PLTRELSZ", "PLTGOT", "HASH", "STRTAB", "SYMTAB", 
                            "RELA", "RELASZ", "RELAENT", "STRSZ", "SYMENT", "INIT", "FINI", "SONAME", 
                            "RPATH", "SYMBOLIC", "REL", "RELSZ", "RELENT", "PLTREL", "DEBUG", "TEXTREL",
                            "JMPREL", "BIND_NOW", "INIT_ARRAY", "FINI_ARRAY", "INIT_ARRAYSZ", "FINI_ARRAYSZ", 
                            "RUNPATH", "FLAGS", "UNK31", "ENCODING", "PREINIT_ARRAYSZ", "NUM" };

uint8_t Dynamic::getValueType(){
    uint8_t treatDun = DynamicValueType_pointer;

#define MAP_TAG_TO_TYPE(__tag, __typ)            \
    else if (GET(d_tag) == __tag) { treatDun = __typ; }

    // check if d_un is to be interpreted as d_val or d_ptr
    if (false){}

    MAP_TAG_TO_TYPE(DT_NULL, DynamicValueType_ignored)
    MAP_TAG_TO_TYPE(DT_TEXTREL, DynamicValueType_ignored)
    MAP_TAG_TO_TYPE(DT_SYMBOLIC, DynamicValueType_ignored)

    MAP_TAG_TO_TYPE(DT_RELASZ, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RELAENT, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_STRSZ, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_SYMENT, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_NEEDED, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_PLTRELSZ, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_SONAME, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RPATH, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RUNPATH, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RELSZ, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RELENT, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_PLTREL, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_VERDEFNUM, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_VERNEEDNUM, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RELACOUNT, DynamicValueType_value)
    MAP_TAG_TO_TYPE(DT_RELCOUNT, DynamicValueType_value)

    MAP_TAG_TO_TYPE(DT_PLTGOT, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_HASH, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_STRTAB, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_SYMTAB, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_RELA, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_INIT, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_FINI, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_REL, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_DEBUG, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_JMPREL, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_VERDEF, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_VERNEED, DynamicValueType_pointer)
    MAP_TAG_TO_TYPE(DT_VERSYM, DynamicValueType_pointer)

    else if (GET(d_tag) > DT_VALRNGLO && GET(d_tag) < DT_VALRNGHI){
        treatDun = DynamicValueType_value;
    } else if (GET(d_tag) > DT_ADDRRNGLO && GET(d_tag) < DT_ADDRRNGHI){
        treatDun = DynamicValueType_pointer;
    }
    else { PRINT_ERROR("Unknown dynamic entry type: %#x", GET(d_tag)); }

    return treatDun;
}

void Dynamic::print(char* str){
    uint64_t tag = GET(d_tag);
    uint8_t treatDun = getValueType();

    char tmpstr[__MAX_STRING_SIZE];
    sprintf(tmpstr,"Th%llx",tag);
    if(tag <= DT_NUM){
        sprintf(tmpstr,"%s",DTagNames[tag]);
    }

    if (treatDun == DynamicValueType_ignored){
        PRINT_INFOR("\tdyn%5d -- typ:%11s",index,tmpstr);
    } else if (treatDun == DynamicValueType_value){
        PRINT_INFOR("\tdyn%5d -- typ:%11s val:%lld",index,tmpstr,GET_A(d_val,d_un));        
    } else if (treatDun == DynamicValueType_pointer){
        PRINT_INFOR("\tdyn%5d -- typ:%11s ptr:%#llx -- %s",index,tmpstr,GET_A(d_ptr,d_un),str);        
    } else {
        PRINT_INFOR("\tdyn%5d -- typ:%#llx v?p:%#llx -- %s",index,GET(d_tag),GET_A(d_ptr,d_un),str);        
    }
}

