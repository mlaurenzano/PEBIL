#include <Base.h>
#include <Instruction.h>
#include <CStructuresX86.h>

Operand::Operand(uint32_t typ, uint64_t val){
    type = typ;
    value = val;
}

uint64_t Operand::setValue(uint64_t val){
    value = val;
    return value;
}

uint32_t Operand::setType(uint32_t typ){
    type = typ;
    return type;
}

uint64_t Instruction::setOperandValue(uint32_t idx, uint64_t value){
    ASSERT(idx >= 0 && idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx]->setValue(value);
    return operands[idx]->getValue();
}

uint32_t Instruction::setOperandType(uint32_t idx, uint32_t type){
    ASSERT(idx >= 0 && idx < MAX_OPERANDS && "Index into operand table has a limited range");
    operands[idx]->setType(type);
    return operands[idx]->getType();
}


uint64_t Instruction::setNextAddress(){
    switch(type){
    case x86_insn_type_cond_branch:
    case x86_insn_type_branch:
        if (operands[JUMP_TARGET_OPERAND]->getType() == x86_operand_type_immrel){
            nextAddress = getAddress() + operands[JUMP_TARGET_OPERAND]->getValue();
            PRINT_DEBUG_OPTARGET("Set next address to 0x%llx = 0x%llx + 0x%llx", nextAddress,  getAddress(), operands[JUMP_TARGET_OPERAND]->getValue());
        } else {
            nextAddress = operands[JUMP_TARGET_OPERAND]->getValue();
        }
        break;
    default:
        nextAddress = virtualAddress + instructionLength;
        break;
    }
    PRINT_DEBUG_OPTARGET("Set next address to 0x%llx", nextAddress);
    return nextAddress;
}


uint32_t Instruction::setOpcodeType(uint32_t formatType, uint32_t idx1, uint32_t idx2){
    PRINT_DEBUG_OPCODE("Setting instruction type %d 0x%08x 0x%08x", formatType, idx1, idx2);

    switch(formatType){
    case x86_insn_format_onebyte:
        type = computeOpcodeTypeOneByte(idx1);
        break;
    case x86_insn_format_twobyte:
        type = computeOpcodeTypeTwoByte(idx1);
        break;
    case x86_insn_format_groups:
        type = computeOpcodeTypeGroups(idx1,idx2);
        break;
    case x86_insn_format_prefix_user_table:
        type = computeOpcodeTypePrefixUser(idx1,idx2);
        break;
    case x86_insn_format_x86_64:
        type = computeOpcodeTypeX8664(idx1,idx2);
        break;
    case x86_insn_format_float_mem:
        type = x86_insn_type_float;
        break;
    case x86_insn_format_float_reg:
        type = x86_insn_type_float;
        break;
    case x86_insn_format_float_groups:
        type = x86_insn_type_float;
        break;
    default:
        type = x86_insn_type_unknown;
        break;
    }

    ASSERT(type && "Instruction type should be known");

}

Instruction::Instruction(){
    instructionLength = 0;
    rawBytes = NULL;
    virtualAddress = 0;
    type = x86_insn_type_unknown;
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        operands[i] = new Operand(x86_operand_type_unused,0);
    }
}

Instruction::~Instruction(){
    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        delete operands[i];
    }
    if (rawBytes){
        delete rawBytes;
    }
}

uint32_t Instruction::getLength(){
    return instructionLength;
}

char* Instruction::getBytes(){
    return rawBytes;
}

char* Instruction::setBytes(char* bytes){
    if (rawBytes){
        delete rawBytes;
        rawBytes = new char[instructionLength];
        memcpy(rawBytes,bytes,instructionLength);
    }
    return rawBytes;
}

uint64_t Instruction::setAddress(uint64_t addr){
    virtualAddress = addr;
    return virtualAddress;
}

uint32_t Instruction::setLength(uint32_t len){
    ASSERT(len <= MAX_X86_INSTRUCTION_LENGTH && "X86 instructions are limited in size");
    if (rawBytes){
        char* newBytes = new char[len];
        memcpy(newBytes,rawBytes,len);
        delete[] rawBytes;
        rawBytes = newBytes;
    }
    instructionLength = len;

    return instructionLength;
}

char* Instruction::setDisassembledString(char* disStr){
    strncpy(disassembledString, disStr, strlen(disStr));
    return disassembledString;
}

uint64_t Instruction::getAddress(){
    return virtualAddress;
}

uint64_t Instruction::getNextAddress(){
    return nextAddress;
}

Operand* Instruction::getOperand(uint32_t idx){
    ASSERT(idx >= 0 && idx < MAX_OPERANDS && "Index into operand table has a limited range");
    return operands[idx];
}

void Instruction::print(){
    PRINT_INFOR("Instruction (type %d) at address 0x%016llx has %d bytes -> 0x%016llx", type, virtualAddress, instructionLength, nextAddress);

    for (uint32_t i = 0; i < MAX_OPERANDS; i++){
        if (operands[i]->getType()){
            PRINT_INFOR("\tOperand %d: %d 0x%016llx", i, operands[i]->getType(), operands[i]->getValue());
        }
    }
}


uint32_t Instruction::computeOpcodeTypeOneByte(uint32_t idx){
    ASSERT(idx >= 0x00 && idx < 0x100 && "Opcode identifier should be limited in range");

    uint32_t typ;

    switch(idx){
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
    case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
    case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
    case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
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
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        typ = x86_insn_type_int;
        break;
    case 0x98: case 0x99: case 0x9a: case 0x9b:
        typ = x86_insn_type_branch;
        break;
    case 0x9c: case 0x9d: case 0x9e: case 0x9f:
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
    case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
    case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
    case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
        typ = x86_insn_type_int;
        break;
    case 0xc0: case 0xc1: case 0xc2: case 0xc3:
        typ = x86_insn_type_branch;
        break;
    case 0xc4: case 0xc5: case 0xc6: case 0xc7: case 0xc8: case 0xc9:
        typ = x86_insn_type_int;
        break;
    case 0xca: case 0xcb: case 0xcc: case 0xcd: case 0xce: case 0xcf:
        typ = x86_insn_type_branch;
        break;
    case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd4: case 0xd5: case 0xd6: case 0xd7:
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
    case 0xe8: case 0xe9: case 0xea: case 0xeb:
        typ = x86_insn_type_branch;
        break;
    case 0xec: case 0xed: case 0xee: case 0xef:
        typ = x86_insn_type_io;
        break;
    case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
        typ = x86_insn_type_syscall;
        break;
    case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
        typ = x86_insn_type_int;
        break;
    }
    return typ;

}


uint32_t Instruction::computeOpcodeTypeTwoByte(uint32_t idx){
    ASSERT(idx >= 0x00 && idx < 0x100 && "Opcode identifier should be limited in range");

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
        case 0x02: case 0x03: case 0x04: case 0x05:
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
