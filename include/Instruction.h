#ifndef _Instruction_h_
#define _Instruction_h_

#include <AddressAnchor.h>
#include <Base.h>
#include <BitSet.h>
#include <RawSection.h>
#include <libudis86/syn.h>
#include <udis86.h>
#include <defines/Instruction.d>

class ElfFileInst;
class Function;
class TextObject;

#define MAX_OPERANDS 3
#define JUMP_TARGET_OPERAND 0
#define JUMP_TABLE_REACHES 0x1000
#define DISASSEMBLY_MODE UD_SYN_ATT
#define MAX_X86_INSTRUCTION_LENGTH 20
#define MIN_CONST_MEMADDR 0x10000

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


// my non-gnu definitions for X86
#define X86_32BIT_GPRS 8
#define X86_64BIT_GPRS 16
#define X86_REG_AX 0
#define X86_REG_CX 1
#define X86_REG_DX 2
#define X86_REG_BX 3
#define X86_REG_SP 4
#define X86_REG_BP 5
#define X86_REG_SI 6
#define X86_REG_DI 7
#define X86_REG_R8 8
#define X86_REG_R9 9
#define X86_REG_R10 10
#define X86_REG_R11 11
#define X86_REG_R12 12
#define X86_REG_R13 13
#define X86_REG_R14 14
#define X86_REG_R15 15

#define X86_SEGMENT_REGS 6
#define X86_SEGREG_ES 0
#define X86_SEGREG_CS 1
#define X86_SEGREG_SS 2
#define X86_SEGREG_DS 3
#define X86_SEGREG_FS 4
#define X86_SEGREG_GS 5

#define X86TRAPCODE_BREAKPOINT   3
#define X86TRAPCODE_OVERFLOW     4


struct ud_itab_entry_operand
{
    uint32_t type;
    uint32_t size;
};
struct ud_itab_entry
{
    enum ud_mnemonic_code         mnemonic;
    struct ud_itab_entry_operand  operand1;
    struct ud_itab_entry_operand  operand2;
    struct ud_itab_entry_operand  operand3;
    uint32_t                      prefix;
};


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

class Operand {
private:
    struct ud_operand entry;
    Instruction* instruction;
    uint32_t operandIndex;

public:
    OPERAND_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    Operand(Instruction* inst, struct ud_operand* init, uint32_t idx);
    ~Operand() {}

    Instruction* getInstruction() { return instruction; }

    void print();
    char* charStream() { return (char*)&entry; }
    bool verify();

    uint32_t getBytesUsed();
    uint32_t getBytePosition();
    uint32_t getBaseRegister();
    uint32_t getIndexRegister();

    void touchedRegisters(BitSet<uint32_t>* regs);
    bool isRelative();
    uint32_t getType() { return GET(type); }
    int64_t getValue();

};

class MemoryOperand {
private:
    Operand* operand;
    ElfFileInst* elfFileInst;

public:
    MemoryOperand(Operand* op, ElfFileInst* elfInst);
    ~MemoryOperand() {}

    Operand* getOperand() { return operand; }
    uint32_t getMemoryRegister();
};

class Instruction : public Base {
private:
    struct ud entry;
    Operand** operands;
    uint32_t instructionIndex;

    uint8_t byteSource;
    uint64_t programAddress;
    AddressAnchor* addressAnchor;
    bool leader;
    TextObject* container;

    bool isExplicitMemoryOperation();    
    bool isImplicitMemoryOperation();    

public:
    INSTRUCTION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    Instruction(struct ud* init);
    Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx);
    ~Instruction();

    Operand* getOperand(uint32_t idx);
    TextObject* getContainer() { return container; }

    void print();
    bool verify();

    void setBaseAddress(uint64_t addr) { baseAddress = addr; }
    uint32_t getSizeInBytes() { return sizeInBytes; }
    uint32_t getIndex() { return instructionIndex; }
    void setIndex(uint32_t idx) { instructionIndex = idx; }
    uint32_t getInstructionType();
    uint64_t getProgramAddress() { return programAddress; }

    void touchedRegisters(BitSet<uint32_t>* regs);
    bool controlFallsThrough();

    // control instruction id
    bool isControl();
    bool isUnconditionalBranch() { return (getInstructionType() == X86InstructionType_uncond_branch); }
    bool isConditionalBranch() { return (getInstructionType() == X86InstructionType_cond_branch); }
    bool isReturn() { return (getInstructionType() == X86InstructionType_return); }
    bool isFunctionCall() { return (getInstructionType() == X86InstructionType_call); }
    bool isSystemCall() { return (getInstructionType() == X86InstructionType_system_call); }
    bool isHalt() { return (getInstructionType() == X86InstructionType_halt); }
    bool isNoop() { return (getInstructionType() == X86InstructionType_nop); }
    bool isConditionCompare();

    uint8_t getByteSource() { return byteSource; }
    bool isRelocatable() { return true; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    void initializeAnchor(Base*);

    bool isJumpTableBase();
    uint64_t findJumpTableBaseAddress(Vector<Instruction*>* functionInstructions);
    void computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList);
    void setSizeInBytes(uint32_t sz) { sizeInBytes = sz; }
    void setLeader(bool ldr) { leader = ldr; }
    bool isLeader() { return leader; }

    uint64_t getBaseAddress() { return baseAddress; }
    bool usesControlTarget();

    bool usesIndirectAddress(); 
    bool usesRelativeAddress();
    int64_t getRelativeValue();
    uint64_t getTargetAddress();
    uint32_t bytesUsedForTarget();
    uint32_t convertTo4ByteTargetOperand();
    void binutilsPrint(FILE* stream);
    void setBytes();
    bool isIndirectBranch();
    uint32_t getIndirectBranchTarget();

    bool isFloatPOperation();
    bool isIntegerOperation();
    bool isStringOperation();
    bool isMemoryOperation();    
    Operand* getMemoryOperand();
};

#endif /* _Instruction_h_ */
