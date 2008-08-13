/* Interface between the opcode library and its callers.

   Copyright 2001, 2002 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   Written by Cygnus Support, 1993.

   The opcode library (libopcodes.a) provides instruction decoders for
   a large variety of instruction sets, callable with an identical
   interface, for making instruction-processing programs more independent
   of the instruction set being processed.  */

#ifndef _CStructuresX86_h_
#define _CStructuresX86_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#define X86_GPRS 16


    /* This struct is passed into the instruction decoding routine, 
       and is passed back out into each callback.  The various fields are used
       for conveying information from your main routine into your callbacks,
       for passing information into the instruction decoders (such as the
       addresses of the callback functions), or for passing information
       back from the instruction decoders to their callers.
       
       It must be initialized before it is first passed; this can be done
       by hand, or using one of the initialization macros below.  */
    
    typedef struct disassemble_info {
        
        unsigned long mach;
        
#define INSN_HAS_RELOC	0x80000000
        void* private_data;
        
        /* These are for buffer_read_memory.  */
        uint8_t *buffer;
        uint64_t buffer_vma;
        
    } disassemble_info;

#define MAX_X86_INSTRUCTION_LENGTH 20

#ifndef UNIXWARE_COMPAT
    /* Set non-zero for broken, compatible instructions.  Set to zero for
       non-broken opcodes.  */
#define UNIXWARE_COMPAT 1
#endif
    
    struct dis_private {
        /* Points to first byte not fetched.  */
        uint8_t *max_fetched;
        uint8_t the_buffer[MAX_X86_INSTRUCTION_LENGTH];
        uint64_t insn_start;
        int orig_sizeflag;
    };
    
    /* The opcode for the fwait instruction, which we treat as a prefix
       when we can.  */
#define FWAIT_OPCODE (0x9b)
    
#define REX_MODE64      8
#define REX_EXTX        4
#define REX_EXTY        2
#define REX_EXTZ        1
    /* Mark parts used in the REX prefix.  When we are testing for
       empty prefix (for 8bit register REX extension), just mask it
       out.  Otherwise test for REX bit is excuse for existence of REX
       only in case value is nonzero.  */
#define USED_REX(value){                                \
    if (value)                                          \
        rex_used |= (rex & value) ? (value) | 0x40 : 0; \
    else                                                \
        rex_used |= 0x40;                               \
}
    
    
    /* Flags stored in PREFIXES.  */
#define PREFIX_REPZ 1
#define PREFIX_REPNZ 2
#define PREFIX_LOCK 4
#define PREFIX_CS 8
#define PREFIX_SS 0x10
#define PREFIX_DS 0x20
#define PREFIX_ES 0x40
#define PREFIX_FS 0x80
#define PREFIX_GS 0x100
#define PREFIX_DATA 0x200
#define PREFIX_ADDR 0x400
#define PREFIX_FWAIT 0x800


#define XX func_NULL, 0

#define Eb func_OP_E, b_mode
#define Ev func_OP_E, v_mode
#define Ed func_OP_E, d_mode
#define indirEb func_OP_indirE, b_mode
#define indirEv func_OP_indirE, v_mode
#define Ew func_OP_E, w_mode
#define Ma func_OP_E, v_mode
#define M func_OP_E, 0               /* lea, lgdt, etc. */
#define Mp func_OP_E, 0              /* 32 or 48 bit memory operand for LDS, LES etc */
#define Gb func_OP_G, b_mode
#define Gv func_OP_G, v_mode
#define Gd func_OP_G, d_mode
#define Gw func_OP_G, w_mode
#define Rd func_OP_Rd, d_mode
#define Rm func_OP_Rd, m_mode
#define Ib func_OP_I, b_mode
#define sIb func_OP_sI, b_mode       /* sign extened byte */
#define Iv func_OP_I, v_mode
#define Iq func_OP_I, q_mode
#define Iv64 func_OP_I64, v_mode
#define Iw func_OP_I, w_mode
#define Jb func_OP_J, b_mode
#define Jv func_OP_J, v_mode
#define Cm func_OP_C, m_mode
#define Dm func_OP_D, m_mode
#define Td func_OP_T, d_mode


#define RMeAX func_OP_REG, eAX_reg
#define RMeBX func_OP_REG, eBX_reg
#define RMeCX func_OP_REG, eCX_reg
#define RMeDX func_OP_REG, eDX_reg
#define RMeSP func_OP_REG, eSP_reg
#define RMeBP func_OP_REG, eBP_reg
#define RMeSI func_OP_REG, eSI_reg
#define RMeDI func_OP_REG, eDI_reg
#define RMrAX func_OP_REG, rAX_reg
#define RMrBX func_OP_REG, rBX_reg
#define RMrCX func_OP_REG, rCX_reg
#define RMrDX func_OP_REG, rDX_reg
#define RMrSP func_OP_REG, rSP_reg
#define RMrBP func_OP_REG, rBP_reg
#define RMrSI func_OP_REG, rSI_reg
#define RMrDI func_OP_REG, rDI_reg
#define RMAL func_OP_REG, al_reg
#define RMAL func_OP_REG, al_reg
#define RMCL func_OP_REG, cl_reg
#define RMDL func_OP_REG, dl_reg
#define RMBL func_OP_REG, bl_reg
#define RMAH func_OP_REG, ah_reg
#define RMCH func_OP_REG, ch_reg
#define RMDH func_OP_REG, dh_reg
#define RMBH func_OP_REG, bh_reg
#define RMAX func_OP_REG, ax_reg
#define RMDX func_OP_REG, dx_reg

#define eAX func_OP_IMREG, eAX_reg
#define eBX func_OP_IMREG, eBX_reg
#define eCX func_OP_IMREG, eCX_reg
#define eDX func_OP_IMREG, eDX_reg
#define eSP func_OP_IMREG, eSP_reg
#define eBP func_OP_IMREG, eBP_reg
#define eSI func_OP_IMREG, eSI_reg
#define eDI func_OP_IMREG, eDI_reg
#define AL func_OP_IMREG, al_reg
#define AL func_OP_IMREG, al_reg
#define CL func_OP_IMREG, cl_reg
#define DL func_OP_IMREG, dl_reg
#define BL func_OP_IMREG, bl_reg
#define AH func_OP_IMREG, ah_reg
#define CH func_OP_IMREG, ch_reg
#define DH func_OP_IMREG, dh_reg
#define BH func_OP_IMREG, bh_reg
#define AX func_OP_IMREG, ax_reg
#define DX func_OP_IMREG, dx_reg
#define indirDX func_OP_IMREG, indir_dx_reg

#define Sw func_OP_SEG, w_mode
#define Ap func_OP_DIR, 0
#define Ob func_OP_OFF, b_mode
#define Ob64 func_OP_OFF64, b_mode
#define Ov func_OP_OFF, v_mode
#define Ov64 func_OP_OFF64, v_mode
#define Xb func_OP_DSreg, eSI_reg
#define Xv func_OP_DSreg, eSI_reg
#define Yb func_OP_ESreg, eDI_reg
#define Yv func_OP_ESreg, eDI_reg
#define DSBX func_OP_DSreg, eBX_reg

#define es func_OP_REG, es_reg
#define ss func_OP_REG, ss_reg
#define cs func_OP_REG, cs_reg
#define ds func_OP_REG, ds_reg
#define fs func_OP_REG, fs_reg
#define gs func_OP_REG, gs_reg

#define MX func_OP_MMX, 0
#define XM func_OP_XMM, 0
#define EM func_OP_EM, v_mode
#define EX func_OP_EX, v_mode
#define MS func_OP_MS, v_mode
#define XS func_OP_XS, v_mode
#define None func_OP_E, 0
#define OPSUF func_OP_3DNowSuffix, 0
#define OPSIMD func_OP_SIMD_Suffix, 0

#define cond_jump_flag func_NULL, cond_jump_mode
#define loop_jcxz_flag func_NULL, loop_jcxz_mode

    /* bits in sizeflag */
#define SUFFIX_ALWAYS 4
#define AFLAG 2
#define DFLAG 1

#define b_mode 1  /* byte operand */
#define v_mode 2  /* operand size depends on prefixes */
#define w_mode 3  /* word operand */
#define d_mode 4  /* double word operand  */
#define q_mode 5  /* quad word operand */
#define x_mode 6
#define m_mode 7  /* d_mode in 32bit, q_mode in 64bit mode.  */
#define cond_jump_mode 8
#define loop_jcxz_mode 9

#define es_reg 100
#define cs_reg 101
#define ss_reg 102
#define ds_reg 103
#define fs_reg 104
#define gs_reg 105

#define eAX_reg 108
#define eCX_reg 109
#define eDX_reg 110
#define eBX_reg 111
#define eSP_reg 112
#define eBP_reg 113
#define eSI_reg 114
#define eDI_reg 115

#define al_reg 116
#define cl_reg 117
#define dl_reg 118
#define bl_reg 119
#define ah_reg 120
#define ch_reg 121
#define dh_reg 122
#define bh_reg 123

#define ax_reg 124
#define cx_reg 125
#define dx_reg 126
#define bx_reg 127
#define sp_reg 128
#define bp_reg 129
#define si_reg 130
#define di_reg 131

#define rAX_reg 132
#define rCX_reg 133
#define rDX_reg 134
#define rBX_reg 135
#define rSP_reg 136
#define rBP_reg 137
#define rSI_reg 138
#define rDI_reg 139

#define indir_dx_reg 150

#define FLOATCODE 1
#define USE_GROUPS 2
#define USE_PREFIX_USER_TABLE 3
#define X86_64_SPECIAL 4

#define FLOAT     NULL, func_NULL, FLOATCODE, func_NULL, 0, func_NULL, 0

#define GRP1b     NULL, func_NULL, USE_GROUPS, func_NULL,  0, func_NULL, 0
#define GRP1S     NULL, func_NULL, USE_GROUPS, func_NULL,  1, func_NULL, 0
#define GRP1Ss    NULL, func_NULL, USE_GROUPS, func_NULL,  2, func_NULL, 0
#define GRP2b     NULL, func_NULL, USE_GROUPS, func_NULL,  3, func_NULL, 0
#define GRP2S     NULL, func_NULL, USE_GROUPS, func_NULL,  4, func_NULL, 0
#define GRP2b_one NULL, func_NULL, USE_GROUPS, func_NULL,  5, func_NULL, 0
#define GRP2S_one NULL, func_NULL, USE_GROUPS, func_NULL,  6, func_NULL, 0
#define GRP2b_cl  NULL, func_NULL, USE_GROUPS, func_NULL,  7, func_NULL, 0
#define GRP2S_cl  NULL, func_NULL, USE_GROUPS, func_NULL,  8, func_NULL, 0
#define GRP3b     NULL, func_NULL, USE_GROUPS, func_NULL,  9, func_NULL, 0
#define GRP3S     NULL, func_NULL, USE_GROUPS, func_NULL, 10, func_NULL, 0
#define GRP4      NULL, func_NULL, USE_GROUPS, func_NULL, 11, func_NULL, 0
#define GRP5      NULL, func_NULL, USE_GROUPS, func_NULL, 12, func_NULL, 0
#define GRP6      NULL, func_NULL, USE_GROUPS, func_NULL, 13, func_NULL, 0
#define GRP7      NULL, func_NULL, USE_GROUPS, func_NULL, 14, func_NULL, 0
#define GRP8      NULL, func_NULL, USE_GROUPS, func_NULL, 15, func_NULL, 0
#define GRP9      NULL, func_NULL, USE_GROUPS, func_NULL, 16, func_NULL, 0
#define GRP10     NULL, func_NULL, USE_GROUPS, func_NULL, 17, func_NULL, 0
#define GRP11     NULL, func_NULL, USE_GROUPS, func_NULL, 18, func_NULL, 0
#define GRP12     NULL, func_NULL, USE_GROUPS, func_NULL, 19, func_NULL, 0
#define GRP13     NULL, func_NULL, USE_GROUPS, func_NULL, 20, func_NULL, 0
#define GRP14     NULL, func_NULL, USE_GROUPS, func_NULL, 21, func_NULL, 0
#define GRPAMD    NULL, func_NULL, USE_GROUPS, func_NULL, 22, func_NULL, 0

#define PREGRP0   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  0, func_NULL, 0
#define PREGRP1   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  1, func_NULL, 0
#define PREGRP2   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  2, func_NULL, 0
#define PREGRP3   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  3, func_NULL, 0
#define PREGRP4   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  4, func_NULL, 0
#define PREGRP5   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  5, func_NULL, 0
#define PREGRP6   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  6, func_NULL, 0
#define PREGRP7   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  7, func_NULL, 0
#define PREGRP8   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  8, func_NULL, 0
#define PREGRP9   NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL,  9, func_NULL, 0
#define PREGRP10  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 10, func_NULL, 0
#define PREGRP11  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 11, func_NULL, 0
#define PREGRP12  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 12, func_NULL, 0
#define PREGRP13  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 13, func_NULL, 0
#define PREGRP14  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 14, func_NULL, 0
#define PREGRP15  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 15, func_NULL, 0
#define PREGRP16  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 16, func_NULL, 0
#define PREGRP17  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 17, func_NULL, 0
#define PREGRP18  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 18, func_NULL, 0
#define PREGRP19  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 19, func_NULL, 0
#define PREGRP20  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 20, func_NULL, 0
#define PREGRP21  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 21, func_NULL, 0
#define PREGRP22  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 22, func_NULL, 0
#define PREGRP23  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 23, func_NULL, 0
#define PREGRP24  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 24, func_NULL, 0
#define PREGRP25  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 25, func_NULL, 0
#define PREGRP26  NULL, func_NULL, USE_PREFIX_USER_TABLE, func_NULL, 26, func_NULL, 0

#define X86_64_0  NULL, func_NULL, X86_64_SPECIAL, func_NULL,  0, func_NULL, 0

    enum op_func {
        func_NULL = 0,
        func_OP_ST,
        func_OP_STi,
        func_OP_indirE,
        func_OP_E, 
        func_OP_G,
        func_OP_REG,
        func_OP_IMREG,
        func_OP_I,
        func_OP_I64,
        func_OP_sI,
        func_OP_J,
        func_OP_SEG,
        func_OP_DIR,
        func_OP_OFF,
        func_OP_OFF64,
        func_OP_ESreg,
        func_OP_DSreg,
        func_OP_C,
        func_OP_D,
        func_OP_T,
        func_OP_Rd,
        func_OP_MMX,
        func_OP_XMM,
        func_OP_EM,
        func_OP_EX,
        func_OP_MS,
        func_OP_XS,
        func_OP_3DNowSuffix,
        func_OP_SIMD_Suffix,
        func_SIMD_Fixup
    };

    struct dis386 {
        const char *name;
        op_func op1;
        int bytemode1;
        op_func op2;
        int bytemode2;
        op_func op3;
        int bytemode3;
    };

    /* Upper case letters in the instruction names here are macros.
   'A' => print 'b' if no register operands or suffix_always is true
   'B' => print 'b' if suffix_always is true
   'E' => print 'e' if 32-bit form of jcxz
   'F' => print 'w' or 'l' depending on address size prefix (loop insns)
   'H' => print ",pt" or ",pn" branch hint
   'L' => print 'l' if suffix_always is true
   'N' => print 'n' if instruction has no wait "prefix"
   'O' => print 'd', or 'o'
   'P' => print 'w', 'l' or 'q' if instruction has an operand size prefix,
   .      or suffix_always is true.  print 'q' if rex prefix is present.
   'Q' => print 'w', 'l' or 'q' if no register operands or suffix_always
   .      is true
   'R' => print 'w', 'l' or 'q' ("wd" or "dq" in intel mode)
   'S' => print 'w', 'l' or 'q' if suffix_always is true
   'T' => print 'q' in 64bit mode and behave as 'P' otherwise
   'U' => print 'q' in 64bit mode and behave as 'Q' otherwise
   'X' => print 's', 'd' depending on data16 prefix (for XMM)
   'W' => print 'b' or 'w' ("w" or "de" in intel mode)
   'Y' => 'q' if instruction has an REX 64bit overwrite prefix

   Many of the above letters print nothing in Intel mode.  See "putop"
   for the details.

   Braces '{' and '}', and vertical bars '|', indicate alternative
   mnemonic strings for AT&T, Intel, X86_64 AT&T, and X86_64 Intel
   modes.  In cases where there are only two alternatives, the X86_64
   instruction is reserved, and "(bad)" is printed.
    */

    static const struct dis386 dis386[] = {
        /* 00 */
        { "addB",           Eb, Gb, XX },
        { "addS",           Ev, Gv, XX },
        { "addB",           Gb, Eb, XX },
        { "addS",           Gv, Ev, XX },
        { "addB",           AL, Ib, XX },
        { "addS",           eAX, Iv, XX },
        { "push{T|}",       es, XX, XX },
        { "pop{T|}",        es, XX, XX },
        /* 08 */
        { "orB",            Eb, Gb, XX },
        { "orS",            Ev, Gv, XX },
        { "orB",            Gb, Eb, XX },
        { "orS",            Gv, Ev, XX },
        { "orB",            AL, Ib, XX },
        { "orS",            eAX, Iv, XX },
        { "push{T|}",       cs, XX, XX },
        { "(bad)",          XX, XX, XX },   /* 0x0f extended opcode escape */
        /* 10 */
        { "adcB",           Eb, Gb, XX },
        { "adcS",           Ev, Gv, XX },
        { "adcB",           Gb, Eb, XX },
        { "adcS",           Gv, Ev, XX },
        { "adcB",           AL, Ib, XX },
        { "adcS",           eAX, Iv, XX },
        { "push{T|}",       ss, XX, XX },
        { "popT|}",         ss, XX, XX },
        /* 18 */
        { "sbbB",           Eb, Gb, XX },
        { "sbbS",           Ev, Gv, XX },
        { "sbbB",           Gb, Eb, XX },
        { "sbbS",           Gv, Ev, XX },
        { "sbbB",           AL, Ib, XX },
        { "sbbS",           eAX, Iv, XX },
        { "push{T|}",       ds, XX, XX },
        { "pop{T|}",        ds, XX, XX },
        /* 20 */
        { "andB",           Eb, Gb, XX },
        { "andS",           Ev, Gv, XX },
        { "andB",           Gb, Eb, XX },
        { "andS",           Gv, Ev, XX },
        { "andB",           AL, Ib, XX },
        { "andS",           eAX, Iv, XX },
        { "(bad)",          XX, XX, XX },   /* SEG ES prefix */
        { "daa{|}",         XX, XX, XX },
        /* 28 */
        { "subB",           Eb, Gb, XX },
        { "subS",           Ev, Gv, XX },
        { "subB",           Gb, Eb, XX },
        { "subS",           Gv, Ev, XX },
        { "subB",           AL, Ib, XX },
        { "subS",           eAX, Iv, XX },
        { "(bad)",          XX, XX, XX },   /* SEG CS prefix */
        { "das{|}",         XX, XX, XX },
        /* 30 */
        { "xorB",           Eb, Gb, XX },
        { "xorS",           Ev, Gv, XX },
        { "xorB",           Gb, Eb, XX },
        { "xorS",           Gv, Ev, XX },
        { "xorB",           AL, Ib, XX },
        { "xorS",           eAX, Iv, XX },
        { "(bad)",          XX, XX, XX },   /* SEG SS prefix */
        { "aaa{|}",         XX, XX, XX },
        /* 38 */
        { "cmpB",           Eb, Gb, XX },
        { "cmpS",           Ev, Gv, XX },
        { "cmpB",           Gb, Eb, XX },
        { "cmpS",           Gv, Ev, XX },
        { "cmpB",           AL, Ib, XX },
        { "cmpS",           eAX, Iv, XX },
        { "(bad)",          XX, XX, XX },   /* SEG DS prefix */
        { "aas{|}",         XX, XX, XX },
        /* 40 */
        { "inc{S|}",        RMeAX, XX, XX },
        { "inc{S|}",        RMeCX, XX, XX },
        { "inc{S|}",        RMeDX, XX, XX },
        { "inc{S|}",        RMeBX, XX, XX },
        { "inc{S|}",        RMeSP, XX, XX },
        { "inc{S|}",        RMeBP, XX, XX },
        { "inc{S|}",        RMeSI, XX, XX },
        { "inc{S|}",        RMeDI, XX, XX },
        /* 48 */
        { "dec{S|}",        RMeAX, XX, XX },
        { "dec{S|}",        RMeCX, XX, XX },
        { "dec{S|}",        RMeDX, XX, XX },
        { "dec{S|}",        RMeBX, XX, XX },
        { "dec{S|}",        RMeSP, XX, XX },
        { "dec{S|}",        RMeBP, XX, XX },
        { "dec{S|}",        RMeSI, XX, XX },
        { "dec{S|}",        RMeDI, XX, XX },
        /* 50 */
        { "pushS",          RMrAX, XX, XX },
        { "pushS",          RMrCX, XX, XX },
        { "pushS",          RMrDX, XX, XX },
        { "pushS",          RMrBX, XX, XX },
        { "pushS",          RMrSP, XX, XX },
        { "pushS",          RMrBP, XX, XX },
        { "pushS",          RMrSI, XX, XX },
        { "pushS",          RMrDI, XX, XX },
        /* 58 */
        { "popS",           RMrAX, XX, XX },
        { "popS",           RMrCX, XX, XX },
        { "popS",           RMrDX, XX, XX },
        { "popS",           RMrBX, XX, XX },
        { "popS",           RMrSP, XX, XX },
        { "popS",           RMrBP, XX, XX },
        { "popS",           RMrSI, XX, XX },
        { "popS",           RMrDI, XX, XX },
        /* 60 */
        { "pusha{P|}",      XX, XX, XX },
        { "popa{P|}",       XX, XX, XX },
        { "bound{S|}",      Gv, Ma, XX },
        { X86_64_0 },
        { "(bad)",          XX, XX, XX },   /* seg fs */
        { "(bad)",          XX, XX, XX },   /* seg gs */
        { "(bad)",          XX, XX, XX },   /* op size prefix */
        { "(bad)",          XX, XX, XX },   /* adr size prefix */
        /* 68 */
        { "pushT",          Iq, XX, XX },
        { "imulS",          Gv, Ev, Iv },
        { "pushT",          sIb, XX, XX },
        { "imulS",          Gv, Ev, sIb },
        { "ins{b||b|}",     Yb, indirDX, XX },
        { "ins{R||R|}",     Yv, indirDX, XX },
        { "outs{b||b|}",    indirDX, Xb, XX },
        { "outs{R||R|}",    indirDX, Xv, XX },
        /* 70 */
        { "joH",            Jb, XX, cond_jump_flag },
        { "jnoH",           Jb, XX, cond_jump_flag },
        { "jbH",            Jb, XX, cond_jump_flag },
        { "jaeH",           Jb, XX, cond_jump_flag },
        { "jeH",            Jb, XX, cond_jump_flag },
        { "jneH",           Jb, XX, cond_jump_flag },
        { "jbeH",           Jb, XX, cond_jump_flag },
        { "jaH",            Jb, XX, cond_jump_flag },
        /* 78 */
        { "jsH",            Jb, XX, cond_jump_flag },
        { "jnsH",           Jb, XX, cond_jump_flag },
        { "jpH",            Jb, XX, cond_jump_flag },
        { "jnpH",           Jb, XX, cond_jump_flag },
        { "jlH",            Jb, XX, cond_jump_flag },
        { "jgeH",           Jb, XX, cond_jump_flag },
        { "jleH",           Jb, XX, cond_jump_flag },
        { "jgH",            Jb, XX, cond_jump_flag },
        /* 80 */
        { GRP1b },
        { GRP1S },
        { "(bad)",          XX, XX, XX },
        { GRP1Ss },
        { "testB",          Eb, Gb, XX },
        { "testS",          Ev, Gv, XX },
        { "xchgB",          Eb, Gb, XX },
        { "xchgS",          Ev, Gv, XX },
        /* 88 */
        { "movB",           Eb, Gb, XX },
        { "movS",           Ev, Gv, XX },
        { "movB",           Gb, Eb, XX },
        { "movS",           Gv, Ev, XX },
        { "movQ",           Ev, Sw, XX },
        { "leaS",           Gv, M, XX },
        { "movQ",           Sw, Ev, XX },
        { "popU",           Ev, XX, XX },
        /* 90 */
        { "nop",            XX, XX, XX },
        /* FIXME: NOP with REPz prefix is called PAUSE.  */
        { "xchgS",          RMeCX, eAX, XX },
        { "xchgS",          RMeDX, eAX, XX },
        { "xchgS",          RMeBX, eAX, XX },
        { "xchgS",          RMeSP, eAX, XX },
        { "xchgS",          RMeBP, eAX, XX },
        { "xchgS",          RMeSI, eAX, XX },
        { "xchgS",          RMeDI, eAX, XX },
        /* 98 */
        { "cW{tR||tR|}",    XX, XX, XX },
        { "cR{tO||tO|}",    XX, XX, XX },
        { "lcall{T|}",      Ap, XX, XX },
        { "(bad)",          XX, XX, XX },   /* fwait */
        { "pushfT",         XX, XX, XX },
        { "popfT",          XX, XX, XX },
        { "sahf{|}",        XX, XX, XX },
        { "lahf{|}",        XX, XX, XX },
        /* a0 */
        { "movB",           AL, Ob64, XX },
        { "movS",           eAX, Ov64, XX },
        { "movB",           Ob64, AL, XX },
        { "movS",           Ov64, eAX, XX },
        { "movs{b||b|}",    Yb, Xb, XX },
        { "movs{R||R|}",    Yv, Xv, XX },
        { "cmps{b||b|}",    Xb, Yb, XX },
        { "cmps{R||R|}",    Xv, Yv, XX },
        /* a8 */
        { "testB",          AL, Ib, XX },
        { "testS",          eAX, Iv, XX },
        { "stosB",          Yb, AL, XX },
        { "stosS",          Yv, eAX, XX },
        { "lodsB",          AL, Xb, XX },
        { "lodsS",          eAX, Xv, XX },
        { "scasB",          AL, Yb, XX },
        { "scasS",          eAX, Yv, XX },
        /* b0 */
        { "movB",           RMAL, Ib, XX },
        { "movB",           RMCL, Ib, XX },
        { "movB",           RMDL, Ib, XX },
        { "movB",           RMBL, Ib, XX },
        { "movB",           RMAH, Ib, XX },
        { "movB",           RMCH, Ib, XX },
        { "movB",           RMDH, Ib, XX },
        { "movB",           RMBH, Ib, XX },
        /* b8 */
        { "movS",           RMeAX, Iv64, XX },
        { "movS",           RMeCX, Iv64, XX },
        { "movS",           RMeDX, Iv64, XX },
        { "movS",           RMeBX, Iv64, XX },
        { "movS",           RMeSP, Iv64, XX },
        { "movS",           RMeBP, Iv64, XX },
        { "movS",           RMeSI, Iv64, XX },
        { "movS",           RMeDI, Iv64, XX },
        /* c0 */
        { GRP2b },
        { GRP2S },
        { "retT",           Iw, XX, XX },
        { "retT",           XX, XX, XX },
        { "les{S|}",        Gv, Mp, XX },
        { "ldsS",           Gv, Mp, XX },
        { "movA",           Eb, Ib, XX },
        { "movQ",           Ev, Iv, XX },
        /* c8 */
        { "enterT",         Iw, Ib, XX },
        { "leaveT",         XX, XX, XX },
        { "lretP",          Iw, XX, XX },
        { "lretP",          XX, XX, XX },
        { "int3",           XX, XX, XX },
        { "int",            Ib, XX, XX },
        { "into{|}",        XX, XX, XX },
        { "iretP",          XX, XX, XX },
        /* d0 */
        { GRP2b_one },
        { GRP2S_one },
        { GRP2b_cl },
        { GRP2S_cl },
        { "aam{|}",         sIb, XX, XX },
        { "aad{|}",         sIb, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "xlat",           DSBX, XX, XX },
        /* d8 */
        { FLOAT },
        { FLOAT },
        { FLOAT },
        { FLOAT },
        { FLOAT },
        { FLOAT },
        { FLOAT },
        { FLOAT },
        /* e0 */
        { "loopneFH",       Jb, XX, loop_jcxz_flag },
        { "loopeFH",        Jb, XX, loop_jcxz_flag },
        { "loopFH",         Jb, XX, loop_jcxz_flag },
        { "jEcxzH",         Jb, XX, loop_jcxz_flag },
        { "inB",            AL, Ib, XX },
        { "inS",            eAX, Ib, XX },
        { "outB",           Ib, AL, XX },
        { "outS",           Ib, eAX, XX },
        /* e8 */
        { "callT",          Jv, XX, XX },
        { "jmpT",           Jv, XX, XX },
        { "ljmp{T|}",       Ap, XX, XX },
        { "jmp",            Jb, XX, XX },
        { "inB",            AL, indirDX, XX },
        { "inS",            eAX, indirDX, XX },
        { "outB",           indirDX, AL, XX },
        { "outS",           indirDX, eAX, XX },
        /* f0 */
        { "(bad)",          XX, XX, XX },   /* lock prefix */
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },   /* repne */
        { "(bad)",          XX, XX, XX },   /* repz */
        { "hlt",            XX, XX, XX },
        { "cmc",            XX, XX, XX },
        { GRP3b },
        { GRP3S },
        /* f8 */
        { "clc",            XX, XX, XX },
        { "stc",            XX, XX, XX },
        { "cli",            XX, XX, XX },
        { "sti",            XX, XX, XX },
        { "cld",            XX, XX, XX },
        { "std",            XX, XX, XX },
        { GRP4 },
        { GRP5 },
    };

    static const struct dis386 dis386_twobyte[] = {
        /* 00 */
        { GRP6 },
        { GRP7 },
        { "larS",           Gv, Ew, XX },
        { "lslS",           Gv, Ew, XX },
        { "(bad)",          XX, XX, XX },
        { "syscall",        XX, XX, XX },
        { "clts",           XX, XX, XX },
        { "sysretP",        XX, XX, XX },
        /* 08 */
        { "invd",           XX, XX, XX },
        { "wbinvd",         XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "ud2a",           XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { GRPAMD },
        { "femms",          XX, XX, XX },
        { "",               MX, EM, OPSUF }, /* See OP_3DNowSuffix.  */
        /* 10 */
        { PREGRP8 },
        { PREGRP9 },
        { "movlpX",         XM, EX, func_SIMD_Fixup, 'h' }, /* really only 2 operands */
        { "movlpX",         EX, XM, func_SIMD_Fixup, 'h' },
        { "unpcklpX",       XM, EX, XX },
        { "unpckhpX",       XM, EX, XX },
        { "movhpX",         XM, EX, func_SIMD_Fixup, 'l' },
        { "movhpX",         EX, XM, func_SIMD_Fixup, 'l' },
        /* 18 */
        { GRP14 },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        /* 20 */
        { "movL",           Rm, Cm, XX },
        { "movL",           Rm, Dm, XX },
        { "movL",           Cm, Rm, XX },
        { "movL",           Dm, Rm, XX },
        { "movL",           Rd, Td, XX },
        { "(bad)",          XX, XX, XX },
        { "movL",           Td, Rd, XX },
        { "(bad)",          XX, XX, XX },
        /* 28 */
        { "movapX",         XM, EX, XX },
        { "movapX",         EX, XM, XX },
        { PREGRP2 },
        { "movntpX",        Ev, XM, XX },
        { PREGRP4 },
        { PREGRP3 },
        { "ucomisX",        XM,EX, XX },
        { "comisX",         XM,EX, XX },
        /* 30 */
        { "wrmsr",          XX, XX, XX },
        { "rdtsc",          XX, XX, XX },
        { "rdmsr",          XX, XX, XX },
        { "rdpmc",          XX, XX, XX },
        { "sysenter",       XX, XX, XX },
        { "sysexit",        XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        /* 38 */
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        /* 40 */
        { "cmovo",          Gv, Ev, XX },
        { "cmovno",         Gv, Ev, XX },
        { "cmovb",          Gv, Ev, XX },
        { "cmovae",         Gv, Ev, XX },
        { "cmove",          Gv, Ev, XX },
        { "cmovne",         Gv, Ev, XX },
        { "cmovbe",         Gv, Ev, XX },
        { "cmova",          Gv, Ev, XX },
        /* 48 */
        { "cmovs",          Gv, Ev, XX },
        { "cmovns",         Gv, Ev, XX },
        { "cmovp",          Gv, Ev, XX },
        { "cmovnp",         Gv, Ev, XX },
        { "cmovl",          Gv, Ev, XX },
        { "cmovge",         Gv, Ev, XX },
        { "cmovle",         Gv, Ev, XX },
        { "cmovg",          Gv, Ev, XX },
        /* 50 */
        { "movmskpX",       Gd, XS, XX },
        { PREGRP13 },
        { PREGRP12 },
        { PREGRP11 },
        { "andpX",          XM, EX, XX },
        { "andnpX",         XM, EX, XX },
        { "orpX",           XM, EX, XX },
        { "xorpX",          XM, EX, XX },
        /* 58 */
        { PREGRP0 },
        { PREGRP10 },
        { PREGRP17 },
        { PREGRP16 },
        { PREGRP14 },
        { PREGRP7 },
        { PREGRP5 },
        { PREGRP6 },
        /* 60 */
        { "punpcklbw",      MX, EM, XX },
        { "punpcklwd",      MX, EM, XX },
        { "punpckldq",      MX, EM, XX },
        { "packsswb",       MX, EM, XX },
        { "pcmpgtb",        MX, EM, XX },
        { "pcmpgtw",        MX, EM, XX },
        { "pcmpgtd",        MX, EM, XX },
        { "packuswb",       MX, EM, XX },
        /* 68 */
        { "punpckhbw",      MX, EM, XX },
        { "punpckhwd",      MX, EM, XX },
        { "punpckhdq",      MX, EM, XX },
        { "packssdw",       MX, EM, XX },
        { PREGRP26 },
        { PREGRP24 },
        { "movd",           MX, Ed, XX },
        { PREGRP19 },
        /* 70 */
        { PREGRP22 },
        { GRP10 },
        { GRP11 },
        { GRP12 },
        { "pcmpeqb",        MX, EM, XX },
        { "pcmpeqw",        MX, EM, XX },
        { "pcmpeqd",        MX, EM, XX },
        { "emms",           XX, XX, XX },
        /* 78 */
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        { PREGRP23 },
        { PREGRP20 },
        /* 80 */
        { "joH",            Jv, XX, cond_jump_flag },
        { "jnoH",           Jv, XX, cond_jump_flag },
        { "jbH",            Jv, XX, cond_jump_flag },
        { "jaeH",           Jv, XX, cond_jump_flag },
        { "jeH",            Jv, XX, cond_jump_flag },
        { "jneH",           Jv, XX, cond_jump_flag },
        { "jbeH",           Jv, XX, cond_jump_flag },
        { "jaH",            Jv, XX, cond_jump_flag },
        /* 88 */
        { "jsH",            Jv, XX, cond_jump_flag },
        { "jnsH",           Jv, XX, cond_jump_flag },
        { "jpH",            Jv, XX, cond_jump_flag },
        { "jnpH",           Jv, XX, cond_jump_flag },
        { "jlH",            Jv, XX, cond_jump_flag },
        { "jgeH",           Jv, XX, cond_jump_flag },
        { "jleH",           Jv, XX, cond_jump_flag },
        { "jgH",            Jv, XX, cond_jump_flag },
        /* 90 */
        { "seto",           Eb, XX, XX },
        { "setno",          Eb, XX, XX },
        { "setb",           Eb, XX, XX },
        { "setae",          Eb, XX, XX },
        { "sete",           Eb, XX, XX },
        { "setne",          Eb, XX, XX },
        { "setbe",          Eb, XX, XX },
        { "seta",           Eb, XX, XX },
        /* 98 */
        { "sets",           Eb, XX, XX },
        { "setns",          Eb, XX, XX },
        { "setp",           Eb, XX, XX },
        { "setnp",          Eb, XX, XX },
        { "setl",           Eb, XX, XX },
        { "setge",          Eb, XX, XX },
        { "setle",          Eb, XX, XX },
        { "setg",           Eb, XX, XX },
        /* a0 */
        { "pushT",          fs, XX, XX },
        { "popT",           fs, XX, XX },
        { "cpuid",          XX, XX, XX },
        { "btS",            Ev, Gv, XX },
        { "shldS",          Ev, Gv, Ib },
        { "shldS",          Ev, Gv, CL },
        { "(bad)",          XX, XX, XX },
        { "(bad)",          XX, XX, XX },
        /* a8 */
        { "pushT",          gs, XX, XX },
        { "popT",           gs, XX, XX },
        { "rsm",            XX, XX, XX },
        { "btsS",           Ev, Gv, XX },
        { "shrdS",          Ev, Gv, Ib },
        { "shrdS",          Ev, Gv, CL },
        { GRP13 },
        { "imulS",          Gv, Ev, XX },
        /* b0 */
        { "cmpxchgB",       Eb, Gb, XX },
        { "cmpxchgS",       Ev, Gv, XX },
        { "lssS",           Gv, Mp, XX },
        { "btrS",           Ev, Gv, XX },
        { "lfsS",           Gv, Mp, XX },
        { "lgsS",           Gv, Mp, XX },
        { "movz{bR|x|bR|x}",Gv, Eb, XX },
        { "movz{wR|x|wR|x}",Gv, Ew, XX }, /* yes, there really is movzww ! */
        /* b8 */
        { "(bad)",          XX, XX, XX },
        { "ud2b",           XX, XX, XX },
        { GRP8 },
        { "btcS",           Ev, Gv, XX },
        { "bsfS",           Gv, Ev, XX },
        { "bsrS",           Gv, Ev, XX },
        { "movs{bR|x|bR|x}",Gv, Eb, XX },
        { "movs{wR|x|wR|x}",Gv, Ew, XX }, /* yes, there really is movsww ! */
        /* c0 */
        { "xaddB",          Eb, Gb, XX },
        { "xaddS",          Ev, Gv, XX },
        { PREGRP1 },
        { "movntiS",        Ev, Gv, XX },
        { "pinsrw",         MX, Ed, Ib },
        { "pextrw",         Gd, MS, Ib },
        { "shufpX",         XM, EX, Ib },
        { GRP9 },
        /* c8 */
        { "bswap",          RMeAX, XX, XX },
        { "bswap",          RMeCX, XX, XX },
        { "bswap",          RMeDX, XX, XX },
        { "bswap",          RMeBX, XX, XX },
        { "bswap",          RMeSP, XX, XX },
        { "bswap",          RMeBP, XX, XX },
        { "bswap",          RMeSI, XX, XX },
        { "bswap",          RMeDI, XX, XX },
        /* d0 */
        { "(bad)",          XX, XX, XX },
        { "psrlw",          MX, EM, XX },
        { "psrld",          MX, EM, XX },
        { "psrlq",          MX, EM, XX },
        { "paddq",          MX, EM, XX },
        { "pmullw",         MX, EM, XX },
        { PREGRP21 },
        { "pmovmskb",       Gd, MS, XX },
        /* d8 */
        { "psubusb",        MX, EM, XX },
        { "psubusw",        MX, EM, XX },
        { "pminub",         MX, EM, XX },
        { "pand",           MX, EM, XX },
        { "paddusb",        MX, EM, XX },
        { "paddusw",        MX, EM, XX },
        { "pmaxub",         MX, EM, XX },
        { "pandn",          MX, EM, XX },
        /* e0 */
        { "pavgb",          MX, EM, XX },
        { "psraw",          MX, EM, XX },
        { "psrad",          MX, EM, XX },
        { "pavgw",          MX, EM, XX },
        { "pmulhuw",        MX, EM, XX },
        { "pmulhw",         MX, EM, XX },
        { PREGRP15 },
        { PREGRP25 },
        /* e8 */
        { "psubsb",         MX, EM, XX },
        { "psubsw",         MX, EM, XX },
        { "pminsw",         MX, EM, XX },
        { "por",            MX, EM, XX },
        { "paddsb",         MX, EM, XX },
        { "paddsw",         MX, EM, XX },
        { "pmaxsw",         MX, EM, XX },
        { "pxor",           MX, EM, XX },
        /* f0 */
        { "(bad)",          XX, XX, XX },
        { "psllw",          MX, EM, XX },
        { "pslld",          MX, EM, XX },
        { "psllq",          MX, EM, XX },
        { "pmuludq",        MX, EM, XX },
        { "pmaddwd",        MX, EM, XX },
        { "psadbw",         MX, EM, XX },
        { PREGRP18 },
        /* f8 */
        { "psubb",          MX, EM, XX },
        { "psubw",          MX, EM, XX },
        { "psubd",          MX, EM, XX },
        { "psubq",          MX, EM, XX },
        { "paddb",          MX, EM, XX },
        { "paddw",          MX, EM, XX },
        { "paddd",          MX, EM, XX },
        { "(bad)",          XX, XX, XX }
    };

    static const unsigned char onebyte_has_modrm[256] = {
        /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
        /*       -------------------------------        */
        /* 00 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 00 */
        /* 10 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 10 */
        /* 20 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 20 */
        /* 30 */ 1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0, /* 30 */
        /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 40 */
        /* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 50 */
        /* 60 */ 0,0,1,1,0,0,0,0,0,1,0,1,0,0,0,0, /* 60 */
        /* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 70 */
        /* 80 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 80 */
        /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 90 */
        /* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a0 */
        /* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b0 */
        /* c0 */ 1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0, /* c0 */
        /* d0 */ 1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* d0 */
        /* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* e0 */
        /* f0 */ 0,0,0,0,0,0,1,1,0,0,0,0,0,0,1,1  /* f0 */
        /*       -------------------------------        */
        /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
    };

    static const unsigned char twobyte_has_modrm[256] = {
        /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
        /*       -------------------------------        */
        /* 00 */ 1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1, /* 0f */
        /* 10 */ 1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0, /* 1f */
        /* 20 */ 1,1,1,1,1,0,1,0,1,1,1,1,1,1,1,1, /* 2f */
        /* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
        /* 40 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 4f */
        /* 50 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 5f */
        /* 60 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 6f */
        /* 70 */ 1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1, /* 7f */
        /* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
        /* 90 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 9f */
        /* a0 */ 0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1, /* af */
        /* b0 */ 1,1,1,1,1,1,1,1,0,0,1,1,1,1,1,1, /* bf */
        /* c0 */ 1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0, /* cf */
        /* d0 */ 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* df */
        /* e0 */ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ef */
        /* f0 */ 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0  /* ff */
        /*       -------------------------------        */
        /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
    };

    static const unsigned char twobyte_uses_SSE_prefix[256] = {
        /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
        /*       -------------------------------        */
        /* 00 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0f */
        /* 10 */ 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 1f */
        /* 20 */ 0,0,0,0,0,0,0,0,0,0,1,0,1,1,0,0, /* 2f */
        /* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 3f */
        /* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 4f */
        /* 50 */ 0,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1, /* 5f */
        /* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1, /* 6f */
        /* 70 */ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1, /* 7f */
        /* 80 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8f */
        /* 90 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9f */
        /* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* af */
        /* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* bf */
        /* c0 */ 0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0, /* cf */
        /* d0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* df */
        /* e0 */ 0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0, /* ef */
        /* f0 */ 0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0  /* ff */
        /*       -------------------------------        */
        /*       0 1 2 3 4 5 6 7 8 9 a b c d e f        */
    };

    /* If we are accessing mod/rm/reg without need_modrm set, then the
   values are stale.  Hitting this abort likely indicates that you
   need to update onebyte_has_modrm or twobyte_has_modrm.  */
#define MODRM_CHECK  if (!need_modrm) abort()

    static const char *intel_names64[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    static const char *intel_names32[] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };
    static const char *intel_names16[] = {
        "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
        "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"
    };
    static const char *intel_names8[] = {
        "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
    };
    static const char *intel_names8rex[] = {
        "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
        "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"
    };
    static const char *intel_names_seg[] = {
        "es", "cs", "ss", "ds", "fs", "gs", "?", "?",
    };
    static const char *intel_index16[] = {
        "bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx"
    };

    static const char *att_names64[] = {
        "%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
        "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
    };
    static const char *att_names32[] = {
        "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi",
        "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d"
    };
    static const char *att_names16[] = {
        "%ax", "%cx", "%dx", "%bx", "%sp", "%bp", "%si", "%di",
        "%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w"
    };
    static const char *att_names8[] = {
        "%al", "%cl", "%dl", "%bl", "%ah", "%ch", "%dh", "%bh",
    };
    static const char *att_names8rex[] = {
        "%al", "%cl", "%dl", "%bl", "%spl", "%bpl", "%sil", "%dil",
        "%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b"
    };
    static const char *att_names_seg[] = {
        "%es", "%cs", "%ss", "%ds", "%fs", "%gs", "%?", "%?",
    };
    static const char *att_index16[] = {
        "%bx,%si", "%bx,%di", "%bp,%si", "%bp,%di", "%si", "%di", "%bp", "%bx"
    };

    static const struct dis386 grps[][8] = {
        /* GRP1b */
        {
            { "addA",       Eb, Ib, XX },
            { "orA",        Eb, Ib, XX },
            { "adcA",       Eb, Ib, XX },
            { "sbbA",       Eb, Ib, XX },
            { "andA",       Eb, Ib, XX },
            { "subA",       Eb, Ib, XX },
            { "xorA",       Eb, Ib, XX },
            { "cmpA",       Eb, Ib, XX }
        },
        /* GRP1S */
        {
            { "addQ",       Ev, Iv, XX },
            { "orQ",        Ev, Iv, XX },
            { "adcQ",       Ev, Iv, XX },
            { "sbbQ",       Ev, Iv, XX },
            { "andQ",       Ev, Iv, XX },
            { "subQ",       Ev, Iv, XX },
            { "xorQ",       Ev, Iv, XX },
            { "cmpQ",       Ev, Iv, XX }
        },
        /* GRP1Ss */
        {
            { "addQ",       Ev, sIb, XX },
            { "orQ",        Ev, sIb, XX },
            { "adcQ",       Ev, sIb, XX },
            { "sbbQ",       Ev, sIb, XX },
            { "andQ",       Ev, sIb, XX },
            { "subQ",       Ev, sIb, XX },
            { "xorQ",       Ev, sIb, XX },
            { "cmpQ",       Ev, sIb, XX }
        },
        /* GRP2b */
        {
            { "rolA",       Eb, Ib, XX },
            { "rorA",       Eb, Ib, XX },
            { "rclA",       Eb, Ib, XX },
            { "rcrA",       Eb, Ib, XX },
            { "shlA",       Eb, Ib, XX },
            { "shrA",       Eb, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "sarA",       Eb, Ib, XX },
        },
        /* GRP2S */
        {
            { "rolQ",       Ev, Ib, XX },
            { "rorQ",       Ev, Ib, XX },
            { "rclQ",       Ev, Ib, XX },
            { "rcrQ",       Ev, Ib, XX },
            { "shlQ",       Ev, Ib, XX },
            { "shrQ",       Ev, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "sarQ",       Ev, Ib, XX },
        },
        /* GRP2b_one */
        {
            { "rolA",       Eb, XX, XX },
            { "rorA",       Eb, XX, XX },
            { "rclA",       Eb, XX, XX },
            { "rcrA",       Eb, XX, XX },
            { "shlA",       Eb, XX, XX },
            { "shrA",       Eb, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "sarA",       Eb, XX, XX },
        },
        /* GRP2S_one */
        {
            { "rolQ",       Ev, XX, XX },
            { "rorQ",       Ev, XX, XX },
            { "rclQ",       Ev, XX, XX },
            { "rcrQ",       Ev, XX, XX },
            { "shlQ",       Ev, XX, XX },
            { "shrQ",       Ev, XX, XX },
            { "(bad)",      XX, XX, XX},
            { "sarQ",       Ev, XX, XX },
        },
        /* GRP2b_cl */
        {
            { "rolA",       Eb, CL, XX },
            { "rorA",       Eb, CL, XX },
            { "rclA",       Eb, CL, XX },
            { "rcrA",       Eb, CL, XX },
            { "shlA",       Eb, CL, XX },
            { "shrA",       Eb, CL, XX },
            { "(bad)",      XX, XX, XX },
            { "sarA",       Eb, CL, XX },
        },
        /* GRP2S_cl */
        {
            { "rolQ",       Ev, CL, XX },
            { "rorQ",       Ev, CL, XX },
            { "rclQ",       Ev, CL, XX },
            { "rcrQ",       Ev, CL, XX },
            { "shlQ",       Ev, CL, XX },
            { "shrQ",       Ev, CL, XX },
            { "(bad)",      XX, XX, XX },
            { "sarQ",       Ev, CL, XX }
        },
        /* GRP3b */
        {
            { "testA",      Eb, Ib, XX },
            { "(bad)",      Eb, XX, XX },
            { "notA",       Eb, XX, XX },
            { "negA",       Eb, XX, XX },
            { "mulA",       Eb, XX, XX },   /* Don't print the implicit %al register,  */
            { "imulA",      Eb, XX, XX },   /* to distinguish these opcodes from other */
            { "divA",       Eb, XX, XX },   /* mul/imul opcodes.  Do the same for div  */
            { "idivA",      Eb, XX, XX }    /* and idiv for consistency.               */
        },
        /* GRP3S */
        {
            { "testQ",      Ev, Iv, XX },
            { "(bad)",      XX, XX, XX },
            { "notQ",       Ev, XX, XX },
            { "negQ",       Ev, XX, XX },
            { "mulQ",       Ev, XX, XX },   /* Don't print the implicit register.  */
            { "imulQ",      Ev, XX, XX },
            { "divQ",       Ev, XX, XX },
            { "idivQ",      Ev, XX, XX },
        },
        /* GRP4 */
        {
            { "incA",       Eb, XX, XX },
            { "decA",       Eb, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* GRP5 */
        {
            { "incQ",       Ev, XX, XX },
            { "decQ",       Ev, XX, XX },
            { "callT",      indirEv, XX, XX },
            { "lcallT",     indirEv, XX, XX },
            { "jmpT",       indirEv, XX, XX },
            { "ljmpT",      indirEv, XX, XX },
            { "pushU",      Ev, XX, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* GRP6 */
        {
            { "sldtQ",      Ev, XX, XX },
            { "strQ",       Ev, XX, XX },
            { "lldt",       Ew, XX, XX },
            { "ltr",        Ew, XX, XX },
            { "verr",       Ew, XX, XX },
            { "verw",       Ew, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX }
        },
        /* GRP7 */
        {
            { "sgdtQ",       M, XX, XX },
            { "sidtQ",       M, XX, XX },
            { "lgdtQ",       M, XX, XX },
            { "lidtQ",       M, XX, XX },
            { "smswQ",      Ev, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "lmsw",       Ew, XX, XX },
            { "invlpg",     Ew, XX, XX },
        },
        /* GRP8 */
        {
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "btQ",        Ev, Ib, XX },
            { "btsQ",       Ev, Ib, XX },
            { "btrQ",       Ev, Ib, XX },
            { "btcQ",       Ev, Ib, XX },
        },
        /* GRP9 */
        {
            { "(bad)",      XX, XX, XX },
            { "cmpxchg8b", Ev, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* GRP10 */
        {
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "psrlw",      MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "psraw",      MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "psllw",      MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* GRP11 */
        {
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "psrld",      MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "psrad",      MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "pslld",      MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* GRP12 */
        {
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "psrlq",      MS, Ib, XX },
            { "psrldq",     MS, Ib, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "psllq",      MS, Ib, XX },
            { "pslldq",     MS, Ib, XX },
        },
        /* GRP13 */
        {
            { "fxsave", Ev, XX, XX },
            { "fxrstor", Ev, XX, XX },
            { "ldmxcsr", Ev, XX, XX },
            { "stmxcsr", Ev, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "lfence", None, XX, XX },
            { "mfence", None, XX, XX },
            { "sfence", None, XX, XX },
            /* FIXME: the sfence with memory operand is clflush!  */
        },
        /* GRP14 */
        {
            { "prefetchnta", Ev, XX, XX },
            { "prefetcht0", Ev, XX, XX },
            { "prefetcht1", Ev, XX, XX },
            { "prefetcht2", Ev, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* GRPAMD */
        {
            { "prefetch", Eb, XX, XX },
            { "prefetchw", Eb, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
        }
    };

    static const struct dis386 prefix_user_table[][4] = {
        /* PREGRP0 */
        {
            { "addps", XM, EX, XX },
            { "addss", XM, EX, XX },
            { "addpd", XM, EX, XX },
            { "addsd", XM, EX, XX },
        },
        /* PREGRP1 */
        {
            { "", XM, EX, OPSIMD }, /* See OP_SIMD_SUFFIX.  */
            { "", XM, EX, OPSIMD },
            { "", XM, EX, OPSIMD },
            { "", XM, EX, OPSIMD },
        },
        /* PREGRP2 */
        {
            { "cvtpi2ps", XM, EM, XX },
            { "cvtsi2ssY", XM, Ev, XX },
            { "cvtpi2pd", XM, EM, XX },
            { "cvtsi2sdY", XM, Ev, XX },
        },
        /* PREGRP3 */
        {
            { "cvtps2pi", MX, EX, XX },
            { "cvtss2siY", Gv, EX, XX },
            { "cvtpd2pi", MX, EX, XX },
            { "cvtsd2siY", Gv, EX, XX },
        },
        /* PREGRP4 */
        {
            { "cvttps2pi", MX, EX, XX },
            { "cvttss2siY", Gv, EX, XX },
            { "cvttpd2pi", MX, EX, XX },
            { "cvttsd2siY", Gv, EX, XX },
        },
        /* PREGRP5 */
        {
            { "divps", XM, EX, XX },
            { "divss", XM, EX, XX },
            { "divpd", XM, EX, XX },
            { "divsd", XM, EX, XX },
        },
        /* PREGRP6 */
        {
            { "maxps", XM, EX, XX },
            { "maxss", XM, EX, XX },
            { "maxpd", XM, EX, XX },
            { "maxsd", XM, EX, XX },
        },
        /* PREGRP7 */
        {
            { "minps", XM, EX, XX },
            { "minss", XM, EX, XX },
            { "minpd", XM, EX, XX },
            { "minsd", XM, EX, XX },
        },
        /* PREGRP8 */
        {
            { "movups", XM, EX, XX },
            { "movss", XM, EX, XX },
            { "movupd", XM, EX, XX },
            { "movsd", XM, EX, XX },
        },
        /* PREGRP9 */
        {
            { "movups", EX, XM, XX },
            { "movss", EX, XM, XX },
            { "movupd", EX, XM, XX },
            { "movsd", EX, XM, XX },
        },
        /* PREGRP10 */
        {
            { "mulps", XM, EX, XX },
            { "mulss", XM, EX, XX },
            { "mulpd", XM, EX, XX },
            { "mulsd", XM, EX, XX },
        },
        /* PREGRP11 */
        {
            { "rcpps", XM, EX, XX },
            { "rcpss", XM, EX, XX },
            { "(bad)", XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
        /* PREGRP12 */
        {
            { "rsqrtps", XM, EX, XX },
            { "rsqrtss", XM, EX, XX },
            { "(bad)", XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
        /* PREGRP13 */
        {
            { "sqrtps", XM, EX, XX },
            { "sqrtss", XM, EX, XX },
            { "sqrtpd", XM, EX, XX },
            { "sqrtsd", XM, EX, XX },
        },
        /* PREGRP14 */
        {
            { "subps", XM, EX, XX },
            { "subss", XM, EX, XX },
            { "subpd", XM, EX, XX },
            { "subsd", XM, EX, XX },
        },
        /* PREGRP15 */
        {
            { "(bad)", XM, EX, XX },
            { "cvtdq2pd", XM, EX, XX },
            { "cvttpd2dq", XM, EX, XX },
            { "cvtpd2dq", XM, EX, XX },
        },
        /* PREGRP16 */
        {
            { "cvtdq2ps", XM, EX, XX },
            { "cvttps2dq",XM, EX, XX },
            { "cvtps2dq",XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
        /* PREGRP17 */
        {
            { "cvtps2pd", XM, EX, XX },
            { "cvtss2sd", XM, EX, XX },
            { "cvtpd2ps", XM, EX, XX },
            { "cvtsd2ss", XM, EX, XX },
        },
        /* PREGRP18 */
        {
            { "maskmovq", MX, MS, XX },
            { "(bad)", XM, EX, XX },
            { "maskmovdqu", XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
        /* PREGRP19 */
        {
            { "movq", MX, EM, XX },
            { "movdqu", XM, EX, XX },
            { "movdqa", XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
        /* PREGRP20 */
        {
            { "movq", EM, MX, XX },
            { "movdqu", EX, XM, XX },
            { "movdqa", EX, XM, XX },
            { "(bad)", EX, XM, XX },
        },
        /* PREGRP21 */
        {
            { "(bad)", EX, XM, XX },
            { "movq2dq", XM, MS, XX },
            { "movq", EX, XM, XX },
            { "movdq2q", MX, XS, XX },
        },
        /* PREGRP22 */
        {
            { "pshufw", MX, EM, Ib },
            { "pshufhw", XM, EX, Ib },
            { "pshufd", XM, EX, Ib },
            { "pshuflw", XM, EX, Ib },
        },
        /* PREGRP23 */
        {
            { "movd", Ed, MX, XX },
            { "movq", XM, EX, XX },
            { "movd", Ed, XM, XX },
            { "(bad)", Ed, XM, XX },
        },
        /* PREGRP24 */
        {
            { "(bad)", MX, EX, XX },
            { "(bad)", XM, EX, XX },
            { "punpckhqdq", XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
        /* PREGRP25 */
        {
            { "movntq", Ev, MX, XX },
            { "(bad)", Ev, XM, XX },
            { "movntdq", Ev, XM, XX },
            { "(bad)", Ev, XM, XX },
        },
        /* PREGRP26 */
        {
            { "(bad)", MX, EX, XX },
            { "(bad)", XM, EX, XX },
            { "punpcklqdq", XM, EX, XX },
            { "(bad)", XM, EX, XX },
        },
    };

    static const struct dis386 x86_64_table[][2] = {
        {
            { "arpl", Ew, Gw, XX },
            { "movs{||lq|xd}", Gv, Ed, XX },
        },
    };

#define INTERNAL_DISASSEMBLER_ERROR ("<internal disassembler error>")


    static const char *float_mem[] = {
        /* d8 */
        "fadd{s||s|}",
        "fmul{s||s|}",
        "fcom{s||s|}",
        "fcomp{s||s|}",
        "fsub{s||s|}",
        "fsubr{s||s|}",
        "fdiv{s||s|}",
        "fdivr{s||s|}",
        /*  d9 */
        "fld{s||s|}",
        "(bad)",
        "fst{s||s|}",
        "fstp{s||s|}",
        "fldenv",
        "fldcw",
        "fNstenv",
        "fNstcw",
        /* da */
        "fiadd{l||l|}",
        "fimul{l||l|}",
        "ficom{l||l|}",
        "ficomp{l||l|}",
        "fisub{l||l|}",
        "fisubr{l||l|}",
        "fidiv{l||l|}",
        "fidivr{l||l|}",
        /* db */
        "fild{l||l|}",
        "(bad)",
        "fist{l||l|}",
        "fistp{l||l|}",
        "(bad)",
        "fld{t||t|}",
        "(bad)",
        "fstp{t||t|}",
        /* dc */
        "fadd{l||l|}",
        "fmul{l||l|}",
        "fcom{l||l|}",
        "fcomp{l||l|}",
        "fsub{l||l|}",
        "fsubr{l||l|}",
        "fdiv{l||l|}",
        "fdivr{l||l|}",
        /* dd */
        "fld{l||l|}",
        "(bad)",
        "fst{l||l|}",
        "fstp{l||l|}",
        "frstor",
        "(bad)",
        "fNsave",
        "fNstsw",
        /* de */
        "fiadd",
        "fimul",
        "ficom",
        "ficomp",
        "fisub",
        "fisubr",
        "fidiv",
        "fidivr",
        /* df */
        "fild",
        "(bad)",
        "fist",
        "fistp",
        "fbld",
        "fild{ll||ll|}",
        "fbstp",
        "fistpll",
    };

#define ST func_OP_ST, 0
#define STi func_OP_STi, 0

#define FGRPd9_2 NULL, func_NULL, 0, func_NULL, 0, func_NULL, 0
#define FGRPd9_4 NULL, func_NULL, 1, func_NULL, 0, func_NULL, 0
#define FGRPd9_5 NULL, func_NULL, 2, func_NULL, 0, func_NULL, 0
#define FGRPd9_6 NULL, func_NULL, 3, func_NULL, 0, func_NULL, 0
#define FGRPd9_7 NULL, func_NULL, 4, func_NULL, 0, func_NULL, 0
#define FGRPda_5 NULL, func_NULL, 5, func_NULL, 0, func_NULL, 0
#define FGRPdb_4 NULL, func_NULL, 6, func_NULL, 0, func_NULL, 0
#define FGRPde_3 NULL, func_NULL, 7, func_NULL, 0, func_NULL, 0
#define FGRPdf_4 NULL, func_NULL, 8, func_NULL, 0, func_NULL, 0

    static const struct dis386 float_reg[][8] = {
        /* d8 */
        {
            { "fadd",       ST, STi, XX },
            { "fmul",       ST, STi, XX },
            { "fcom",       STi, XX, XX },
            { "fcomp",      STi, XX, XX },
            { "fsub",       ST, STi, XX },
            { "fsubr",      ST, STi, XX },
            { "fdiv",       ST, STi, XX },
            { "fdivr",      ST, STi, XX },
        },
        /* d9 */
        {
            { "fld",        STi, XX, XX },
            { "fxch",       STi, XX, XX },
            { FGRPd9_2 },
            { "(bad)",      XX, XX, XX },
            { FGRPd9_4 },
            { FGRPd9_5 },
            { FGRPd9_6 },
            { FGRPd9_7 },
        },
        /* da */
        {
            { "fcmovb",     ST, STi, XX },
            { "fcmove",     ST, STi, XX },
            { "fcmovbe",ST, STi, XX },
            { "fcmovu",     ST, STi, XX },
            { "(bad)",      XX, XX, XX },
            { FGRPda_5 },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* db */
        {
            { "fcmovnb",ST, STi, XX },
            { "fcmovne",ST, STi, XX },
            { "fcmovnbe",ST, STi, XX },
            { "fcmovnu",ST, STi, XX },
            { FGRPdb_4 },
            { "fucomi",     ST, STi, XX },
            { "fcomi",      ST, STi, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* dc */
        {
            { "fadd",       STi, ST, XX },
            { "fmul",       STi, ST, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
#if UNIXWARE_COMPAT
            { "fsub",       STi, ST, XX },
            { "fsubr",      STi, ST, XX },
            { "fdiv",       STi, ST, XX },
            { "fdivr",      STi, ST, XX },
#else
            { "fsubr",      STi, ST, XX },
            { "fsub",       STi, ST, XX },
            { "fdivr",      STi, ST, XX },
            { "fdiv",       STi, ST, XX },
#endif
        },
        /* dd */
        {
            { "ffree",      STi, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "fst",        STi, XX, XX },
            { "fstp",       STi, XX, XX },
            { "fucom",      STi, XX, XX },
            { "fucomp",     STi, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
        },
        /* de */
        {
            { "faddp",      STi, ST, XX },
            { "fmulp",      STi, ST, XX },
            { "(bad)",      XX, XX, XX },
            { FGRPde_3 },
#if UNIXWARE_COMPAT
            { "fsubp",      STi, ST, XX },
            { "fsubrp",     STi, ST, XX },
            { "fdivp",      STi, ST, XX },
            { "fdivrp",     STi, ST, XX },
#else
            { "fsubrp",     STi, ST, XX },
            { "fsubp",      STi, ST, XX },
            { "fdivrp",     STi, ST, XX },
            { "fdivp",      STi, ST, XX },
#endif
        },
        /* df */
        {
            { "ffreep",     STi, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { "(bad)",      XX, XX, XX },
            { FGRPdf_4 },
            { "fucomip",ST, STi, XX },
            { "fcomip", ST, STi, XX },
            { "(bad)",      XX, XX, XX },
        },
    };
    static char *fgrps[][8] = {
        /* d9_2  0 */
        {
            "fnop","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
        },

        /* d9_4  1 */
        {
            "fchs","fabs","(bad)","(bad)","ftst","fxam","(bad)","(bad)",
        },

        /* d9_5  2 */
        {
            "fld1","fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz","(bad)",
        },

        /* d9_6  3 */
        {
            "f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp","fincstp",
        },

        /* d9_7  4 */
        {
            "fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos",
        },

        /* da_5  5 */
        {
            "(bad)","fucompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
        },

        /* db_4  6 */
        {
            "feni(287 only)","fdisi(287 only)","fNclex","fNinit",
            "fNsetpm(287 only)","(bad)","(bad)","(bad)",
        },

        /* de_3  7 */
        {
            "(bad)","fcompp","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
        },

        /* df_4  8 */
        {
            "fNstsw","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)","(bad)",
        },
    };

    static const char *Suffix3DNow[] = {
        /* 00 */        NULL,           NULL,           NULL,           NULL,
        /* 04 */        NULL,           NULL,           NULL,           NULL,
        /* 08 */        NULL,           NULL,           NULL,           NULL,
        /* 0C */        "pi2fw",        "pi2fd",        NULL,           NULL,
        /* 10 */        NULL,           NULL,           NULL,           NULL,
        /* 14 */        NULL,           NULL,           NULL,           NULL,
        /* 18 */        NULL,           NULL,           NULL,           NULL,
        /* 1C */        "pf2iw",        "pf2id",        NULL,           NULL,
        /* 20 */        NULL,           NULL,           NULL,           NULL,
        /* 24 */        NULL,           NULL,           NULL,           NULL,
        /* 28 */        NULL,           NULL,           NULL,           NULL,
        /* 2C */        NULL,           NULL,           NULL,           NULL,
        /* 30 */        NULL,           NULL,           NULL,           NULL,
        /* 34 */        NULL,           NULL,           NULL,           NULL,
        /* 38 */        NULL,           NULL,           NULL,           NULL,
        /* 3C */        NULL,           NULL,           NULL,           NULL,
        /* 40 */        NULL,           NULL,           NULL,           NULL,
        /* 44 */        NULL,           NULL,           NULL,           NULL,
        /* 48 */        NULL,           NULL,           NULL,           NULL,
        /* 4C */        NULL,           NULL,           NULL,           NULL,
        /* 50 */        NULL,           NULL,           NULL,           NULL,
        /* 54 */        NULL,           NULL,           NULL,           NULL,
        /* 58 */        NULL,           NULL,           NULL,           NULL,
        /* 5C */        NULL,           NULL,           NULL,           NULL,
        /* 60 */        NULL,           NULL,           NULL,           NULL,
        /* 64 */        NULL,           NULL,           NULL,           NULL,
        /* 68 */        NULL,           NULL,           NULL,           NULL,
        /* 6C */        NULL,           NULL,           NULL,           NULL,
        /* 70 */        NULL,           NULL,           NULL,           NULL,
        /* 74 */        NULL,           NULL,           NULL,           NULL,
        /* 78 */        NULL,           NULL,           NULL,           NULL,
        /* 7C */        NULL,           NULL,           NULL,           NULL,
        /* 80 */        NULL,           NULL,           NULL,           NULL,
        /* 84 */        NULL,           NULL,           NULL,           NULL,
        /* 88 */        NULL,           NULL,           "pfnacc",       NULL,
        /* 8C */        NULL,           NULL,           "pfpnacc",      NULL,
        /* 90 */        "pfcmpge",      NULL,           NULL,           NULL,
        /* 94 */        "pfmin",        NULL,           "pfrcp",        "pfrsqrt",
        /* 98 */        NULL,           NULL,           "pfsub",        NULL,
        /* 9C */        NULL,           NULL,           "pfadd",        NULL,
        /* A0 */        "pfcmpgt",      NULL,           NULL,           NULL,
        /* A4 */        "pfmax",        NULL,           "pfrcpit1",     "pfrsqit1",
        /* A8 */        NULL,           NULL,           "pfsubr",       NULL,
        /* AC */        NULL,           NULL,           "pfacc",        NULL,
        /* B0 */        "pfcmpeq",      NULL,           NULL,           NULL,
        /* B4 */        "pfmul",        NULL,           "pfrcpit2",     "pfmulhrw",
        /* B8 */        NULL,           NULL,           NULL,           "pswapd",
        /* BC */        NULL,           NULL,           NULL,           "pavgusb",
        /* C0 */        NULL,           NULL,           NULL,           NULL,
        /* C4 */        NULL,           NULL,           NULL,           NULL,
        /* C8 */        NULL,           NULL,           NULL,           NULL,
        /* CC */        NULL,           NULL,           NULL,           NULL,
        /* D0 */        NULL,           NULL,           NULL,           NULL,
        /* D4 */        NULL,           NULL,           NULL,           NULL,
        /* D8 */        NULL,           NULL,           NULL,           NULL,
        /* DC */        NULL,           NULL,           NULL,           NULL,
        /* E0 */        NULL,           NULL,           NULL,           NULL,
        /* E4 */        NULL,           NULL,           NULL,           NULL,
        /* E8 */        NULL,           NULL,           NULL,           NULL,
        /* EC */        NULL,           NULL,           NULL,           NULL,
        /* F0 */        NULL,           NULL,           NULL,           NULL,
        /* F4 */        NULL,           NULL,           NULL,           NULL,
        /* F8 */        NULL,           NULL,           NULL,           NULL,
        /* FC */        NULL,           NULL,           NULL,           NULL,
    };

    static const char *simd_cmp_op[] = {
        "eq",
        "lt",
        "le",
        "unord",
        "neq",
        "nlt",
        "nle",
        "ord"
    };


    enum dis_insn_type {
        dis_noninsn,		/* Not a valid instruction */
        dis_nonbranch,		/* Not a branch instruction */
        dis_branch,			/* Unconditional branch */
        dis_condbranch,		/* Conditional branch */
        dis_jsr,			/* Jump to subroutine */
        dis_condjsr,		/* Conditional jump to subroutine */
        dis_dref,			/* Data reference instruction */
        dis_dref2			/* Two data references in instruction */
    };
    
    
    //*********************************************************
    //        extracted from bfd.h
    // ********************************************************
    
    // 64 bit
    /*
#define sprintf_vma(s,x) sprintf (s, "%016lx", (uint64_t)x)
#define fprintf_vma(f,x) fprintf (f, "%016lx", (uint64_t)x)
    */    

      #define _bfd_int64_low(x) ((unsigned long) (((x) & 0xffffffff)))
      #define _bfd_int64_high(x) ((unsigned long) (((x) >> 32) & 0xffffffff))
      #define fprintf_vma(s,x) \
      fprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
      #define sprintf_vma(s,x) \
      sprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))

    
    /* Extracted from archures.c.  */
    enum mach_architecture
        {
            mach_arch_unknown,   /* File arch not known.  */
            mach_arch_obscure,   /* Arch known, not one of these.  */
            mach_arch_i386,      /* Intel 386 */
#define mach_i386_i386 0
#define mach_i386_i8086 1
#define mach_i386_i386_intel_syntax 2
#define mach_x86_64 3
#define mach_x86_64_intel_syntax 4
        };
    
    enum byte_endian { BYTE_ENDIAN_BIG, BYTE_ENDIAN_LITTLE, BYTE_ENDIAN_UNKNOWN };
    
    //*********************************************************************
    
    
    extern int x86inst_intern_read_mem_func(uint64_t, uint8_t *, uint32_t, disassemble_info*);
    extern void x86inst_set_disassemble_info(disassemble_info*, uint32_t);
    
    
    /* Just print the address in hex.  This is included for completeness even
       though both GDB and objdump provide their own (to print symbolic
       addresses).  */
    extern void generic_print_address(uint64_t, struct disassemble_info *);
    
    
#ifdef __cplusplus
};
#endif

#endif /* _CStructuresX86_h_ */
