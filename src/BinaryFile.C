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

#include <BinaryFile.h>

#include <Base.h>
#include <ElfFile.h>

EmbeddedBinaryInputFile::EmbeddedBinaryInputFile(void* file_start, uint64_t size) {
    inBufferPointer = inBuffer = (char*)file_start;
    inBufferSize = size;
}

EmbeddedBinaryInputFile::~EmbeddedBinaryInputFile() {
    // Do nothing
}

void BinaryInputFile::readFileInMemory(char* fileName, bool inform) {

    if(inBuffer){
        PRINT_INFOR("This object has already read an executable file\n");
        return;
    }

    FILE* inFile = NULL;

    inFile = fopen(fileName,"r");
    if(!inFile){
        PRINT_ERROR("Input file can not be opened [%s]",fileName);
    }

    fseek(inFile,0,SEEK_END);
    int32_t length = (int32_t)ftell(inFile);

    if(length <= 0){
        PRINT_ERROR("Input file size is 0 [%s]",fileName);
    }

    inBuffer = new char[length];
    fseek(inFile,0,SEEK_SET);
    int32_t check = (int32_t)fread(inBuffer,sizeof(char),length,inFile);
    if(check != length){
        PRINT_ERROR("Input file can not be read completely [%s]",fileName);
    }

    fclose(inFile);

    if (inform){
        PRINT_INFOR("Input file is read with success [%s] with %d bytes",fileName,length);
    }

    inBufferPointer = inBuffer;
    inBufferSize = length;
}

BinaryInputFile::~BinaryInputFile(){
    if(inBuffer){
        delete[] inBuffer;
    }
}

// Copy bytes to buff
char* BinaryInputFile::copyBytes(void* buff,uint32_t bytes) 
{ 

    char* last = inBuffer + inBufferSize;
    char* next = inBufferPointer + bytes;

    if(next > last){
        PRINT_DEBUG("Returning NULL from copyBytes because next > last");
        return NULL;
    }

    void* ret = memcpy(buff,inBufferPointer,bytes); 
    if(ret != buff){
        PRINT_DEBUG("Returning NULL from copyBytes because ret != buff");
        return NULL;
    }
    return inBufferPointer;
}

// Copy bytes to buff and advance buffer pointer
char* BinaryInputFile::copyBytesIterate(void* buff,uint32_t bytes)
{ 

    if(!copyBytes(buff,bytes)){
        return NULL;
    }

    inBufferPointer += bytes; 
    return inBufferPointer;
}

// Advance buffer pointer by bytes
char* BinaryInputFile::onlyIterate(uint32_t bytes){
    char* last = inBuffer + inBufferSize;
    char* next = inBufferPointer + bytes;

    if(next > last){
        return NULL;
    }

    inBufferPointer += bytes; 
    return inBufferPointer;
}

// Get current buffer pointer
char* BinaryInputFile::moreBytes(){
    char* last = inBuffer + inBufferSize;
    if(inBufferPointer >= last){
        return NULL;
    }
    return inBufferPointer;
}

// Return a pointer to buffer+fileOffset
char* BinaryInputFile::fileOffsetToPointer(uint64_t fileOffset){
    char* last = inBuffer + inBufferSize;
    char* next = inBuffer + fileOffset;

    if(next >= last){
        return NULL;
    }
    return next;
}

// Set current buffer pointer to buffer+fileOffset
void BinaryInputFile::setInBufferPointer(uint64_t fileOffset){
    char* next = fileOffsetToPointer(fileOffset);
    if(next){
        inBufferPointer = next;
    }
}

// Return ptr iff it is a valid location in buffer
char* BinaryInputFile::isInBuffer(char* ptr){
    char* last = inBuffer + inBufferSize;

    if((ptr >= last) || (ptr < inBuffer)){
        return NULL;
    }
    return ptr;
}

// Set current buffer pointer to ptr iff valid
void BinaryInputFile::setInPointer(char* ptr){
    if(isInBuffer(ptr)){
        inBufferPointer = ptr;
    } else {
        ptr = NULL;
    }
}

// Return number of remaining bytes in buffer from current buffer pointer
uint32_t BinaryInputFile::bytesLeftInBuffer(){
    char* last = inBuffer + inBufferSize;
    return (uint32_t)(last-inBufferPointer);
}

void BinaryOutputFile::copyBytes(char* buffer,uint32_t size,uint32_t offset) {
    //    PRINT_INFOR("Writing %d bytes to offset %x in file", size, offset);
    int32_t error_code = fseek(outFile,offset,SEEK_SET);
    if(error_code){
        PRINT_ERROR("Error seeking to the output file");
    }
    error_code = fwrite(buffer,sizeof(char),size,outFile);
    if((uint32_t)error_code != size){
        PRINT_ERROR("Error writing to the output file");
    }
}

void BinaryOutputFile::open(char* filenm) { 
    uint32_t namelen = strlen(filenm);
    fileName = new char[__MAX_STRING_SIZE];
    strncpy(fileName, filenm, namelen);
    fileName[namelen] = '\0';
    outFile = fopen(fileName,"w");
    ASSERT(outFile && "Cannot open output file");
}

bool BinaryOutputFile::operator!() { return !outFile; }

void BinaryOutputFile::close() { 
    fclose(outFile);     
    if (fileName){
        chmod(fileName,0750);
    }
}

BinaryOutputFile::~BinaryOutputFile(){
    if (fileName){
        delete[] fileName;
    }
}

/********************* EmbeddedBinaryOutputFile *****************************/
EmbeddedBinaryOutputFile::EmbeddedBinaryOutputFile()
    : written_size(0), buffer_size(INITIAL_BUFFER_SIZE)
{
    this->buffer = new char[INITIAL_BUFFER_SIZE];
}

EmbeddedBinaryOutputFile::~EmbeddedBinaryOutputFile()
{
    close();
}

void EmbeddedBinaryOutputFile::close(){
    if(buffer) {
        delete[] buffer;
        buffer = NULL;
    }
}

void EmbeddedBinaryOutputFile::copyBytes(char* buffer_in, uint32_t size, uint32_t offset)
{
    assert(this->buffer != NULL);

    // grow buffer if too small
    if(offset + size > buffer_size) {
        uint32_t newsize = buffer_size;
        do {
            newsize = newsize << 1;
        } while(offset + size > newsize);
        char* old = this->buffer;
        this->buffer = new char[newsize];
        memcpy(this->buffer, old, buffer_size);
        buffer_size = newsize;
        delete[] old;
    }

    // copy bytes
    assert(offset + size <= buffer_size);
    memcpy(this->buffer + offset, buffer_in, size);

    // update size of file
    if(offset + size > written_size)
        written_size = offset + size;
}

