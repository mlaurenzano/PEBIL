/* This program is free software: you can redistribute it and/or modify
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

#ifndef _ProgramHeader_h_
#define _ProgramHeader_h_

#include <Base.h>
#include <defines/ProgramHeader.d>

class ProgramHeader : public Base {
protected:
    ProgramHeader() : Base(PebilClassType_ProgramHeader),index(0) {}

    uint16_t index;
public:

    virtual ~ProgramHeader() {}
    PROGRAMHEADER_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    void print();
    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
    bool isReadable() { return ISPF_R(GET(p_flags)); }
    bool isWritable() { return ISPF_W(GET(p_flags)); } 
    bool isExecutable() { return ISPF_X(GET(p_flags)); } 

    bool inRange(uint64_t addr);
    uint16_t getIndex() { return index; }
    void setIndex(uint16_t idx) { index = idx; }

    bool verify();
};

class ProgramHeader32 : public ProgramHeader {
protected:
    Elf32_Phdr entry;

public:

    PROGRAMHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    ProgramHeader32(uint32_t idx) { sizeInBytes = Size__32_bit_Program_Header; index = idx; }
    ~ProgramHeader32() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

class ProgramHeader64 : public ProgramHeader {
protected:
    Elf64_Phdr entry;

public:

    PROGRAMHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    ProgramHeader64(uint32_t idx) { sizeInBytes = Size__64_bit_Program_Header; index = idx; }
    ~ProgramHeader64() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};


#endif
