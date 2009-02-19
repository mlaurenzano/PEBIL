#include <AddressAnchor.h>

#include <BinaryFile.h>
#include <Instruction.h>
#include <RawSection.h>

Base* AddressAnchor::updateLink(Base* newLink){
    ASSERT(newLink->containsProgramBits());
    Base* oldLink = link;
    link = newLink;

    verify();

    return oldLink;
}

void AddressAnchor::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    if (linkedParent->getType() == ElfClassTypes_Instruction){
        dumpInstruction(binaryOutputFile, offset);
    } else if (linkedParent->getType() == ElfClassTypes_DataReference){
        dumpDataReference(binaryOutputFile, offset);
    } else {
        __FUNCTION_NOT_IMPLEMENTED;
    }
}

void AddressAnchor::dump8(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint8_t value = (uint8_t)getLinkOffset();
    ASSERT((uint8_t)(getLinkOffset() - value) == 0 && "Need more than 8 bits for relative immediate");
    binaryOutputFile->copyBytes((char*)&value,sizeof(uint8_t),offset);
}

void AddressAnchor::dump16(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint16_t value = (uint16_t)getLinkOffset();
    ASSERT((uint16_t)(getLinkOffset() - value) == 0 && "Need more than 16 bits for relative immediate");
    binaryOutputFile->copyBytes((char*)&value,sizeof(uint16_t),offset);
}

void AddressAnchor::dump32(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t value = (uint32_t)getLinkOffset();
    ASSERT((uint32_t)(getLinkOffset() - value) == 0 && "Need more than 32 bits for relative immediate");
    binaryOutputFile->copyBytes((char*)&value,sizeof(uint32_t),offset);
}

void AddressAnchor::dump64(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint64_t value = (uint64_t)getLinkOffset();
    ASSERT((uint64_t)(getLinkOffset() - value) == 0 && "Need more than 64 bits for relative immediate");
    binaryOutputFile->copyBytes((char*)&value,sizeof(uint64_t),offset);
}

void AddressAnchor::dumpDataReference(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    DataReference* dataReference = (DataReference*)linkedParent;
    if (dataReference->is64Bit()){
        dump64(binaryOutputFile, offset + dataReference->getSectionOffset());
    } else {
        dump32(binaryOutputFile, offset + dataReference->getSectionOffset());
    }
}

void AddressAnchor::dumpInstruction(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    Instruction* linkedInstruction = (Instruction*)linkedParent;
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        Operand op = linkedInstruction->getOperand(i);
        if (op.isRelative()){
            if (op.getBytesUsed() == sizeof(uint8_t)){
                dump8(binaryOutputFile, offset + op.getBytePosition());
            } else if (op.getBytesUsed() == sizeof(uint16_t)){
                dump16(binaryOutputFile, offset + op.getBytePosition());
            } else if (op.getBytesUsed() == sizeof(uint32_t)){
                dump32(binaryOutputFile, offset + op.getBytePosition());
            } else if (op.getBytesUsed() == sizeof(uint64_t)){
                dump64(binaryOutputFile, offset + op.getBytePosition());
            } else {
                PRINT_ERROR("an operand cannot use %d bytes", op.getBytesUsed());
                __SHOULD_NOT_ARRIVE;
            }
        }
    }
}

uint64_t AddressAnchor::getLinkOffset(){
    if (linkedParent->getType() == ElfClassTypes_Instruction){
        return link->getBaseAddress() - linkedParent->getBaseAddress() - linkedParent->getSizeInBytes() + offsetInLink;
    } else if (linkedParent->getType() == ElfClassTypes_DataReference){
        return link->getBaseAddress() + offsetInLink;
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}

AddressAnchor::AddressAnchor(Base* lnk, uint32_t off, Base* par){
    link = lnk;
    linkedParent = par;

    offsetInLink = off;

    verify();
}

AddressAnchor::~AddressAnchor(){
}

bool AddressAnchor::verify(){
    if (!link->containsProgramBits()){
        PRINT_ERROR("Address link not allowed to be type %d", link->getType());
        return false;
    }
    if (!linkedParent->containsProgramBits()){
        PRINT_ERROR("Address link base not allowed to be type %d", linkedParent->getType());
        return false;
    }

    if (link->getType() == ElfClassTypes_Instruction){
        if (offsetInLink >= link->getSizeInBytes()){
            print();
            PRINT_ERROR("Offset inside Instruction (%d) shouldn't be more than instruction size (%d)", offsetInLink, link->getSizeInBytes());
            return false;
        }
    } else if (link->getType() == ElfClassTypes_DataReference){
        if (offsetInLink){
            PRINT_ERROR("Offset inside DataReference (%d) should be zero -- alignment is not constrained", offsetInLink, link->getSizeInBytes());
            return false;
        }
    } else {
        PRINT_ERROR("Address link cannot have type %d", link->getType());
        return false;
    }

    return true;
}

void AddressAnchor::print(){
    PRINT_INFOR("Address Anchor %#llx", getLinkOffset());
    linkedParent->print();
    link->print();
}
