#include <AddressAnchor.h>

#include <BinaryFile.h>
#include <Instruction.h>
#include <RawSection.h>

int searchLinkBaseAddressExact(const void* arg1, const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    AddressAnchor* a = *((AddressAnchor**)arg2);

    ASSERT(a && "AddressAnchor should exist");

    uint64_t val = a->linkBaseAddress;

    if (key < val)
        return -1;
    if (key > val)
        return 1;
    return 0;
}

int searchLinkBaseAddress(const void* arg1, const void* arg2){
    uint64_t key = *((uint64_t*)arg1);
    AddressAnchor* a = *((AddressAnchor**)arg2);

    ASSERT(a && "AddressAnchor should exist");

    uint64_t val = a->linkBaseAddress;

    if (key < val)
        return -1;
    if (key >= val + a->getLink()->getSizeInBytes())
        return 1;
    return 0;
}


int compareLinkBaseAddress(const void* arg1, const void* arg2){
    AddressAnchor* a1 = *((AddressAnchor**)arg1);
    AddressAnchor* a2 = *((AddressAnchor**)arg2);
    //    uint64_t vl1 = a1->linkBaseAddress;
    //uint64_t vl2 = a2->linkBaseAddress;

    if(a1->linkBaseAddress < a2->linkBaseAddress)
        return -1;
    if(a1->linkBaseAddress > a2->linkBaseAddress)
        return 1;
    return 0;
}


Base* AddressAnchor::updateLink(Base* newLink){
    ASSERT(newLink->containsProgramBits());
    Base* oldLink = link;
    PRINT_DEBUG_ANCHOR("updating link: %#llx -> %#llx", linkBaseAddress, linkedParent->getBaseAddress());
    link = newLink;

    linkBaseAddress = link->getBaseAddress();
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
        return link->getBaseAddress() - linkedParent->getBaseAddress() - linkedParent->getSizeInBytes();
    } else if (linkedParent->getType() == ElfClassTypes_DataReference){
        return link->getBaseAddress();
    }
    __SHOULD_NOT_ARRIVE;
    return 0;
}

AddressAnchor::AddressAnchor(Base* lnk, Base* par){
    link = lnk;
    linkedParent = par;
    
    linkBaseAddress = link->getBaseAddress();

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
    } else if (link->getType() == ElfClassTypes_DataReference){
    } else {
        PRINT_ERROR("Address link cannot have type %d", link->getType());
        return false;
    }

    if (linkBaseAddress != link->getBaseAddress()){
        PRINT_ERROR("Link base address %#llx cached does not match actual value %#llx", linkBaseAddress, link->getBaseAddress());
        return false;
    }

    return true;
}

void AddressAnchor::print(){
    PRINT_INFOR("DataRef: addr %#llx, link offset %#llx", linkBaseAddress, getLinkOffset());
    if (linkedParent){
        linkedParent->print();
    }
    if (link){
        link->print();
    }
}
