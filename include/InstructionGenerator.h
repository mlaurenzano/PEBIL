#ifndef _InstructionGenerator_h_
#define _InstructionGenerator_h_

#include <Instruction.h>


class InstructionGenerator : public Instruction {
protected:
    static Instruction* generateInstructionBase(uint32_t sz, char* buf);

public:
    static Instruction* generateNoop();
    static Instruction* generateInterrupt(uint8_t idx);

    static Instruction* generateBranchJL(uint64_t offset);
    static Instruction* generateJumpRelative(uint64_t addr, uint64_t tgt);
    static Instruction* generateCallRelative(uint64_t addr, uint64_t tgt);
    static Instruction* generateReturn();

    static Instruction* generateSetDirectionFlag(bool backward);
    static Instruction* generateMoveImmToSegmentReg(uint64_t imm, uint32_t idx);
    static Instruction* generateSTOSByte(bool repeat);
    static Instruction* generateSTOSWord(bool repeat);
    static Instruction* generateMoveString(bool repeat);

    static Instruction* generatePushEflags();
    static Instruction* generatePopEflags();
    static Instruction* generatePushSegmentReg(uint32_t idx);
    static Instruction* generatePopSegmentReg(uint32_t idx);
    static Instruction* generateStackPushImm(uint64_t imm);

    static Instruction* generateMoveImmByteToMemIndirect(uint8_t byt, uint64_t off, uint32_t idx);
    static Instruction* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static Instruction* generateMoveImmByteToReg(uint8_t imm, uint32_t idx);
    static Instruction* generateRegAddImm(uint8_t idx, uint32_t imm);
    static Instruction* generateMoveRegToRegaddr(uint32_t srcidx, uint32_t destidx);

    static Instruction* generateRegIncrement(uint32_t idx);
    static Instruction* generateRegSubImm(uint8_t idx, uint32_t imm);
    static Instruction* generateAddByteToRegaddr(uint8_t byt, uint32_t idx);
    static Instruction* generateAndImmReg(uint64_t, uint32_t);
};

class InstructionGenerator64 : public InstructionGenerator {
private:
    static Instruction* generateInstructionBase(uint32_t sz, char* buf);

    static Instruction* generateMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static Instruction* generateMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);

    static Instruction* generateRegSubImm4Byte(uint8_t idx, uint32_t imm);
    static Instruction* generateRegSubImm1Byte(uint8_t idx, uint32_t imm);

    static Instruction* generateRegAddImm4Byte(uint8_t idx, uint32_t imm);

public:
    static Instruction* generateCompareImmReg(uint64_t imm, uint8_t reg);

    static Instruction* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateStackPush(uint32_t idx);
    static Instruction* generateStackPop(uint32_t idx);
    static Instruction* generateIndirectRelativeJump(uint64_t addr, uint64_t tgt);
    static Instruction* generateStackPush4Byte(uint32_t idx);
    static Instruction* generateRegAddImm(uint8_t, uint32_t);
    static Instruction* generateRegSubImm(uint8_t, uint32_t);
    static Instruction* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t, bool);
    static Instruction* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static Instruction* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static Instruction* generateMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff);
    static Instruction* generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest);

    static Instruction* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static Instruction* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static Instruction* generateStoreAHToFlags();
    static Instruction* generateLoadAHFromFlags();

    static Instruction* generateShiftLeftLogical(uint8_t imm, uint8_t reg);
    static Instruction* generateShiftRightLogical(uint8_t imm, uint8_t reg);

    static Instruction* generateAddImmByteToMem(uint8_t, uint64_t);
    static Instruction* generateXorRegReg(uint8_t, uint8_t);
    static Instruction* generateRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg);
    static Instruction* generateRegImmMultReg(uint32_t src, uint32_t imm, uint32_t dest);
    static Instruction* generateLoadRipImmToReg(uint32_t imm, uint32_t destreg);
    static Instruction* generateMoveRegToReg(uint32_t srcreg, uint32_t destreg);
    static Instruction* generateLoadRegImmReg(uint8_t src, uint64_t imm, uint8_t dest);
};

class InstructionGenerator32 : public InstructionGenerator {
private:
    static Instruction* generateInstructionBase(uint32_t sz, char* buf);

public:
    static Instruction* generateExchangeMemReg(uint64_t addr, uint8_t idx);

    static Instruction* generateCompareImmReg(uint64_t imm, uint8_t reg);
    static Instruction* generateLoadRegImmReg(uint8_t src, uint64_t imm, uint8_t dest);

    static Instruction* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateJumpIndirect(uint64_t tgt);
    static Instruction* generateStackPush(uint32_t idx);
    static Instruction* generateStackPop(uint32_t idx);

    static Instruction* generateRegAddImm(uint8_t idx, uint32_t imm);
    static Instruction* generateRegSubImm(uint8_t idx, uint32_t imm);

    static Instruction* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static Instruction* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static Instruction* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static Instruction* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);

    static Instruction* generateStoreAHToFlags();
    static Instruction* generateLoadAHFromFlags();

    static Instruction* generateShiftLeftLogical(uint8_t imm, uint8_t reg);
    static Instruction* generateShiftRightLogical(uint8_t imm, uint8_t reg);

    static Instruction* generateAddImmByteToMem(uint8_t, uint64_t);
    static Instruction* generateRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg);
    static Instruction* generateRegImm1ByteMultReg(uint32_t src, uint8_t imm, uint32_t dest);
    static Instruction* generateMoveRegToReg(uint32_t srcreg, uint32_t destreg);
};

#endif /* _InstructionGenerator_h_ */
