#ifndef _Udis_h_
#define _Udis_h_

#include <AddressAnchor.h>
#include <Base.h>
#include <RawSection.h>
#include <libudis86/syn.h>
#include <udis86.h>
#include <defines/Udis.d>

class Function;
class TextSection;

#define MAX_DISASM_STR_LENGTH 80
#define INVALID_OPCODE_INDEX 0xffffffff
#define MAX_OPERANDS 3
#define JUMP_TARGET_OPERAND 2
#define JUMP_TABLE_REACHES 0x1000

#define IS_8BIT_GPR(__val) ((__val >= UD_R_AL) && (__val <= UD_R_R15B))
#define IS_16BIT_GPR(__val) ((__val >= UD_R_AX) && (__val <= UD_R_R15W))
#define IS_32BIT_GPR(__val) ((__val >= UD_R_EAX) && (__val <= UD_R_R15D))
#define IS_64BIT_GPR(__val) ((__val >= UD_R_RAX) && (__val <= UD_R_R15))
#define IS_SEGMENT_REG(__val) ((__val >= UD_R_ES) && (__val <= UD_R_GS))
#define IS_CONTROL_REG(__val) ((__val >= UD_R_CR0) && (__val <= UD_R_CR15))
#define IS_DEBUG_REG(__val) ((__val >= UD_R_DR0) && (__val <= UD_R_DR15))
#define IS_MMX_REG(__val) ((__val >= UD_R_MM0) && (__val <= UD_R_MM7))
#define IS_X87_REG(__val) ((__val >= UD_R_ST0) && (__val <= UD_R_ST7))
#define IS_XMM_REG(__val) ((__val >= UD_R_XMM0) && (__val <= UD_R_XMM15))
#define IS_PC_REG(__val) (__val == UD_R_RIP)
#define IS_OPERAND_TYPE(__val) ((__val >= UD_OP_REG) && (__val <= UD_OP_CONST))

#define IS_GPR(__val) (IS_8BIT_GPR(__val) || IS_16BIT_GPR(__val) || IS_32BIT_GPR(__val) || IS_64BIT_GPR(__val))
#define IS_REG(__val) (IS_GPR(__val) || IS_SEGMENT_REG(__val) || IS_CONTROL_REG(__val) || IS_DEBUG_REG(__val) || \
                       IS_MMX_REG(__val) || IS_X87_REG(__val) || IS_XMM_REG(__val) || IS_PC_REG(__val))

enum X86InstructionType {
    X86InstructionType_unknown = 0,
    X86InstructionType_invalid,
    X86InstructionType_cond_branch,
    X86InstructionType_uncond_branch,
    X86InstructionType_call,
    X86InstructionType_return,
    X86InstructionType_int,
    X86InstructionType_float,
    X86InstructionType_string,
    X86InstructionType_simd,
    X86InstructionType_io,
    X86InstructionType_prefetch,
    X86InstructionType_system_call,
    X86InstructionType_halt,
    X86InstructionType_hwcount,
    X86InstructionType_nop,
    X86InstructionType_trap,
    X86InstructionType_vmx,
    X86InstructionType_special,
    X86InstructionType_Total
};


typedef enum {
    RegType_undefined = 0,
    RegType_8Bit,
    RegType_16Bit,
    RegType_32Bit,
    RegType_64Bit,
    RegType_Segment,
    RegType_Control,
    RegType_Debug,
    RegType_MMX,
    RegType_X87,
    RegType_XMM,
    RegType_PC,
    RegType_Total_Types
} RegTypes;

extern uint32_t regbase_to_type(uint32_t base);

class UD_OPERAND_CLASS {
private:
    struct ud_operand entry;
    uint32_t registerType;
    uint8_t operandIndex;

public:
    OPERAND_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    UD_OPERAND_CLASS(struct ud_operand* init, uint32_t idx);
    ~UD_OPERAND_CLASS() {}

    void print();
    char* charStream() { return (char*)&entry; }
    bool verify();

    bool isRelative();
    uint32_t getBytesUsed() { return (GET(size) >> 3); }
    uint32_t getBytePosition() { return 0; }
    uint32_t getType() { return 0; }
    uint32_t getValue() { return 0; }

};

class UD_INSTRUCTION_CLASS : public Base {
private:
    struct ud entry;
    UD_OPERAND_CLASS** operands;
    uint32_t instructionIndex;
    char* rawBytes;

    uint8_t byteSource;
    AddressAnchor* addressAnchor;
    bool leader;
    TextSection* textSection;

public:
    INSTRUCTION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    UD_INSTRUCTION_CLASS(struct ud* init);
    UD_INSTRUCTION_CLASS(TextSection* text, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx, bool doReformat);
    ~UD_INSTRUCTION_CLASS();

    UD_OPERAND_CLASS* getOperand(uint32_t idx);

    void print();
    bool verify();

    void setBaseAddress(uint64_t addr) { baseAddress = addr; }
    uint32_t getSizeInBytes() { return sizeInBytes; }
    uint32_t getIndex() { return instructionIndex; }
    void setIndex(uint32_t idx) { instructionIndex = idx; }
    uint32_t getInstructionType();

    bool controlFallsThrough();

    // control instruction id
    bool isControl();
    bool isUnconditionalBranch() { return (getInstructionType() == X86InstructionType_uncond_branch); }
    bool isConditionalBranch() { return (getInstructionType() == X86InstructionType_cond_branch); }
    bool isReturn() { return (getInstructionType() == X86InstructionType_return); }
    bool isFunctionCall() { return (getInstructionType() == X86InstructionType_call); }
    bool isSystemCall() { return (getInstructionType() == X86InstructionType_system_call); }
    bool isHalt() { return (getInstructionType() == X86InstructionType_halt); }
    bool isIndirectBranch();
    uint32_t getIndirectBranchTarget();

    bool isNoop() { return (getInstructionType() == X86InstructionType_nop); }

    uint8_t getByteSource() { return byteSource; }
    bool isRelocatable() { return true; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    bool usesIndirectAddress(); 

    void initializeAnchor(Base*);

    bool isJumpTableBase();
    uint64_t findJumpTableBaseAddress(Vector<Instruction*>* functionInstructions);
    void computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList);
    void setSizeInBytes(uint32_t sz) { sizeInBytes = sz; }
    void setLeader(bool ldr) { leader = ldr; }
    bool isLeader() { return leader; }

    uint64_t getBaseAddress() { return baseAddress; }


    bool usesControlTarget();
    bool usesRelativeAddress() { return false; }
    uint64_t getRelativeValue() { return 0; }
    uint64_t getTargetAddress() { return getBaseAddress() + getSizeInBytes(); }
    uint32_t bytesUsedForTarget() { return 0; }
    void convertTo4ByteOperand() {}
    void binutilsPrint(FILE* stream) {}
    void setBytes();

};

class UD_INSTRUCTIONGENERATOR_CLASS : public UD_INSTRUCTION_CLASS {
protected:
    static UD_INSTRUCTION_CLASS* generateInstructionBase(uint32_t sz, char* buf);

public:
    static UD_INSTRUCTION_CLASS* generateNoop();

    static UD_INSTRUCTION_CLASS* generateStringMove(bool repeat);
    static UD_INSTRUCTION_CLASS* generateSetDirectionFlag(bool backward);
    static UD_INSTRUCTION_CLASS* generateMoveImmToSegmentReg(uint64_t imm, uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateSTOSByte(bool repeat);

    static UD_INSTRUCTION_CLASS* generateReturn();
    static UD_INSTRUCTION_CLASS* generatePushEflags();
    static UD_INSTRUCTION_CLASS* generatePopEflags();
    static UD_INSTRUCTION_CLASS* generatePushSegmentReg(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generatePopSegmentReg(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateMoveImmByteToMemIndirect(uint8_t byt, uint64_t off, uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateMoveImmByteToReg(uint8_t imm, uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateJumpRelative(uint64_t addr, uint64_t tgt);
    static UD_INSTRUCTION_CLASS* generateStackPushImmediate(uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateCallRelative(uint64_t addr, uint64_t tgt);
    static UD_INSTRUCTION_CLASS* generateRegAddImmediate(uint32_t idx, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateRegIncrement(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateRegSubImmediate(uint32_t idx, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateAddByteToRegaddr(uint8_t byt, uint32_t idx);

    static UD_INSTRUCTION_CLASS* generateMoveRegToRegaddr(uint32_t srcidx, uint32_t destidx);
    static UD_INSTRUCTION_CLASS* generateInterrupt(uint8_t idx);

    static UD_INSTRUCTION_CLASS* generateAndImmReg(uint64_t, uint32_t);
};

class InstructionGenerator64 : public UD_INSTRUCTIONGENERATOR_CLASS {
private:
    static UD_INSTRUCTION_CLASS* generateMoveRegToRegaddrImm4Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateMoveRegToRegaddrImm1Byte(uint32_t idxsrc, uint32_t idxdest, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateMoveRegToRegaddr(uint32_t idxsrc, uint32_t idxdest);

    static UD_INSTRUCTION_CLASS* generateRegSubImmediate4Byte(uint32_t idx, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateRegSubImmediate1Byte(uint32_t idx, uint64_t imm);

    static UD_INSTRUCTION_CLASS* generateRegAddImmediate4Byte(uint32_t idx, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateRegAddImmediate1Byte(uint32_t idx, uint64_t imm);

public:
    static UD_INSTRUCTION_CLASS* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static UD_INSTRUCTION_CLASS* generateStackPush(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateStackPop(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateIndirectRelativeJump(uint64_t addr, uint64_t tgt);
    static UD_INSTRUCTION_CLASS* generateStackPush4Byte(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateRegAddImmediate(uint32_t, uint64_t);
    static UD_INSTRUCTION_CLASS* generateRegSubImmediate(uint32_t, uint64_t);
    static UD_INSTRUCTION_CLASS* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);
    static UD_INSTRUCTION_CLASS* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static UD_INSTRUCTION_CLASS* generateMoveImmToReg(uint64_t imm, uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateMoveImmToRegaddrImm(uint64_t immval, uint32_t idx, uint64_t immoff);

    static UD_INSTRUCTION_CLASS* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static UD_INSTRUCTION_CLASS* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static UD_INSTRUCTION_CLASS* generateStoreEflagsToAH();
    static UD_INSTRUCTION_CLASS* generateLoadEflagsFromAH();

    static UD_INSTRUCTION_CLASS* generateAddImmByteToMem(uint8_t, uint64_t);
    static UD_INSTRUCTION_CLASS* generateXorRegReg(uint8_t, uint8_t);
};

class InstructionGenerator32 : public UD_INSTRUCTIONGENERATOR_CLASS {
public:
    static UD_INSTRUCTION_CLASS* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static UD_INSTRUCTION_CLASS* generateJumpIndirect(uint64_t tgt);
    static UD_INSTRUCTION_CLASS* generateStackPush(uint32_t idx);
    static UD_INSTRUCTION_CLASS* generateStackPop(uint32_t idx);

    static UD_INSTRUCTION_CLASS* generateRegAddImmediate(uint32_t idx, uint64_t imm);
    static UD_INSTRUCTION_CLASS* generateRegSubImmediate(uint32_t idx, uint64_t imm);

    static UD_INSTRUCTION_CLASS* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static UD_INSTRUCTION_CLASS* generateMoveMemToReg(uint64_t addr, uint32_t idx);

    static UD_INSTRUCTION_CLASS* generateMoveRegaddrImmToReg(uint32_t, uint64_t, uint32_t);
    static UD_INSTRUCTION_CLASS* generateMoveRegToRegaddrImm(uint32_t, uint32_t, uint64_t);

    static UD_INSTRUCTION_CLASS* generateStoreEflagsToAH();
    static UD_INSTRUCTION_CLASS* generateLoadEflagsFromAH();

    static UD_INSTRUCTION_CLASS* generateAddImmByteToMem(uint8_t, uint64_t);
};

#endif /* _Udis_h_ */
