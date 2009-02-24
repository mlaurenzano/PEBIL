#include <RawSection.h>

#include <AddressAnchor.h>
#include <BinaryFile.h>
#include <Disassembler.h>
#include <ElfFile.h>
#include <Instruction.h>
#include <SectionHeader.h>

char* RawSection::getStreamAtAddress(uint64_t addr){
    uint32_t offset = addr - getSectionHeader()->GET(sh_addr);
    return charStream(offset);
}

void DataReference::print(){
    PRINT_INFOR("DATAREF: Offset %#x in section %d -- %#llx", sectionOffset, rawSection->getSectionIndex(), data);
}

RawSection::~RawSection(){
    for (uint32_t i = 0; i < dataReferences.size(); i++){
        delete dataReferences[i];
    }
}


DataReference::DataReference(uint64_t dat, RawSection* rawsect, bool is64, uint32_t off)
    : Base(ElfClassTypes_DataReference)
{
    data = dat;
    rawSection = rawsect;
    sectionOffset = off;
    is64bit = is64;
    if (is64bit){
        sizeInBytes = sizeof(uint64_t);
    } else {
        sizeInBytes = sizeof(uint32_t);
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


RawSection::RawSection(ElfClassTypes classType, char* rawPtr, uint32_t size, uint16_t scnIdx, ElfFile* elf)
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


void RawSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset)
{ 
    if (getType() != ElfClassTypes_RawSection && getType() != ElfClassTypes_no_type &&
        getType() != ElfClassTypes_DwarfSection && getType() != ElfClassTypes_DwarfLineInfoSection){
        PRINT_ERROR("You should implement the dump function for class type %d", getType());
    }

    if (getSectionHeader()->hasBitsInFile() && getSizeInBytes()){
        binaryOutputFile->copyBytes(charStream(),getSizeInBytes(),offset); 
        for (uint32_t i = 0; i < dataReferences.size(); i++){
            dataReferences[i]->dump(binaryOutputFile,offset);
        }
    }
}

void RawSection::printBytes(uint32_t bytesPerWord, uint32_t bytesPerLine){
    if (bytesPerWord <= 0){
        bytesPerWord = 8;
    }
    if (bytesPerLine <= 0){
        bytesPerLine = 64;
    }
    
    uint32_t currByte = 0;

    fprintf(stdout, "\n");
    PRINT_INFOR("Raw bytes for section %d:", sectionIndex);
    for (currByte = 0; currByte < sizeInBytes; currByte++){     
        if (currByte % bytesPerLine == 0){
            if (currByte){
                fprintf(stdout, "\n");
            }
            fprintf(stdout, "(%16llx) %8x: ", getSectionHeader()->GET(sh_offset)+currByte, currByte);
        } else if (currByte && currByte % bytesPerWord == 0){
            fprintf(stdout, " ");
        }
        fprintf(stdout, "%02hhx", *(char*)(rawDataPtr + currByte));
    }
    fprintf(stdout, "\n");
}

