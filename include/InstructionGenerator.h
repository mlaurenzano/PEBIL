#ifndef _InstructionGenerator_h_
#define _InstructionGenerator_h_

#include <Instruction.h>

class InstructionGenerator : public Instruction {
protected:
    static Instruction* generateInstructionBase(uint32_t sz, char* buf);

public:
    static Instruction* generateNoop();

    static Instruction* generateStringMove(bool repeat);
    static Instruction* generateSetDirectionFlag(bool backward);
    static Instruction* generateMoveImmToSegmentReg(uint64_t imm, uint32_t idx);
    static Instruction* generateSTOSByte(bool repeat);

    static Instruction* generateReturn();
    static Instruction* generatePushEflags();
    static Instruction* generatePopEflags();
    static Instruction* generatePushSegmentReg(uint32_t idx);
    static Instruction* generatePopSegmentReg(uint32_t idx);
    static Instruction* generateMoveImmByteToMemIndirect(uint8_t byt, uint64_t off, uint32_t idx);
    static Instruction* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static Instruction* generateMoveImmByteToReg(uint8_t imm, uint32_t idx);
    static Instruction* generateJumpRelative(uint64_t addr, uint64_t tgt);
    static Instruction* generateStackPushImmediate(uint64_t imm);
    static Instruction* generateCallRelative(uint64_t addr, uint64_t tgt);
    static Instruction* generateRegAddImmediate(uint32_t idx, uint64_t imm);
    static Instruction* generateRegIncrement(uint32_t idx);
    static Instruction* generateRegSubImmediate(uint32_t idx, uint64_t imm);
    static Instruction* generateAddByteToRegaddr(uint8_t byt, uint32_t idx);

    static Instruction* generateMoveRegToRegaddr(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateInterrupt(uint8_t idx);

    static Instruction* generateAndImmReg(uint64_t, uint32_t);
};

class InstructionGenerator64 : public InstructionGenerator {
private:
    static Instruction* generateMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static Instruction* generateMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static Instruction* generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest);

    static Instruction* generateRegSubImmediate4Byte(uint32_t idx, uint64_t imm);
    static Instruction* generateRegSubImmediate1Byte(uint32_t idx, uint64_t imm);

    static Instruction* generateRegAddImmediate4Byte(uint32_t idx, uint64_t imm);
    static Instruction* generateRegAddImmediate1Byte(uint32_t idx, uint64_t imm);

public:
    static Instruction* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateStackPush(uint32_t idx);
    static Instruction* generateStackPop(uint32_t idx);
    static Instruction* generateIndirectRelativeJump(uint64_t addr, uint64_t tgt);
    static Instruction* generateStackPush4Byte(uint32_t idx);
    static Instruction* generateRegAddImmediate(uint32_t, uint64_t);
    static Instruction* generateRegSubImmediate(uint32_t, uint64_t);
    static Instruction* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);
    static Instruction* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static Instruction* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static Instruction* generateMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff);

    static Instruction* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static Instruction* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static Instruction* generateStoreEflagsToAH();
    static Instruction* generateLoadEflagsFromAH();

    static Instruction* generateAddImmByteToMem(uint8_t, uint64_t);
    static Instruction* generateXorRegReg(uint8_t, uint8_t);
};

class InstructionGenerator32 : public InstructionGenerator {
public:
    static Instruction* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateJumpIndirect(uint64_t tgt);
    static Instruction* generateStackPush(uint32_t idx);
    static Instruction* generateStackPop(uint32_t idx);

    static Instruction* generateRegAddImmediate(uint32_t idx, uint64_t imm);
    static Instruction* generateRegSubImmediate(uint32_t idx, uint64_t imm);

    static Instruction* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static Instruction* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static Instruction* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static Instruction* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);

    static Instruction* generateStoreEflagsToAH();
    static Instruction* generateLoadEflagsFromAH();

    static Instruction* generateAddImmByteToMem(uint8_t, uint64_t);
};

#endif /* _InstructionGenerator_h_ */
