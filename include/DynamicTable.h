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

#ifndef _DynamicTable_h_
#define _DynamicTable_h_

#include <Base.h>
#include <defines/DynamicTable.d>
#include <RawSection.h>
#include <Vector.h>

class BinaryInputFile;

typedef enum {
    DynamicValueType_undefined = 0,
    DynamicValueType_ignored,
    DynamicValueType_value,
    DynamicValueType_pointer,
    DynamicValueType_Total_Types,
} DynamicValueTypes;

class Dynamic : public Base {
public:
    uint32_t index;
    char* dynPtr;

    virtual uint32_t read(BinaryInputFile* binaryInputFile) { __SHOULD_NOT_ARRIVE; }
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; }

    char* getDynamicPtr() { return dynPtr; }
    Dynamic(char* dPtr, uint32_t idx);
    ~Dynamic() {}
    void print(char* s);
    virtual void clear() { __SHOULD_NOT_ARRIVE; }
    uint8_t getValueType();

    DYNAMIC_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");
};


class Dynamic32 : public Dynamic {
private:
    Elf32_Dyn entry;
public:
    Dynamic32(char* dPtr, uint32_t idx) : Dynamic(dPtr,idx){}
    ~Dynamic32() {}

    virtual char* charStream() { return (char*)&entry; }

    DYNAMIC_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    void clear() { bzero(charStream(), Size__32_bit_Dynamic_Entry); }
};

class Dynamic64 : public Dynamic {
private:
    Elf64_Dyn entry;
public:
    Dynamic64(char* dPtr, uint32_t idx) : Dynamic(dPtr,idx){}
    ~Dynamic64() {}

    virtual char* charStream() { return (char*)&entry; }

    DYNAMIC_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    uint32_t read(BinaryInputFile* binaryInputFile);
    void clear() { bzero(charStream(), Size__64_bit_Dynamic_Entry); }
};

class DynamicTable : public RawSection {
protected:
    uint32_t dynamicSize;
    uint16_t segmentIndex;

    Vector<Dynamic*> dynamics;

public:
    DynamicTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint16_t segmentIdx, ElfFile* elf);
    ~DynamicTable();

    void print();
    void printSharedLibraries();
    uint32_t read(BinaryInputFile* b);
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t findEmptyDynamic();
    uint32_t getDynamicSize() { return dynamicSize; }

    Dynamic* getDynamic(uint32_t idx) { return dynamics[idx]; }
    uint32_t getNumberOfDynamics() { return dynamics.size(); }
    uint16_t getSegmentIndex() { return segmentIndex; }
    uint32_t countDynamics(uint32_t type);
    Dynamic* getDynamicByType(uint32_t type, uint32_t idx);
    uint32_t extendTable(uint32_t num);

    void relocateStringTable(uint64_t newAddr);
    uint32_t prependEntry(uint32_t type, uint32_t strOffset);

    bool verify();
    void wedge(uint32_t shamt);
};


#endif /* _DynamicTable_h_ */
