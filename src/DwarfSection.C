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

#include <DwarfSection.h>

#include <BinaryFile.h>
#include <LineInformation.h>
#include <SectionHeader.h>

void DwarfLineInfoSection::wedge(uint32_t shamt){
    for (uint32_t i = 0; i < lineInfoTables.size(); i++){
        lineInfoTables[i]->wedge(shamt);
    }
}

uint32_t DwarfLineInfoSection::read(BinaryInputFile* binaryInputFile){
    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currByte = 0;
    uint32_t numberOfLineInfoTables = 0;
    while (currByte < sizeInBytes){
        lineInfoTables.append(new LineInfoTable(numberOfLineInfoTables++,rawDataPtr+currByte,this));
        lineInfoTables.back()->read(binaryInputFile);
        currByte += lineInfoTables.back()->getSizeInBytes();
    }

    PRINT_DEBUG_LINEINFO("Using %d lineinfo tables -- total size is %d", lineInfoTables.size(), currByte);

    ASSERT(currByte == sizeInBytes && "Number of bytes read from DwarfLineInfoSection does not match its size");
    verify();
    return sizeInBytes;
}

DwarfLineInfoSection::~DwarfLineInfoSection(){
    for (uint32_t i = 0; i < lineInfoTables.size(); i++){
        delete lineInfoTables[i];
    }
}

DwarfLineInfoSection::DwarfLineInfoSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : DwarfSection(filePtr, size, scnIdx, idx, elf) 
{ 
    type = PebilClassType_DwarfLineInfoSection; 
}


void DwarfLineInfoSection::dump(BinaryOutputFile* b, uint32_t offset){
    RawSection::dump(b,offset);
}

void DwarfLineInfoSection::print(){

    PRINT_INFOR("Dwarf Line Info Section (%d)", getSectionIndex());

    printBytes(0,0,0);

    for (uint32_t i = 0; i < lineInfoTables.size(); i++){
        lineInfoTables[i]->print();
    }
}

bool DwarfLineInfoSection::verify(){ 
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < lineInfoTables.size(); i++){
        if (!lineInfoTables[i]){
            PRINT_ERROR("Line info table %d should be initialized", i);
            return false;
        }
        if (!lineInfoTables[i]->verify()){
            return false;
        }
        totalSize += lineInfoTables[i]->getSizeInBytes();
    }
    if (totalSize != sizeInBytes){
        PRINT_ERROR("Total size of DWARF Line Info section does not match the size of all of the line info tables it contains");
        return false;
    }

    return true;
}
