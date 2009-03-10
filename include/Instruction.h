#ifndef _Instruction_h_
#define _Instruction_h_

#include <Base.h>
#include <Vector.h>

class AddressAnchor;
class BinaryOutputFile;
class Disassembler;
class ElfFile;
class Function;
class RawSection;
class TextSection;

#define MAX_DISASM_STR_LENGTH 80
#define INVALID_OPCODE_INDEX 0xffffffff
#define MAX_OPERANDS 3
#define JUMP_TARGET_OPERAND 2
#define JUMP_TABLE_REACHES 0x1000

static char* instruction_without_dis = "<__no_info__x86_instrumentor>";

enum x86_insn_format {
    x86_insn_format_unknown = 0,
    x86_insn_format_onebyte,
    x86_insn_format_twobyte,
    x86_insn_format_groups,
    x86_insn_format_prefix_user_table,
    x86_insn_format_x86_64,
    x86_insn_format_float_mem,
    x86_insn_format_float_reg,
    x86_insn_format_float_groups,
    x86_insn_format_Total
};

enum x86_insn_type {
    x86_insn_type_unknown = 0,
    x86_insn_type_bad,
    x86_insn_type_cond_branch,
    x86_insn_type_branch,
    x86_insn_type_call,
    x86_insn_type_return,
    x86_insn_type_int,
    x86_insn_type_float,
    x86_insn_type_simd,
    x86_insn_type_io,
    x86_insn_type_prefetch,
    x86_insn_type_syscall,
    x86_insn_type_halt,
    x86_insn_type_hwcount,
    x86_insn_type_noop,
    x86_insn_type_trap,
    x86_insn_type_Total
};

enum x86_operand_type {
    x86_operand_type_unused = 0,        // 0
    x86_operand_type_immrel,
    x86_operand_type_reg,
    x86_operand_type_imreg,
    x86_operand_type_imm,
    x86_operand_type_mem,               // 5
    x86_operand_type_func_ST,
    x86_operand_type_func_STi,
    x86_operand_type_func_indirE,
    x86_operand_type_func_E,
    x86_operand_type_func_G,            // 10
    x86_operand_type_func_IMREG,
    x86_operand_type_func_I,
    x86_operand_type_func_I64,
    x86_operand_type_func_sI,
    x86_operand_type_func_J,            // 15
    x86_operand_type_func_SEG,    
    x86_operand_type_func_DIR,    
    x86_operand_type_func_OFF,    
    x86_operand_type_func_OFF64,    
    x86_operand_type_func_ESreg,        // 20  
    x86_operand_type_func_DSreg,    
    x86_operand_type_func_C,    
    x86_operand_type_func_D,    
    x86_operand_type_func_T,    
    x86_operand_type_func_Rd,           // 25
    x86_operand_type_func_MMX,
    x86_operand_type_func_XMM,
    x86_operand_type_func_EM,
    x86_operand_type_func_EX,
    x86_operand_type_func_MS,           // 30
    x86_operand_type_func_XS,
    x86_operand_type_func_3DNowSuffix,
    x86_operand_type_func_SIMD_Suffix,
    x86_operand_type_func_SIMD_Fixup,
    x86_operand_type_Total              // 35
};


class Operand {
protected:
    uint32_t type;
    uint64_t value;
    uint32_t bytePosition;
    uint32_t bytesUsed;
    bool relative;

    uint32_t index;
public:
    Operand(uint32_t type, uint64_t value, uint32_t idx);
    Operand(uint32_t idx);
    Operand();
    ~Operand() {}

    uint32_t getType() { return type; }
    uint64_t getValue() { return value; }
    uint32_t getBytePosition() { return bytePosition; }
    uint32_t getBytesUsed() { return bytesUsed; }
    bool isRelative() { return relative; }
    bool isIndirect();

    uint32_t setType(uint32_t typ);
    uint64_t setValue(uint64_t val);
    uint32_t setBytePosition(uint32_t pos);
    uint32_t setBytesUsed(uint32_t usd);
    bool setRelative(bool rel);

    void print();
};

class Instruction : public Base {
protected:
    uint32_t index;
    char* rawBytes;
    uint32_t instructionType;
    char disassembledString[MAX_DISASM_STR_LENGTH];
    Operand operands[MAX_OPERANDS];    
    bool leader;

    uint64_t programAddress;
    ByteSources source;
    TextSection* textSection;
    AddressAnchor* addressAnchor;

    static Instruction* generateInstructionBase(uint32_t sz, char* buf);

public:

    Instruction();
    Instruction(TextSection* text, uint64_t baseAddr, char* buff, ByteSources src, uint32_t idx);
    ~Instruction();

    bool isJumpTableBase();
    void computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList);
    uint64_t findJumpTableBaseAddress(Vector<Instruction*>* functionInstructions);
    void binutilsPrint(FILE* stream);

    uint64_t findInstrumentationPoint(uint32_t size, InstLocations loc);
    void print();
    bool verify();

    void setLeader(bool lead) { leader = lead; }
    bool isLeader() { return leader; }

    void initializeAnchor(Base* link);
    void deleteAnchor();
    bool usesRelativeAddress();
    bool usesIndirectAddress();
    uint64_t getRelativeValue();
    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    
    char* charStream() { return rawBytes; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t convertTo4ByteOperand();

    uint32_t getIndex() { return index; }
    uint32_t bytesUsedForTarget();
    uint64_t getTargetAddress();
    uint64_t getBaseAddress();
    char* getBytes();
    Operand getOperand(uint32_t idx);
    uint32_t getInstructionType() { return instructionType; }
    bool isRelocatable();
    bool controlFallsThrough();
    ByteSources getByteSource();
    uint64_t getProgramAddress();

    // control instruction id
    bool isControl();
    bool usesControlTarget();
    bool isUnconditionalBranch() { return (instructionType == x86_insn_type_branch); }
    bool isConditionalBranch() { return (instructionType == x86_insn_type_cond_branch); }
    bool isReturn() { return (instructionType == x86_insn_type_return); }
    bool isFunctionCall() { return (instructionType == x86_insn_type_call); }
    bool isSystemCall() { return (instructionType == x86_insn_type_syscall); }
    bool isHalt() { return (instructionType == x86_insn_type_halt); }
    bool isIndirectBranch();
    uint32_t getIndirectBranchTarget();

    bool isNoop();

    uint32_t setIndex(uint32_t newidx);
    void setBaseAddress(uint64_t addr);
    uint32_t setSizeInBytes(uint32_t len);
    char* setBytes(char* bytes);
    uint64_t setOperandValue(uint32_t idx, uint64_t val);
    uint32_t setOperandType(uint32_t idx, uint32_t typ);
    uint32_t setOperandBytePosition(uint32_t idx, uint32_t pos);
    uint32_t setOperandBytesUsed(uint32_t idx, uint32_t usd);
    bool setOperandRelative(uint32_t idx, bool rel);
    ByteSources setByteSource(ByteSources src);
    uint64_t setProgramAddress(uint64_t addr);
    void setInstructionType(uint32_t typ) { instructionType = typ; }

    char* setDisassembledString(char* disStr);

    uint32_t setOpcodeType(uint32_t formatType, uint32_t idx1, uint32_t idx2);

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
    static Instruction* generateMoveRegToMem(uint32_t idx, uint64_t addr);
    static Instruction* generateJumpRelative(uint64_t addr, uint64_t tgt);
    static Instruction* generateStackPushImmediate(uint64_t imm);
    static Instruction* generateCallRelative(uint64_t addr, uint64_t tgt);
    static Instruction* generateRegAddImmediate(uint32_t idx, uint64_t imm);
    static Instruction* generateRegIncrement(uint32_t idx);
    static Instruction* generateRegSubImmediate(uint32_t idx, uint64_t imm);
    static Instruction* generateAddByteToRegaddr(uint8_t byt, uint32_t idx);

    static Instruction* generateMoveRegToRegaddr(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateInterrupt(uint8_t idx);

    static uint32_t computeOpcodeTypeOneByte(uint32_t idx);
    static uint32_t computeOpcodeTypeTwoByte(uint32_t idx);
    static uint32_t computeOpcodeTypeGroups(uint32_t idx1, uint32_t idx2);
    static uint32_t computeOpcodeTypePrefixUser(uint32_t idx1, uint32_t idx2);
    static uint32_t computeOpcodeTypeX8664(uint32_t idx1, uint32_t idx2);

};

class Instruction64 : public Instruction {
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
    static Instruction* generateMoveImmToReg(uint64_t imm, uint32_t idx);

};

class Instruction32 : public Instruction {
public:
    static Instruction* generateMoveRegaddrToReg(uint32_t srcidx, uint32_t destidx);
    static Instruction* generateJumpIndirect(uint64_t tgt);
    static Instruction* generateStackPush(uint32_t idx);
    static Instruction* generateStackPop(uint32_t idx);

    static Instruction* generateRegAddImmediate(uint32_t idx, uint64_t imm);
    static Instruction* generateRegSubImmediate(uint32_t idx, uint64_t imm);
};

extern int searchInstructionAddress(const void* arg1,const void* arg2);
extern int compareInstructionAddress(const void* arg1,const void* arg2);

#endif /* _Instruction_h_ */
