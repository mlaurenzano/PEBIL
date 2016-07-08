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

#include <Base.h>

#include <BinaryFile.h>
#include <FileHeader.h>
#include <X86InstructionFactory.h>

X86Instruction* X86InstructionFactory::assemble(const char* buf, bool is64){
    char* name = NULL;
    char ascmd[__MAX_STRING_SIZE];
    FILE* o = GetTempFile(&name);
    fclose(o);

    ASSERT(name);

    // build an assembler command to run
    sprintf(ascmd, "/usr/bin/as --%d -o %s -- << EOF\n%s\nEOF\n", (is64? 64:32), name, buf);
    //PRINT_INFOR("%s", ascmd);

    // run disassembly command
    if (system(ascmd)){
        PRINT_ERROR("Assembler command failed attempting to disassemble the following: %s", buf);
    }

    // read disassembled results into a file
    BinaryInputFile* b = new BinaryInputFile();
    b->readFileInMemory(name, false);

    // get to the instruction bytes
    FileHeader* f;
    if (is64){
        f = new FileHeader64();
    } else {
        f = new FileHeader32();
    }
    ASSERT(f);

    f->read(b);
    uint64_t toffset = f->GetTextEntryOffset();

    // create X86Instructions from assembled machine code
    b->setInBufferPointer(toffset);
    X86Instruction* x = X86Instruction::disassemble(b->moreBytes());
    if (x == NULL){
        PRINT_ERROR("Error disassembling bytes for assembled instruction: %s", buf);
    }

    delete b;
    delete f;

    unlink(name);
    delete[] name;

    return x;
}

X86Instruction* X86InstructionFactory32::assemble(const char* buf){
    return X86InstructionFactory::assemble(buf, false);
}

X86Instruction* X86InstructionFactory64::assemble(const char* buf){
    return X86InstructionFactory::assemble(buf, true);    
}

X86Instruction* X86InstructionFactory64::emitExchangeAdd(uint8_t src, uint8_t dest, bool lock){
    ASSERT(src < X86_64BIT_GPRS);
    ASSERT(dest < X86_64BIT_GPRS);

    uint32_t len = 4;
    uint32_t st = 0;
    if (lock){
        len++;
        st++;
    }
    if (dest % X86_32BIT_GPRS == X86_REG_SP){
        len++;
    }
    char* buff = new char[len];

    buff[0] = 0xf0;
    buff[st + 0] = 0x48;
    if (src >= X86_32BIT_GPRS){
        buff[st + 0] += 0x04;
    }
    if (dest >= X86_32BIT_GPRS){
        buff[st + 0] += 0x01;
    }
    buff[st + 1] = 0x0f;
    buff[st + 2] = 0xc1;
    buff[st + 3] = 0x00 + (dest % X86_32BIT_GPRS) + (8 * (src % X86_32BIT_GPRS));
    if (dest % X86_32BIT_GPRS == X86_REG_SP){
        buff[st + 4] = 0x24;
    }

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitAddImmToRegaddrImm(uint32_t b, uint8_t reg, uint32_t imm){
    ASSERT(reg < X86_64BIT_GPRS);

    uint32_t len = 10;
    uint32_t immoff = 2;
    uint32_t st = 0;
    if (reg >= X86_32BIT_GPRS){
        len++;
        immoff++;
        st++;
    }
    if (reg % X86_32BIT_GPRS == X86_REG_SP){
        len++;
        immoff++;
    }
    char* buff = new char[len];

    buff[0] = 0x41;
    buff[st] = 0x81;
    buff[st + 1] = 0x80 + (reg % X86_32BIT_GPRS);
    buff[st + 2] = 0x24;

    memcpy(buff+immoff,&imm,sizeof(uint32_t));
    memcpy(buff+immoff+sizeof(uint32_t),&b,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitAddImmByteToRegaddrImm(uint8_t byte, uint8_t reg, uint32_t imm){
    ASSERT(reg < X86_64BIT_GPRS);

    uint32_t len = 7;
    uint32_t immoff = 2;
    uint32_t st = 0;
    if (reg >= X86_32BIT_GPRS){
        len++;
        immoff++;
        st++;
    }
    if (reg % X86_32BIT_GPRS == X86_REG_SP){
        len++;
        immoff++;
    }
    char* buff = new char[len];

    buff[0] = 0x41;
    buff[st] = 0x83;
    buff[st + 1] = 0x80 + (reg % X86_32BIT_GPRS);
    buff[st + 2] = 0x24;

    memcpy(buff+immoff,&imm,sizeof(uint32_t));
    memcpy(buff+immoff+sizeof(uint32_t),&byte,sizeof(uint8_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitImmAndReg(uint32_t imm, uint8_t dest){
    ASSERT(dest < X86_64BIT_GPRS);
    
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x48;
    if (dest >= X86_32BIT_GPRS){
        buff[0] += 0x01;
    }
    buff[1] = 0x81;
    buff[2] = 0xe0 + (0x01 * (dest % X86_32BIT_GPRS));
        
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}
X86Instruction* X86InstructionFactory64::emitImmOrReg(uint32_t imm, uint8_t dest){
    ASSERT(dest < X86_64BIT_GPRS);
    
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x48;
    if (dest >= X86_32BIT_GPRS){
        buff[0] += 0x01;
    }
    buff[1] = 0x81;
    buff[2] = 0xc8 + (0x01 * (dest % X86_32BIT_GPRS));
        
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitMoveTLSOffsetToReg(uint32_t imm, uint8_t dest){
    ASSERT(dest < X86_64BIT_GPRS);
    
    uint32_t len = 9;
    char* buff = new char[len];

    buff[0] = 0x64;
    buff[1] = 0x48;
    if (dest >= X86_32BIT_GPRS){
        buff[1] += 0x04;
    }
    buff[2] = 0x8b;
    buff[3] = 0x04 + (0x08 * (dest % X86_32BIT_GPRS));
    buff[4] = 0x25;
        
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the address");
    memcpy(buff+5,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitMoveThreadIdToReg(uint8_t dest){
    return emitMoveTLSOffsetToReg(0x10, dest);
}

X86Instruction* X86InstructionFactory32::emitMoveImmToMem(uint64_t imm, uint64_t addr){
    uint32_t len = 10;
    char* buff = new char[len];

    buff[0] = 0xc7;
    buff[1] = 0x05;
    
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+2,&addr32,sizeof(uint32_t));

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the address");
    memcpy(buff+6,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitMoveImmToMem(uint64_t imm, uint64_t addr){
    uint32_t len = 11;
    char* buff = new char[len];

    buff[0] = 0xc7;
    buff[1] = 0x04;
    buff[2] = 0x25;
    
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&addr32,sizeof(uint32_t));

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the address");
    memcpy(buff+7,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

// kmov %kreg, %gpr
X86Instruction* X86InstructionFactory64::emitMoveKToReg(uint32_t kreg_in, uint32_t gpr_in)
{
    assert(gpr_in >= X86_REG_AX && gpr_in <= X86_REG_R15);
    assert(kreg_in >= X86_REG_K0 && kreg_in <= X86_REG_K7);
    uint8_t gpr = gpr_in - X86_REG_AX;
    uint8_t kreg = kreg_in - X86_REG_K0;

    uint8_t len = 4;
    char* buff = new char[len];

    uint8_t mod = 0xc0;
    uint8_t reg = gpr << 3;
    uint8_t rm = kreg;
    uint8_t modrm = mod | reg | rm;

    buff[0] = 0xc5; // prefix
    buff[1] = 0xf8; // R vvvv L pp
    buff[2] = 0x93; // opcode
    buff[3] = modrm;

    return emitInstructionBase(len, buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToK(uint32_t gpr_in, uint32_t kreg_in)
{
    assert(gpr_in >= X86_REG_AX && gpr_in <= X86_REG_R15);
    assert(kreg_in >= X86_REG_K0 && kreg_in <= X86_REG_K7);
    uint8_t gpr = gpr_in - X86_REG_AX;
    uint8_t kreg = kreg_in - X86_REG_K0;

    uint8_t len = 4;
    char* buff = new char[len];

    uint8_t mod = 0xc0;
    uint8_t reg = kreg << 3;
    uint8_t rm = gpr;
    uint8_t modrm = mod | reg | rm;

    buff[0] = 0xc5;
    buff[1] = 0xf8;
    buff[2] = 0x92;
    buff[3] = modrm;

    return emitInstructionBase(len, buff);
}

/*
 * vmovapd (rsp+disp), zmmx
 */
X86Instruction* X86InstructionFactory64::emitMoveAlignedStackToZmmx(uint8_t zmm, uint8_t disp)
{
    uint32_t len = 8;
    char* buff = new char[len];

    uint8_t R = (~zmm & 0x10) << 3;
    uint8_t XB = 0x60;
    uint8_t r = (~zmm & 0x08) << 1;
    uint8_t mmmm = 1;
    uint8_t RXBrmmmm =  R | XB | r | mmmm;

    uint8_t mod = 1 << 6;
    uint8_t reg = (zmm & 0x07) << 3;
    uint8_t rm = 1 << 2;
    uint8_t modrm = mod | reg | rm;

    buff[0] = 0x62;
    buff[1] = RXBrmmmm;
    buff[2] = 0xf9;
    buff[3] = 0x08;
    buff[4] = 0x28;
    buff[5] = modrm;
    buff[6] = 0x24;
    buff[7] = disp;

    if(disp == 0 && zmm == 0) {
        buff[1] = 0xf1;
        buff[5] = 0x04;
        len = 7;
    }
    return emitInstructionBase(len, buff);
}

/*
 * vmovnrngoapd zmmx, (rsp+disp)
 */
// | 62 |R X B R' mmmm|W vvvv 0  pp |E SSS  v' aaa |
X86Instruction* X86InstructionFactory64::emitMoveZmmxToAlignedStack(uint8_t zmm, uint8_t disp)
{
    uint32_t len = 8;
    char* buff = new char[len];

    // mmmm = 0001
    // reg is encoded in R:r:reg
    //
    //
    // For (rsp+disp)
    // mod = 01
    // ~X:~B:rm  = 00100
    //
    uint8_t R = (~zmm & 0x10) << 3;
    uint8_t XB = 0x60;
    uint8_t r = (~zmm & 0x08) << 1;
    uint8_t mmmm = 1;
    uint8_t RXBrmmmm =  R | XB | r | mmmm;

    uint8_t mod = 1 << 6;
    uint8_t reg = (zmm & 0x07) << 3;
    uint8_t rm = 1 << 2;
    uint8_t modrm = mod | reg | rm;

    buff[0] = 0x62;
    buff[1] = RXBrmmmm;
    buff[2] = 0xfa;
    buff[3] = 0x88;
    buff[4] = 0x29;
    buff[5] = modrm;
    buff[6] = 0x24;
    buff[7] = disp;

    if(disp == 0 && zmm == 0) {
        buff[1] = 0xf1;
        buff[5] = 0x04;
        len = 7;
    }
    return emitInstructionBase(len, buff);
}


// vector store zmm1, mem {k}
//
// vpackstoreld zmm1, (addr) {k}
// vpackstorehd zmm1, (addr+64) {k}
Vector<X86Instruction*>* X86InstructionFactory64::emitUnalignedPackstoreRegaddrImm(
        uint32_t zmm_in,
        uint32_t kreg_in,
        uint32_t base_in,
        uint32_t disp)
{
    assert(zmm_in >= X86_FPREG_ZMM0 && zmm_in <= X86_FPREG_ZMM31);
    assert(kreg_in >= X86_REG_K0 && kreg_in <= X86_REG_K7);
    assert(base_in >= X86_REG_AX && base_in <= X86_REG_R15);

    uint8_t zmm = zmm_in - X86_FPREG_ZMM0;
    uint8_t kreg = kreg_in - X86_REG_K0;
    uint8_t base = base_in - X86_REG_AX;
     
    // mmmm = 0010
    // zmm is encoded in R:r:reg
    // kreg is encoded in aaa
    //
    // addressing mode is [base]+disp32
    //   mod = 10
    // base is encoded in ~X:~B:rm

    uint8_t R = (~zmm & 0x10) << 3;
    uint8_t XB = 0x60;
    uint8_t r = (~zmm & 0x08) << 1;
    uint8_t mmmm = 1 << 1;
    uint8_t RXBrmmmm =  R | XB | r | mmmm;

    uint8_t mod = 1 << 7;
    uint8_t reg = (zmm & 0x07) << 3;
    uint8_t rm = base;
    uint8_t modrm = mod | reg | rm;

    uint8_t W = 0;
    uint8_t vvvv = 0xf << 3;
    uint8_t pp = 1;
    uint8_t Wvvvvpp = W | vvvv | pp;

    uint8_t E = 0;
    uint8_t SSS = 0;
    uint8_t vp = 1 << 3;
    uint8_t ESSSvpaaa = E | SSS | vp | kreg;

    uint8_t opcode = 0xD0;
    int len = 10;
    char* buff = new char[len];
    buff[0] = 0x62;
    buff[1] = RXBrmmmm;
    buff[2] = Wvvvvpp;
    buff[3] = ESSSvpaaa;
    buff[4] = opcode;
    buff[5] = modrm;
    memcpy(buff+6, &disp, sizeof(disp));

    char* buff2 = new char[len];
    memcpy(buff2, buff, len);
    opcode = 0xD4;
    buff2[4] = opcode;
    disp += 64;
    memcpy(buff2+6, &disp, sizeof(disp));

    Vector<X86Instruction*>* retval = new Vector<X86Instruction*>();
    retval->append(emitInstructionBase(len, buff));
    retval->append(emitInstructionBase(len, buff2));
    return retval;
}
/*
 * vmovdqa32 zmm1, mem {k}
 */
// | 62 |R X B R' mmmm|W vvvv 0  pp |E SSS  v' aaa |
X86Instruction* X86InstructionFactory64::emitMoveZmmToAlignedRegaddrImm(
        uint32_t zmm_in,
        uint32_t kreg_in,
        uint32_t base_in,
        uint32_t disp)
{
    assert(zmm_in >= X86_FPREG_ZMM0 && zmm_in <= X86_FPREG_ZMM31);
    assert(kreg_in >= X86_REG_K0 && kreg_in <= X86_REG_K7);
    assert(base_in >= X86_REG_AX && base_in <= X86_REG_R15);

    uint8_t zmm = zmm_in - X86_FPREG_ZMM0;
    uint8_t kreg = kreg_in - X86_REG_K0;
    uint8_t base = base_in - X86_REG_AX;

    uint32_t len = 10;
    char* buff = new char[len];

    // mmmm = 0001
    // zmm is encoded in R:r:reg
    // kreg is encoded in aaa
    //
    // addressing mode is [base]+disp32
    //   mod = 10
    // base is encoded in ~X:~B:rm

    uint8_t R = (~zmm & 0x10) << 3;
    uint8_t XB = 0x60;
    uint8_t r = (~zmm & 0x08) << 1;
    uint8_t mmmm = 1;
    uint8_t RXBrmmmm =  R | XB | r | mmmm;

    uint8_t mod = 1 << 7;
    uint8_t reg = (zmm & 0x07) << 3;
    uint8_t rm = base;
    uint8_t modrm = mod | reg | rm;

    buff[0] = 0x62;
    buff[1] = RXBrmmmm;
    buff[2] = 0x79;        // 0 1111 0 01
    buff[3] = 0x08 | kreg; // 0 000 1 kreg

    buff[4] = 0x7F;        // opcode

    buff[5] = modrm;
    memcpy(buff+6, &disp, sizeof(disp));

    return emitInstructionBase(len, buff);
}
X86Instruction* X86InstructionFactory64::emitFxSave(uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x0f;
    buff[1] = 0xae;
    buff[2] = 0x05;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&addr32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitFxRstor(uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x0f;
    buff[1] = 0xae;
    buff[2] = 0x0d;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&addr32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitFxSaveReg(uint8_t reg){
    ASSERT(reg < X86_64BIT_GPRS);
    uint32_t len = 3;
    uint32_t off = 0;

    if (reg >= X86_32BIT_GPRS){
        len++;
        off++;
    }
    if (reg % X86_32BIT_GPRS == X86_REG_SP){
        len++;
    }

    char* buff = new char[len];

    if (reg >= X86_32BIT_GPRS){
        buff[0] = 0x41;
    }
    buff[off] = 0x0f;
    buff[off+1] = 0xae;
    buff[off+2] = 0x00 + (reg % X86_32BIT_GPRS);

    if (reg % X86_32BIT_GPRS == X86_REG_SP){
        buff[off+3] = 0x24;
    }

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitFxRstorReg(uint8_t reg){
    ASSERT(reg < X86_64BIT_GPRS);
    uint32_t len = 3;
    uint32_t off = 0;

    if (reg >= X86_32BIT_GPRS){
        len++;
        off++;
    }
    if (reg % X86_32BIT_GPRS == X86_REG_SP){
        len++;
    }

    char* buff = new char[len];

    if (reg >= X86_32BIT_GPRS){
        buff[0] = 0x41;
    }
    buff[off] = 0x0f;
    buff[off+1] = 0xae;
    buff[off+2] = 0x08 + (reg % X86_32BIT_GPRS);

    if (reg % X86_32BIT_GPRS == X86_REG_SP){
        buff[off+3] = 0x24;
    }

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory32::emitFxSave(uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x0f;
    buff[1] = 0xae;
    buff[2] = 0x05;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&addr32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory32::emitFxRstor(uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x0f;
    buff[1] = 0xae;
    buff[2] = 0x0d;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+3,&addr32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitMoveSegmentRegToReg(uint32_t src, uint32_t dest){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitMoveSegmentRegToReg(uint32_t src, uint32_t dest){
    ASSERT(src < X86_SEGMENT_REGS);
    ASSERT(dest < X86_32BIT_GPRS);

    uint32_t len = 2;
    char* buff = new char[len];

    buff[0] = 0x8c;
    buff[1] = 0xc0 + 8*src + dest;

    return emitInstructionBase(len,buff);
}

Vector<X86Instruction*>* X86InstructionFactory64::emitAddressComputation(X86Instruction* instruction, uint32_t dest){
    ASSERT(dest < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(instruction->isMemoryOperation());

    DEBUG_LOADADDR(
    instruction->print();
    PRINT_DEBUG_LOADADDR("---");
                   )

    Vector<X86Instruction*>* compInstructions = new Vector<X86Instruction*>();
    OperandX86* op = NULL;

    if (instruction->isExplicitMemoryOperation() || instruction->isSoftwarePrefetch()){

        op = instruction->getMemoryOperand();

        if (instruction->GET(pfx_seg)){
            uint32_t segIdx = instruction->GET(pfx_seg) - UD_R_ES;
            uint64_t imm = op->getValue();
            // FIXME ignores offsets
            (*compInstructions).append(emitMoveSegmentRegToReg(segIdx, dest));
        } else if (op->GET(base) == UD_R_RIP){
            PRINT_DEBUG_LOADADDR("making lea: mov rip imm");
            uint64_t addr = op->getInstruction()->getProgramAddress() + op->getValue() + op->getInstruction()->getSizeInBytes();
            (*compInstructions).append(emitMoveImmToReg(addr, dest));
        } else {
            (*compInstructions).append(emitLoadEffectiveAddress(op, dest));
            ASSERT((*compInstructions).back() && op && (*compInstructions).back()->getOperand(SRC1_OPERAND));
            ASSERT(op->isSameOperand((*compInstructions).back()->getOperand(SRC1_OPERAND)) && "The emitd Address Computation operand does not match the operand given");
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
        (*compInstructions).append(emitLoadEffectiveAddress(X86_REG_SP, 0, 1, stackOffset, dest, true, false));
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

Vector<X86Instruction*>* X86InstructionFactory32::emitAddressComputation(X86Instruction* instruction, uint32_t dest){
    ASSERT(dest < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(instruction->isMemoryOperation());

    DEBUG_LOADADDR(
    instruction->print();
    PRINT_DEBUG_LOADADDR("---");
                   )

    Vector<X86Instruction*>* compInstructions = new Vector<X86Instruction*>();
    OperandX86* op = NULL;

    if (instruction->isExplicitMemoryOperation()){

        op = instruction->getMemoryOperand();

        if (instruction->GET(pfx_seg)){
            uint32_t segIdx = instruction->GET(pfx_seg) - UD_R_ES;
            uint64_t imm = op->getValue();

            (*compInstructions).append(emitMoveSegmentRegToReg(segIdx, dest));
        } else if (op->GET(base) == UD_R_RIP){
            __SHOULD_NOT_ARRIVE;
            PRINT_DEBUG_LOADADDR("making lea: mov rip imm");
            uint64_t addr = op->getInstruction()->getProgramAddress() + op->getValue() + op->getInstruction()->getSizeInBytes();
            (*compInstructions).append(emitMoveImmToReg(addr, dest));
        } else {
            (*compInstructions).append(emitLoadEffectiveAddress(op, dest));
            ASSERT((*compInstructions).back() && op && (*compInstructions).back()->getOperand(SRC1_OPERAND));
            ASSERT(op->isSameOperand((*compInstructions).back()->getOperand(SRC1_OPERAND)) && "The emitd Address Computation operand does not match the operand given");
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
        (*compInstructions).append(emitLoadEffectiveAddress(X86_REG_SP, 0, 1, stackOffset, dest, true, false));
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

X86Instruction* X86InstructionFactory64::emitLoadEffectiveAddress(uint32_t baseReg, uint32_t indexReg, uint8_t scale, uint64_t value, uint32_t dest, bool hasBase, bool hasIndex){
    ASSERT(dest < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(baseReg < X86_64BIT_GPRS || baseReg + UD_R_RAX == UD_R_RIP);
    ASSERT(indexReg < X86_64BIT_GPRS && "Illegal register index given");
    if (scale){
        ASSERT(isPowerOfTwo(scale) && scale <= 8);
    }
    uint32_t len;
    char* buff;
    X86Instruction* lea = NULL;

    // scale/index/base
    if (hasIndex){
        uint8_t sa = 0;
        if (scale){
            sa = (logBase2(scale)) << 6;
        }

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
            
            lea = emitInstructionBase(len,buff);
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
            lea = emitInstructionBase(len,buff);
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
        lea = emitInstructionBase(len,buff);
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
        lea = emitInstructionBase(len,buff);
    }

    ASSERT(lea);
    return lea;
}

X86Instruction* X86InstructionFactory32::emitLoadEffectiveAddress(uint32_t baseReg, uint32_t indexReg, uint8_t scale, uint64_t value, uint32_t dest, bool hasBase, bool hasIndex){
    ASSERT(dest < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(baseReg < X86_32BIT_GPRS);
    ASSERT(indexReg < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(scale && isPowerOfTwo(scale) && scale <= 8);

    uint32_t len;
    char* buff;
    X86Instruction* lea = NULL;

    // scale/index/base
    if (hasIndex){
        uint8_t sa = (logBase2(scale)) << 6;

        // base is a reg
        if (hasBase && !value){
            PRINT_DEBUG_LOADADDR("making lea: SIB, base is only reg");

            len = 3;
            if (baseReg == X86_REG_BP){
                len++;
            }
            buff = new char[len];
            
            buff[0] = 0x8d;
            buff[1] = 0x04 + 8*dest;
            if (baseReg == X86_REG_BP){
                buff[1] += 0x40;
                buff[3] = 0x00;
            }
            buff[2] = 0x00 + 8*indexReg + baseReg + sa;
            
            lea = emitInstructionBase(len,buff);
        }

        // base is a value
        else {
            PRINT_DEBUG_LOADADDR("making lea: SIB, base is value");

            len = 7;
            buff = new char[len];

            buff[0] = 0x8d;
            buff[1] = 0x04 + 8 * dest;
            buff[2] = 0x00 + 8 * indexReg;
            if (hasBase){
                PRINT_DEBUG_LOADADDR("\tbase also has reg");
                buff[1] += 0x80;
                buff[2] += baseReg;
            } else {
                buff[2] += 0x05;
            }
            
            buff[2] += sa;
            
            memcpy(buff + 3, &value, sizeof(uint32_t));
            lea = emitInstructionBase(len,buff);
        } 

    } 

    // base + offset
    else if (hasBase){
        len = 6;
        uint32_t immoff = 2;
        PRINT_DEBUG_LOADADDR("making lea: base + offset");

        if (baseReg == X86_REG_SP){
            len++;
            immoff++;
        }
        buff = new char[len];

        buff[0] = 0x8d;
        buff[1] = 0x80 + 8*dest + baseReg;

        if (baseReg == X86_REG_SP){
            buff[2] = 0x24;
        }

        uint32_t addr32 = value;
        memcpy(buff + immoff, &addr32, sizeof(uint32_t));
        lea = emitInstructionBase(len,buff);
    } 

    // constant
    else {
        PRINT_DEBUG_LOADADDR("making lea: const");

        len = 7;
        buff = new char[len];
        buff[0] = 0x8d;
        buff[1] = 0x04 + 8 * (dest % X86_32BIT_GPRS);
        buff[2] = 0x25;

        uint32_t addr32 = value;
        memcpy(buff + 3, &addr32, sizeof(uint32_t));
        lea = emitInstructionBase(len,buff);
    }

    ASSERT(lea);
    return lea;
}

X86Instruction* X86InstructionFactory64::emitLoadEffectiveAddress(OperandX86* op, uint32_t dest){
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

    X86Instruction* lea = emitLoadEffectiveAddress(baseReg, indexReg, scale, value, dest, hasBase, hasIndex);
    if (!lea || !op || !lea->getOperand(SRC1_OPERAND)){
        fprintf(stderr, "emitLoadEffectiveAddress:\n");
        lea->print();
    }
    ASSERT(lea && op && lea->getOperand(SRC1_OPERAND));
    ASSERT(op->isSameOperand(lea->getOperand(SRC1_OPERAND)) && "The emitted LEA operand does not match the operand given");

    return lea;
}

X86Instruction* X86InstructionFactory32::emitLoadEffectiveAddress(OperandX86* op, uint32_t dest){
    ASSERT(dest < X86_32BIT_GPRS && "Illegal register index given");
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
        baseReg = op->GET(base) - UD_R_EAX;
    }
    bool hasIndex = false;
    if (op->GET(index)){
        hasIndex = true;
        indexReg = op->GET(index) - UD_R_EAX;
    }

    X86Instruction* lea = emitLoadEffectiveAddress(baseReg, indexReg, scale, value, dest, hasBase, hasIndex);
    if (!lea || !op || !lea->getOperand(SRC1_OPERAND)){
        fprintf(stderr, "emitLoadEffectiveAddress:\n");
        lea->print();
    }
    ASSERT(lea && op && lea->getOperand(SRC1_OPERAND));
    ASSERT(op->isSameOperand(lea->getOperand(SRC1_OPERAND)) && "The emitted LEA operand does not match the operand given");

    return lea;
}

// this function deletes the incoming buffer after copying it to the new instruction's local memory
X86Instruction* X86InstructionFactory32::emitInstructionBase(uint32_t sz, char* buff){
    return X86InstructionFactory::emitInstructionBase(sz, buff);
}

X86Instruction* X86InstructionFactory32::emitExchangeMemReg(uint64_t addr, uint8_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x87;
    buff[1] = 0x05 + 8 * idx;
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the address");
    memcpy(buff+2,&addr32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitLoadRegImmReg(uint8_t idxsrc, uint64_t imm, uint8_t idxdest){
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

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory64::emitLoadRegImmReg(uint8_t idxsrc, uint64_t imm, uint8_t idxdest){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitLoadRipImmReg(uint64_t imm, uint8_t idxdest){
    ASSERT(idxdest < X86_64BIT_GPRS && "Illegal register index given");    

    uint32_t len = 7;
    uint32_t immoff = 3;

    if (idxdest == X86_REG_SP){
        len++;
        immoff++;
    }

    char* buff = new char[len];

    if (idxdest < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }

    if (idxdest < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x8d;
    buff[2] = 0x05 + 8*(idxdest % X86_32BIT_GPRS);
    buff[3] = 0x24;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitInstructionBase(uint32_t sz, char* buff){
    X86Instruction* ret = new X86Instruction(NULL, 0, buff, ByteSource_Instrumentation, 0, true, sz);
    if (ret->getSizeInBytes() != sz){
        fprintf(stderr, "emitInstructionBase:\n");
        ret->print();
    }
    ASSERT(ret->getSizeInBytes() == sz);
    delete[] buff;
    return ret;
}

X86Instruction* X86InstructionFactory::emitInstructionBase(uint32_t sz, char* buff){
    X86Instruction* ret = new X86Instruction(NULL, 0, buff, ByteSource_Instrumentation, 0, false, sz);
    ASSERT(ret->getSizeInBytes() == sz);
    delete[] buff;
    return ret;
}

X86Instruction* X86InstructionFactory64::emitShiftRightLogical(uint8_t imm, uint8_t reg){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitShiftLeftLogical(uint8_t imm, uint8_t reg){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitShiftRightLogical(uint8_t imm, uint8_t reg){
    ASSERT(reg < X86_32BIT_GPRS && "Illegal register index given");

    uint32_t len = 3;
    char* buff = new char[len];

    buff[0] = 0xc1;
    buff[1] = 0xe8 + reg;
    buff[2] = imm;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitShiftLeftLogical(uint8_t imm, uint8_t reg){
    ASSERT(reg < X86_32BIT_GPRS && "Illegal register index given");

    uint32_t len = 3;
    char* buff = new char[len];

    buff[0] = 0xc1;
    buff[1] = 0xe0 + reg;
    buff[2] = imm;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitCompareImmReg(uint64_t imm, uint8_t reg){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitCompareImmReg(uint64_t imm, uint8_t reg){
    ASSERT(reg < X86_32BIT_GPRS && "Illegal register index given");

    uint32_t len = 6;
    char* buff = new char[len];

    buff[0] = 0x81;
    buff[1] = 0xf8 + reg;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for imm");
    memcpy(buff+2, &imm32, sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitBranchGeneric(uint64_t off, uint8_t code){
    uint32_t len = 6;
    char* buff = new char[len];

    buff[0] = 0x0f;
    buff[1] = code;

    uint32_t off32 = (uint32_t)off;
    ASSERT(off32 == off && "Cannot use more than 32 bits for off");
    memcpy(buff+2, &off32, sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitBranchJNE(uint64_t off){
    return emitBranchGeneric(off, 0x85);
}

X86Instruction* X86InstructionFactory::emitBranchJE(uint64_t off){
    return emitBranchGeneric(off, 0x84);
}

X86Instruction* X86InstructionFactory::emitBranchJL(uint64_t off){
    return emitBranchGeneric(off, 0x8c);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToReg(uint32_t srcreg, uint32_t destreg){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitMoveRegToReg(uint32_t srcreg, uint32_t destreg){
    ASSERT(srcreg < X86_32BIT_GPRS && "Illegal register index given");    
    ASSERT(destreg < X86_32BIT_GPRS && "Illegal register index given");    

    uint32_t len = 2;
    char* buff = new char[len];

    buff[0] = 0x89;
    buff[1] = 0xc0 + X86_32BIT_GPRS*srcreg + destreg;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitLoadRipImmToReg(uint32_t imm, uint32_t destreg){
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
    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory32::emitRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg){
    ASSERT(srcdestreg < X86_32BIT_GPRS && "Illegal register index given");    
    ASSERT(srcreg < X86_32BIT_GPRS && "Illegal register index given");    
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x01;
    buff[1] = 0xc0 + 8*srcdestreg + srcreg;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitRegImm1ByteMultReg(uint32_t src, uint8_t imm, uint32_t dest){
    ASSERT(src < X86_32BIT_GPRS && "Illegal register index given");    
    ASSERT(dest < X86_32BIT_GPRS && "Illegal register index given");    

    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x6b;
    buff[1] = 0xc0 + 8*dest + src;
    buff[2] = imm;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitRegImmMultReg(uint32_t src, uint32_t imm, uint32_t dest){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitRegAddImm(uint8_t idx, uint32_t imm){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x81;
    buff[1] = 0xc0 + (char)idx;
    memcpy(buff+2, &imm, sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitXorRegReg(uint8_t src, uint8_t tgt){
    ASSERT(src < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(tgt < X86_32BIT_GPRS && "Illegal register index given");
    
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x31;
    buff[1] = 0xc0 + tgt + 8*src;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitXorRegReg(uint8_t src, uint8_t tgt){
    ASSERT(src < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(tgt < X86_32BIT_GPRS && "Illegal register index given");
    
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x48;
    buff[1] = 0x31;
    buff[2] = 0xc0 + tgt + 8*src;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitLoadAHFromFlags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9f;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitStoreAHToFlags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9e;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitAddImmByteToMem(uint8_t imm, uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x83;
    buff[1] = 0x05;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+2,&addr32,sizeof(uint32_t));
    memcpy(buff+6,&imm,sizeof(uint8_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitAddImmToMem(uint32_t imm, uint64_t addr){
    uint32_t len = 12;
    char* buff = new char[len];

    buff[0] = 0x48;
    buff[1] = 0x81;
    buff[2] = 0x04;
    buff[3] = 0x25;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+4,&addr32,sizeof(uint32_t));
    memcpy(buff+8,&imm,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitAddImmByteToMem(uint8_t imm, uint64_t addr){
    uint32_t len = 9;
    char* buff = new char[len];

    buff[0] = 0x48;
    buff[1] = 0x83;
    buff[2] = 0x04;
    buff[3] = 0x25;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+4,&addr32,sizeof(uint32_t));
    memcpy(buff+8,&imm,sizeof(uint8_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitSubImmByteToMem(uint8_t imm, uint64_t addr){
    uint32_t len = 7;
    char* buff = new char[len];

    buff[0] = 0x83;
    buff[1] = 0x2d;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+2,&addr32,sizeof(uint32_t));
    memcpy(buff+6,&imm,sizeof(uint8_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitSubImmByteToMem(uint8_t imm, uint64_t addr){
    uint32_t len = 9;
    char* buff = new char[len];

    buff[0] = 0x48;
    buff[1] = 0x83;
    buff[2] = 0x2c;
    buff[3] = 0x25;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == (uint32_t)addr && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+4,&addr32,sizeof(uint32_t));
    memcpy(buff+8,&imm,sizeof(uint8_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitRegAndReg(uint32_t src, uint32_t srcdest){
    ASSERT(src < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(srcdest < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 3;
    char* buff = new char[len];
    
    if (srcdest < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }
    if (src < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }
    buff[1] = 0x21;
    buff[2] = 0xc0 + 8*(srcdest % X86_32BIT_GPRS) + (src % X86_32BIT_GPRS);

    return emitInstructionBase(len,buff);
}
X86Instruction* X86InstructionFactory64::emitRegOrReg(uint32_t src, uint32_t srcdest){
    ASSERT(src < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(srcdest < X86_64BIT_GPRS && "Illegal register index given");

    uint32_t len = 3;
    char* buff = new char[len];
    
    if (srcdest < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }
    if (src < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }
    buff[1] = 0x09;
    buff[2] = 0xc0 + 8*(srcdest % X86_32BIT_GPRS) + (src % X86_32BIT_GPRS);

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff){
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

    return emitInstructionBase(len,buff);
}

// mov imm, (base+off)
X86Instruction* X86InstructionFactory64::emitMoveImmToRegaddrImm(
    uint64_t imm,
    uint8_t  mem_size,
    int32_t  base,
    uint32_t off)
{
    ASSERT(base >= X86_REG_AX && base <= X86_REG_R15);
    ASSERT(mem_size <= sizeof(imm));

    // REX.W : operand size = 64
    // REX.R : ModRm.reg
    // REX.X : SIB.index
    // REX.B : ModRm.rm

    uint8_t W = mem_size == 8 ? 1 << 3 : 0;
    uint8_t R = 0;
    uint8_t X = 0;
    uint8_t B = (base & 0x8) >> 3;
    uint8_t rex = 0x40 | W | R | X | B;
    uint8_t opcode = mem_size == 1 ? 0xC6 : 0xC7;
    uint8_t mod = 0x80;
    uint8_t reg = 0;
    uint8_t rm = base & 0x7;
    uint8_t modrm = mod | reg | rm;

    int len = 4 + sizeof(off) + mem_size;
    char* buff = new char[len];
    buff[0] = 0x67;
    buff[1] = rex;
    buff[2] = opcode;
    buff[3] = modrm;
    memcpy(buff+4, &off, sizeof(off));
    char* immptr = (char*)&imm;
    memcpy(buff+4+sizeof(off), immptr, mem_size);
    return emitInstructionBase(len, buff);
}

X86Instruction* X86InstructionFactory64::emitMoveImmToRegaddrImm(uint64_t val, uint32_t idx, uint64_t off){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 12;
    uint32_t immoff = 4;
    if (idx % X86_32BIT_GPRS == X86_REG_SP){
        len++;
        immoff++;
    }
    char* buff = new char[len];
    // set opcode
    buff[0] = 0x67; // address override prefix
    buff[1] = 0x48; // rex prefix 0x4WRXB
    if (idx >= X86_32BIT_GPRS){
        buff[1]++;
    }
    buff[2] = 0xc7; // opcode
    buff[3] = 0x80 + (char)(idx % X86_32BIT_GPRS); // modrm?


    buff[4] = 0x24; // N/A?

    uint32_t off32 = (uint32_t)off;
    ASSERT(off32 == (uint32_t)off && "Cannot use more than 32 bits for the immediate");

    uint32_t val32 = (uint32_t)val;
    ASSERT(val32 == (uint32_t)val && "Cannot use more than 32 bits for the immediate");

    memcpy(buff+immoff,&off32,sizeof(uint32_t));
    memcpy(buff+immoff+sizeof(uint32_t),&val32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegaddrImmToReg(uint32_t idxsrc, uint64_t imm, uint32_t idxdest){
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
    if (idxdest < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x4c;
    }
    if (idxsrc < X86_32BIT_GPRS){
    } else {
        buff[0]++;
    }

    buff[1] = 0x8b;
    buff[2] = 0x80 + (char)((idxdest % X86_32BIT_GPRS) * 8) + (char)(idxsrc % X86_32BIT_GPRS);
    buff[3] = 0x24;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitMoveRegaddrImmToReg(uint32_t idxsrc, uint64_t imm, uint32_t idxdest){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveImmByteToReg(uint8_t imm, uint32_t idx){
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
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitRegIncrement(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x40 + idx;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitInterrupt(uint8_t idx){
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
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitSetDirectionFlag(bool backward){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xfc;
    if (backward){
        buff[0]++;
    }    

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitSTOSByte(bool repeat){
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0xf3;
    buff[len-1] = 0xaa;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitSTOSWord(bool repeat){
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0xf3;
    buff[len-1] = 0xab;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveString(bool repeat){
    uint32_t len = 1;
    if (repeat){
        len++;
    }
    char* buff = new char[len];
    buff[0] = 0xf3;
    buff[len-1] = 0xa5;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveRipImmToSegmentReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_SEGMENT_REGS && "Illegal segment register index given");
    ASSERT(idx != X86_SEGREG_CS && "Illegal segment register index given");

    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x8e;
    buff[1] = 0x05 + (char)(8*idx);

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate");
    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveImmToSegmentReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_SEGMENT_REGS && "Illegal segment register index given");
    ASSERT(idx != X86_SEGREG_CS && "Illegal segment register index given");

    uint32_t len = 7;
    char* buff = new char[len];
    buff[0] = 0x8e;
    buff[1] = 0x04 + (char)(8*idx);
    buff[2] = 0x25;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate");
    memcpy(buff+3,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory::emitPushSegmentReg(uint32_t idx){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitPopSegmentReg(uint32_t idx){
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

    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory64::emitStackPush4Byte(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x66; 
    buff[1] = 0x50 + idx;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitAddByteToRegaddr(uint8_t byt, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    char* buff = new char[len];
    buff[0] = 0x83; 
    buff[1] = 0x00 + idx;
    buff[2] = byt;
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx){
    ASSERT(srcidx < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x8b; 
    buff[1] = 0x00 + (char)(srcidx) + (char)(8*destidx);
    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToRegaddr(uint32_t destidx, uint32_t srcidx){
    ASSERT(srcidx < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    if (srcidx % X86_32BIT_GPRS == X86_REG_SP || srcidx % X86_32BIT_GPRS == X86_REG_BP){
        len++;
    }

    char* buff = new char[len];
    buff[0] = 0x48;
    if (srcidx >= X86_32BIT_GPRS){
        buff[0]++;
    }
    if (destidx >= X86_32BIT_GPRS){
        buff[0] += 0x04;
    }
    buff[1] = 0x89;
    buff[2] = 0x00 + (char)(srcidx % X86_32BIT_GPRS) + (char)(8* (destidx % X86_32BIT_GPRS));
    if (srcidx % X86_32BIT_GPRS == X86_REG_SP){
        buff[3] = 0x24;
    }
    if (srcidx % X86_32BIT_GPRS == X86_REG_BP){
        buff[2] += 0x40;
        buff[3] = 0x00;
    }
    //PRINT_INFOR("%hhx %hhx %hhx", buff[0], buff[1], buff[2]);

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx){
    ASSERT(srcidx < X86_64BIT_GPRS && "Illegal register index given");
    ASSERT(destidx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 3;
    if (srcidx % X86_32BIT_GPRS == X86_REG_SP || srcidx % X86_32BIT_GPRS == X86_REG_BP){
        len++;
    }

    char* buff = new char[len];
    buff[0] = 0x48;
    if (srcidx >= X86_32BIT_GPRS){
        buff[0]++;
    }
    if (destidx >= X86_32BIT_GPRS){
        buff[0] += 0x04;
    }
    buff[1] = 0x8b;
    buff[2] = 0x00 + (char)(srcidx % X86_32BIT_GPRS) + (char)(8* (destidx % X86_32BIT_GPRS));
    if (srcidx % X86_32BIT_GPRS == X86_REG_SP){
        buff[3] = 0x24;
    }
    if (srcidx % X86_32BIT_GPRS == X86_REG_BP){
        buff[2] += 0x40;
        buff[3] = 0x00;
    }
    //PRINT_INFOR("%hhx %hhx %hhx", buff[0], buff[1], buff[2]);

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitRegSubImm(uint8_t idx, uint32_t imm){
    ASSERT(idx > 0 && idx < X86_64BIT_GPRS && "Illegal register index given");

    if (!imm){
        return X86InstructionFactory64::emitNop();
    } else {
        return X86InstructionFactory64::emitRegSubImm4Byte(idx, imm);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

X86Instruction* X86InstructionFactory64::emitRegSubImm4Byte(uint8_t idx, uint32_t imm){
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

    return emitInstructionBase(len,buff);
}



X86Instruction* X86InstructionFactory64::emitRegAddImm(uint8_t idx, uint32_t imm){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");

    if (!imm){
        return X86InstructionFactory64::emitNop();
    } else {
        return X86InstructionFactory64::emitRegAddImm4Byte(idx, imm);
    }
    __SHOULD_NOT_ARRIVE;
    return NULL;
}

X86Instruction* X86InstructionFactory64::emitRegAddImm4Byte(uint8_t idx, uint32_t imm){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitRegSubImm(uint8_t idx, uint32_t imm){
    ASSERT(idx > 0 && idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 6;
    char* buff = new char[len];
    buff[0] = 0x81;
    buff[1] = 0xe8 + (char)idx;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immset");
    memcpy(buff+2,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitReturn(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0xc3;

    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory64::emitStackPush(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    if (idx < X86_32BIT_GPRS){
        return X86InstructionFactory32::emitStackPush(idx);
    }
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x41;
    buff[1] = 0x48 + (char)idx;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitStackPop(uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    if (idx < X86_32BIT_GPRS){
        return X86InstructionFactory32::emitStackPop(idx);
    }
    uint32_t len = 2;
    char* buff = new char[len];
    buff[0] = 0x41;
    buff[1] = 0x50 + (char)idx;

    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory::emitPushEflags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9c;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitPopEflags(){
    uint32_t len = 1;
    char* buff = new char[len];
    buff[0] = 0x9d;

    return emitInstructionBase(len,buff);
}


Vector<X86Instruction*>* X86InstructionFactory::emitNopSeries(uint32_t len){
    Vector<X86Instruction*>* series = new Vector<X86Instruction*>();
    for (int32_t i = 7; i >= 0; i--){
        while (len > i){
            (*series).append(emitNop(i+1));
            len -= (i+1);
        }
    }
    ASSERT(!len);
    return series;
}

X86Instruction* X86InstructionFactory::emitNop(uint32_t len){
    ASSERT(len <= MAX_NOP_LENGTH);
    /* from the AMD k8 optimization manual
    NOP1_OVERRIDE_NOP TEXTEQU <DB 090h>
    NOP2_OVERRIDE_NOP TEXTEQU <DB 066h,090h>
    NOP3_OVERRIDE_NOP TEXTEQU <DB 066h,066h,090h>
    NOP4_OVERRIDE_NOP TEXTEQU <DB 066h,066h,066h,090h>
    NOP5_OVERRIDE_NOP TEXTEQU <DB 066h,066h,090h,066h,090h>
    NOP6_OVERRIDE_NOP TEXTEQU <DB 066h,066h,090h,066h,066h,090h>
    NOP7_OVERRIDE_NOP TEXTEQU <DB 066h,066h,066h,090h,066h,066h,090h>
    NOP8_OVERRIDE_NOP TEXTEQU <DB 066h,066h,066h,090h,066h,066h,066h,090h>
    NOP9_OVERRIDE_NOP TEXTEQU <DB 066h,066h,090h,066h,066h,090h,066h,066h,090h>
    */

    /* uses eax-dependencies
    +#define P6_NOP1GENERIC_NOP1
    +#define P6_NOP2".byte 0x66,0x90\n"
    +#define P6_NOP3".byte 0x0f,0x1f,0x00\n"
    +#define P6_NOP4".byte 0x0f,0x1f,0x40,0\n"
    +#define P6_NOP5".byte 0x0f,0x1f,0x44,0x00,0\n"
    +#define P6_NOP6".byte 0x66,0x0f,0x1f,0x44,0x00,0\n"
    +#define P6_NOP7".byte 0x0f,0x1f,0x80,0,0,0,0\n"
    +#define P6_NOP8".byte 0x0f,0x1f,0x84,0x00,0,0,0,0\n"
    */
    char* buff = new char[len];
    switch(len){
    case 1:
        buff[0] = 0x90;
        break;
    case 2:
        buff[0] = 0x66;
        buff[1] = 0x90;
        break;
    case 3:
        buff[0] = 0x0f;
        buff[1] = 0x1f;
        buff[2] = 0x00;
        break;
    case 4:
        buff[0] = 0x0f;
        buff[1] = 0x1f;
        buff[2] = 0x40;
        buff[3] = 0x00;
        break;
    case 5:
        buff[0] = 0x0f;
        buff[1] = 0x1f;
        buff[2] = 0x44;
        buff[3] = 0x00;
        buff[4] = 0x00;
        break;
    case 6:
        buff[0] = 0x66;
        buff[1] = 0x0f;
        buff[2] = 0x1f;
        buff[3] = 0x44;
        buff[4] = 0x00;
        buff[5] = 0x00;
        break;
    case 7:
        buff[0] = 0x0f;
        buff[1] = 0x1f;
        buff[2] = 0x80;
        buff[3] = 0x00;
        buff[4] = 0x00;
        buff[5] = 0x00;
        buff[6] = 0x00;
        break;
    case 8:
        buff[0] = 0x0f;
        buff[1] = 0x1f;
        buff[2] = 0x84;
        buff[3] = 0x00;
        buff[4] = 0x00;
        buff[5] = 0x00;
        buff[6] = 0x00;
        buff[7] = 0x00;
        break;
    default:
        __SHOULD_NOT_ARRIVE;
    }

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitNop(){
    return emitNop(1);
}

X86Instruction* X86InstructionFactory32::emitMoveRegToRegaddrImm(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
    ASSERT(idxsrc < X86_32BIT_GPRS && "Illegal register index given");
    ASSERT(idxdest < X86_32BIT_GPRS && "Illegal register index given");    

    uint32_t len = 6;
    uint32_t immoff = 2;

    if (idxdest == X86_REG_SP){
        len++;
        immoff++;
    }
    char* buff = new char[len];

    buff[0] = 0x89;
    buff[1] = 0x80 + 8*idxsrc + idxdest;
    buff[2] = 0x24;

    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == (uint32_t)imm && "Cannot use more than 32 bits for the immediate");
    memcpy(buff+immoff,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory64::emitMoveRegToRegaddrImm(uint32_t idxsrc, uint32_t idxdest, uint64_t imm, bool source64Bit){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToRegaddrImm2Byte(int32_t src, int32_t base, uint32_t off)
{
    ASSERT(src >= X86_REG_AX && src <= X86_REG_R15);
    ASSERT(base >= X86_REG_AX && base <= X86_REG_R15);

    uint8_t W = 0;
    uint8_t R = (src & 0x8) >> 1;
    uint8_t X = 0;
    uint8_t B = (base & 0x8) >> 3;
    uint8_t rex = 0x40 | W | R | X | B;
    uint8_t opcode = 0x89;
    uint8_t mod = 0x80;
    uint8_t reg = src & 0x7;
    uint8_t rm = base & 0x7;
    uint8_t modrm = mod | reg | rm;

    int len = 4 + sizeof(off);
    char* buff = new char[len];
    buff[0] = 0x66;
    buff[1] = rex;
    buff[2] = opcode;
    buff[3] = modrm;
    memcpy(buff+3, &off, sizeof(off));
    return emitInstructionBase(len, buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest){
    ASSERT(idxsrc < 8 && "Illegal register index given");
    ASSERT(idxdest < 8 && "Illegal register index given");    
    uint32_t len = 2;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x89;
    buff[1] = 0x00 + (char)(idxsrc*8) + (char)(idxdest);

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitIndirectRelativeJump(uint64_t addr, uint64_t tgt){
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

    return emitInstructionBase(len,buff);
}

// FIXME
X86Instruction* X86InstructionFactory64::emitMoveMemToXMMReg(uint64_t addr, uint32_t idx){
    ASSERT(idx >= X86_64BIT_GPRS && idx < X86_64BIT_GPRS + X86_XMM_REGS && "Illegal register index given");
    char* buff;
    uint32_t next;
    uint32_t len;

    if( idx < X86_64BIT_GPRS + 8 ) {
        len = 9;
        buff = new char[len];
        buff[0] = 0xf2;
        buff[1] = 0x0f;
        buff[2] = 0x10;
        next = 3;
    } else {
        len = 10;
        buff = new char[len];
        buff[0] = 0xf2;
        buff[1] = 0x44;
        buff[2] = 0x0f;
        buff[3] = 0x10;
        next = 4;
    }
    buff[next] = 0x04 + 0x08 * (idx % 8);
    buff[next+1] = 0x25;

    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr == addr32 && "Cannot use more than 32 bits for the address");

    memcpy(buff+next+2, &addr32, sizeof(uint32_t));

    return emitInstructionBase(len, buff);
    
}

X86Instruction* X86InstructionFactory64::emitMoveImmToReg(uint64_t imm, uint32_t idx){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveImm64ToReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 10;
    char* buff = new char[len];

    // set opcode
    if (idx < X86_32BIT_GPRS){
        buff[0] = 0x48;
    } else {
        buff[0] = 0x49;
    }
    buff[1] = 0xb8 + (char)(idx % X86_32BIT_GPRS);    

    // set target address
    memcpy(buff+2,&imm,sizeof(uint64_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitMoveImmToReg(uint64_t imm, uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xb8 + (char)idx;

    // set target address
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitMoveRegToMem(uint32_t idx, uint64_t addr){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveRegToMem(uint32_t idx, uint64_t addr){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitMoveMemToReg(uint64_t addr, uint32_t idx){
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

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory64::emitMoveMemToReg(uint64_t addr, uint32_t idx, bool is64Bit){
    ASSERT(idx < X86_64BIT_GPRS && "Illegal register index given");
    uint32_t len = 8;
    uint32_t baseidx = 1;
    if (!is64Bit){
        ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
        len--;
        baseidx--;
    }

    char* buff = new char[len];

    // set opcode
    if (idx < X86_32BIT_GPRS){
        buff[0] = 0x48;
        buff[baseidx + 1] = 0x04 + 0x8*(char)idx;
    } else {
        buff[0] = 0x4c;
        buff[baseidx + 1] = 0x04 + 0x8*((char)(idx-X86_32BIT_GPRS));
    }
    buff[baseidx] = 0x8b;
    buff[baseidx + 2] = 0x25;

    // set target address
    uint32_t addr32 = (uint32_t)addr;
    ASSERT(addr32 == addr && "Cannot use more than 32 bits for address");
    memcpy(buff + baseidx + 3,&addr32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitStackPush(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x50 + (char)idx;

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory32::emitStackPop(uint32_t idx){
    ASSERT(idx < X86_32BIT_GPRS && "Illegal register index given");
    uint32_t len = 1;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x58 + (char)idx;

    return emitInstructionBase(len,buff);
}


X86Instruction* X86InstructionFactory::emitCallRelative(uint64_t addr, uint64_t tgt){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xe8;

    uint64_t imm = tgt - addr - len;
    uint32_t imm32 = (uint32_t)imm;

    ASSERT(addr == (uint32_t)addr && "Cannot use more than 32 bits for address");
    ASSERT(tgt == (uint32_t)tgt && "Cannot use more than 32 bits for target");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);    
}

X86Instruction* X86InstructionFactory32::emitJumpIndirect(uint64_t tgt){
    uint32_t len = 6;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xff;
    buff[1] = 0x25;

    // set target address
    uint32_t tgt32 = (uint32_t)tgt;
    ASSERT(tgt32 == tgt && "Cannot use more than 32 bits for jump target");
    memcpy(buff+2,&tgt32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitJumpRelative(uint64_t addr, uint64_t tgt){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0xe9;

    uint64_t imm = tgt - addr - len;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(addr == (int32_t)addr && "Cannot use more than 32 bits for address");
    ASSERT(tgt == (int32_t)tgt && "Cannot use more than 32 bits for target");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

X86Instruction* X86InstructionFactory::emitStackPushImm(uint64_t imm){
    uint32_t len = 5;
    char* buff = new char[len];

    // set opcode
    buff[0] = 0x68;
    uint32_t imm32 = (uint32_t)imm;
    ASSERT(imm32 == imm && "Cannot use more than 32 bits for immediate value");
    memcpy(buff+1,&imm32,sizeof(uint32_t));

    return emitInstructionBase(len,buff);
}

