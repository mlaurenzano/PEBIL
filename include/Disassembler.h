#ifndef _Disassembler_h_
#define _Disassembler_h_

#include <Base.h>
#include <CStructuresX86.h>

class ElfFile;

class Disassembler : public Base {
protected:
    ElfFile* elfFile;
    uint64_t machineType;
    struct disassemble_info disassembleInfo;

    char obuf[100];
    char *obufp;
    char scratchbuf[100];
    unsigned char *start_codep;
    unsigned char *insn_codep;
    unsigned char *codep;
    disassemble_info *the_info;
    int mod;
    int rm;
    int reg;
    unsigned char need_modrm;

    const char **names64;
    const char **names32;
    const char **names16;
    const char **names8;
    const char **names8rex;
    const char **names_seg;
    const char **index16;

    char op1out[100], op2out[100], op3out[100];
    int op_ad, op_index[3];
    uint64_t op_address[3];
    uint64_t op_riprel[3];
    uint64_t start_pc;

    /*
     *   On the 386's of 1988, the maximum length of an instruction is 15 bytes.
     *   (see topic "Redundant prefixes" in the "Differences from 8086"
     *   section of the "Virtual 8086 Mode" chapter.)
     * 'pc' should be the address of this instruction, it will
     *   be used to print the target address if this is a relative jump or call
     * The function returns the length of this instruction in bytes.
     */

    char intel_syntax;
    char open_char;
    char close_char;
    char separator_char;
    char scale_char;


public:
    Disassembler(ElfFile* elffile);
    ~Disassembler();

    void print();

    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset) {}
    ElfFile* getElfFile() { return elfFile; }

    void get_ops(op_func op, uint32_t bytemode, uint32_t sizeflag);

    /* legacy functions from GNU libopcodes. at least some of these should eventually be gone */
    uint32_t print_insn(uint64_t addr);
    uint32_t readMemory(long long unsigned int, uint8_t*, unsigned int, disassemble_info*);
    void ckprefix();
    const char* prefix_name(int pref, int sizeflag);
    void dofloat(uint32_t sizeflag);
    uint32_t putop(const char* templatevar, int32_t sizeflag);
    void oappend(const char* s);
    void append_seg();
    void print_operand_value(char* buf, uint32_t hex, uint64_t disp);
    uint64_t get64();
    int64_t get32();
    int64_t get32s();
    int32_t get16();
    void set_op(uint64_t op, int riprel);
    void ptr_reg(uint32_t code, uint32_t sizeflag);

    /* functions to deal with different op modes */
    void OP_ST(uint32_t bytemode, uint32_t sizeflag);
    void OP_STi(uint32_t bytemode, uint32_t sizeflag);
    void OP_indirE(uint32_t bytemode, uint32_t sizeflag);
    void OP_E(uint32_t bytemode, uint32_t sizeflag);
    void OP_G(uint32_t bytemode, uint32_t sizeflag);
    void OP_REG(uint32_t code, uint32_t sizeflag);
    void OP_IMREG(uint32_t code, uint32_t sizeflag);
    void OP_I(uint32_t bytemode, uint32_t sizeflag);
    void OP_I64(uint32_t bytemode, uint32_t sizeflag);
    void OP_sI(uint32_t bytemode, uint32_t sizeflag);
    void OP_J(uint32_t bytemode, uint32_t sizeflag);
    void OP_SEG(uint32_t dummy, uint32_t sizeflag);
    void OP_DIR(uint32_t dummy, uint32_t sizeflag);
    void OP_OFF(uint32_t bytemode, uint32_t sizeflag);
    void OP_OFF64(uint32_t bytemode, uint32_t sizeflag);
    void OP_ESreg(uint32_t code, uint32_t sizeflag);
    void OP_DSreg(uint32_t code, uint32_t sizeflag);
    void OP_C(uint32_t dummy, uint32_t sizeflag);
    void OP_D(uint32_t dummy, uint32_t sizeflag);
    void OP_T(uint32_t dummy, uint32_t sizeflag);
    void OP_Rd(uint32_t bytemode, uint32_t sizeflag);
    void OP_MMX(uint32_t bytemode, uint32_t sizeflag);
    void OP_XMM(uint32_t bytemode, uint32_t sizeflag);
    void OP_EM(uint32_t bytemode, uint32_t sizeflag);
    void OP_EX(uint32_t bytemode, uint32_t sizeflag);
    void OP_MS(uint32_t bytemode, uint32_t sizeflag);
    void OP_XS(uint32_t bytemode, uint32_t sizeflag);
    void OP_3DNowSuffix(uint32_t bytemode, uint32_t sizeflag);
    void OP_SIMD_Suffix(uint32_t bytemode, uint32_t sizeflag);
    void SIMD_Fixup(int32_t extrachar, uint32_t sizeflag);
    void BadOp(void);


};
#endif /* _Disassembler_h_ */
