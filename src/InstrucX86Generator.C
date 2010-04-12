#include <Base.h>

#include <InstrucX86Generator.h>

InstrucX86* InstrucX86Generator64::generateMoveSegmentRegToReg(uint32_t src, uint32_t dest){
    ASSERT(src < X86_SEGMENT_REGS);
    ASSERT(dest < X86_64BIT_GPRS);

    uint32_t len = 3;
    char* buff = new char[len];

    if (dest < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0x8c;
    buff[2] = 0xc0 + 8 * src + (dest % X86_32BIT_GPRS);

    return generateInstructionBase(len,buff);
}

Vector<InstrucX86*>* InstrucX86Generator64::generateAddressComputation(InstrucX86* instruction, uint32_t dest){
    ASSERT(dest < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(instruction->isMemoryOperation());

    DEBUG_LOADADDR(
    instruction->print();
    PRINT_DEBUG_LOADADDR("---");
                   )

    Vector<InstrucX86*>* compInstructions = new Vector<InstrucX86*>();
    OperandX86* op = NULL;

    if (instruction->isExplicitMemoryOperation()){

        op = instruction->getMemoryOperand();

        if (instruction->GET(pfx_seg)){
            uint32_t segIdx = instruction->GET(pfx_seg) - UD_R_ES;
            uint64_t imm = op->getValue();

            (*compInstructions).append(generateMoveSegmentRegToReg(segIdx, dest));
            (*compInstructions).append(generateRegAddImm(dest, imm));
        } else if (op->GET(base) == UD_R_RIP){
            PRINT_DEBUG_LOADADDR("making lea: mov rip imm");
            uint64_t addr = op->getInstruction()->getProgramAddress() + op->getValue() + op->getInstruction()->getSizeInBytes();
            (*compInstructions).append(generateMoveImmToReg(addr, dest));
        } else {
            (*compInstructions).append(generateLoadEffectiveAddress(op, dest));
            ASSERT((*compInstructions).back() && op && (*compInstructions).back()->getOperand(COMP_SRC_OPERAND));
            ASSERT(op->isSameOperand((*compInstructions).back()->getOperand(COMP_SRC_OPERAND)) && "The generated Address Computation operand does not match the operand given");
        }

    } else {
        ASSERT(instruction->isImplicitMemoryOperation());
        uint64_t stackOffset = 0;
        if (instruction->isStackPush()){
            stackOffset = -1 * sizeof(uint64_t);
        } else {
            ASSERT(instruction->isStackPop());
            stackOffset = 0;
        }
        (*compInstructions).append(generateLoadEffectiveAddress(X86_REG_SP, 0, 1, stackOffset, dest, true, false));
    }

    
    PRINT_DEBUG_LOADADDR("have %d instructions for addr computation", (*compInstructions).size());
    DEBUG_LOADADDR(
    for (uint32_t i = 0; i < (*compInstructions).size(); i++){
        (*compInstructions)[i]->print();
    }
    PRINT_DEBUG_LOADADDR("-----------------------------------------------------------");
                   )

    return compInstructions;
}

InstrucX86* InstrucX86Generator64::generateLoadEffectiveAddress(uint32_t baseReg, uint32_t indexReg, uint8_t scale, uint64_t value, uint32_t dest, bool hasBase, bool hasIndex){
    ASSERT(dest < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(baseReg < X86_64BIT_GPRS || baseReg + UD_R_RAX == UD_R_RIP);
    ASSERT(indexReg < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(scale && isPowerOfTwo(scale) && scale <= 8);

    uint32_t len;
    char* buff;
    InstrucX86* lea = NULL;

    // scale/index/base
    if (hasIndex){
        uint8_t sa = (logBase2(scale)) << 6;

        // base is a reg
        if (hasBase && !value){
            PRINT_DEBUG_LOADADDR("making lea: SIB, base is only reg");

            len = 4;
            if (baseReg % X86_32BIT_GPRS == X86_REG_BP){
                len++;
            }
            buff = new char[len];
            
            buff[0] = 0x48;
            if (baseReg >= X86_32BIT_GPRS){
                buff[0]++;
            }
            if (indexReg >= X86_32BIT_GPRS){
                buff[0] += 2;
            }
            if (dest >= X86_32BIT_GPRS){
                buff[0] += 4;
            }
            
            buff[1] = 0x8d;
            buff[2] = 0x04 + 8 * (dest % X86_32BIT_GPRS);
            if (baseReg % X86_32BIT_GPRS == X86_REG_BP){
                buff[2] += 0x40;
                buff[4] = 0x00;
            }
            buff[3] = 0x00 + 8 * (indexReg % X86_32BIT_GPRS) + (baseReg % X86_32BIT_GPRS) + sa;
            
            lea = generateInstructionBase(len,buff);
        }

        // base is a value
        else {
            PRINT_DEBUG_LOADADDR("making lea: SIB, base is value");

            len = 8;
            buff = new char[len];

            buff[0] = 0x48;
            if (hasBase && baseReg >= X86_32BIT_GPRS){
                buff[0]++;
            }
            if (indexReg >= X86_32BIT_GPRS){
                buff[0] += 2;
            }
            if (dest >= X86_32BIT_GPRS){
                buff[0] += 4;
            }

            buff[1] = 0x8d;
            buff[2] = 0x04 + 8 * (dest % X86_32BIT_GPRS);
            buff[3] = 0x00 + 8 * (indexReg % X86_32BIT_GPRS);
            if (hasBase){
                PRINT_DEBUG_LOADADDR("\tbase also has reg");
                buff[2] += 0x80;
                buff[3] += (baseReg % X86_32BIT_GPRS);
            } else {
                buff[3] += 0x05;
            }
            
            buff[3] += sa;
            
            memcpy(buff + 4, &value, sizeof(uint32_t));
            lea = generateInstructionBase(len,buff);
        } 

    } 

    // base + offset
    else if (hasBase){
        len = 7;
        uint32_t immoff = 3;
        PRINT_DEBUG_LOADADDR("making lea: base + offset");

        if (baseReg % X86_32BIT_GPRS == X86_REG_SP){
            len++;
            immoff++;
        }
        buff = new char[len];

        if (dest < X86_32BIT_GPRS){
            buff[0] = 0x48;
        } else {
            buff[0] = 0x4c;
        }
        if (baseReg < X86_32BIT_GPRS){
        } else {
            buff[0]++;
        }
        buff[1] = 0x8d;
        if (baseReg + UD_R_RAX == UD_R_RIP){
            buff[2] = 0x05;
        } else {
            buff[2] = 0x80 + (baseReg % X86_32BIT_GPRS);
        }
        buff[2] += 8 * (dest % X86_32BIT_GPRS);

        if (baseReg % X86_32BIT_GPRS == X86_REG_SP){
            buff[3] = 0x24;
        }

        uint32_t addr32 = value;
        memcpy(buff + immoff, &addr32, sizeof(uint32_t));
        lea = generateInstructionBase(len,buff);
    } 

    // constant
    else {
        PRINT_DEBUG_LOADADDR("making lea: const");

        len = 8;
        buff = new char[len];
        if (dest < X86_32BIT_GPRS){
            buff[0] = 0x48;
        } else {
            buff[0] = 0x4c;
        }
        buff[1] = 0x8d;
        buff[2] = 0x04 + 8 * (dest % X86_32BIT_GPRS);
        buff[3] = 0x25;

        uint32_t addr32 = value;
        memcpy(buff + 4, &addr32, sizeof(uint32_t));
        lea = generateInstructionBase(len,buff);
    }

    ASSERT(lea);
    return lea;
}

InstrucX86* InstrucX86Generator64::generateLoadEffectiveAddress(OperandX86* op, uint32_t dest){
    ASSERT(dest < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(op);

    uint32_t baseReg = 0;
    uint32_t indexReg = 0;
    uint8_t scale = op->GET(scale);
    if (!scale) { 
        scale++; 
    }
    uint64_t value = op->getValue();

    bool hasBase = false;
    if (op->GET(base)){
        hasBase = true;
        baseReg = op->GET(base) - UD_R_RAX;
    }
    bool hasIndex = false;
    if (op->GET(index)){
        hasIndex = true;
        indexReg = op->GET(index) - UD_R_RAX;
    }

    InstrucX86* lea = generateLoadEffectiveAddress(baseReg, indexReg, scale, value, dest, hasBase, hasIndex);
    if (!lea || !op || !lea->getOperand(COMP_SRC_OPERAND)){
        lea->print();
    }
    ASSERT(lea && op && lea->getOperand(COMP_SRC_OPERAND));
    ASSERT(op->isSameOperand(lea->getOperand(COMP_SRC_OPERAND)) && "The generated LEA operand does not match the operand given");

    return lea;
}

// this function deletes the incoming buffer after copying it to the new instruction's local memory
InstrucX86* InstrucX86Generator32::generateInstructionBase(uint32_t sz, char* buff){
    return InstrucX86Generator::generateInstructionBase(sz, buff);
}

InstrucX86* InstrucX86Generator32::generateExchangeMemReg(uint64_t addr, uint8_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x87;
    buff[1] = 0x05 + 8 * idx;
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+2,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateLoadRegImmReg(uint8_t idxsrc, uint64_t imm, uint8_t idxdest){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 6;
    if (idxdest == X86_REG_SP){
        len++;
    }    
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x8d;
    buff[1] = 0x80 + (char)(idxsrc*8) + (char)(idxdest);
    uint32_t imm32 = (uint32_t)imm;
    uint32_t immoff = 2;
    if (idxdest == X86_REG_SP){
        buff[2] = 0x24;
        immoff++;
    }
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);    
}

InstrucX86* InstrucX86Generator64::generateLoadRegImmReg(uint8_t idxsrc, uint64_t imm, uint8_t idxdest){
    ASSERT(idxsrc < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_64BIT_GPRS && "Illegal register index given");    

    uint32_t len = 7;
    uint32_t immoff = 3;

    if (idxdest == X86_REG_SP){
        len++;
        immoff++;
    }

    char* buff = new char[len];

    if (idxsrc < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }

    if (idxdest < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x8d;
    buff[2] = 0x80 + 8*(idxsrc % X86_32BIT_GPRS) + (idxdest % X86_32BIT_GPRS);
    buff[3] = 0x24;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateInstructionBase(uint32_t sz, char* buff){
    InstrucX86* ret = new InstrucX86(NULL, 0, buff, ByteSource_Instrumentation, 0, true, sz);
    ASSERT(ret->getSizeInBytes() == sz);
    delete[] buff;
    return ret;
}

InstrucX86* InstrucX86Generator::generateInstructionBase(uint32_t sz, char* buff){
    InstrucX86* ret = new InstrucX86(NULL, 0, buff, ByteSource_Instrumentation, 0, false, sz);
    ASSERT(ret->getSizeInBytes() == sz);
    delete[] buff;
    return ret;
}

InstrucX86* InstrucX86Generator64::generateShiftRightLogical(uint8_t imm, uint8_t reg){
    ASSERT(reg < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 4;
    char* buff = new char[len];

    if (reg < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0xc1;
    buff[2] = 0xe8 + reg % X86_32BIT_GPRS;
    buff[3] = imm;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateShiftLeftLogical(uint8_t imm, uint8_t reg){
    ASSERT(reg < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 4;
    char* buff = new char[len];

    if (reg < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0xc1;
    buff[2] = 0xe0 + reg % X86_32BIT_GPRS;
    buff[3] = imm;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateShiftRightLogical(uint8_t imm, uint8_t reg){
    ASSERT(reg < X86_32BIT_GPRS && "Illegal register index given");

    uint32_t len = 3;
    char* buff = new char[len];

    buff[0] = 0xc1;
    buff[1] = 0xe8 + reg;
    buff[2] = imm;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateShiftLeftLogical(uint8_t imm, uint8_t reg){
    ASSERT(reg < X86_32BIT_GPRS && "Illegal register index given");

    uint32_t len = 3;
    char* buff = new char[len];

    buff[0] = 0xc1;
    buff[1] = 0xe0 + reg;
    buff[2] = imm;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateCompareImmReg(uint64_t imm, uint8_t reg){
    ASSERT(reg < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 7;
    char* buff = new char[len];

    if (reg < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0x81;
    buff[2] = 0xf8 + (reg % X86_32BIT_GPRS);

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for imm");
    memcpy(buff+3, &imm32, sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateCompareImmReg(uint64_t imm, uint8_t reg){
    ASSERT(reg < X86_32BIT_GPRS && "Illegal register index given");

    uint32_t len = 6;
    char* buff = new char[len];

    buff[0] = 0x81;
    buff[1] = 0xf8 + reg;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for imm");
    memcpy(buff+2, &imm32, sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateBranchJL(uint64_t off){
    uint32_t len = 6;
    char* buff = new char[len];

    buff[0] = 0x0f;
    buff[1] = 0x8c;

    uint32_t off32 = (uint32_t)off;
    ASSERT(off32 == off && "Cannot use more than 32 bits for off");
    memcpy(buff+2, &off32, sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateMoveRegToReg(uint32_t srcreg, uint32_t destreg){
    ASSERT(srcreg < X86_64BIT_GPRS && "Illegal register index given");    
    ASSERT(destreg < X86_64BIT_GPRS && "Illegal register index given");    

    uint32_t len = 3;
    char* buff = new char[len];

    if (srcreg < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }

    if (destreg < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x89;
    buff[2] = 0xc0 + 8*(srcreg % X86_32BIT_GPRS) + (destreg % X86_32BIT_GPRS);

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateMoveRegToReg(uint32_t srcreg, uint32_t destreg){
    ASSERT(srcreg < X86_32BIT_GPRS && "Illegal register index given");    
    ASSERT(destreg < X86_32BIT_GPRS && "Illegal register index given");    

    uint32_t len = 2;
    char* buff = new char[len];

    buff[0] = 0x89;
    buff[1] = 0xc0 + X86_32BIT_GPRS*srcreg + destreg;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateLoadRipImmToReg(uint32_t imm, uint32_t destreg){
    ASSERT(destreg < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 7;
    char* buff = new char[len];

    if (destreg < X86_32BIT_GPRS){
        buff[0] = 0x48;
        buff[2] = 0x05 + 8*destreg;
    } else {
        buff[0] = 0x4c;
        buff[2] = 0x05 + 8*(destreg-X86_32BIT_GPRS);
    }
    buff[1] = 0x8d;

    memcpy(buff+3,&imm,sizeof(uint32_t));
    return generateInstructionBase(len,buff);
}


InstrucX86* InstrucX86Generator32::generateRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg){
    ASSERT(srcdestreg < X86_32BIT_GPRS && "Illegal register index given");    
    ASSERT(srcreg < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x01;
    buff[1] = 0xc0 + 8*srcdestreg + srcreg;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg){
    ASSERT(srcdestreg < X86_64BIT_GPRS && "Illegal register index given");    
    ASSERT(srcreg < X86_64BIT_GPRS && "Illegal register index given");    

    uint32_t len = 3;
    char* buff = new char[len];

    if (srcdestreg < X86_32BIT_GPRS){
        buff[0] = 0x48;
        buff[2] = 0xc0 + 8*srcdestreg + (srcreg % X86_32BIT_GPRS);
    } else {
        buff[0] = 0x4c;
        buff[2] = 0xc0 + 8*(srcdestreg-X86_32BIT_GPRS) + (srcreg % X86_32BIT_GPRS);
    }
    if (srcreg < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x01;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateRegImm1ByteMultReg(uint32_t src, uint8_t imm, uint32_t dest){
    ASSERT(src < X86_32BIT_GPRS && "Illegal register index given");    
    ASSERT(dest < X86_32BIT_GPRS && "Illegal register index given");    

    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x6b;
    buff[1] = 0xc0 + 8*dest + src;
    buff[2] = imm;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateRegImmMultReg(uint32_t src, uint32_t imm, uint32_t dest){
    ASSERT(src < X86_64BIT_GPRS && "Illegal register index given");    
    ASSERT(dest < X86_64BIT_GPRS && "Illegal register index given");    

    ASSERT(src == dest);

    uint32_t len = 7;
    char* buff = new char[len];
    if (dest < X86_32BIT_GPRS){
        buff[0] = 0x48;
        buff[2] = 0xc0 + 8*dest + (src % X86_32BIT_GPRS);
    } else {
        buff[0] = 0x4c;
        buff[2] = 0xc0 + 8*(dest-X86_32BIT_GPRS) + (src % X86_32BIT_GPRS);
    }

    if (src < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x69;
    memcpy(buff+3,&imm,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateRegAddImm(uint8_t idx, uint32_t imm){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x81;
    buff[1] = 0xc0 + (char)idx;
    memcpy(buff+2, &imm, sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateXorRegReg(uint8_t src, uint8_t tgt){
    ASSERT(src < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(tgt < X86_32BIT_GPRS && "Illegal register index given");
    
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x48;
    buff[1] = 0x31;
    buff[2] = 0xc0 + tgt + 8*src;
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateLoadAHFromFlags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9f;
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateStoreAHToFlags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9e;
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateAddImmByteToMem(uint8_t imm, uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x83;
    buff[1] = 0x05;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+2,&addr32,sizeof(uint32_t));
    memcpy(buff+6,&imm,sizeof(uint8_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateAddImmByteToMem(uint8_t imm, uint64_t addr){
    uint32_t len = 8;
    char* buff = new char[len];

    buff[0] = 0x83;
    buff[1] = 0x04;
    buff[2] = 0x25;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+3,&addr32,sizeof(uint32_t));
    memcpy(buff+7,&imm,sizeof(uint8_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateAndImmReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    uint32_t immoff = 2;
    if (idx == X86_REG_AX){
        len--;
        immoff--;
    }

    char* buff = new char[len];

    // set opcode
    buff[0] = 0x81;
    buff[1] = 0xe0 + (char)(idx);
    if (idx == X86_REG_AX){
        buff[0] = 0x25;
    }

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");

    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff){
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

InstrucX86* InstrucX86Generator64::generateMoveRegaddrImmToReg(uint32_t idxsrc, uint64_t imm, uint32_t idxdest){
    ASSERT(idxsrc < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_64BIT_GPRS && "Illegal register index given");    
    uint32_t len = 7;
    uint32_t immoff = 3;

    if (idxsrc % X86_32BIT_GPRS == X86_REG_SP){
        len++;
        immoff++;
    }    
    char* buff = new char[len];

    // set opcode
    if (idxsrc < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }
    if (idxdest < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x8b;
    buff[2] = 0x80 + (char)(idxdest*8) + (char)(idxsrc);
    buff[3] = 0x24;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateMoveRegaddrImmToReg(uint32_t idxsrc, uint64_t imm, uint32_t idxdest){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 6;
    if (idxsrc == X86_REG_SP){
        len++;
    }    
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x8b;
    buff[1] = 0x80 + (char)(idxdest*8) + (char)(idxsrc);
    uint32_t imm32 = (uint32_t)imm;
    uint32_t immoff = 2;
    if (idxsrc == X86_REG_SP){
        buff[2] = 0x24;
        immoff++;
    }
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateMoveImmByteToReg(uint8_t imm, uint32_t idx){
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

InstrucX86* InstrucX86Generator::generateRegIncrement(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x40 + idx;
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateInterrupt(uint8_t idx){
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

InstrucX86* InstrucX86Generator::generateSetDirectionFlag(bool backward){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xfc;
    if (backward){
        buff[0]++;
    }    

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateSTOSByte(bool repeat){
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0xf3;
    buff[len-1] = 0xaa;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateSTOSWord(bool repeat){
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0xf3;
    buff[len-1] = 0xab;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateMoveString(bool repeat){
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0xf3;
    buff[len-1] = 0xa5;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateMoveImmToSegmentReg(uint64_t imm, uint32_t idx){
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


InstrucX86* InstrucX86Generator::generatePushSegmentReg(uint32_t idx){
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

InstrucX86* InstrucX86Generator::generatePopSegmentReg(uint32_t idx){
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


InstrucX86* InstrucX86Generator64::generateStackPush4Byte(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x66; 
    buff[1] = 0x50 + idx;
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateAddByteToRegaddr(uint8_t byt, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x83; 
    buff[1] = 0x00 + idx;
    buff[2] = byt;
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx){
    ASSERT(srcidx < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x8b; 
    buff[1] = 0x00 + (char)(srcidx) + (char)(8*destidx);
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx){
    ASSERT(srcidx < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x67;
    buff[1] = 0x8b; 
    buff[2] = 0x00 + (char)(srcidx) + (char)(8*destidx);
    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateRegSubImm(uint8_t idx, uint32_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");

    if (!imm){
        return InstrucX86Generator64::generateNoop();
    } else {
        return InstrucX86Generator64::generateRegSubImm4Byte(idx, imm);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

InstrucX86* InstrucX86Generator64::generateRegSubImm4Byte(uint8_t idx, uint32_t imm){
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



InstrucX86* InstrucX86Generator64::generateRegAddImm(uint8_t idx, uint32_t imm){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");

    if (!imm){
        return InstrucX86Generator64::generateNoop();
    } else {
        return InstrucX86Generator64::generateRegAddImm4Byte(idx, imm);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

InstrucX86* InstrucX86Generator64::generateRegAddImm4Byte(uint8_t idx, uint32_t imm){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 7;
    uint32_t immoff = 3;
    if (idx == X86_REG_AX){
        len--;
        immoff--;
    }
    char* buff = new char[len];
    if (idx < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }

    buff[1] = 0x81;
    buff[2] = 0xc0 + (char)(idx % X86_32BIT_GPRS);

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateRegSubImm(uint8_t idx, uint32_t imm){
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

InstrucX86* InstrucX86Generator::generateMoveImmByteToMemIndirect(uint8_t byt, uint64_t off, uint32_t idx){
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

InstrucX86* InstrucX86Generator::generateReturn(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xc3;

    return generateInstructionBase(len,buff);
}


InstrucX86* InstrucX86Generator64::generateStackPush(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    if (idx < X86_32BIT_GPRS){
        return InstrucX86Generator32::generateStackPush(idx);
    }
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x41;
    buff[1] = 0x48 + (char)idx;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateStackPop(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    if (idx < X86_32BIT_GPRS){
        return InstrucX86Generator32::generateStackPop(idx);
    }
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x41;
    buff[1] = 0x50 + (char)idx;

    return generateInstructionBase(len,buff);
}


InstrucX86* InstrucX86Generator::generatePushEflags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9c;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generatePopEflags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9d;

    return generateInstructionBase(len,buff);
}


InstrucX86* InstrucX86Generator::generateNoop(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x90;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateMoveRegToRegaddrImm(uint32_t idxsrc, uint32_t idxdest, uint64_t imm, bool source64Bit){
    ASSERT(idxsrc < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_64BIT_GPRS && "Illegal register index given");    

    uint32_t len = 7;
    uint32_t immoff = 3;

    if (idxdest % X86_32BIT_GPRS == X86_REG_SP){
        len++;
        immoff++;
    }

    char* buff = new char[len];

    if (idxsrc < X86_32BIT_GPRS){
        buff[0] = 0x40;
    } else {
        buff[0] = 0x44;
    }
    if (source64Bit){
        buff[0] |= 0x08;
    }
    if (idxdest < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x89;
    buff[2] = 0x80 + 8*(idxsrc % X86_32BIT_GPRS) + (idxdest % X86_32BIT_GPRS);
    buff[3] = 0x24;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}


InstrucX86* InstrucX86Generator64::generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    if (idxdest == X86_REG_BP){
        return InstrucX86Generator64::generateMoveRegToRegaddrImm1Byte(idxsrc,idxdest,0);
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

InstrucX86* InstrucX86Generator64::generateMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
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

InstrucX86* InstrucX86Generator64::generateMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
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

InstrucX86* InstrucX86Generator32::generateMoveRegToRegaddrImm(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 6;
    if (idxdest == X86_REG_SP){
        len++;
    }    
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x80 + (char)(idxsrc*8) + (char)(idxdest);
    uint32_t imm32 = (uint32_t)imm;
    uint32_t immoff = 2;
    if (idxdest == X86_REG_SP){
        buff[2] = 0x24;
        immoff++;
    }
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest){
    ASSERT(idxsrc < 8 && "Illegal register index given");
    ASSERT(idxdest < 8 && "Illegal register index given");    
    uint32_t len = 2;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x00 + (char)(idxsrc*8) + (char)(idxdest);

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateIndirectRelativeJump(uint64_t addr, uint64_t tgt){
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

InstrucX86* InstrucX86Generator64::generateMoveImmToReg(uint64_t imm, uint32_t idx){
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
    buff[2] = 0xc0 + (char)(idx % X86_32BIT_GPRS);    

    // set target address
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+3,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator::generateMoveImmToReg(uint64_t imm, uint32_t idx){
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

InstrucX86* InstrucX86Generator32::generateMoveRegToMem(uint32_t idx, uint64_t addr){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    uint32_t immoff = 2;
    if (idx == X86_REG_AX){
        len--;
        immoff--;
    }

    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x05 + 0x8*(char)idx;
    if (idx == X86_REG_AX){
        buff[0] = 0xa3;
    }

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+immoff,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateMoveRegToMem(uint32_t idx, uint64_t addr){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 8;
    char* buff = new char[len];

    // set opcode
    if (idx < X86_32BIT_GPRS){
        buff[0] = 0x48;
        buff[2] = 0x04 + 0x8*(char)idx;
    } else {
        buff[0] = 0x4c;
        buff[2] = 0x04 + 0x8*((char)(idx-X86_32BIT_GPRS));
    }
    buff[1] = 0x89;
    buff[3] = 0x25;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+4,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateMoveMemToReg(uint64_t addr, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    uint32_t immoff = 2;
    if (idx == X86_REG_AX){
        len--;
        immoff--;
    }

    char* buff = new char[len];

    // set opcode
    buff[0] = 0x8b;
    buff[1] = 0x05 + 0x8*(char)idx;
    if(idx == X86_REG_AX){
        buff[0] = 0xa1;
    }

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+immoff,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator64::generateMoveMemToReg(uint64_t addr, uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 8;
    char* buff = new char[len];

    // set opcode
    if (idx < X86_32BIT_GPRS){
        buff[0] = 0x48;
        buff[2] = 0x04 + 0x8*(char)idx;
    } else {
        buff[0] = 0x4c;
        buff[2] = 0x04 + 0x8*((char)(idx-X86_32BIT_GPRS));
    }
    buff[1] = 0x8b;
    buff[3] = 0x25;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff+4,&addr32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateStackPush(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x50 + (char)idx;

    return generateInstructionBase(len,buff);
}

InstrucX86* InstrucX86Generator32::generateStackPop(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x58 + (char)idx;

    return generateInstructionBase(len,buff);
}


InstrucX86* InstrucX86Generator::generateCallRelative(uint64_t addr, uint64_t tgt){
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

InstrucX86* InstrucX86Generator32::generateJumpIndirect(uint64_t tgt){
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

InstrucX86* InstrucX86Generator::generateJumpRelative(uint64_t addr, uint64_t tgt){
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

InstrucX86* InstrucX86Generator::generateStackPushImm(uint64_t imm){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x68;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return generateInstructionBase(len,buff);
}

