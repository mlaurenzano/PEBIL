#include <BinaryFile.h>

#include <Base.h>
#include <ElfFile.h>

void BinaryInputFile::readFileInMemory(char* fileName) {

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

    PRINT_INFOR("Input file is read with success [%s] with %d bytes",fileName,length);

    inBufferPointer = inBuffer;
    inBufferSize = length;

    PRINT_INFOR("Input file ranges %x[0x0,%#x]",inBufferPointer,length);

}

BinaryInputFile::~BinaryInputFile(){
    if(inBuffer){
        delete[] inBuffer;
    }
}


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

char* BinaryInputFile::copyBytesIterate(void* buff,uint32_t bytes)
{ 

    if(!copyBytes(buff,bytes)){
        return NULL;
    }

    inBufferPointer += bytes; 
    return inBufferPointer;
}

char* BinaryInputFile::onlyIterate(uint32_t bytes){
    char* last = inBuffer + inBufferSize;
    char* next = inBufferPointer + bytes;

    if(next > last){
        return NULL;
    }

    inBufferPointer += bytes; 
    return inBufferPointer;
}

char* BinaryInputFile::moreBytes(){
    char* last = inBuffer + inBufferSize;
    if(inBufferPointer >= last){
        return NULL;
    }
    return inBufferPointer;
}


char* BinaryInputFile::fileOffsetToPointer(uint64_t fileOffset){
    char* last = inBuffer + inBufferSize;
    char* next = inBuffer + fileOffset;

    if(next >= last){
        return NULL;
    }
    return next;
}

void BinaryInputFile::setInBufferPointer(uint64_t fileOffset){
    char* next = fileOffsetToPointer(fileOffset);
    if(next){
        inBufferPointer = next;
    }
}

char* BinaryInputFile::isInBuffer(char* ptr){
    char* last = inBuffer + inBufferSize;

    if((ptr >= last) || (ptr < inBuffer)){
        return NULL;
    }
    return ptr;
}

void BinaryInputFile::setInPointer(char* ptr){
    if(isInBuffer(ptr)){
        inBufferPointer = ptr;
    } else {
        ptr = NULL;
    }
}

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

uint32_t BinaryOutputFile::alreadyWritten(){
    return (uint32_t)ftell(outFile);
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
