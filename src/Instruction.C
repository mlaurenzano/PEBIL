#include <Instruction.h>

#include <AddressAnchor.h>
#include <Base.h>
#include <BinaryFile.h>
#include <CStructuresX86.h>
#include <Disassembler.h>
#include <ElfFile.h>
#include <Function.h>
#include <RawSection.h>
#include <SectionHeader.h>

#define JUMP_TGT_NOT_FOUND "<jump_tgt_not_found>"

Instruction* Instruction::generateAndImmReg(uint64_t imm, uint32_t idx){
    ASSERT(idx && idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x81;
    buff[1] = 0xe0 + (char)(idx);

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");

    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 10;
    if (idx == X86_REG_SP){
        len++;
    }
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xc7;
    buff[1] = 0x80 + (char)idx;
    uint32_t offoff = 2;
    uint32_t valoff = 6;
    if (idx == X86_REG_SP){
        buff[2] = 0x24;
        valoff++;
        offoff++;
    }

    uint32_t val32 = (uint32_t)immval;
    ASSERT(val32 == (uint32_t)immval && "Cannot use more than 32 bits for the immediate");
    uint32_t off32 = (uint32_t)immoff;
    ASSERT(off32 == (uint32_t)immoff && "Cannot use more than 32 bits for the immediate");

    memcpy(buff+valoff,&val32,sizeof(uint32_t));
    memcpy(buff+offoff,&off32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveRegaddrImmToReg(uint32_t idxsrc, uint64_t imm, uint32_t idxdest){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 7;
    if (idxsrc == X86_REG_SP){
        len++;
    }    
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x48;
    buff[1] = 0x8b;
    buff[2] = 0x80 + (char)(idxdest*8) + (char)(idxsrc);
    uint32_t imm32 = (uint32_t)imm;
    uint32_t immoff = 3;
    if (idxsrc == X86_REG_SP){
        buff[3] = 0x24;
        immoff++;
    }
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateStoreEflagsToAH(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9f;
    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateLoadEflagsFromAH(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9e;
    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateMoveImmByteToReg(uint8_t imm, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    uint32_t reg = 0;
    uint8_t opc = 0xb0;

    if (idx > 3){
        len += 2;
        reg++;
        opc += 0x08;
    } 
    char* buff = new char[len];
    buff[0] = 0x66;
    buff[reg] = opc + idx;
    buff[reg+1] = imm;
    if (idx > 3){
        buff[3] = 0x00;
    }
    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateRegIncrement(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x40 + idx;
    return generateInstructionBase(len,buff);
}

void Instruction::binutilsPrint(FILE* stream){
    fprintf(stream, "%llx: ", getBaseAddress());

    int extraSpaces = 8;
    for (uint32_t j = 0; j < getSizeInBytes(); j++){
        fprintf(stream, "%02hhx ", rawBytes[j]);
        extraSpaces--;
    }
    while (extraSpaces > 0){
        fprintf(stream, "   ");
        extraSpaces--;
    }

    Base::disassembler->disassembleInstructionInPlace(this);

    if (usesRelativeAddress()){
        if (addressAnchor){
            fprintf(stream, "\t#x@ %llx", addressAnchor->linkBaseAddress);
        } else {
            fprintf(stream, "\t#x  %llx", getRelativeValue()+getBaseAddress());
        }
    }

    if (isNoop()){
        fprintf(stream, "\t#x nop");
    }

    fprintf(stream, "\n");
}

bool Instruction::usesControlTarget(){
    if (isConditionalBranch() ||
        isUnconditionalBranch() ||
        isFunctionCall() ||
        isSystemCall()){
        return true;
    }
    return false;
}

void Instruction::computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList){
    ASSERT(isJumpTableBase() && "Cannot compute jump table targets for this instruction");
    ASSERT(func);
    ASSERT(addressList);

    RawSection* dataSection = textSection->getElfFile()->findDataSectionAtAddr(tableBase);
    if (!dataSection){
        print();
        PRINT_ERROR("Cannot find table base %#llx for this instruction", tableBase);
    }
    ASSERT(dataSection);
    ASSERT(dataSection->getSectionHeader()->hasBitsInFile());

    // read the first location to decide what type of info is stored in the jump table
    uint64_t rawData;
    if (textSection->getElfFile()->is64Bit()){
        rawData = getUInt64(dataSection->getStreamAtAddress(tableBase));
    } else {
        rawData = (uint64_t)getUInt32(dataSection->getStreamAtAddress(tableBase));
    }

    bool directMode;
    // the data found is an address
    if (func->inRange(rawData)){
        directMode = true;
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Direct", tableBase);
    } 
    // the data found is an address offset
    else if (func->inRange(rawData+baseAddress) || absoluteValue(rawData) < JUMP_TABLE_REACHES){
        directMode = false;
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Indirect", tableBase);
    }
    // the data found is neither of the above -- we interpret this to mean that it is instructions
    else {
        (*addressList).append(tableBase);
        PRINT_DEBUG_JUMP_TABLE("\tJumpMode for table base %#llx -- Instructions", tableBase);
        return;
    }


    uint32_t currByte = 0;
    uint32_t dataLen;
    if (textSection->getElfFile()->is64Bit()){
        dataLen = sizeof(uint64_t);
    } else {
        dataLen = sizeof(uint32_t);
    }

    do {
        if (textSection->getElfFile()->is64Bit()){
            rawData = getUInt64(dataSection->getStreamAtAddress(tableBase+currByte));
        } else {
            rawData = (uint64_t)getUInt32(dataSection->getStreamAtAddress(tableBase+currByte));
        }
        currByte += dataLen;

        if (!directMode){
            rawData += baseAddress;
        }
        PRINT_DEBUG_JUMP_TABLE("Jump Table target %#llx", rawData);
        (*addressList).append(rawData);
    } while (func->inRange((*addressList).back()) &&
             (tableBase+currByte)-dataSection->getSectionHeader()->GET(sh_addr) < dataSection->getSizeInBytes());
    (*addressList).remove((*addressList).size()-1);
}

uint64_t Instruction::findJumpTableBaseAddress(Vector<Instruction*>* functionInstructions){
    ASSERT(isJumpTableBase() && "Cannot compute jump table base for this instruction");

    uint64_t jumpOperand = operands[JUMP_TARGET_OPERAND].getValue();
    PRINT_DEBUG_JUMP_TABLE("Finding jump table base address for instruction at %#llx", baseAddress);

    // jump target is a register
    if (jumpOperand < X86_64BIT_GPRS){
        if ((*functionInstructions).size()){
            Instruction** allInstructions = new Instruction*[(*functionInstructions).size()];
            for (uint32_t i = 0; i < (*functionInstructions).size(); i++){
                allInstructions[i] = (*functionInstructions)[i];
            }
            qsort(allInstructions,(*functionInstructions).size(),sizeof(Instruction*),compareBaseAddress);

            // search backwards through instructions to find jump table base
            uint64_t prevAddr = baseAddress-1;
            void* prev = NULL;
            do {
                PRINT_DEBUG_JUMP_TABLE("\tTrying Jump base address %#llx", prevAddr);
                prev = bsearch(&prevAddr,allInstructions,(*functionInstructions).size(),sizeof(Instruction*),searchBaseAddress);
                if (prev){
                    Instruction* previousInstruction = *(Instruction**)prev;
                    bool jumpOpFound = false;
                    uint64_t immediate = 0;
                    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
                        Operand op = previousInstruction->getOperand(i);
                        if (op.getType() && op.getValue() == jumpOperand){
                            jumpOpFound = true;
                        }
                        if (op.getType() && op.getValue() >= X86_64BIT_GPRS){
                            immediate = op.getValue();
                        }
                        if (previousInstruction->usesRelativeAddress()){
                            immediate = previousInstruction->getRelativeValue() + previousInstruction->getBaseAddress() + previousInstruction->getSizeInBytes();
                        }
                    }
                    if (jumpOpFound && immediate){
                        delete[] allInstructions;
                        PRINT_DEBUG_JUMP_TABLE("\t\tFound jump table base at %#llx", immediate);
                        if (!textSection->getElfFile()->findDataSectionAtAddr(immediate)){
                            return 0;
                        }
                        return immediate;
                    }
                    prevAddr = previousInstruction->getBaseAddress()-1;
                }
            } while (prev);
            delete[] allInstructions;
        }
    } 
    // jump target is a memory location
    else {
        if (!textSection->getElfFile()->findDataSectionAtAddr(operands[JUMP_TARGET_OPERAND].getValue())){
            return 0;
        }
        return operands[JUMP_TARGET_OPERAND].getValue();
    }
    return 0;
}

bool Instruction::isJumpTableBase(){
    return (isUnconditionalBranch() && usesIndirectAddress());
}

bool Instruction::controlFallsThrough(){
    if (isHalt()
        || isReturn()
        || isUnconditionalBranch()
        || isJumpTableBase()){
        return false;
    }

    return true;
}

bool Operand::isIndirect(){
    if (type == x86_operand_type_func_indirE){
        return true;
    }
    return false;
}


uint32_t Instruction::bytesUsedForTarget(){
    if (isControl()){
        if (isUnconditionalBranch() || isConditionalBranch() || isFunctionCall()){
            return operands[JUMP_TARGET_OPERAND].getBytesUsed();
        } else {
            return 0;
        }
    }
    return 0;
}

uint32_t Instruction::convertTo4ByteOperand(){
    ASSERT(isControl());
    ASSERT(rawBytes);

#ifdef DEBUG_INST
    PRINT_INFOR("Before mod");
    print();
#endif
    if (bytesUsedForTarget() && bytesUsedForTarget() < sizeof(uint32_t)){
        if (isUnconditionalBranch()){
            ASSERT(sizeInBytes == 2);
            if (!addressAnchor){
                print();
            }
            ASSERT(addressAnchor);
            //ASSERT(rawBytes[0] == 0xeb && "Expected a 2-byte jmp opcode here");
            sizeInBytes += 3;
            char* newBytes = new char[sizeInBytes];
            newBytes[0] = rawBytes[0] - 0x02;
            uint32_t addr = baseAddress - getTargetAddress();
            memcpy(newBytes+1,&addr,sizeof(uint32_t));
            setBytes(newBytes);
            operands[JUMP_TARGET_OPERAND].setBytesUsed(sizeof(uint32_t));
            delete[] newBytes;
        } else if (isConditionalBranch()){
            if (sizeInBytes != 2){
                PRINT_WARN(4,"Conditional Branch with 3 bytes encountered");
                print();
            }
            //            ASSERT(sizeInBytes == 2 || sizeInBytes == 3);
            if (!addressAnchor){
                PRINT_ERROR("Instruction at address %#llx (%#llx) should have an address anchor", getBaseAddress(), getProgramAddress());
            }
            ASSERT(addressAnchor);
            //ASSERT(rawBytes[0] != 0xe3 && "We don't handle jcxz instruction currently");
            sizeInBytes += 4;
            char* newBytes = new char[sizeInBytes];
            newBytes[0] = 0x0f;
            newBytes[1] = rawBytes[0] + 0x10;
            uint32_t addr = baseAddress - getTargetAddress();
            memcpy(newBytes+2,&addr,sizeof(uint32_t));
            setBytes(newBytes);
            operands[JUMP_TARGET_OPERAND].setBytesUsed(sizeof(uint32_t));
            operands[JUMP_TARGET_OPERAND].setBytePosition(operands[JUMP_TARGET_OPERAND].getBytePosition()+1);
            delete[] newBytes;
        } else if (isFunctionCall()){
            __FUNCTION_NOT_IMPLEMENTED;
        } else if (isReturn()){
            ASSERT(sizeInBytes == 1);
            // nothing to do since returns dont have target ops
        } else {
            PRINT_ERROR("Unknown branch type %d not handled currently", instructionType);
            __SHOULD_NOT_ARRIVE;
        }
    }
#ifdef DEBUG_INST
    PRINT_INFOR("After mod");
    print();
#endif
    return sizeInBytes;
}

bool Instruction::verify(){
    uint32_t relCount = 0;
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].isRelative()){
            relCount++;
        }
    }
    if (relCount > 1){
        PRINT_ERROR("Cannot have more than one relative operand in an instruction");
        return false;
    }
    if (instructionType > x86_insn_type_Total){
        PRINT_ERROR("Instruction type malformed");
        return false;
    }
    /*
    if (instructionType == x86_insn_type_bad){
        PRINT_ERROR("Instruction type bad -- likely a misinterpretation of data as instructions or an error in control flow");
        return false;
    }
    */
    /*
    if (instructionType == x86_insn_type_unknown){
        PRINT_ERROR("Instruction type unknown");
        return false;
    }
    */
    return true;
}

bool Instruction::usesIndirectAddress(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].getType() && operands[i].isIndirect()){
            return true;
        }
    }
    return false;        
}

bool Instruction::usesRelativeAddress(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].getType() && operands[i].isRelative()){
            return true;
        }
    }
    return false;    
}

uint64_t Instruction::getRelativeValue(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].getType() && operands[i].isRelative()){
            return operands[i].getValue();
        }
    }
    return 0;
}

void Instruction::deleteAnchor(){
    if (addressAnchor){
        delete addressAnchor;
    }
    addressAnchor = NULL;
}

void Instruction::initializeAnchor(Base* link){
    ASSERT(!addressAnchor);
    ASSERT(link->containsProgramBits());
    addressAnchor = new AddressAnchor(link,this);
}

uint64_t Instruction::getProgramAddress(){
    return programAddress;
}

ByteSources Instruction::getByteSource(){
    return source;
}

bool Instruction::isNoop(){
    return (instructionType == x86_insn_type_noop);
}

bool Instruction::isIndirectBranch(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].getType() == x86_operand_type_func_indirE){
            return true;
        }
    }
    return false;
}

uint32_t Instruction::getIndirectBranchTarget(){
    ASSERT(isIndirectBranch());
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].getType() == x86_operand_type_func_indirE){
            return operands[i].getValue();
        }
    }
    __SHOULD_NOT_ARRIVE;
}

bool Instruction::isControl(){
    return  (isConditionalBranch() || isUnconditionalBranch() || isSystemCall() || isFunctionCall() || isReturn());
}

bool Instruction::isRelocatable(){
    return true;
}

void Instruction::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    ASSERT(rawBytes && sizeInBytes && "This instruction has no bytes thus it cannot be dumped");
    binaryOutputFile->copyBytes(rawBytes,sizeInBytes,offset);

    // the anchor will now overwrite any original instruction bytes that relate to relative addresses
    if (addressAnchor){
        addressAnchor->dump(binaryOutputFile,offset);
    }
}

// this function deletes the incoming buffer aftetr copying it to the new instruction's local memory
Instruction* Instruction::generateInstructionBase(uint32_t sz, char* buff){
    Instruction* ret = new Instruction();
    ret->setSizeInBytes(sz);
    ret->setBytes(buff);
    delete[] buff;
    return ret;
}

Instruction* Instruction::generateInterrupt(uint8_t idx){
    uint32_t len = 1;
    if (idx != X86TRAPCODE_BREAKPOINT &&
        idx != X86TRAPCODE_OVERFLOW){
        len++;
    }

    char* buff = new char[len];
    if (idx == X86TRAPCODE_BREAKPOINT){
        buff[0] = 0xcc;
    } else if (idx == X86TRAPCODE_OVERFLOW){
        buff[0] = 0xce;
    } else {
        buff[0] = 0xcd;
        buff[1] = idx;
    }
    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateSetDirectionFlag(bool backward){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xfc;
    if (backward){
        buff[0]++;
    }    

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateSTOSByte(bool repeat){
    ASSERT(!repeat && "Repeat prefix not implemented yet for this instruction");
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xaa;

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateStringMove(bool repeat){
    Instruction* ret = new Instruction();
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[len-1] = 0xa4;
    if (repeat){
        buff[0] = 0xf3;
    }

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateMoveImmToSegmentReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_SEGMENT_REGS && "Illegal segment register index given");
    ASSERT(idx != X86_SEGREG_CS && "Illegal segment register index given");

    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x8e;
    buff[1] = 0x05 + (char)(8*idx);

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate");
    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}


Instruction* Instruction::generatePushSegmentReg(uint32_t idx){
    ASSERT(idx < X86_SEGMENT_REGS && "Illegal segment register index given");
    ASSERT(idx != X86_SEGREG_CS && "Illegal segment register index given");

    uint32_t len = 1;
    if (idx == X86_SEGREG_FS || idx == X86_SEGREG_GS){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0x06 + (char)(8*idx);
    if (idx == X86_SEGREG_FS || idx == X86_SEGREG_GS){
        buff[0] = 0x0f;
        buff[1] = 0x80 + (char)idx;
    }    

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generatePopSegmentReg(uint32_t idx){
    ASSERT(idx < X86_SEGMENT_REGS && "Illegal segment register index given");
    ASSERT(idx != X86_SEGREG_CS && "Illegal segment register index given");

    uint32_t len = 1;
    if (idx == X86_SEGREG_FS || idx == X86_SEGREG_GS){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0x07 + (char)(8*idx);
    if (idx == X86_SEGREG_FS || idx == X86_SEGREG_GS){
        buff[0] = 0x0f;
        buff[1] = 0x81 + (char)idx;
    }    

    return generateInstructionBase(len,buff);
}


Instruction* Instruction64::generateStackPush4Byte(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x66; 
    buff[1] = 0x50 + idx;
    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateAddByteToRegaddr(uint8_t byt, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x83; 
    buff[1] = 0x00 + idx;
    buff[2] = byt;
    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx){
    ASSERT(srcidx < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x8b; 
    buff[1] = 0x00 + (char)(srcidx) + (char)(8*destidx);
    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx){
    ASSERT(srcidx < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x67;
    buff[1] = 0x8b; 
    buff[2] = 0x00 + (char)(srcidx) + (char)(8*destidx);
    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateRegSubImmediate(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");

    if (!imm){
        return Instruction64::generateNoop();
    } else if (imm <= 0xff){
        return Instruction64::generateRegSubImmediate1Byte(idx,imm);
    } else if (imm <= 0xffffffff){
        return Instruction64::generateRegSubImmediate4Byte(idx,imm);
    } else {
        PRINT_ERROR("Cannot use more than 32 bits for immediate");
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

Instruction* Instruction64::generateRegSubImmediate1Byte(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 4;
    char* buff = new char[len];

    if (idx < X86_32BIT_GPRS){        
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0x83;
    buff[2] = 0xe8 + (char)(idx % X86_32BIT_GPRS);
    uint32_t imm8 = (uint8_t)imm;
    ASSERT(imm8 == imm && "Cannot use more than 8 bits for immediate");
    memcpy(buff+3,&imm8,sizeof(uint8_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateRegSubImmediate4Byte(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 7;
    uint32_t immoff = 3;
    if (idx == X86_REG_AX){
        len--;
        immoff--;
    }
    char* buff = new char[len];
    buff[0] = 0x48;
    buff[1] = 0x81;
    buff[2] = 0xe8 + (char)(idx % X86_32BIT_GPRS);
    if (idx == X86_REG_AX){
        buff[1] = 0x2d;
    }
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}



Instruction* Instruction64::generateRegAddImmediate(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");

    if (!imm){
        return Instruction64::generateNoop();
    } else if (imm <= 0xff){
        return Instruction64::generateRegAddImmediate1Byte(idx,imm);
    } else if (imm <= 0xffffffff){
        return Instruction64::generateRegAddImmediate4Byte(idx,imm);
    } else {
        PRINT_ERROR("Cannot use more than 32 bits for immediate");
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

Instruction* Instruction64::generateRegAddImmediate1Byte(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 4;
    char* buff = new char[len];

    if (idx < X86_32BIT_GPRS){        
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0x83;
    buff[2] = 0xc0 + (char)(idx % X86_32BIT_GPRS);
    uint32_t imm8 = (uint8_t)imm;
    ASSERT(imm8 == imm && "Cannot use more than 8 bits for immediate");
    memcpy(buff+3,&imm8,sizeof(uint8_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateRegAddImmediate4Byte(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 7;
    uint32_t immoff = 3;
    if (idx == X86_REG_AX){
        len--;
        immoff--;
    }
    char* buff = new char[len];
    buff[0] = 0x48;
    buff[1] = 0x81;
    buff[2] = 0xc0 + (char)(idx % X86_32BIT_GPRS);
    if (idx == X86_REG_AX){
        buff[1] = 0x05;
    }
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateRegSubImmediate(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x81;
    buff[1] = 0xe8 + (char)idx;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immset");
    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}


Instruction* Instruction32::generateRegAddImmediate(uint32_t idx, uint64_t imm){
    ASSERT(idx > 0 && idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x81;
    buff[1] = 0xc0 + (char)idx;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immset");
    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateMoveImmByteToMemIndirect(uint8_t byt, uint64_t off, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 7;
    char* buff = new char[len];
    buff[0] = 0xc6;
    buff[1] = 0x80 + (char)idx;
    buff[6] = byt;
    uint32_t off32 = (uint32_t)off;
    ASSERT(off32 == off && "Cannot use more than 32 bits for offset");
    memcpy(buff+2,&off32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateReturn(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xc3;

    return generateInstructionBase(len,buff);
}


Instruction* Instruction64::generateStackPush(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    if (idx < X86_32BIT_GPRS){
        return Instruction32::generateStackPush(idx);
    }
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x41;
    buff[1] = 0x48 + (char)idx;

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateStackPop(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    if (idx < X86_32BIT_GPRS){
        return Instruction32::generateStackPop(idx);
    }
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x41;
    buff[1] = 0x50 + (char)idx;

    return generateInstructionBase(len,buff);
}


Instruction* Instruction::generatePushEflags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9c;

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generatePopEflags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9d;

    return generateInstructionBase(len,buff);
}


Instruction* Instruction::generateNoop(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x90;

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveRegToRegaddrImm(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    

    if (!imm){
        return Instruction64::generateMoveRegToRegaddr(idxsrc,idxdest);
    } else if (imm <= 0xff){
        return Instruction64::generateMoveRegToRegaddrImm1Byte(idxsrc,idxdest,imm);
    } else {
        return Instruction64::generateMoveRegToRegaddrImm4Byte(idxsrc,idxdest,imm);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}


Instruction* Instruction64::generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    if (idxdest == X86_REG_BP){
        return Instruction64::generateMoveRegToRegaddrImm1Byte(idxsrc,idxdest,0);
    }
    uint32_t len = 3;
    if (idxdest == X86_REG_SP){
        len++;
    }
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x48;
    buff[1] = 0x89;
    buff[2] = 0x00 + (char)(idxsrc*8) + (char)(idxdest);
    if (idxdest == X86_REG_SP){
        buff[3] = 0x24;
    }

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 4;
    if (idxdest == X86_REG_SP){
        len++;
    }    
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x48;
    buff[1] = 0x89;
    buff[2] = 0x40 + (char)(idxsrc*8) + (char)(idxdest);
    uint8_t imm8 = (uint8_t)imm;
    uint32_t immoff = 3;
    if (idxdest == X86_REG_SP){
        buff[3] = 0x24;
        immoff++;
    }
    ASSERT(imm8 == (uint8_t)imm && "Cannot use more than 8 bits for the immediate");
    memcpy(buff+immoff,&imm8,sizeof(uint8_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 7;
    if (idxdest == X86_REG_SP){
        len++;
    }    
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x48;
    buff[1] = 0x89;
    buff[2] = 0x80 + (char)(idxsrc*8) + (char)(idxdest);
    uint32_t imm32 = (uint32_t)imm;
    uint32_t immoff = 3;
    if (idxdest == X86_REG_SP){
        buff[3] = 0x24;
        immoff++;
    }
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest){
    ASSERT(idxsrc < 8 && "Illegal register index given");
    ASSERT(idxdest < 8 && "Illegal register index given");    
    uint32_t len = 2;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x00 + (char)(idxsrc*8) + (char)(idxdest);

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateIndirectRelativeJump(uint64_t addr, uint64_t tgt){
    uint32_t len = 6;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xff;
    buff[1] = 0x25;
    uint64_t imm = tgt - addr - len;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(addr == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    ASSERT(tgt == (uint32_t)tgt && "Cannot use more than 32 bits for the address");
    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveImmToReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 7;
    char* buff = new char[len];

    // set opcode
    if (idx < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0xc7;
    buff[2] = 0xc0 + (char)(idx % 8);    

    // set target address
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+3,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateMoveImmToReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xb8 + (char)idx;

    // set target address
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateMoveRegToMem(uint32_t idx, uint64_t addr){
    ASSERT(idx > 0 && idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x05 + 0x8*(char)idx;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+2,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveRegToMem(uint32_t idx, uint64_t addr){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 8;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x48;
    buff[1] = 0x89;
    buff[2] = 0x04 + 0x8*(char)idx;
    buff[3] = 0x25;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+4,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateMoveMemToReg(uint64_t addr, uint32_t idx){
    ASSERT(idx > 0 && idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x05 + 0x8*(char)idx;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+2,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction64::generateMoveMemToReg(uint64_t addr, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 8;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x48;
    buff[1] = 0x8b;
    buff[2] = 0x04 + 0x8*(char)idx;
    buff[3] = 0x25;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+4,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateStackPush(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x50 + (char)idx;

    return generateInstructionBase(len,buff);
}

Instruction* Instruction32::generateStackPop(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x58 + (char)idx;

    return generateInstructionBase(len,buff);
}


Instruction* Instruction::generateCallRelative(uint64_t addr, uint64_t tgt){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xe8;

    uint64_t imm = tgt - addr - len;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(addr == (uint32_t)addr && "Cannot use more than 32 bits for address");
    ASSERT(tgt == (uint32_t)tgt && "Cannot use more than 32 bits for target");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);    
}

Instruction* Instruction32::generateJumpIndirect(uint64_t tgt){
    uint32_t len = 6;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xff;
    buff[1] = 0x25;

    // set target address
    uint32_t tgt32 = (uint32_t)tgt;
    ASSERT(tgt32 == tgt && "Cannot use more than 32 bits for jump target");
    memcpy(buff+2,&tgt32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateJumpRelative(uint64_t addr, uint64_t tgt){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xe9;

    uint64_t imm = tgt - addr - len;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(addr == (int32_t)addr && "Cannot use more than 32 bits for address");
    ASSERT(tgt == (int32_t)tgt && "Cannot use more than 32 bits for target");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Instruction* Instruction::generateStackPushImmediate(uint64_t imm){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x68;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

Operand::Operand(){
    type = x86_operand_type_unused;
    value = 0;
    bytesUsed = 0;
    relative = false;
    bytePosition = 0;

    index = 0;
}

Operand::Operand(uint32_t idx){
    type = x86_operand_type_unused;
    value = 0;
    bytesUsed = 0;
    relative = false;
    bytePosition = 0;

    index = idx;
}

Operand::Operand(uint32_t typ, uint64_t val, uint32_t idx){
    type = typ;
    value = val;
    bytesUsed = 0;
    relative = false;
    bytePosition = 0;

    index = idx;
}

void Operand::setBytesUsed(uint32_t usd){
    ASSERT(usd == sizeof(uint32_t) || usd == sizeof(uint16_t) || usd == sizeof(uint8_t));
    bytesUsed = usd;
}

void Instruction::setOperandValue(uint32_t idx, uint64_t value){
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx].setValue(value);
}

void Instruction::setOperandType(uint32_t idx, uint32_t typ){
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx].setType(typ);
}

void Instruction::setOperandBytePosition(uint32_t idx, uint32_t pos){
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx].setBytePosition(pos);
}

void Instruction::setOperandBytesUsed(uint32_t idx, uint32_t usd){
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx].setBytesUsed(usd);
}

void Instruction::setOperandRelative(uint32_t idx, bool rel){
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx].setRelative(rel);
}

uint64_t Instruction::getTargetAddress(){
    uint64_t nextAddress;
    if (instructionType == x86_insn_type_branch ||
        instructionType == x86_insn_type_cond_branch ||
        instructionType == x86_insn_type_call){
        if (operands[JUMP_TARGET_OPERAND].getType() == x86_operand_type_immrel){
            nextAddress = getBaseAddress() + operands[JUMP_TARGET_OPERAND].getValue();
            PRINT_DEBUG_OPTARGET("Set next address to 0x%llx = 0x%llx + 0x%llx", nextAddress,  getBaseAddress(), operands[JUMP_TARGET_OPERAND].getValue());
        } else {
            nextAddress = operands[JUMP_TARGET_OPERAND].getValue();
        }
    } else if (instructionType == x86_insn_type_syscall){
        nextAddress = 0;
    } else {
        nextAddress = baseAddress + sizeInBytes;
    }

    PRINT_DEBUG_OPTARGET("Set next address to 0x%llx", nextAddress);
    return nextAddress;
}


void Instruction::setOpcodeType(uint32_t formatType, uint32_t idx1, uint32_t idx2){
    PRINT_DEBUG_OPCODE("Setting instruction type %d 0x%08x 0x%08x", formatType, idx1, idx2);

    switch(formatType){
    case x86_insn_format_onebyte:
        instructionType = computeOpcodeTypeOneByte(idx1);
        break;
    case x86_insn_format_twobyte:
        instructionType = computeOpcodeTypeTwoByte(idx1);
        break;
    case x86_insn_format_groups:
        instructionType = computeOpcodeTypeGroups(idx1,idx2);
        break;
    case x86_insn_format_prefix_user_table:
        instructionType = computeOpcodeTypePrefixUser(idx1,idx2);
        break;
    case x86_insn_format_x86_64:
        instructionType = computeOpcodeTypeX8664(idx1,idx2);
        break;
    case x86_insn_format_float_mem:
        instructionType = x86_insn_type_float;
        break;
    case x86_insn_format_float_reg:
        instructionType = x86_insn_type_float;
        break;
    case x86_insn_format_float_groups:
        instructionType = x86_insn_type_float;
        break;
    default:
        instructionType = x86_insn_type_unknown;
        break;
    }

    ASSERT(instructionType && "Instruction type should be known");

}

uint64_t Instruction::findInstrumentationPoint(uint32_t size, InstLocations loc){
    __SHOULD_NOT_ARRIVE;
}

Instruction::Instruction(TextSection* text, uint64_t baseAddr, char* buff, ByteSources src, uint32_t idx)
    : Base(ElfClassTypes_Instruction)
{
    textSection = text;
    baseAddress = baseAddr;
    sizeInBytes = MAX_X86_INSTRUCTION_LENGTH;
    index = idx;
    rawBytes = NULL;
    setBytes(buff);
    source = src;
    instructionType = x86_insn_type_unknown;
    leader = false;

    programAddress = 0;
    if (IS_BYTE_SOURCE_APPLICATION(source)){
        programAddress = baseAddress;
    }

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        operands[i] = Operand(i);
    }

    sizeInBytes = Base::disassembler->disassemble((uint64_t)buff, this);
    if (!sizeInBytes){
        sizeInBytes = 1;
    }

    addressAnchor = NULL;
    verify();
}

Instruction::Instruction()
    : Base(ElfClassTypes_Instruction)
{
    textSection = NULL;
    baseAddress = 0;
    sizeInBytes = MAX_X86_INSTRUCTION_LENGTH;
    index = 0;
    rawBytes = NULL;
    source = ByteSource_Instrumentation;
    instructionType = x86_insn_type_unknown;
    leader = false;

    programAddress = 0;

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        operands[i] = Operand(i);
    }

    addressAnchor = NULL;
    verify();
}

Instruction::~Instruction(){
    if (rawBytes){
        delete[] rawBytes;
    }
    if (addressAnchor){
        delete addressAnchor;
    }
}

char* Instruction::getBytes(){
    return rawBytes;
}

void Instruction::setBytes(char* bytes){
    if (rawBytes){
        delete[] rawBytes;
    }
    rawBytes = new char[sizeInBytes];
    memcpy(rawBytes,bytes,sizeInBytes);
}

void Instruction::setSizeInBytes(uint32_t len){
    ASSERT(len <= MAX_X86_INSTRUCTION_LENGTH && "X86 instructions are limited in size");
    ASSERT(len <= sizeInBytes);
    sizeInBytes = len;
}

void Instruction::setDisassembledString(char* disStr){
    strncpy(disassembledString, disStr, strlen(disStr));
}

uint64_t Instruction::getBaseAddress(){
    return baseAddress;
}

Operand Instruction::getOperand(uint32_t idx){
    ASSERT(idx < MAX_OPERANDS && "Index into operand table has a limited range");
    return operands[idx];
}

const char* OpTypeNames[] = {
    "type_unused",        // 0
    "type_immrel",
    "type_reg",
    "type_imreg",
    "type_imm",
    "type_mem",               // 5
    "type_func_ST",
    "type_func_STi",
    "type_func_indirE",
    "type_func_E",
    "type_func_G",            // 10
    "type_func_IMREG",
    "type_func_I",
    "type_func_I64",
    "type_func_sI",
    "type_func_J",            // 15
    "type_func_SEG",
    "type_func_DIR",
    "type_func_OFF",
    "type_func_OFF64",
    "type_func_ESreg",        // 20
    "type_func_DSreg",
    "type_func_C",
    "type_func_D",
    "type_func_T",
    "type_func_Rd",           // 25    
    "type_func_MMX",
    "type_func_XMM",
    "type_func_EM",
    "type_func_EX",
    "type_func_MS",           // 30
    "type_func_XS",
    "type_func_3DNowSuffix",
    "type_func_SIMD_Suffix",
    "type_func_SIMD_Fixup"
    };

void Operand::print(){
    char* relStr = "NOREL";
    if (isRelative()){
        relStr =   "IPREL";
    }
    PRINT_INFOR("\tOPERAND(%d) %16s %5s %1d+%1d 0x%08x", index, OpTypeNames[getType()], relStr, getBytePosition(), getBytesUsed(), getValue());
}


const char* InstTypeNames[] = {
    "type_unknown",
    "type_bad",
    "type_cond_branch",
    "type_branch",
    "type_call",
    "type_return",
    "type_int",
    "type_float",
    "type_simd",
    "type_io",
    "type_prefetch",
    "type_syscall",
    "type_halt",
    "type_hwcount",
    "type_noop",
    "type_trap"
};

void Instruction::print(){
    char* relStr = "     ";
    if (isRelocatable()){
        relStr =   "RELOC";
    }
    char* fromStr = "   ";
    if (getByteSource() == ByteSource_Application){
        fromStr = "AppGnrl";
    } else if (getByteSource() == ByteSource_Application_FreeText){
        fromStr = "AppFree";
    } else if (getByteSource() == ByteSource_Application_Function){
        fromStr = "AppFunc";
    } else if (getByteSource() == ByteSource_Instrumentation){
        fromStr = "InstGen";
    } else {
        __SHOULD_NOT_ARRIVE;
    }

    PRINT_INFO();

    PRINT_OUT("INSTRUCTION(%d) %15s %5s %7s [%d bytes -- ", index, InstTypeNames[instructionType], relStr, fromStr, sizeInBytes);

    if (rawBytes){
        for (uint32_t i = 0; i < sizeInBytes; i++){
            PRINT_OUT("%02hhx", rawBytes[i]);
        }
    } else {
        PRINT_OUT("NOBYTES");
    }
    PRINT_OUT("] 0x%llx -> 0x%llx (paddr %#llx)", baseAddress, getTargetAddress(), getProgramAddress());
    PRINT_OUT("\n");

    if (isJumpTableBase()){
        PRINT_INFOR("\tis jump table");
    }

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i].getType()){
            operands[i].print();
        }
    }
}


uint32_t Instruction::computeOpcodeTypeOneByte(uint32_t idx){
    ASSERT(idx < 0x100 && "Opcode identifier should be limited in range");

    uint32_t typ;

    switch(idx){
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: 
        typ = x86_insn_type_int;
        break;
    case 0x2e: 
        typ = x86_insn_type_bad;
        break;
    case 0x2f:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: 
        typ = x86_insn_type_int;
        break;
    case 0x36: 
        typ = x86_insn_type_bad;
        break;
    case 0x37:
    case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: 
        typ = x86_insn_type_int;
        break;
    case 0x3e: 
        typ = x86_insn_type_bad;
        break;
    case 0x3f:
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
    case 0x60: case 0x61: case 0x62: case 0x63: 
        typ = x86_insn_type_int;
        break;
    case 0x64: case 0x65: case 0x66: case 0x67:
        typ = x86_insn_type_bad;
        break;
    case 0x68: case 0x69: case 0x6a: case 0x6b:
        typ = x86_insn_type_int;
        break;
    case 0x6c: case 0x6d: case 0x6e: case 0x6f:
        typ = x86_insn_type_io;
        break;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
        typ = x86_insn_type_cond_branch;
        break;
    case 0x80: case 0x81: 
        typ = x86_insn_type_int;
        break;
    case 0x82: 
        typ = x86_insn_type_bad;
        break;
    case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
        typ = x86_insn_type_int;
        break;
    case 0x90: 
        typ = x86_insn_type_noop;
        break;
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        typ = x86_insn_type_int;
        break;
    case 0x98: case 0x99: case 0x9a: 
        typ = x86_insn_type_call;
        break;
    case 0x9b:
        typ = x86_insn_type_bad;
        break;
    case 0x9c: case 0x9d: case 0x9e: case 0x9f:
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
    case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
    case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
    case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
        typ = x86_insn_type_int;
        break;
    case 0xc0: case 0xc1: 
        typ = x86_insn_type_branch;
        break;
    case 0xc2: case 0xc3:
        typ = x86_insn_type_return; // return
        break;
    case 0xc4: case 0xc5: case 0xc6: case 0xc7: case 0xc8: case 0xc9:
        typ = x86_insn_type_int;
        break;
    case 0xca: case 0xcb: 
        typ = x86_insn_type_return; // return
        break;
    case 0xcc: case 0xcd: case 0xce: 
        typ = x86_insn_type_trap;
        break;
    case 0xcf:
        typ = x86_insn_type_return; // return
        break;
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: 
        typ = x86_insn_type_int;
        break;
    case 0xd6: 
        typ = x86_insn_type_bad;
        break;
    case 0xd7:
        typ = x86_insn_type_int;
        break;
    case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
        typ = x86_insn_type_float;
        break;
    case 0xe0: case 0xe1: case 0xe2: case 0xe3:
        typ = x86_insn_type_cond_branch;
        break;
    case 0xe4: case 0xe5: case 0xe6: case 0xe7:
        typ = x86_insn_type_io;
        break;
    case 0xe8: 
        typ = x86_insn_type_call;
        break;
    case 0xe9: case 0xea: case 0xeb:
        typ = x86_insn_type_branch;
        break;
    case 0xec: case 0xed: case 0xee: case 0xef:
        typ = x86_insn_type_io;
        break;
    case 0xf0: case 0xf1: case 0xf2: case 0xf3: 
        typ = x86_insn_type_bad;
        break;
    case 0xf4: 
        typ = x86_insn_type_halt;
        break;
    case 0xf5: case 0xf6: case 0xf7:
        typ = x86_insn_type_syscall;
        break;
    case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
        typ = x86_insn_type_int;
        break;
    }
    return typ;
}


uint32_t Instruction::computeOpcodeTypeTwoByte(uint32_t idx){
    ASSERT(idx < 0x100 && "Opcode identifier should be limited in range");

    uint32_t typ;

    switch(idx){
    case 0x00: case 0x01: case 0x02: case 0x03:
        typ = x86_insn_type_int;
        break;
    case 0x04: case 0x05:
        typ = x86_insn_type_syscall;
        break;
    case 0x06:
        typ = x86_insn_type_int;
        break;
    case 0x07:
        typ = x86_insn_type_syscall;
        break;
    case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
        typ = x86_insn_type_int;
        break;
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
        typ = x86_insn_type_simd;
        break;
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
        typ = x86_insn_type_int;
        break;
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
        typ = x86_insn_type_float;
        break;
    case 0x30: case 0x31: case 0x32: case 0x33:
        typ = x86_insn_type_hwcount;
        break;
    case 0x34: case 0x35: case 0x36: case 0x37:
        typ = x86_insn_type_syscall;
        break;
    case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
        typ = x86_insn_type_int;
        break;
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
    case 0x68: case 0x69: case 0x6a: case 0x6b:
        typ = x86_insn_type_float;
        break;
    case 0x6c: case 0x6d: case 0x6e: case 0x6f: 
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
        typ = x86_insn_type_int;
        break;
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
        typ = x86_insn_type_cond_branch;
        break;
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
    case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
    case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
    case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
    case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
    case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc4: case 0xc5: case 0xc6: case 0xc7:
    case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf:
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
    case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
    case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6: case 0xe7:
    case 0xe8: case 0xe9: case 0xea: case 0xeb: case 0xec: case 0xed: case 0xee: case 0xef:
    case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
    case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
        typ = x86_insn_type_int;
        break;
    }
    return typ;

}


uint32_t Instruction::computeOpcodeTypeGroups(uint32_t idx1, uint32_t idx2){
    ASSERT(idx1 >= 0x00 && idx1 < 0x17 && "Opcode identifier should be limited in range");
    ASSERT(idx2 >= 0x0 && idx2 < 0x8 && "Opcode identifier should be limited in range");

    uint32_t typ;
    typ = x86_insn_type_unknown;
    switch(idx1){
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0a: case 0x0b:
        typ = x86_insn_type_int;
        break;
    case 0x0c:
        switch(idx2){
        case 0x00: case 0x01:
            typ = x86_insn_type_int;
            break;
        case 0x02: case 0x03:
            typ = x86_insn_type_call;
            break;
        case 0x04: case 0x05:
            typ = x86_insn_type_branch;
            break;
        case 0x06: case 0x07:
            typ = x86_insn_type_int;
            break;
        }
        break;
    case 0x0d: case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12: case 0x13:
        typ = x86_insn_type_int;
        break;
    case 0x14:
        switch(idx2){
        case 0x00: case 0x01: case 0x02: case 0x03:
            typ = x86_insn_type_float;
            break;
        case 0x04: case 0x05: case 0x06: case 0x07:
            typ = x86_insn_type_int;
            break;
        }
        break;
    case 0x15: case 0x16:
        typ = x86_insn_type_prefetch;
        break;
    }

    return typ;

}


uint32_t Instruction::computeOpcodeTypePrefixUser(uint32_t idx1, uint32_t idx2){
    ASSERT(idx1 >= 0x00 && idx1 < 0x1b && "Opcode identifier should be limited in range");
    ASSERT(idx2 >= 0x0 && idx2 < 0x4 && "Opcode identifier should be limited in range");

    uint32_t typ;

    switch(idx1){
    case 0x00:
        typ = x86_insn_type_float;
        break;
    case 0x01:
        typ = x86_insn_type_simd;
        break;
    case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1a:
        typ = x86_insn_type_float;
        break;
    }

    return typ;

}


uint32_t Instruction::computeOpcodeTypeX8664(uint32_t idx1, uint32_t idx2){
    ASSERT(idx1 >= 0x0 && idx1 < 0x1 && "Opcode identifier should be limited in range");
    ASSERT(idx2 >= 0x0 && idx2 < 0x2 && "Opcode identifier should be limited in range");

    uint32_t typ;

    switch(idx2){
    case 0x0:
        typ = x86_insn_type_int;
        break;
    case 0x1:
        typ = x86_insn_type_float;
        break;
    }
    return typ;

}
