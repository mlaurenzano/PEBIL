#ifndef _X86Instruction_h_
#define _X86Instruction_h_

#include <AddressAnchor.h>
#include <Base.h>
#include <BitSet.h>
#include <RawSection.h>
#include <libudis86/syn.h>
#include <udis86.h>
#include <defines/X86Instruction.d>

class ElfFileInst;
class Function;
class TextObject;

#define MAX_OPERANDS 3
#define JUMP_TARGET_OPERAND 0
#define COMP_DEST_OPERAND 0
#define COMP_SRC_OPERAND 1
#define JUMP_TABLE_REACHES 0x1000
#define DISASSEMBLY_MODE UD_SYN_ATT
#define MAX_X86_INSTRUCTION_LENGTH 20
#define MIN_CONST_MEMADDR 0x10000

#define UD_R_NAME_LOOKUP(__ud_reg) (ud_reg_tab[__ud_reg - 1])
#define UD_OP_NAME_LOOKUP(__ud_type) (ud_optype_str[__ud_type - UD_OP_REG])

#define IS_8BIT_GPR(__reg) ((__reg >= UD_R_AL) && (__reg <= UD_R_R15B))
#define IS_16BIT_GPR(__reg) ((__reg >= UD_R_AX) && (__reg <= UD_R_R15W))
#define IS_32BIT_GPR(__reg) ((__reg >= UD_R_EAX) && (__reg <= UD_R_R15D))
#define IS_64BIT_GPR(__reg) ((__reg >= UD_R_RAX) && (__reg <= UD_R_R15))
#define IS_SEGMENT_REG(__reg) ((__reg >= UD_R_ES) && (__reg <= UD_R_GS))
#define IS_CONTROL_REG(__reg) ((__reg >= UD_R_CR0) && (__reg <= UD_R_CR15))
#define IS_DEBUG_REG(__reg) ((__reg >= UD_R_DR0) && (__reg <= UD_R_DR15))
#define IS_MMX_REG(__reg) ((__reg >= UD_R_MM0) && (__reg <= UD_R_MM7))
#define IS_X87_REG(__reg) ((__reg >= UD_R_ST0) && (__reg <= UD_R_ST7))
#define IS_XMM_REG(__reg) ((__reg >= UD_R_XMM0) && (__reg <= UD_R_XMM15))
#define IS_PC_REG(__reg) (__reg == UD_R_RIP)
#define IS_OPERAND_TYPE(__opr) ((__opr >= UD_OP_REG) && (__opr <= UD_OP_CONST))

#define IS_GPR(__reg) (IS_8BIT_GPR(__reg) || IS_16BIT_GPR(__reg) || IS_32BIT_GPR(__reg) || IS_64BIT_GPR(__reg))
#define IS_REG(__reg) (IS_GPR(__reg) || IS_SEGMENT_REG(__reg) || IS_CONTROL_REG(__reg) || IS_DEBUG_REG(__reg) || \
                       IS_MMX_REG(__reg) || IS_X87_REG(__reg) || IS_XMM_REG(__reg) || IS_PC_REG(__reg))

#define IS_LOADADDR(__mne) (__mne == UD_Ilea)
#define IS_PREFETCH(__mne) (__mne == UD_Iprefetch || __mne == UD_Iprefetchnta || __mne == UD_Iprefetcht0 || \
                            __mne == UD_Iprefetcht1 || __mne == UD_Iprefetcht2)

#define ANALYSIS_MAXREG 32

#define X86_FLAG_CF 0
#define X86_FLAG_PF 2
#define X86_FLAG_AF 4
#define X86_FLAG_ZF 6
#define X86_FLAG_SF 7
#define X86_FLAG_TF 8
#define X86_FLAG_IF 9
#define X86_FLAG_DF 10
#define X86_FLAG_OF 11
#define X86_FLAG_IOPL1 12
#define X86_FLAG_IOPL2 13
#define X86_FLAG_NT 14
#define X86_FLAG_RF 16
#define X86_FLAG_VF 17
#define X86_FLAG_AC 18
#define X86_FLAG_VI 19
#define X86_FLAG_VP 20
#define X86_FLAG_ID 21

#define __flag_reserved "reserved"
const static char* flag_name_map[32] = { "carry", __flag_reserved, "parity", __flag_reserved, 
                           "adjust", __flag_reserved, "zero", "sign",
                           "trap", "interrupt", "direction", "overflow",
                           "iopl1", "iopl2", "nested_task", __flag_reserved,
                           "resume", "v8086", "alignchk", "vint",
                           "vint_pending", "ident", __flag_reserved, __flag_reserved,
                           __flag_reserved, __flag_reserved, __flag_reserved, __flag_reserved,
                           __flag_reserved, __flag_reserved, __flag_reserved, __flag_reserved };

#define __flag_mask__protect_none  0x11111111
#define __flag_mask__protect_light 0x11111100
#define __flag_mask__protect_full  0x11110000
#define __x86_flagset_alustd       (__bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_OF))

#define __flags_use 0
#define __flags_def 1

#define __flags_define(__array, __mnemonic, __use, __def) \
    if (GET(mnemonic) == __mnemonic) { __array[__flags_use] = __use; __array[__flags_def] = __def; }

// my non-gnu definitions for X86
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
#define X86_32BIT_GPRS 8
#define X86_64BIT_GPRS 16

#define X86_SEGREG_ES 0
#define X86_SEGREG_CS 1
#define X86_SEGREG_SS 2
#define X86_SEGREG_DS 3
#define X86_SEGREG_FS 4
#define X86_SEGREG_GS 5
#define X86_SEGMENT_REGS 6

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

class OperandX86 {
private:
    struct ud_operand entry;
    X86Instruction* instruction;
    uint32_t operandIndex;

public:
    OPERAND_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    OperandX86(X86Instruction* inst, struct ud_operand* init, uint32_t idx);
    ~OperandX86() {}

    X86Instruction* getInstruction() { return instruction; }
    bool isSameOperand(OperandX86* other);

    void print();
    char* charStream() { return (char*)&entry; }
    bool verify();

    uint32_t getOperandIndex() { return operandIndex; }
    
    uint32_t getBytesUsed();
    uint32_t getBytePosition();
    uint32_t getBaseRegister();
    uint32_t getIndexRegister();

    void touchedRegisters(BitSet<uint32_t>* regs);
    bool isRelative();
    uint32_t getType() { return GET(type); }
    int64_t getValue();
};

class X86Instruction : public Base {
private:

    struct ud entry;
    BitSet<uint32_t>* liveIns;
    BitSet<uint32_t>* liveOuts;
    uint32_t* flags_usedef;
    uint32_t* impreg_usedef;

    OperandX86** operands;
    uint32_t instructionIndex;

    char* rawBytes;
    uint8_t byteSource;
    uint64_t programAddress;
    AddressAnchor* addressAnchor;
    bool leader;
    TextObject* container;
    uint32_t instructionType;
    //    uint64_t wastespace[2048];

    HashCode hashCode;

    uint32_t setInstructionType();

public:
    INSTRUCTION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    X86Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx);
    X86Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx, bool is64bit, uint32_t sz);
    ~X86Instruction();

    OperandX86* getOperand(uint32_t idx);
    TextObject* getContainer() { return container; }

    static void initializeInstructionAPIDecoder(bool is64bit);
    void setFlags();
    void setImpliedRegs();
    static void destroyInstructionAPIDecoder();
    BitSet<uint32_t>* getUseRegs();
    BitSet<uint32_t>* getDefRegs();
    bool allFlagsDeadIn();
    bool allFlagsDeadOut();
    bool isGPRegDeadIn(uint32_t idx);

    bool usesFlag(uint32_t flg);
    bool defsFlag(uint32_t flg);

    void print();
    bool verify();

    char* charStream() { return rawBytes; }

    HashCode getHashCode() { return hashCode; }
    void setLiveIns(BitSet<uint32_t>* live);
    void setLiveOuts(BitSet<uint32_t>* live);

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
    bool isBranch() { return isUnconditionalBranch() || isConditionalBranch(); }
    bool isUnconditionalBranch() { return (getInstructionType() == X86InstructionType_uncond_branch); }
    bool isConditionalBranch() { return (getInstructionType() == X86InstructionType_cond_branch); }
    bool isReturn() { return (getInstructionType() == X86InstructionType_return); }
    bool isFunctionCall() { return (getInstructionType() == X86InstructionType_call); }
    bool isSystemCall() { return (getInstructionType() == X86InstructionType_system_call); }
    bool isHalt() { return (getInstructionType() == X86InstructionType_halt); }
    bool isNop() { return (getInstructionType() == X86InstructionType_nop); }
    bool isConditionCompare();
    bool isStackPush();
    bool isStackPop();

    uint8_t getByteSource() { return byteSource; }
    bool isRelocatable() { return true; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    void initializeAnchor(Base*);

    bool isJumpTableBase();
    uint64_t findJumpTableBaseAddress(Vector<X86Instruction*>* functionInstructions);
    TableModes computeJumpTableTargets(uint64_t tableBase, Function* func, Vector<uint64_t>* addressList, Vector<uint64_t>* tableStorageEntries);
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
    bool isExplicitMemoryOperation();    
    bool isImplicitMemoryOperation();    

    OperandX86* getMemoryOperand();
};

#endif /* _X86Instruction_h_ */
