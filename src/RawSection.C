#include <RawSection.h>

#include <AddressAnchor.h>
#include <Base.h>
#include <BinaryFile.h>
#include <ElfFile.h>
#include <X86Instruction.h>
#include <SectionHeader.h>

void DataSection::printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine){
    fprintf(stdout, "\n");
    PRINT_INFOR("Raw bytes for DATA section %d:", sectionIndex);

    uint32_t printMax = getSectionHeader()->GET(sh_size);
    if (0x400 < printMax){
        printMax = 0x400;
    }
    printBufferPretty(charStream() + offset, printMax, getSectionHeader()->GET(sh_addr) + offset, bytesPerWord, bytesPerLine);
}

uint32_t DataSection::extendSize(uint32_t sz){
    char* newBytes = new char[sz + sizeInBytes];
    bzero(newBytes, sz + sizeInBytes);

    if (rawBytes){
        memcpy(newBytes, rawBytes, sizeInBytes);
        delete[] rawBytes;
    }

    rawBytes = newBytes;
    sizeInBytes += sz;

    ASSERT(rawBytes);
    return sizeInBytes;
}

void DataSection::setBytesAtAddress(uint64_t addr, uint32_t size, char* content){
    ASSERT(getSectionHeader()->inRange(addr));
    ASSERT(size);
    ASSERT(getSectionHeader()->inRange(addr + size - 1));
    setBytesAtOffset(addr - getSectionHeader()->GET(sh_addr), size, content);
}

void DataSection::setBytesAtOffset(uint64_t offset, uint32_t size, char* content){
    ASSERT(offset + size <= getSizeInBytes());
    ASSERT(rawBytes);

    memcpy(rawBytes + offset, content, size);
}

DataSection::DataSection(char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : RawSection(PebilClassType_DataSection, rawPtr, size, scnIdx, elf)
{
    rawBytes = NULL;
}

uint32_t DataSection::read(BinaryInputFile* b){
    ASSERT(sizeInBytes);
    ASSERT(!rawBytes);

    rawBytes = new char[sizeInBytes];
    if (getSectionHeader()->GET(sh_type) == SHT_NOBITS){
        bzero(rawBytes, sizeInBytes);
    } else {
        memcpy(rawBytes, rawDataPtr, sizeInBytes);
    }

    verify();
    return sizeInBytes;
}

uint32_t RawSection::read(BinaryInputFile* b){
    b->setInPointer(rawDataPtr);
    setFileOffset(b->currentOffset());

    verify();
    return sizeInBytes;
}

char* RawSection::getStreamAtAddress(uint64_t addr){
    uint32_t offset = addr - getSectionHeader()->GET(sh_addr);
    return charStream(offset);
}

void DataReference::print(){
    uint16_t sidx = 0;
    if (rawSection){
        sidx = rawSection->getSectionIndex();
    }
    PRINT_INFOR("DATAREF %#llx: Offset %#llx in section %d -- %#llx", getBaseAddress(), sectionOffset, sidx, data);
}

DataSection::~DataSection(){
    if (rawBytes){
        delete[] rawBytes;
    }
}

RawSection::~RawSection(){
    for (uint32_t i = 0; i < dataReferences.size(); i++){
        delete dataReferences[i];
    }
}

DataReference::DataReference(uint64_t dat, RawSection* rawsect, uint32_t addrAlign, uint64_t off)
    : Base(PebilClassType_DataReference)
{
    data = dat;
    rawSection = rawsect;
    sectionOffset = off;
    sizeInBytes = addrAlign;

    if (sizeInBytes == sizeof(uint32_t)){
        is64bit = false;
    } else {
        ASSERT(sizeInBytes == sizeof(uint64_t));
        is64bit = true;
    }
    addressAnchor = NULL;
}

void DataReference::initializeAnchor(Base* link){
    ASSERT(!addressAnchor);
    addressAnchor = new AddressAnchor(link,this);
}

DataReference::~DataReference(){
    if (addressAnchor){
        delete addressAnchor;
    }
}

uint64_t DataReference::getBaseAddress(){
    if (rawSection){
        return rawSection->getSectionHeader()->GET(sh_addr) + sectionOffset;
    }
    return sectionOffset;
}

void DataReference::dump(BinaryOutputFile* b, uint32_t offset){
    if (addressAnchor){
        addressAnchor->dump(b,offset);
    }
}


RawSection::RawSection(PebilClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
    : Base(classType),rawDataPtr(rawPtr),sectionIndex(scnIdx),elfFile(elf)
{ 
    sizeInBytes = size; 

    hashCode = HashCode((uint32_t)sectionIndex);
    PRINT_DEBUG_HASHCODE("Section %d Hashcode: 0x%04llx", (uint32_t)sectionIndex, hashCode.getValue());

    verify();
}

bool RawSection::verify(){
    if (!hashCode.isSection()){
        PRINT_ERROR("RawSection %d HashCode is malformed", (uint32_t)sectionIndex);
        return false;
    }

    /*
    if (getSectionHeader()->GET(sh_size) != getSizeInBytes()){
        PRINT_ERROR("RawSection %d: size of section (%d) does not match section header size (%d)", sectionIndex, getSizeInBytes(), getSectionHeader()->GET(sh_size));
        return false;
    }
    */
    return true;
}

SectionHeader* RawSection::getSectionHeader(){
    return elfFile->getSectionHeader(getSectionIndex());
}

bool DataSection::verify(){
    if (getType() != PebilClassType_DataSection){
        PRINT_ERROR("Data section has wrong class type");
        return false;
    }
    if (!getSizeInBytes()){
        PRINT_ERROR("Data section should have valid size");
        return false;        
    }
    return true;
}

void DataSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(rawBytes);
    binaryOutputFile->copyBytes(charStream(), getSizeInBytes(), offset);
    for (uint32_t i = 0; i < dataReferences.size(); i++){
        dataReferences[i]->dump(binaryOutputFile,offset);
    }
}

void RawSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){ 
    if (getType() != PebilClassType_RawSection && getType() != PebilClassType_no_type &&
        getType() != PebilClassType_DwarfSection && getType() != PebilClassType_DwarfLineInfoSection){
        PRINT_ERROR("You should implement the dump function for class type %d", getType());
    }

    
    if (getSectionHeader()->hasBitsInFile() && getSizeInBytes()){
        char* sectionOutput = getFilePointer();
        
        binaryOutputFile->copyBytes(sectionOutput, getSizeInBytes(), offset); 
        for (uint32_t i = 0; i < dataReferences.size(); i++){
            dataReferences[i]->dump(binaryOutputFile,offset);
        }
    }
}

void RawSection::printBytes(uint64_t offset, uint32_t bytesPerWord, uint32_t bytesPerLine){
    fprintf(stdout, "\n");
    PRINT_INFOR("Raw bytes for section %d:", sectionIndex);
    printBufferPretty(charStream() + offset, getSizeInBytes(), getSectionHeader()->GET(sh_offset) + offset, bytesPerWord, bytesPerLine);
}

