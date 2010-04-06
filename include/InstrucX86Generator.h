#ifndef _InstrucX86Generator_h_
#define _InstrucX86Generator_h_

#include <InstrucX86.h>


class InstrucX86Generator : public InstrucX86 {
protected:
    static InstrucX86* generateInstructionBase(uint32_t sz, char* buf);

public:
    static InstrucX86* generateNoop();
    static InstrucX86* generateInterrupt(uint8_t idx);

    static InstrucX86* generateBranchJL(uint64_t offset);
    static InstrucX86* generateJumpRelative(uint64_t addr, uint64_t tgt);
    static InstrucX86* generateCallRelative(uint64_t addr, uint64_t tgt);
    static InstrucX86* generateReturn();

    static InstrucX86* generateSetDirectionFlag(bool backward);
    static InstrucX86* generateMoveImmToSegmentReg(uint64_t imm, uint32_t idx);
    static InstrucX86* generateSTOSByte(bool repeat);
    static InstrucX86* generateSTOSWord(bool repeat);
    static InstrucX86* generateMoveString(bool repeat);

    static InstrucX86* generatePushEflags();
    static InstrucX86* generatePopEflags();
    static InstrucX86* generatePushSegmentReg(uint32_t idx);
    static InstrucX86* generatePopSegmentReg(uint32_t idx);
    static InstrucX86* generateStackPushImm(uint64_t imm);

    static InstrucX86* generateMoveImmByteToMemIndirect(uint8_t byt, uint64_t off, uint32_t idx);
    static InstrucX86* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static InstrucX86* generateMoveImmByteToReg(uint8_t imm, uint32_t idx);
    static InstrucX86* generateRegAddImm(uint8_t idx, uint32_t imm);
    static InstrucX86* generateMoveRegToRegaddr(uint32_t srcidx, uint32_t destidx);

    static InstrucX86* generateRegIncrement(uint32_t idx);
    static InstrucX86* generateRegSubImm(uint8_t idx, uint32_t imm);
    static InstrucX86* generateAddByteToRegaddr(uint8_t byt, uint32_t idx);
    static InstrucX86* generateAndImmReg(uint64_t, uint32_t);

    static InstrucX86* generateStoreAHToFlags();
    static InstrucX86* generateLoadAHFromFlags();

};

class InstrucX86Generator64 : public InstrucX86Generator {
private:
    static InstrucX86* generateInstructionBase(uint32_t sz, char* buf);

    static InstrucX86* generateMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static InstrucX86* generateMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);

    static InstrucX86* generateRegSubImm4Byte(uint8_t idx, uint32_t imm);
    static InstrucX86* generateRegSubImm1Byte(uint8_t idx, uint32_t imm);

    static InstrucX86* generateRegAddImm4Byte(uint8_t idx, uint32_t imm);

public:
    static InstrucX86* generateCompareImmReg(uint64_t imm, uint8_t reg);

    static InstrucX86* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static InstrucX86* generateStackPush(uint32_t idx);
    static InstrucX86* generateStackPop(uint32_t idx);
    static InstrucX86* generateIndirectRelativeJump(uint64_t addr, uint64_t tgt);
    static InstrucX86* generateStackPush4Byte(uint32_t idx);
    static InstrucX86* generateRegAddImm(uint8_t, uint32_t);
    static InstrucX86* generateRegSubImm(uint8_t, uint32_t);
    static InstrucX86* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t, bool);
    static InstrucX86* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static InstrucX86* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static InstrucX86* generateMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff);
    static InstrucX86* generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest);

    static InstrucX86* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static InstrucX86* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static InstrucX86* generateShiftLeftLogical(uint8_t imm, uint8_t reg);
    static InstrucX86* generateShiftRightLogical(uint8_t imm, uint8_t reg);

    static InstrucX86* generateAddImmByteToMem(uint8_t, uint64_t);
    static InstrucX86* generateXorRegReg(uint8_t, uint8_t);
    static InstrucX86* generateRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg);
    static InstrucX86* generateRegImmMultReg(uint32_t src, uint32_t imm, uint32_t dest);
    static InstrucX86* generateLoadRipImmToReg(uint32_t imm, uint32_t destreg);
    static InstrucX86* generateMoveRegToReg(uint32_t srcreg, uint32_t destreg);
    static InstrucX86* generateLoadRegImmReg(uint8_t src, uint64_t imm, uint8_t dest);

    static Vector<InstrucX86*>* generateAddressComputation(InstrucX86* instruction, uint32_t dest);
    static InstrucX86* generateLoadEffectiveAddress(OperandX86* op, uint32_t dest);
    static InstrucX86* generateLoadEffectiveAddress(uint32_t baseReg, uint32_t indexReg, uint8_t scale, uint64_t value, uint32_t dest, bool hasBase, bool hasIndex);

    static InstrucX86* generateMoveSegmentRegToReg(uint32_t src, uint32_t dest);
};

class InstrucX86Generator32 : public InstrucX86Generator {
private:
    static InstrucX86* generateInstructionBase(uint32_t sz, char* buf);

public:
    static InstrucX86* generateExchangeMemReg(uint64_t addr, uint8_t idx);

    static InstrucX86* generateCompareImmReg(uint64_t imm, uint8_t reg);
    static InstrucX86* generateLoadRegImmReg(uint8_t src, uint64_t imm, uint8_t dest);

    static InstrucX86* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static InstrucX86* generateJumpIndirect(uint64_t tgt);
    static InstrucX86* generateStackPush(uint32_t idx);
    static InstrucX86* generateStackPop(uint32_t idx);

    static InstrucX86* generateRegAddImm(uint8_t idx, uint32_t imm);
    static InstrucX86* generateRegSubImm(uint8_t idx, uint32_t imm);

    static InstrucX86* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static InstrucX86* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static InstrucX86* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static InstrucX86* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);

    static InstrucX86* generateShiftLeftLogical(uint8_t imm, uint8_t reg);
    static InstrucX86* generateShiftRightLogical(uint8_t imm, uint8_t reg);

    static InstrucX86* generateAddImmByteToMem(uint8_t, uint64_t);
    static InstrucX86* generateRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg);
    static InstrucX86* generateRegImm1ByteMultReg(uint32_t src, uint8_t imm, uint32_t dest);
    static InstrucX86* generateMoveRegToReg(uint32_t srcreg, uint32_t destreg);
};

#endif /* _InstrucX86Generator_h_ */
