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

#ifndef _RawSection_h_
#define _RawSection_h_

#include <Base.h>
#include <Vector.h>

class AddressAnchor;
class BinaryInputFile;
class BinaryOutputFile;
class ElfFile;
class X86Instruction;
class RawSection;
class SectionHeader;

class DataReference : public Base {
private:
    uint64_t data;
    uint64_t sectionOffset;
    bool is64bit;

    RawSection* rawSection;
    AddressAnchor* addressAnchor;

public:
    DataReference(uint64_t dat, RawSection* rawsect, uint32_t addrAlign, uint64_t off);
    ~DataReference();

    uint64_t getBaseAddress(); 
    uint64_t getSectionOffset() { return sectionOffset; }
    void initializeAnchor(Base* link);
    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    uint64_t getData() { return data; }
    RawSection* getSection() { return rawSection; }

    bool is64Bit() { return is64bit; }

    void dump(BinaryOutputFile* b, uint32_t offset);
    void print();
};

class RawSection : public Base {
protected:
    char* rawDataPtr;
    uint16_t sectionIndex;
    ElfFile* elfFile;
    HashCode hashCode;

    Vector<DataReference*> dataReferences;

public:
    RawSection(PebilClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~RawSection();

    virtual uint32_t read(BinaryInputFile* b);
    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual bool verify();

    char* charStream(uint32_t offset) { ASSERT(offset < sizeInBytes); return (char*)(rawDataPtr+offset); }
    virtual char* charStream() { return rawDataPtr; }
    char* getFilePointer() { return rawDataPtr; }
    char* getStreamAtAddress(uint64_t addr);
    uint64_t getAddressFromOffset(uint32_t offset);

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    virtual void printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine);
    uint32_t addDataReference(DataReference* dr) { dataReferences.append(dr); return dataReferences.size(); }

    SectionHeader* getSectionHeader();
    uint16_t getSectionIndex() { return sectionIndex; }
    void setSectionIndex(uint16_t newidx) { sectionIndex = newidx; }
    ElfFile* getElfFile() { return elfFile; }
    virtual void wedge(uint32_t shamt);

    HashCode getHashCode() { return hashCode; }
    uint32_t containsIntroString();
};

class DataSection : public RawSection {
protected:
    char* rawBytes;

public:
    DataSection(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf);
    ~DataSection();

    char* charStream() { return rawBytes; }
    void printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine);
    void printBytes() { printBytes(0,0,0); }

    uint32_t extendSize(uint32_t sz);
    void setBytesAtAddress(uint64_t addr, uint32_t size, char* buff);
    void setBytesAtOffset(uint64_t offset, uint32_t size, char* buff);

    uint32_t read(BinaryInputFile* b);
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
};

#endif /* _RawSection_h_ */

