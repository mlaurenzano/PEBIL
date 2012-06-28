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

#include <RawSection.h>

#include <AddressAnchor.h>
#include <Base.h>
#include <BinaryFile.h>
#include <ElfFile.h>
#include <X86Instruction.h>
#include <SectionHeader.h>

uint32_t RawSection::containsIntroString(){
    char* str = charStream();

    if (str[0] == 'G' &&
        str[1] == 'C' &&
        str[2] == 'C'){
        return strlen(str) + 1;
    }
    return 0;
}

void RawSection::wedge(uint32_t shamt){
    //PRINT_INFOR("Original raw/data section %d", getSectionIndex());

    uint32_t intro = containsIntroString();
    if (intro){
        //PRINT_INFOR("INTRO STRING (%d) %s", intro, charStream());
    }
    //printBufferPretty(charStream(), getSizeInBytes(), getSectionHeader()->GET(sh_offset), 0, 0);

    if (elfFile->is64Bit()){
        uint32_t inc = sizeof(uint64_t);
        for (uint32_t current = intro; current < getSizeInBytes(); current += inc){
            uint64_t data;
            memcpy(&data, charStream() + current, sizeof(uint64_t));
            if (elfFile->isDataWedgeAddress(data)){
                data += shamt;
                memcpy(charStream() + current, &data, sizeof(uint64_t));
            }
        }
    }

    //PRINT_INFOR("Patched raw/data section %d", getSectionIndex());
    //printBufferPretty(charStream(), getSizeInBytes(), getSectionHeader()->GET(sh_offset), 0, 0);
}

void DataSection::printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine){
    fprintf(stdout, "\n");
    PRINT_INFOR("Raw bytes for DATA section %d:", sectionIndex);

    uint32_t printMax = getSectionHeader()->GET(sh_size);
    if (0x400 < printMax){
        printMax = 0x400;
    }
    printBufferPretty(charStream() + offset, printMax, getSectionHeader()->GET(sh_addr) + offset, bytesPerWord, bytesPerLine);
}

uint32_t DataSection::extendSize(uint32_t sz){
    char* newBytes = new char[sz + sizeInBytes];
    bzero(newBytes, sz + sizeInBytes);

    if (rawBytes){
        memcpy(newBytes, rawBytes, sizeInBytes);
        delete[] rawBytes;
    }

    rawBytes = newBytes;
    sizeInBytes += sz;

    ASSERT(rawBytes);
    return sizeInBytes;
}

void DataSection::setBytesAtAddress(uint64_t addr, uint32_t size, char* content){
    ASSERT(getSectionHeader()->inRange(addr));
    ASSERT(size);
    ASSERT(getSectionHeader()->inRange(addr + size - 1));
    setBytesAtOffset(addr - getSectionHeader()->GET(sh_addr), size, content);
}

void DataSection::setBytesAtOffset(uint64_t offset, uint32_t size, char* content){
    ASSERT(offset + size <= getSizeInBytes());
    ASSERT(rawBytes);

    memcpy(rawBytes + offset, content, size);
}

DataSection::DataSection(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(PebilClassType_DataSection, rawPtr, size, scnIdx, elf)
{
    rawBytes = NULL;
}

uint32_t DataSection::read(BinaryInputFile* b){
    ASSERT(sizeInBytes);
    ASSERT(!rawBytes);

    rawBytes = new char[sizeInBytes];
    if (getSectionHeader()->GET(sh_type) == SHT_NOBITS){
        bzero(rawBytes, sizeInBytes);
    } else {
        memcpy(rawBytes, rawDataPtr, sizeInBytes);
    }

    verify();
    return sizeInBytes;
}

uint32_t RawSection::read(BinaryInputFile* b){
    b->setInPointer(rawDataPtr);
    setFileOffset(b->currentOffset());

    verify();
    return sizeInBytes;
}

char* RawSection::getStreamAtAddress(uint64_t addr){
    uint32_t offset = addr - getSectionHeader()->GET(sh_addr);
    return charStream(offset);
}

void DataReference::print(){
    uint16_t sidx = 0;
    if (rawSection){
        sidx = rawSection->getSectionIndex();
    }
    PRINT_INFOR("DATAREF %#llx: Offset %#llx in section %d -- %#llx", getBaseAddress(), sectionOffset, sidx, data);
}

DataSection::~DataSection(){
    if (rawBytes){
        delete[] rawBytes;
    }
}

RawSection::~RawSection(){
    for (uint32_t i = 0; i < dataReferences.size(); i++){
        delete dataReferences[i];
    }
}

DataReference::DataReference(uint64_t dat, RawSection* rawsect, uint32_t addrAlign, uint64_t off)
    : Base(PebilClassType_DataReference)
{
    data = dat;
    rawSection = rawsect;
    sectionOffset = off;
    sizeInBytes = addrAlign;

    if (sizeInBytes == sizeof(uint32_t)){
        is64bit = false;
    } else {
        ASSERT(sizeInBytes == sizeof(uint64_t));
        is64bit = true;
    }
    addressAnchor = NULL;
}

void DataReference::initializeAnchor(Base* link){
    ASSERT(!addressAnchor);
    addressAnchor = new AddressAnchor(link,this);
}

DataReference::~DataReference(){
    if (addressAnchor){
        delete addressAnchor;
    }
}

uint64_t DataReference::getBaseAddress(){
    if (rawSection){
        return rawSection->getSectionHeader()->GET(sh_addr) + sectionOffset;
    }
    return sectionOffset;
}

void DataReference::dump(BinaryOutputFile* b, uint32_t offset){
    if (addressAnchor){
        addressAnchor->dump(b,offset);
    }
}


RawSection::RawSection(PebilClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : Base(classType),rawDataPtr(rawPtr),sectionIndex(scnIdx),elfFile(elf)
{ 
    sizeInBytes = size; 

    hashCode = HashCode((uint32_t)sectionIndex);
    PRINT_DEBUG_HASHCODE("Section %d Hashcode: 0x%04llx", (uint32_t)sectionIndex, hashCode.getValue());

    verify();
}

bool RawSection::verify(){
    if (!hashCode.isSection()){
        PRINT_ERROR("RawSection %d HashCode is malformed", (uint32_t)sectionIndex);
        return false;
    }

    /*
    if (getSectionHeader()->GET(sh_size) != getSizeInBytes()){
        PRINT_ERROR("RawSection %d: size of section (%d) does not match section header size (%d)", sectionIndex, getSizeInBytes(), getSectionHeader()->GET(sh_size));
        return false;
    }
    */
    return true;
}

SectionHeader* RawSection::getSectionHeader(){
    return elfFile->getSectionHeader(getSectionIndex());
}

bool DataSection::verify(){
    if (getType() != PebilClassType_DataSection){
        PRINT_ERROR("Data section has wrong class type");
        return false;
    }
    return true;
}

void DataSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(rawBytes);
    binaryOutputFile->copyBytes(charStream(), getSizeInBytes(), offset);
    for (uint32_t i = 0; i < dataReferences.size(); i++){
        dataReferences[i]->dump(binaryOutputFile,offset);
    }
}

void RawSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){ 
    if (getType() != PebilClassType_RawSection && getType() != PebilClassType_no_type &&
        getType() != PebilClassType_DwarfSection && getType() != PebilClassType_DwarfLineInfoSection){
        PRINT_ERROR("You should implement the dump function for class type %d", getType());
    }

    
    if (getSectionHeader()->hasBitsInFile() && getSizeInBytes()){
        char* sectionOutput = getFilePointer();
        
        binaryOutputFile->copyBytes(sectionOutput, getSizeInBytes(), offset); 
        for (uint32_t i = 0; i < dataReferences.size(); i++){
            dataReferences[i]->dump(binaryOutputFile,offset);
        }
    }
}

void RawSection::printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine){
    fprintf(stdout, "\n");
    PRINT_INFOR("Raw bytes for RAW section %d:", sectionIndex);
    printBufferPretty(charStream() + offset, getSizeInBytes(), getSectionHeader()->GET(sh_offset) + offset, bytesPerWord, bytesPerLine);
}

