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
    uint8_t type;
    uint8_t bytePosition;
    uint8_t bytesUsed;
    uint8_t index;

    uint64_t value;
    bool relative;

public:
    Operand(uint32_t type, uint64_t value, uint32_t idx);
    Operand(uint32_t idx);
    Operand();
    ~Operand() {}

    uint8_t getType() { return type; }
    uint64_t getValue() { return value; }
    uint8_t getBytePosition() { return bytePosition; }
    uint8_t getBytesUsed() { return bytesUsed; }
    bool isRelative() { return relative; }
    bool isIndirect();

    void setType(uint8_t typ) { type = typ; }
    void setValue(uint64_t val) { value = val; }
    void setBytePosition(uint8_t pos) { bytePosition = pos; }
    void setBytesUsed(uint8_t usd);
    void setRelative(bool rel) { relative = rel; }

    void print();
};

class Instruction : public Base {
protected:
    uint8_t instructionType;
    uint8_t source;
    uint32_t index;
    char* rawBytes;
    Operand** operands;    
    bool leader;
    bool reformat;

    uint64_t programAddress;
    TextSection* textSection;
    AddressAnchor* addressAnchor;

public:

    Instruction();
    Instruction(TextSection* text, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx, bool doReformat);
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
    bool doReformat() { return reformat; }

    void initializeAnchor(Base* link);
    void deleteAnchor();
    bool usesRelativeAddress();
    bool usesIndirectAddress();
    uint64_t getRelativeValue();
    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    
    char* charStream() { return rawBytes; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    uint32_t convertTo4ByteOperand();

    uint16_t getIndex() { return index; }
    uint32_t bytesUsedForTarget();
    uint64_t getTargetAddress();
    uint64_t getBaseAddress();
    char* getBytes();
    Operand* getOperand(uint32_t idx);
    uint32_t getInstructionType() { return instructionType; }
    bool isRelocatable();
    bool controlFallsThrough();
    uint8_t getByteSource();
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

    void setIndex(uint32_t newidx) { index = newidx; ASSERT(index == newidx); }
    void setBaseAddress(uint64_t addr) { baseAddress = addr; }
    void setSizeInBytes(uint32_t len);
    void setBytes(char* bytes);
    void setOperandValue(uint32_t idx, uint64_t val);
    void setOperandType(uint32_t idx, uint8_t typ);
    void setOperandBytePosition(uint32_t idx, uint8_t pos);
    void setOperandBytesUsed(uint32_t idx, uint8_t usd);
    void setOperandRelative(uint32_t idx, bool rel);
    void setByteSource(ByteSources src) { source = src; }
    void setProgramAddress(uint64_t addr) { programAddress = addr; }
    void setInstructionType(uint32_t typ) { instructionType = typ; }

    void setOpcodeType(uint32_t formatType, uint32_t idx1, uint32_t idx2);

    static uint32_t computeOpcodeTypeOneByte(uint32_t idx);
    static uint32_t computeOpcodeTypeTwoByte(uint32_t idx);
    static uint32_t computeOpcodeTypeGroups(uint32_t idx1, uint32_t idx2);
    static uint32_t computeOpcodeTypePrefixUser(uint32_t idx1, uint32_t idx2);
    static uint32_t computeOpcodeTypeX8664(uint32_t idx1, uint32_t idx2);
};

extern int searchInstructionAddress(const void* arg1,const void* arg2);
extern int compareInstructionAddress(const void* arg1,const void* arg2);

#endif /* _Instruction_h_ */
