#include <AddressAnchor.h>

#include <BinaryFile.h>

void AddressAnchor::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        Operand op = linkedInstruction->getOperand(i);
        if (op.isRelative()){
            if (op.getBytesUsed() == sizeof(uint8_t)){
                uint8_t value = getLinkOffset();
                binaryOutputFile->copyBytes((char*)&value,op.getBytesUsed(),offset + op.getBytePosition());
            } else if (op.getBytesUsed() == sizeof(uint16_t)){
                uint16_t value = getLinkOffset();
                binaryOutputFile->copyBytes((char*)&value,op.getBytesUsed(),offset + op.getBytePosition());
            } else if (op.getBytesUsed() == sizeof(uint32_t)){
                uint32_t value = getLinkOffset();
                binaryOutputFile->copyBytes((char*)&value,op.getBytesUsed(),offset + op.getBytePosition());
            } else if (op.getBytesUsed() == sizeof(uint64_t)){
                uint64_t value = getLinkOffset();
                binaryOutputFile->copyBytes((char*)&value,op.getBytesUsed(),offset + op.getBytePosition());
            }
            else {
                PRINT_ERROR("an operand cannot use %d bytes", op.getBytesUsed());
            }
        }
    }
}

uint64_t AddressAnchor::getLinkOffset(){
    uint64_t linkOffset = link->getBaseAddress() - linkedInstruction->getBaseAddress() - linkedInstruction->getLength();
    //    PRINT_DEBUG_ANCHOR("Found linkoffset: %#llx = %#llx + %#llx", linkOffset, linkedInstruction->getBaseAddress(), link->getBaseAddress());
    return linkOffset;
}

AddressAnchor::AddressAnchor(Base* lnk, Instruction* par){
    link = lnk;
    linkedInstruction = par;

    verify();
}

AddressAnchor::~AddressAnchor(){
}

bool AddressAnchor::verify(){
    if (!link->isCodeContainer()){
        PRINT_ERROR("Instrumentation point not allowed to be type %d", link->getType());
        return false;
    }
    return true;
}

void AddressAnchor::print(){
    PRINT_INFOR("Address Anchor %#llx", getLinkOffset());
    linkedInstruction->print();
    link->print();
}
