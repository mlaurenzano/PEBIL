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

#ifndef _FileHeader_h_
#define _FileHeader_h_

#include <Base.h>
#include <defines/FileHeader.d>

class FileHeader : public Base {
protected:

public:

    FileHeader() : Base(PebilClassType_FileHeader){}
    bool verify();

    uint64_t GetTextEntryOffset();

    virtual ~FileHeader() {}
    FILEHEADER_MACROS_BASIS("For the get_X/set_X field macros check the defines directory");

    void initFilePointers(BinaryInputFile* b);
    void print();
    virtual char* charStream() { __SHOULD_NOT_ARRIVE; return NULL; }

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
    const char* getTypeName();
    void wedge(uint32_t shamt);
};

class FileHeader32 : public FileHeader {
protected:
    Elf32_Ehdr entry;

public:

    FILEHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    FileHeader32() { sizeInBytes = Size__32_bit_File_Header; }
    ~FileHeader32() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

class FileHeader64 : public FileHeader {
protected:
    Elf64_Ehdr entry;

public:

    FILEHEADER_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    FileHeader64() { sizeInBytes = Size__64_bit_File_Header; }
    ~FileHeader64() {}
    uint32_t read(BinaryInputFile* b);

    char* charStream() { return (char*)&entry; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

#endif /* _FileHeader_h_ */
