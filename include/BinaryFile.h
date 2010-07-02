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

#ifndef _BinaryFile_h_
#define _BinaryFile_h_

#include <Base.h>

class BinaryInputFile {
private:
    char*        inBufferPointer;
    uint32_t     inBufferSize;
    char*        inBuffer;
public:
    BinaryInputFile() : inBufferPointer(NULL),inBufferSize(0),inBuffer(NULL) {}
    ~BinaryInputFile();

    void     readFileInMemory(char* f);

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


class BinaryOutputFile {
private:
    FILE* outFile;
    char* fileName;
public:

    BinaryOutputFile() : outFile(NULL),fileName(NULL) {}
    ~BinaryOutputFile();

    void open(char* flnm);
    bool operator!();
    void copyBytes(char* buffer,uint32_t size,uint32_t offset);
    uint32_t alreadyWritten();
    void close();
};

#endif
