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

#ifndef _BinaryFile_h_
#define _BinaryFile_h_

#include <Base.h>

class BinaryInputFile {
protected:
    char*        inBufferPointer;
    uint32_t     inBufferSize;
    char*        inBuffer;
public:
    BinaryInputFile() : inBufferPointer(NULL),inBufferSize(0),inBuffer(NULL) {}
    ~BinaryInputFile();

    virtual void readFileInMemory(char* f, bool inform=true);

    char*    copyBytes(void* buff,uint32_t bytes);
    char*    copyBytesIterate(void* buff,uint32_t bytes);
    char*    onlyIterate(uint32_t bytes);
    char*    moreBytes();

    char*    fileOffsetToPointer(uint64_t fileOffset);
    void     setInBufferPointer(uint64_t fileOffset);

    char*    isInBuffer(char* f);
    void     setInPointer(char* f);

    char*    inPtrBase() { return inBuffer; }

    uint32_t alreadyRead() { return (uint32_t)(inBufferPointer-inBuffer); }
    uint32_t bytesLeftInBuffer();

    uint32_t getSize() { return inBufferSize; }

    uint32_t currentOffset() { return (uint32_t)(inBufferPointer-inBuffer); }
};

class EmbeddedBinaryInputFile : public BinaryInputFile {
public:
    EmbeddedBinaryInputFile(void* file_start, uint64_t size);
    ~EmbeddedBinaryInputFile();
    void readFileInMemory(char* f, bool inform=true) { /* Do Nothing */ }
};

class BinaryOutputFile {
private:
    FILE* outFile;
    char* fileName;
public:

    BinaryOutputFile() : outFile(NULL),fileName(NULL) {}
    ~BinaryOutputFile();

    virtual void open(char* flnm);
    virtual bool operator!();
    virtual void copyBytes(char* buffer,uint32_t size,uint32_t offset);
    virtual void close();
};

/* Writes to a buffer instead of a file */
class EmbeddedBinaryOutputFile : public BinaryOutputFile {
private:
    static const uint32_t INITIAL_BUFFER_SIZE = 1024;
    char* buffer;
    uint32_t buffer_size;
    uint32_t written_size;

public:
    EmbeddedBinaryOutputFile();
    ~EmbeddedBinaryOutputFile();

    void open(char* flnm) { /* Do nothing */ }
    bool operator!() { return false; }

    // copy size bytes from buffer to this->buffer + offset
    void copyBytes(char* buffer,uint32_t size,uint32_t offset);
    void close();

    char* charStream() { return buffer; }
    uint32_t size() { return written_size; }
   
};

#endif
