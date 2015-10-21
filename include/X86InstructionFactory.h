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

#ifndef _X86InstructionFactory_h_
#define _X86InstructionFactory_h_

#include <X86Instruction.h>

#define MAX_NOP_LENGTH 8

class X86InstructionFactory : public X86Instruction {
protected:
    static X86Instruction* emitInstructionBase(uint32_t sz, char* buf) __attribute__ ((noinline));
    static X86Instruction* emitBranchGeneric(uint64_t off, uint8_t code);

public:

    static X86Instruction* assemble(const char* buf, bool is64);

    static X86Instruction* emitNop();
    static X86Instruction* emitNop(uint32_t len);
    static Vector<X86Instruction*>* emitNopSeries(uint32_t len);
    static X86Instruction* emitInterrupt(uint8_t idx);

    static X86Instruction* emitBranchJL(uint64_t offset);
    static X86Instruction* emitBranchJE(uint64_t offset);
    static X86Instruction* emitBranchJNE(uint64_t offset);
    static X86Instruction* emitJumpRelative(uint64_t addr, uint64_t tgt);
    static X86Instruction* emitCallRelative(uint64_t addr, uint64_t tgt);
    static X86Instruction* emitReturn();

    static X86Instruction* emitSetDirectionFlag(bool backward);
    static X86Instruction* emitMoveRipImmToSegmentReg(uint64_t imm, uint32_t idx);
    static X86Instruction* emitMoveImmToSegmentReg(uint64_t imm, uint32_t idx);
    static X86Instruction* emitSTOSByte(bool repeat);
    static X86Instruction* emitSTOSWord(bool repeat);
    static X86Instruction* emitMoveString(bool repeat);

    static X86Instruction* emitPushEflags();
    static X86Instruction* emitPopEflags();
    static X86Instruction* emitPushSegmentReg(uint32_t idx);
    static X86Instruction* emitPopSegmentReg(uint32_t idx);
    static X86Instruction* emitStackPushImm(uint64_t imm);

    static X86Instruction* emitMoveImmToReg(uint64_t imm, uint32_t idx);
    static X86Instruction* emitMoveImmByteToReg(uint8_t imm, uint32_t idx);
    static X86Instruction* emitRegAddImm(uint8_t idx, uint32_t imm);
    static X86Instruction* emitMoveRegToRegaddr(uint32_t srcidx, uint32_t destidx);

    static X86Instruction* emitRegIncrement(uint32_t idx);
    static X86Instruction* emitRegSubImm(uint8_t idx, uint32_t imm);
    static X86Instruction* emitAddByteToRegaddr(uint8_t byt, uint32_t idx);

    static X86Instruction* emitStoreAHToFlags();
    static X86Instruction* emitLoadAHFromFlags();

    static X86Instruction* emitMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff);
};

class X86InstructionFactory64 : public X86InstructionFactory {
private:
    static X86Instruction* emitInstructionBase(uint32_t sz, char* buf) __attribute__ ((noinline));

    static X86Instruction* emitMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static X86Instruction* emitMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);

    static X86Instruction* emitRegSubImm4Byte(uint8_t idx, uint32_t imm);
    static X86Instruction* emitRegSubImm1Byte(uint8_t idx, uint32_t imm);

    static X86Instruction* emitRegAddImm4Byte(uint8_t idx, uint32_t imm);

public:
    static X86Instruction* assemble(const char* buf);

    static X86Instruction* emitExchangeAdd(uint8_t src, uint8_t dest, bool lock);

    static X86Instruction* emitMoveKToReg(uint8_t kreg, uint8_t gpr);
    static X86Instruction* emitMoveRegToK(uint8_t gpr, uint8_t kreg);
    static X86Instruction* emitMoveAlignedStackToZmmx(uint8_t reg, uint8_t disp);
    static X86Instruction* emitMoveZmmxToAlignedStack(uint8_t reg, uint8_t disp);
    static X86Instruction* emitFxSave(uint64_t addr);
    static X86Instruction* emitFxRstor(uint64_t addr);
    static X86Instruction* emitFxSaveReg(uint8_t reg);
    static X86Instruction* emitFxRstorReg(uint8_t reg);

    static X86Instruction* emitMoveTLSOffsetToReg(uint32_t imm, uint8_t dest);
    static X86Instruction* emitMoveThreadIdToReg(uint8_t dest);
    static X86Instruction* emitCompareImmReg(uint64_t imm, uint8_t reg);

    static X86Instruction* emitMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static X86Instruction* emitStackPush(uint32_t idx);
    static X86Instruction* emitStackPop(uint32_t idx);
    static X86Instruction* emitIndirectRelativeJump(uint64_t addr, uint64_t tgt);
    static X86Instruction* emitStackPush4Byte(uint32_t idx);
    static X86Instruction* emitRegAddImm(uint8_t, uint32_t);
    static X86Instruction* emitRegSubImm(uint8_t, uint32_t);
    static X86Instruction* emitMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t, bool);
    static X86Instruction* emitMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static X86Instruction* emitMoveImmToReg(uint64_t imm, uint32_t idx);
    static X86Instruction* emitMoveImm64ToReg(uint64_t imm, uint32_t idx);
    static X86Instruction* emitMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest);
    static X86Instruction* emitAddImmByteToRegaddrImm(uint8_t byte, uint8_t reg, uint32_t imm);
    static X86Instruction* emitAddImmToRegaddrImm(uint32_t b, uint8_t reg, uint32_t imm);
    static X86Instruction* emitMoveImmToRegaddrImm(uint64_t, uint32_t, uint64_t);

    static X86Instruction* emitMoveRegToMem(uint32_t idx, uint64_t addr);
    static X86Instruction* emitMoveMemToReg(uint64_t addr, uint32_t idx, bool is64bit);
    static X86Instruction* emitMoveImmToMem(uint64_t imm, uint64_t addr);

    static X86Instruction* emitMoveMemToXMMReg(uint64_t addr, uint32_t idx);

    static X86Instruction* emitShiftLeftLogical(uint8_t imm, uint8_t reg);
    static X86Instruction* emitShiftRightLogical(uint8_t imm, uint8_t reg);

    static X86Instruction* emitAddImmByteToMem(uint8_t, uint64_t);
    static X86Instruction* emitAddImmToMem(uint32_t, uint64_t);
    static X86Instruction* emitSubImmByteToMem(uint8_t, uint64_t);
    static X86Instruction* emitXorRegReg(uint8_t, uint8_t);
    static X86Instruction* emitRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg);
    static X86Instruction* emitRegImmMultReg(uint32_t src, uint32_t imm, uint32_t dest);
    static X86Instruction* emitLoadRipImmToReg(uint32_t imm, uint32_t destreg);
    static X86Instruction* emitMoveRegToReg(uint32_t srcreg, uint32_t destreg);
    static X86Instruction* emitLoadRegImmReg(uint8_t src, uint64_t imm, uint8_t dest);
    static X86Instruction* emitLoadRipImmReg(uint64_t imm, uint8_t dest);

    static Vector<X86Instruction*>* emitAddressComputation(X86Instruction* instruction, uint32_t dest);
    static X86Instruction* emitLoadEffectiveAddress(OperandX86* op, uint32_t dest);
    static X86Instruction* emitLoadEffectiveAddress(uint32_t baseReg, uint32_t indexReg, uint8_t scale, uint64_t value, uint32_t dest, bool hasBase, bool hasIndex);

    static X86Instruction* emitMoveSegmentRegToReg(uint32_t src, uint32_t dest);
    static X86Instruction* emitRegAndReg(uint32_t, uint32_t);
    static X86Instruction* emitImmAndReg(uint32_t, uint8_t);
    static X86Instruction* emitRegOrReg(uint32_t, uint32_t);
    static X86Instruction* emitImmOrReg(uint32_t, uint8_t);
};

class X86InstructionFactory32 : public X86InstructionFactory {
private:
    static X86Instruction* emitInstructionBase(uint32_t sz, char* buf) __attribute__ ((noinline));

public:
    static X86Instruction* assemble(const char* buf);

    static X86Instruction* emitFxSave(uint64_t addr);
    static X86Instruction* emitFxRstor(uint64_t addr);

    static Vector<X86Instruction*>* emitAddressComputation(X86Instruction* instruction, uint32_t dest);
    static X86Instruction* emitLoadEffectiveAddress(OperandX86* op, uint32_t dest);
    static X86Instruction* emitLoadEffectiveAddress(uint32_t baseReg, uint32_t indexReg, uint8_t scale, uint64_t value, uint32_t dest, bool hasBase, bool hasIndex);
    static X86Instruction* emitMoveSegmentRegToReg(uint32_t src, uint32_t dest);

    static X86Instruction* emitExchangeMemReg(uint64_t addr, uint8_t idx);

    static X86Instruction* emitCompareImmReg(uint64_t imm, uint8_t reg);
    static X86Instruction* emitLoadRegImmReg(uint8_t src, uint64_t imm, uint8_t dest);

    static X86Instruction* emitMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static X86Instruction* emitJumpIndirect(uint64_t tgt);
    static X86Instruction* emitStackPush(uint32_t idx);
    static X86Instruction* emitStackPop(uint32_t idx);

    static X86Instruction* emitRegAddImm(uint8_t idx, uint32_t imm);
    static X86Instruction* emitRegSubImm(uint8_t idx, uint32_t imm);
    static X86Instruction* emitXorRegReg(uint8_t, uint8_t);

    static X86Instruction* emitMoveRegToMem(uint32_t idx, uint64_t addr);
    static X86Instruction* emitMoveMemToReg(uint64_t addr, uint32_t idx);
    static X86Instruction* emitMoveImmToMem(uint64_t imm, uint64_t addr);

    static X86Instruction* emitMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static X86Instruction* emitMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);

    static X86Instruction* emitShiftLeftLogical(uint8_t imm, uint8_t reg);
    static X86Instruction* emitShiftRightLogical(uint8_t imm, uint8_t reg);

    static X86Instruction* emitAddImmByteToMem(uint8_t, uint64_t);
    static X86Instruction* emitSubImmByteToMem(uint8_t, uint64_t);
    static X86Instruction* emitRegAddReg2OpForm(uint32_t srcdestreg, uint32_t srcreg);
    static X86Instruction* emitRegImm1ByteMultReg(uint32_t src, uint8_t imm, uint32_t dest);
    static X86Instruction* emitMoveRegToReg(uint32_t srcreg, uint32_t destreg);
};

#endif /* _X86InstructionFactory_h_ */
