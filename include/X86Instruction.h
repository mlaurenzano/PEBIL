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

#ifndef _X86Instruction_h_
#define _X86Instruction_h_

#include <AddressAnchor.h>
#include <Base.h>
#include <BitSet.h>
#include <LinkedList.h>
#include <RawSection.h>
#include <libudis86/syn.h>
#include <udis86.h>
#include <defines/X86Instruction.d>
#include <map>

class BasicBlock;
class ElfFileInst;
class Function;
class TextObject;

#define JUMP_TABLE_REACHES 0x1000
#define DISASSEMBLY_MODE UD_SYN_ATT
#define MAX_X86_INSTRUCTION_LENGTH 20
#define MIN_CONST_MEMADDR 0x10000

#define MAX_OPERANDS 4

/* branches */
#define JUMP_TARGET_OPERAND 0

/* comparisons */
#define CMP_SRC1_OPERAND 0
#define CMP_SRC2_OPERAND 1

/* everything else */
#define DEST_OPERAND 0
#define SRC1_OPERAND 1
#define SRC2_OPERAND 0
#define SRC3_OPERAND 2
#define SRC4_OPERAND 3

#define UD_R_NAME_LOOKUP(__ud_reg) (ud_reg_tab[__ud_reg - 1])
#define UD_OP_NAME_LOOKUP(__ud_type) (ud_optype_str[__ud_type - UD_OP_REG])

#define IS_8BIT_GPR(__reg) (IS_L8BIT_GPR(__reg) || IS_H8BIT_GPR(__reg))
#define IS_L8BIT_GPR(__reg) ((__reg >= UD_R_AL) && (__reg <= UD_R_BL))
#define IS_H8BIT_GPR(__reg) ((__reg >= UD_R_AH) && (__reg <= UD_R_R15B))
#define IS_16BIT_GPR(__reg) ((__reg >= UD_R_AX) && (__reg <= UD_R_R15W))
#define IS_32BIT_GPR(__reg) ((__reg >= UD_R_EAX) && (__reg <= UD_R_R15D))
#define IS_64BIT_GPR(__reg) ((__reg >= UD_R_RAX) && (__reg <= UD_R_R15))
#define IS_SEGMENT_REG(__reg) ((__reg >= UD_R_ES) && (__reg <= UD_R_GS))
#define IS_CONTROL_REG(__reg) ((__reg >= UD_R_CR0) && (__reg <= UD_R_CR15))
#define IS_DEBUG_REG(__reg) ((__reg >= UD_R_DR0) && (__reg <= UD_R_DR15))
#define IS_MMX_REG(__reg) ((__reg >= UD_R_MM0) && (__reg <= UD_R_MM7))
#define IS_X87_REG(__reg) ((__reg >= UD_R_ST0) && (__reg <= UD_R_ST7))
#define IS_XMM_REG(__reg) ((__reg >= UD_R_XMM0) && (__reg <= UD_R_XMM15))
#define IS_YMM_REG(__reg) ((__reg >= UD_R_YMM0) && (__reg <= UD_R_YMM15))
#define IS_ZMM_REG(__reg) ((__reg >= UD_R_ZMM0) && (__reg <= UD_R_ZMM31))
#define IS_K_REG(__reg) ((__reg >= UD_R_K0) && (__reg <= UD_R_K7))
#define IS_PC_REG(__reg) (__reg == UD_R_RIP)
#define IS_OPERAND_TYPE(__opr) ((__opr >= UD_OP_REG) && (__opr <= UD_OP_CONST))

#define IS_GPR(__reg) (IS_8BIT_GPR(__reg) || IS_16BIT_GPR(__reg) || IS_32BIT_GPR(__reg) || IS_64BIT_GPR(__reg))
#define IS_REG(__reg) (IS_GPR(__reg) || IS_SEGMENT_REG(__reg) || IS_CONTROL_REG(__reg) || IS_DEBUG_REG(__reg) || \
                       IS_MMX_REG(__reg) || IS_X87_REG(__reg) || IS_XMM_REG(__reg) || IS_YMM_REG(__reg) || \
                       IS_ZMM_REG(__reg) || IS_K_REG(__reg) || IS_PC_REG(__reg))
#define IS_ALU_REG(__reg) (IS_GPR(__reg) || IS_XMM_REG(__reg))

#define IS_LOADADDR(__mne) (__mne == UD_Ilea)
#define IS_PREFETCH(__mne) (__mne == UD_Iprefetch || __mne == UD_Iprefetchnta || __mne == UD_Iprefetcht0 || \
                            __mne == UD_Iprefetcht1 || __mne == UD_Iprefetcht2)


#define __reg_use 0
#define __reg_def 1
#define __reg_define(__array, __mnemonic, __use, __def) \
    if (GET(mnemonic) == __mnemonic) { __array[__reg_use] = __use; __array[__reg_def] = __def; }

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
#define X86_FLAG_BITS 32

// useful flags bit sets
// doesn't require saving
#define __flag_mask__protect_none  (0)
// can be handled by lahf/sahf
#define __flag_mask__protect_light (__bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_SF))
// has to be handled by pushf/popf
#define __flag_mask__protect_full  (__bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_TF) | __bit_shift(X86_FLAG_IF) | __bit_shift(X86_FLAG_DF) | __bit_shift(X86_FLAG_OF) | __bit_shift(X86_FLAG_IOPL1) | __bit_shift(X86_FLAG_IOPL2) | __bit_shift(X86_FLAG_NT))
// a set of flags used by a ton of ALU instructions
#define __x86_flagset_alustd       (__bit_shift(X86_FLAG_CF) | __bit_shift(X86_FLAG_PF) | __bit_shift(X86_FLAG_AF) | __bit_shift(X86_FLAG_ZF) | __bit_shift(X86_FLAG_SF) | __bit_shift(X86_FLAG_OF))

#define __flag_reserved "reserved"
const static char* flag_name_map[X86_FLAG_BITS] = { "carry", __flag_reserved, "parity", __flag_reserved, 
                           "adjust", __flag_reserved, "zero", "sign",
                           "trap", "interrupt", "direction", "overflow",
                           "iopl1", "iopl2", "nested_task", __flag_reserved,
                           "resume", "v8086", "alignchk", "vint",
                           "vint_pending", "ident", __flag_reserved, __flag_reserved,
                           __flag_reserved, __flag_reserved, __flag_reserved, __flag_reserved,
                           __flag_reserved, __flag_reserved, __flag_reserved, __flag_reserved };

#define CONTAINS_FLAG(__val, __flg) (((__val >> __flg) & 0x1) == 1)

// my non-gnu definitions for X86
#define X86_REG_INVALID (-1)
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

#define X86_FPREG_XMM0 (0 + X86_64BIT_GPRS)
#define X86_FPREG_XMM1 (1 + X86_64BIT_GPRS)
#define X86_FPREG_XMM2 (2 + X86_64BIT_GPRS)
#define X86_FPREG_XMM3 (3 + X86_64BIT_GPRS)
#define X86_FPREG_XMM4 (4 + X86_64BIT_GPRS)
#define X86_FPREG_XMM5 (5 + X86_64BIT_GPRS)
#define X86_FPREG_XMM6 (6 + X86_64BIT_GPRS)
#define X86_FPREG_XMM7 (7 + X86_64BIT_GPRS)
#define X86_FPREG_XMM8 (8 + X86_64BIT_GPRS)
#define X86_FPREG_XMM9 (9 + X86_64BIT_GPRS)
#define X86_FPREG_XMM10 (10 + X86_64BIT_GPRS)
#define X86_FPREG_XMM11 (11 + X86_64BIT_GPRS)
#define X86_FPREG_XMM12 (12 + X86_64BIT_GPRS)
#define X86_FPREG_XMM13 (13 + X86_64BIT_GPRS)
#define X86_FPREG_XMM14 (14 + X86_64BIT_GPRS)
#define X86_FPREG_XMM15 (15 + X86_64BIT_GPRS)
#define X86_XMM_REGS 16

#define X87_REG_ST0 (0 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST1 (1 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST2 (2 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST3 (3 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST4 (4 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST5 (5 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST6 (6 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REG_ST7 (7 + X86_64BIT_GPRS + X86_XMM_REGS)
#define X87_REGS 8

#define X86_ALU_REGS (X86_64BIT_GPRS + X86_XMM_REGS + X87_REGS)
const static char* alu_name_map[X86_ALU_REGS] = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
                                                  "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", 
                                                  "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
                                                  "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7" };

    
enum Confidence {
  Unknown, Maybe, Definitely
};

enum SwizzleOperation {
  None,
  SwapInnerPairs,
  SwapWithTwoAway,
  CrossProduct,
  BroadcastA,
  BroadcastB,
  BroadcastC,
  BroadcastD
};

struct RuntimeValue {
    Confidence confidence;
    uint64_t value;
};

struct VectorInfo {
    uint8_t nElements;
    uint8_t elementSize;
    struct RuntimeValue kval;
};

class RegisterSet {
public:
    RegisterSet();
    RegisterSet& operator=(const RegisterSet& rhs);
    RegisterSet& operator|=(const RegisterSet& rhs);
    const RegisterSet operator|(const RegisterSet& rhs);
    RegisterSet& operator-=(const RegisterSet& rhs);
    const RegisterSet operator-(const RegisterSet& rhs);
    bool operator==(const RegisterSet& rhs);
    void addRegister(uint32_t regNum);
    void addFlag(uint32_t flagNum);
    bool containsFlag(uint32_t flagNum);
    bool containsRegister(uint32_t regNum);
    void print(const char * const name);

private:
    BitSet<uint32_t> regs;
};

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

extern void copy_ud_to_compact(struct ud_compact* comp, struct ud* reg);

// keep a much smaller rep of the instruction to save memory
struct ud_compact
{
    //int 			(*inp_hook) (struct ud*);
    //uint8_t		inp_curr;
    //uint8_t		inp_fill;
    //FILE*			inp_file;
    //uint8_t		inp_ctr;
    //uint8_t*		inp_buff;
    //uint8_t*		inp_buff_end;
    //uint8_t		inp_end;
    //void		(*translator)(struct ud*);
    uint64_t		insn_offset;
    char		insn_bytes[16];
    //char		insn_hexcode[32];
    char		insn_buffer[INSTRUCTION_PRINT_SIZE];
    //unsigned int	insn_fill;
    //uint8_t		dis_mode;
    //uint64_t		pc;
    //uint8_t		vendor;
    //struct map_entry*	mapen;
    enum ud_mnemonic_code	mnemonic;
    struct ud_operand	operand[4];
    //uint8_t		error;
    //uint8_t	 	pfx_rex;
    uint8_t 		pfx_seg;
    //uint8_t 		pfx_opr;
    //uint8_t 		pfx_adr;
    //uint8_t 		pfx_lock;
    uint8_t 		pfx_rep;
    //uint8_t 		pfx_repe;
    //uint8_t 		pfx_repne;
    //uint8_t 		pfx_insn;
    //uint8_t           pfx_avx;
    //uint8_t           avx_vex[2];
    enum ud_type        vector_mask_register;
    uint8_t             conversion;
    //uint8_t		default64;
    //uint8_t		opr_mode;
    uint8_t		adr_mode;
    //uint8_t		br_far;
    //uint8_t		br_near;
    //uint8_t		implicit_addr;
    //uint8_t		c1;
    //uint8_t		c2;
    //uint8_t		c3;
    //uint8_t 		inp_cache[256];
    //uint8_t		inp_sess[64];
    uint32_t            flags_use;
    uint32_t            flags_def;
    uint64_t            impreg_use;
    uint64_t            impreg_def;
    //struct ud_itab_entry * itab_entry;
};

enum X86InstructionType {
    X86InstructionType_unknown = 0,
    X86InstructionType_invalid,
    X86InstructionType_condbr,
    X86InstructionType_uncondbr,
    X86InstructionType_call,
    X86InstructionType_return,
    X86InstructionType_int,
    X86InstructionType_move,
    X86InstructionType_float,
    X86InstructionType_string,
    X86InstructionType_simdFloat,
    X86InstructionType_simdInt,
    X86InstructionType_simdMove,
    X86InstructionType_aes,
    X86InstructionType_io,
    X86InstructionType_prefetch,
    X86InstructionType_syscall,
    X86InstructionType_halt,
    X86InstructionType_hwcount,
    X86InstructionType_nop,
    X86InstructionType_trap,
    X86InstructionType_vmx,
    X86InstructionType_special,
    X86InstructionType_Total
};

enum X86OperandFormat {
    X86OperandFormat_unknown = 0,   // Unknown
    X86OperandFormat_di,            // no explicit dest operand (mem reference)
    X86OperandFormat_si,            // no explicit source operand (mem reference)
    X86OperandFormat_dsi,           // no explicit dest and source operands (mem references)
    X86OperandFormat_Total
};
#define CHECK_IMPLICIT_LOAD  ((X86InstructionClassifier::getInstructionFormat(this) == X86OperandFormat_si) || (X86InstructionClassifier::getInstructionFormat(this) == X86OperandFormat_dsi))
#define CHECK_IMPLICIT_STORE ((X86InstructionClassifier::getInstructionFormat(this) == X86OperandFormat_di) || (X86InstructionClassifier::getInstructionFormat(this) == X86OperandFormat_dsi))

enum X86InstructionBin {
    X86InstructionBin_unknown = 0,   // Unknown
    X86InstructionBin_invalid,       // Invalid
    X86InstructionBin_cond,          // Control
    X86InstructionBin_uncond,        // Control, including call and return
    X86InstructionBin_bin,           // Binary
    X86InstructionBin_binv,          // Binary
    X86InstructionBin_int,           // Integer
    X86InstructionBin_intv,          // Integer
    X86InstructionBin_ints,
    X86InstructionBin_float,         // Floating
    X86InstructionBin_floatv,        // Floating
    X86InstructionBin_floats,        // Floating
    X86InstructionBin_move,          // Data movement
    X86InstructionBin_stack,         // Stack operations
    X86InstructionBin_string,        // String operations
    X86InstructionBin_system,        // system calls
    X86InstructionBin_cache,         // Floating
    X86InstructionBin_other,         // System (including halt, hwcount, nops, trap, vmx, and special)
    X86InstructionBin_total
};

#define INSTBIN_DATATYPE(u) ( u << BinSizeShift )

const uint16_t BinMask = 0xFF;
const uint16_t BinSizeShift = 12;
const uint16_t BinLoad = 0x800;
const uint16_t BinStore = 0x400;
const uint16_t BinStack = 0x200;
const uint16_t BinFrame = 0x100;

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
    RegType_YMM,
    RegType_ZMM,
    RegType_K,
    RegType_PC,
    RegType_Total_Types
} RegTypes;

extern uint32_t regbase_to_type(uint32_t base);

class X86InstructionClassifier;

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
   
    uint32_t getBitsUsed(); 
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
    struct ud_compact entry;

    RegisterSet* liveIns;
    RegisterSet* liveOuts;
    uint32_t defUseDist;

    uint32_t* flags_usedef;

    OperandX86** operands;
    uint32_t instructionIndex;

    char* rawBytes;
    uint8_t byteSource;
    uint64_t programAddress;
    AddressAnchor* addressAnchor;
    bool leader;
    TextObject* container;

    bool defXIter;

    uint32_t countValidNonimm();

    struct VectorInfo vectorInfo;

    void countElementsUnalignedLoadStore(bool, bool, bool);
public:

    X86Instruction* getFallthroughInstruction();

    static X86Instruction* disassemble(char* buff);

    void setDefXIter() { defXIter = true; }
    bool hasDefXIter() { return defXIter; }

    uint64_t cacheBaseAddress;

    OperandX86* getDestOperand();
    OperandX86* getSourceOperand(uint32_t index);
    Vector<OperandX86*>* getSourceOperands();
    Vector<OperandX86*>* getOperands();
    std::map<uint32_t, uint32_t>* getOperandLengthCounts();

    SwizzleOperation getSwizzleOperation();

    INSTRUCTION_MACROS_CLASS("For the get_X/set_X field macros check the defines directory");

    X86Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx);
    X86Instruction(TextObject* cont, uint64_t baseAddr, char* buff, uint8_t src, uint32_t idx, bool is64bit, uint32_t sz);
    ~X86Instruction();

    static void initBlankUd(bool is64Bit);

    OperandX86* getOperand(uint32_t idx);
    TextObject* getContainer() { return container; }
    void setContainer(TextObject* cont) { container = cont; }

    void setFlags();

    BitSet<uint32_t>* getFlagsUsed();
    BitSet<uint32_t>* getFlagsDefined();
    RegisterSet * getUnusableRegisters();
    RegisterSet * getRegistersUsed();
    RegisterSet * getRegistersDefined();

    bool allFlagsDeadIn();
    bool allFlagsDeadOut();
    bool isRegDeadIn(uint32_t regNum);
    bool isRegDeadOut(uint32_t regNum);
    bool isFlagDeadIn(uint32_t flagNum);
    bool isFlagDeadOut(uint32_t flagNum);
    BitSet<uint32_t>* getDeadRegIn(BitSet<uint32_t>* invalidRegs);
    BitSet<uint32_t>* getDeadRegOut(BitSet<uint32_t>* invalidRegs);
    BitSet<uint32_t>* getDeadRegIn(BitSet<uint32_t>* invalidRegs, uint32_t cnt);
    BitSet<uint32_t>* getDeadRegOut(BitSet<uint32_t>* invalidRegs, uint32_t cnt);

    bool usesFlag(uint32_t flg);
    bool defsFlag(uint32_t flg);
    bool implicitlyUsesReg(uint32_t alu);
    bool implicitlyDefinesReg(uint32_t alu);

    struct VectorInfo getVectorInfo();
    struct RuntimeValue getRegisterValue(enum ud_type reg);
    void setKRegister(RuntimeValue val);

    struct DefLocation {
        enum ud_type type;
        int64_t value;
        uint32_t base;
        uint32_t index;
        uint8_t offset;
        uint8_t scale;
    };

    struct ReachingDefinition {
        X86Instruction * defined_by;
        DefLocation location;
        ReachingDefinition(X86Instruction* ins, DefLocation loc) : defined_by(ins), location(loc) {}
        bool invalidatedBy (ReachingDefinition* other);
        void print();
    };
    LinkedList<ReachingDefinition*>* getDefs();
    LinkedList<ReachingDefinition*>* getUses();


    uint32_t getDefUseDist();
    void setDefUseDist(uint32_t dudist);

    void print();
    bool verify();

    char* charStream() { return rawBytes; }

    HashCode* generateHashCode(BasicBlock* bb);

    void setLiveIns(RegisterSet* live);
    void setLiveOuts(RegisterSet* live);
    RegisterSet* getLiveIns() {return liveIns;}
    RegisterSet* getLiveOuts() {return liveOuts;}

    void setBaseAddress(uint64_t addr) { baseAddress = addr; cacheBaseAddress = addr; }
    uint32_t getSizeInBytes() { return sizeInBytes; }
    uint32_t getIndex() { return instructionIndex; }
    void setIndex(uint32_t idx) { instructionIndex = idx; }
    X86InstructionType getInstructionType();

    uint64_t getProgramAddress() { return programAddress; }

    uint32_t getDstSizeInBytes();

    void impliedUses(BitSet<uint32_t>* regs);
    void impliedDefs(BitSet<uint32_t>* regs);
    void usesRegisters(BitSet<uint32_t>* regs);
    void defsRegisters(BitSet<uint32_t>* regs);
    void touchedRegisters(BitSet<uint32_t>* regs);
    bool controlFallsThrough();

    // control instruction id
    bool isControl();
    bool isBranch() { return isUnconditionalBranch() || isConditionalBranch(); }
    bool isUnconditionalBranch() { return (getInstructionType() == X86InstructionType_uncondbr); }
    bool isConditionalBranch() { return (getInstructionType() == X86InstructionType_condbr); }
    bool isReturn() { return (getInstructionType() == X86InstructionType_return); }
    bool isFunctionCall() { return (getInstructionType() == X86InstructionType_call); }
    bool isSystemCall() { return (getInstructionType() == X86InstructionType_syscall); }
    bool isCall() { return isSystemCall() || isFunctionCall(); }
    bool isHalt() { return (getInstructionType() == X86InstructionType_halt); }
    bool isNop() { return (getInstructionType() == X86InstructionType_nop); }
    bool isConditionCompare();
    bool isStackPush();
    bool isStackPop();
    bool isLoad();
    bool isStore();
    bool isSpecialRegOp();
    bool isLogicOp();
    bool isConditionalMove();
    bool isScatterGatherOp();
    bool isVectorMaskOp();

    bool isBinUnknown();
    bool isBinInvalid();
    bool isBinCond();
    bool isBinUncond();
    bool isBinBin();
    bool isBinBinv();
    bool isBinInt();
    bool isBinIntv();
    bool isBinInts();
    bool isBinFloat();
    bool isBinFloatv();
    bool isBinFloats();
    bool isBinMove();
    bool isBinSystem();
    bool isBinStack();
    bool isBinOther();
    bool isBinCache();
    bool isBinString();
    bool isBinByte();
    bool isBinBytev();
    bool isBinWord();
    bool isBinWordv();
    bool isBinDword();
    bool isBinDwordv();
    bool isBinQword();
    bool isBinQwordv();
    bool isBinSingle();
    bool isBinSinglev();
    bool isBinSingles();
    bool isBinDouble();
    bool isBinDoublev();
    bool isBinDoubles();
    bool isBinMem();

    void printBin();

    uint8_t getByteSource() { return byteSource; }
    bool isRelocatable() { return true; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

    AddressAnchor* getAddressAnchor() { return addressAnchor; }
    void initializeAnchor(Base*, bool imm);
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

    uint32_t countExplicitOperands();

    bool isFloatPOperation();
    bool isIntegerOperation();
    bool isStringOperation();
    bool isMoveOperation();
    uint32_t getNumberOfMemoryBytes();
    bool isMemoryOperation();
    bool isExplicitMemoryOperation();    
    bool isImplicitMemoryOperation();

    OperandX86* getMemoryOperand();
};

class X86InstructionClassifier {
private:
    X86InstructionClassifier() {}
    ~X86InstructionClassifier() {}

    static uint32_t getMnemonic(X86Instruction* x);

public:
    static bool verify();
    static void generateTable();

    static X86InstructionBin getInstructionBin(X86Instruction* x);
    static uint8_t getInstructionMemLocation(X86Instruction* x);
    static uint8_t getInstructionMemSize(X86Instruction* x);
    static uint8_t getInstructionElemSize(X86Instruction* x);
    static X86InstructionType getInstructionType(X86Instruction* x);
    static X86OperandFormat getInstructionFormat(X86Instruction* x);

    static void print(X86Instruction* x);

};

#endif /* _X86Instruction_h_ */

