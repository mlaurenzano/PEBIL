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

#ifndef _StringTable_h_
#define _StringTable_h_

#include <RawSection.h>

class BinaryOutputFile;
class ElfFile;

class StringTable : public RawSection {
protected:
    uint32_t index;
    char* strings;

public:
    StringTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
        : RawSection(PebilClassType_StringTable,rawPtr,size,scnIdx,elf),index(idx) {};

    ~StringTable();

    void print();
    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    char* getString(uint32_t offset);
    uint32_t addString(const char* name);
    uint32_t getIndex() { return index; }
    void wedge(uint32_t shamt);
};

#endif /* _StringTable_h_ */
