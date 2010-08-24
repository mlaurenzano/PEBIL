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

#ifndef _DwarfSection_h_
#define _DwarfSection_h_

#include <Base.h>
#include <CStructuresDwarf.h>
#include <RawSection.h>
#include <Vector.h>
#include <defines/LineInformation.d>

class LineInfoTable;

#define SIZE_DWARF_NAME_BEGINS 7
#define DWARF_NAME_BEGINS ".debug_"

#define DWARF_LINE_INFO_SCN_NAME ".debug_line"

class DwarfSection : public RawSection {
protected:
    uint32_t index;

public:
    DwarfSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
        : RawSection(PebilClassType_DwarfSection, filePtr, size, scnIdx, elf),index(idx) {}
    ~DwarfSection() {}

    uint32_t getIndex() { return index; }
};

class DwarfLineInfoSection : public DwarfSection {
protected:
    Vector<LineInfoTable*> lineInfoTables; 

public:
    DwarfLineInfoSection(char* filePtr, uint64_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~DwarfLineInfoSection();

    bool verify();
    void print();
    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* b, uint32_t offset);

    uint32_t getNumberOfLineInfoTables() { return lineInfoTables.size(); }
    LineInfoTable* getLineInfoTable(uint32_t idx) { return lineInfoTables[idx]; }
};

#endif /* _DwarfSection_h_ */

