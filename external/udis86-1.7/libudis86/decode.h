#ifndef UD_DECODE_H
#define UD_DECODE_H

#define MAX_INSN_LENGTH 15

#define SHFT_1(__n) (1 << __n)
#define SHFT_1L(__n) (((uint64_t)(1 << 30)) << (__n - 30))

/* flags definitions */
#define F_none ( 0 )
#define F_CF SHFT_1(0)
#define F_01 SHFT_1(1)
#define F_PF SHFT_1(2)
#define F_03 SHFT_1(3)
#define F_AF SHFT_1(4)
#define F_05 SHFT_1(5)
#define F_ZF SHFT_1(6)
#define F_SF SHFT_1(7)
#define F_TF SHFT_1(8)
#define F_IF SHFT_1(9)
#define F_DF SHFT_1(10)
#define F_OF SHFT_1(11)
#define F_IOPL1 SHFT_1(12)
#define F_IOPL2 SHFT_1(13)
#define F_NT SHFT_1(14)
#define F_15 SHFT_1(15)
#define F_RF SHFT_1(16)
#define F_VF SHFT_1(17)
#define F_AC SHFT_1(18)
#define F_VI SHFT_1(19)
#define F_VP SHFT_1(20)
#define F_ID SHFT_1(21)
#define F_22 SHFT_1(22)
#define F_23 SHFT_1(23)
#define F_24 SHFT_1(24)
#define F_25 SHFT_1(25)
#define F_26 SHFT_1(26)
#define F_27 SHFT_1(27)
#define F_28 SHFT_1(28)
#define F_29 SHFT_1(29)
#define F_30 SHFT_1(30)
#define F_31 SHFT_1(31)
#define F_ALU (F_CF | F_PF | F_AF | F_ZF | F_SF | F_OF)

/* implied register definitions */
#define R_none ( 0 )
/* general purpose */
#define R_AX   SHFT_1(0)
#define R_CX   SHFT_1(1)
#define R_DX   SHFT_1(2)
#define R_BX   SHFT_1(3)
#define R_SP   SHFT_1(4)
#define R_BP   SHFT_1(5)
#define R_SI   SHFT_1(6)
#define R_DI   SHFT_1(7)
#define R_8    SHFT_1(8)
#define R_9    SHFT_1(9)
#define R_10   SHFT_1(10)
#define R_11   SHFT_1(11)
#define R_12   SHFT_1(12)
#define R_13   SHFT_1(13)
#define R_14   SHFT_1(14)
#define R_15   SHFT_1(15)
/* multimedia */
#define R_MM0  SHFT_1(16)
#define R_MM1  SHFT_1(17)
#define R_MM2  SHFT_1(18)
#define R_MM3  SHFT_1(19)
#define R_MM4  SHFT_1(20)
#define R_MM5  SHFT_1(21)
#define R_MM6  SHFT_1(22)
#define R_MM7  SHFT_1(23)
#define R_MM8  SHFT_1(24)
#define R_MM9  SHFT_1(25)
#define R_MM10 SHFT_1(26)
#define R_MM11 SHFT_1(27)
#define R_MM12 SHFT_1(28)
#define R_MM13 SHFT_1(29)
#define R_MM14 SHFT_1(30)
#define R_MM15 SHFT_1L(31)
#define R_MMA  (R_MM0 | R_MM1 | R_MM2 | R_MM3 | R_MM4 | R_MM5 | R_MM6 | R_MM7 \
                | R_MM8 | R_MM9 | R_MM10 | R_MM11 | R_MM12 | R_MM13 | R_MM14 | R_MM15)
/* x87 */
#define R_ST0  SHFT_1L(32)
#define R_ST1  SHFT_1L(33)
#define R_ST2  SHFT_1L(34)
#define R_ST3  SHFT_1L(35)
#define R_ST4  SHFT_1L(36)
#define R_ST5  SHFT_1L(37)
#define R_ST6  SHFT_1L(38)
#define R_ST7  SHFT_1L(39)
#define R_STA  (R_ST0 | R_ST1 | R_ST2 | R_ST3 | R_ST4 | R_ST5 | R_ST6 | R_ST7)
/* segment */
#define R_ES   SHFT_1L(40)
#define R_CS   SHFT_1L(41)
#define R_SS   SHFT_1L(42)
#define R_DS   SHFT_1L(43)
#define R_FS   SHFT_1L(44)
#define R_GS   SHFT_1L(45)

/* register classes */
#define T_NONE  0
#define T_GPR   1
#define T_MMX   2
#define T_CRG   3
#define T_DBG   4
#define T_SEG   5
#define T_XMM   6
#define T_YMM   7
#define T_ZMM   8
#define T_K     9

/* itab prefix bits */
#define P_none          ( 0 )
#define P_c1            ( 1 << 0 )
#define P_C1(n)         ( ( n >> 0 ) & 1 )
#define P_rexb          ( 1 << 1 )
#define P_REXB(n)       ( ( n >> 1 ) & 1 )
#define P_depM          ( 1 << 2 )
#define P_DEPM(n)       ( ( n >> 2 ) & 1 )
#define P_c3            ( 1 << 3 )
#define P_C3(n)         ( ( n >> 3 ) & 1 )
#define P_inv64         ( 1 << 4 )
#define P_INV64(n)      ( ( n >> 4 ) & 1 )
#define P_rexw          ( 1 << 5 )
#define P_REXW(n)       ( ( n >> 5 ) & 1 )
#define P_c2            ( 1 << 6 )
#define P_C2(n)         ( ( n >> 6 ) & 1 )
#define P_def64         ( 1 << 7 )
#define P_DEF64(n)      ( ( n >> 7 ) & 1 )
#define P_rexr          ( 1 << 8 )
#define P_REXR(n)       ( ( n >> 8 ) & 1 )
#define P_oso           ( 1 << 9 )
#define P_OSO(n)        ( ( n >> 9 ) & 1 )
#define P_aso           ( 1 << 10 )
#define P_ASO(n)        ( ( n >> 10 ) & 1 )
#define P_rexx          ( 1 << 11 )
#define P_REXX(n)       ( ( n >> 11 ) & 1 )
#define P_ImpAddr       ( 1 << 12 )
#define P_IMPADDR(n)    ( ( n >> 12 ) & 1 )
#define P_vexlz         ( 1 << 13 )
#define P_VEXLZ(n)      ( ( n >> 13 ) & 1 )
#define P_vexlig        ( 1 << 14 )
#define P_VEXLIG(n)     ( ( n >> 14 ) & 1 )
#define P_vexix         ( 1 << 15 )
#define P_VEXIX(n)      ( ( n >> 15 ) & 1 )
#define P_c4            ( 1 << 16 )
#define P_C4(n)         ( ( n >> 16 ) & 1 )

/* rex prefix bits */
#define REX_W(r)        ( ( 0xF & ( r ) )  >> 3 )
#define REX_R(r)        ( ( 0x7 & ( r ) )  >> 2 )
#define REX_X(r)        ( ( 0x3 & ( r ) )  >> 1 )
#define REX_B(r)        ( ( 0x1 & ( r ) )  >> 0 )
#define REX_PFX_MASK(n) ( ( P_REXW(n) << 3 ) | \
                          ( P_REXR(n) << 2 ) | \
                          ( P_REXX(n) << 1 ) | \
                          ( P_REXB(n) << 0 ) )

/* scable-index-base bits */
#define SIB_S(b)        ( ( b ) >> 6 )
#define SIB_I(b)        ( ( ( b ) >> 3 ) & 7 )
#define SIB_B(b)        ( ( b ) & 7 )

#define SIB_SCALE(b)    ((1 << SIB_S(b)) & ~1)
/* modrm bits */
#define MODRM_REG(b)    ( ( ( b ) >> 3 ) & 7 )
#define MODRM_NNN(b)    ( ( ( b ) >> 3 ) & 7 )
#define MODRM_MOD(b)    ( ( ( b ) >> 6 ) & 3 )
#define MODRM_RM(b)     ( ( b ) & 7 )

/* vex bits (avx) */
#define P_AVX(n)        ( ( n == 0xC5 ) || ( n == 0xC4 ) )
#define VEX_IX_REG(b)   ( ( b >> 4 ) & 15 )
/* only for the 2-byte version */
#define VEX_M5(b)       ( ( ( b ) >> 0 ) & 31 )
#define VEX_REXB(b)     ( ( ( ~( b ) ) >> 5 ) & 1 )
#define VEX_REXX(b)     ( ( ( ~( b ) ) >> 6 ) & 1 )
#define VEX_REXW(b)     ( ( ( ( b ) ) >> 7 ) & 1 )
/* 1-byte and 2-byte versions */
#define VEX_REXR(b)      ( ( ( ~( b ) ) >> 7 ) & 1 )
#define VEX_L(b)         ( ( ( b ) >> 2 ) & 1 )
#define VEX_PP(b)        ( ( ( b ) >> 0 ) & 3 )
#define VEX_VVVV(b)      ( ( ( ~( b ) ) >> 3 ) & 15 )
#define VEX_REX_DEF(b, x, r, w) (((b & 1) << 0) | ((x & 1) << 1) | ((r & 1) << 2) | ((w & 1) << 3))

// MVEX byte 0
#define P_MVEX(n)        ( n == 0x62 )

// MVEX byte 1
#define MVEX_R(b)        ( ( ~( b ) >> 7 ) & 1 )
#define MVEX_X(b)        ( ( ~( b ) >> 6 ) & 1 )
#define MVEX_B(b)        ( ( ~( b ) >> 5 ) & 1 )
#define MVEX_RP(b)       ( ( ~( b ) >> 4 ) & 1 )
#define MVEX_M4(b)       ( ( ( b ) >> 0 ) & 15 )

// MVEX byte 2
#define MVEX_W(b)        ( ( ( b ) >> 7 ) & 1 )
#define MVEX_VVVV(b)     ( ( ( ~b ) >> 3 ) & 15 )
#define MVEX_PP(b)       ( ( ( b ) >> 0 ) & 3 )

// MVEX byte 3
#define MVEX_E(b)        ( ( ( b ) >> 7 ) & 1 )
#define MVEX_SSS(b)      ( ( ( b ) >> 4 ) & 7 )
#define MVEX_VP(b)       ( ( ( ~b ) >> 3 ) & 1 )
#define MVEX_KKK(b)      ( ( ( b ) >> 0 ) & 7 )

#define MVEX_REX_DEF(b,x,r,w) VEX_REX_DEF(b,x,r,w)

/* operand type constants -- order is important! */

enum ud_operand_code {
    OP_NONE,

    OP_A,      OP_E,      OP_M,       OP_G,       OP_GV,
    OP_I,

    OP_AL,     OP_CL,     OP_DL,      OP_BL,
    OP_AH,     OP_CH,     OP_DH,      OP_BH,

    OP_ALr8b,  OP_CLr9b,  OP_DLr10b,  OP_BLr11b,
    OP_AHr12b, OP_CHr13b, OP_DHr14b,  OP_BHr15b,

    OP_AX,     OP_CX,     OP_DX,      OP_BX,
    OP_SI,     OP_DI,     OP_SP,      OP_BP,

    OP_rAX,    OP_rCX,    OP_rDX,     OP_rBX,  
    OP_rSP,    OP_rBP,    OP_rSI,     OP_rDI,

    OP_rAXr8,  OP_rCXr9,  OP_rDXr10,  OP_rBXr11,  
    OP_rSPr12, OP_rBPr13, OP_rSIr14,  OP_rDIr15,

    OP_eAX,    OP_eCX,    OP_eDX,     OP_eBX,
    OP_eSP,    OP_eBP,    OP_eSI,     OP_eDI,

    OP_ES,     OP_CS,     OP_SS,      OP_DS,  
    OP_FS,     OP_GS,

    OP_ST0,    OP_ST1,    OP_ST2,     OP_ST3,
    OP_ST4,    OP_ST5,    OP_ST6,     OP_ST7,

    OP_J,      OP_S,      OP_O,          
    OP_I1,     OP_I3, 

    OP_V,      OP_W,      OP_Q,       OP_P, 

    OP_R,      OP_C,  OP_D,       OP_VR,  OP_PR,
    OP_X,
    OP_ZR,     OP_ZM, OP_ZRM, OP_ZV, OP_ZVM,
    OP_KR, OP_KRM, OP_KV
};


/* operand size constants */

enum ud_operand_size {
    SZ_NA  = 0,
    SZ_Z   = 1,
    SZ_V   = 2,
    SZ_P   = 3,
    SZ_WP  = 4,
    SZ_DP  = 5,
    SZ_MDQ = 6,
    SZ_RDQ = 7,

    /* the following values are used as is,
     * and thus hard-coded. changing them 
     * will break internals 
     */
    SZ_B   = 8,
    SZ_W   = 16,
    SZ_D   = 32,
    SZ_Q   = 64,
    SZ_T   = 80,
    SZ_X   = 128,
    SZ_Y   = 256,
    SZ_XZ   = 512
};

/* itab entry operand definitions */

#define O_rSPr12  { OP_rSPr12,   SZ_NA    }
#define O_BL      { OP_BL,       SZ_NA    }
#define O_BH      { OP_BH,       SZ_NA    }
#define O_BP      { OP_BP,       SZ_NA    }
#define O_AHr12b  { OP_AHr12b,   SZ_NA    }
#define O_BX      { OP_BX,       SZ_NA    }
#define O_Jz      { OP_J,        SZ_Z     }
#define O_Jv      { OP_J,        SZ_V     }
#define O_Jb      { OP_J,        SZ_B     }
#define O_rSIr14  { OP_rSIr14,   SZ_NA    }
#define O_GS      { OP_GS,       SZ_NA    }
#define O_D       { OP_D,        SZ_NA    }
#define O_rBPr13  { OP_rBPr13,   SZ_NA    }
#define O_Ob      { OP_O,        SZ_B     }
#define O_P       { OP_P,        SZ_NA    }
#define O_Pq      { OP_P,        SZ_Q     }
#define O_Ow      { OP_O,        SZ_W     }
#define O_Ov      { OP_O,        SZ_V     }
#define O_Gw      { OP_G,        SZ_W     }
#define O_Gv      { OP_G,        SZ_V     }
#define O_Gq      { OP_G,        SZ_Q     }
#define O_GVd     { OP_GV,       SZ_D     }
#define O_GVq     { OP_GV,       SZ_Q     }
#define O_rDX     { OP_rDX,      SZ_NA    }
#define O_Gx      { OP_G,        SZ_MDQ   }
#define O_Gd      { OP_G,        SZ_D     }
#define O_Gb      { OP_G,        SZ_B     }
#define O_rBXr11  { OP_rBXr11,   SZ_NA    }
#define O_rDI     { OP_rDI,      SZ_NA    }
#define O_rSI     { OP_rSI,      SZ_NA    }
#define O_ALr8b   { OP_ALr8b,    SZ_NA    }
#define O_eDI     { OP_eDI,      SZ_NA    }
#define O_Gz      { OP_G,        SZ_Z     }
#define O_eDX     { OP_eDX,      SZ_NA    }
#define O_DHr14b  { OP_DHr14b,   SZ_NA    }
#define O_rSP     { OP_rSP,      SZ_NA    }
#define O_PR      { OP_PR,       SZ_NA    }
#define O_PRq     { OP_PR,       SZ_Q     }
#define O_NONE    { OP_NONE,     SZ_NA    }
#define O_rCX     { OP_rCX,      SZ_NA    }
#define O_jWP     { OP_J,        SZ_WP    }
#define O_rDXr10  { OP_rDXr10,   SZ_NA    }
#define O_Md      { OP_M,        SZ_D     }
#define O_C       { OP_C,        SZ_NA    }
#define O_G       { OP_G,        SZ_NA    }
#define O_Mb      { OP_M,        SZ_B     }
#define O_Mt      { OP_M,        SZ_T     }
#define O_S       { OP_S,        SZ_NA    }
#define O_Mq      { OP_M,        SZ_Q     }
#define O_X       { OP_X,        SZ_X     }
#define O_x       { OP_x,        SZ_NA    }
#define O_W       { OP_W,        SZ_NA    }
#define O_Wd      { OP_W,        SZ_D     }
#define O_Wq      { OP_W,        SZ_Q     }
#define O_Wx      { OP_W,        SZ_X     }
#define O_ES      { OP_ES,       SZ_NA    }
#define O_rBX     { OP_rBX,      SZ_NA    }
#define O_Ed      { OP_E,        SZ_D     }
#define O_Eq      { OP_E,        SZ_Q     }
#define O_DLr10b  { OP_DLr10b,   SZ_NA    }
#define O_Mw      { OP_M,        SZ_W     }
#define O_Eb      { OP_E,        SZ_B     }
#define O_Ex      { OP_E,        SZ_MDQ   }
#define O_Ez      { OP_E,        SZ_Z     }
#define O_Ew      { OP_E,        SZ_W     }
#define O_Ev      { OP_E,        SZ_V     }
#define O_Ep      { OP_E,        SZ_P     }
#define O_FS      { OP_FS,       SZ_NA    }
#define O_Ms      { OP_M,        SZ_W     }
#define O_rAXr8   { OP_rAXr8,    SZ_NA    }
#define O_eBP     { OP_eBP,      SZ_NA    }
#define O_Isb     { OP_I,        SZ_SB    }
#define O_eBX     { OP_eBX,      SZ_NA    }
#define O_rCXr9   { OP_rCXr9,    SZ_NA    }
#define O_jDP     { OP_J,        SZ_DP    }
#define O_CH      { OP_CH,       SZ_NA    }
#define O_CL      { OP_CL,       SZ_NA    }
#define O_R       { OP_R,        SZ_RDQ   }
#define O_V       { OP_V,        SZ_NA    }
#define O_Vd      { OP_V,        SZ_D     }
#define O_Vq      { OP_V,        SZ_Q     }
#define O_Vx      { OP_V,        SZ_X     }
#define O_CS      { OP_CS,       SZ_NA    }
#define O_CHr13b  { OP_CHr13b,   SZ_NA    }
#define O_eCX     { OP_eCX,      SZ_NA    }
#define O_eSP     { OP_eSP,      SZ_NA    }
#define O_SS      { OP_SS,       SZ_NA    }
#define O_SP      { OP_SP,       SZ_NA    }
#define O_BLr11b  { OP_BLr11b,   SZ_NA    }
#define O_SI      { OP_SI,       SZ_NA    }
#define O_eSI     { OP_eSI,      SZ_NA    }
#define O_DL      { OP_DL,       SZ_NA    }
#define O_DH      { OP_DH,       SZ_NA    }
#define O_DI      { OP_DI,       SZ_NA    }
#define O_DX      { OP_DX,       SZ_NA    }
#define O_rBP     { OP_rBP,      SZ_NA    }
#define O_Gvw     { OP_G,        SZ_MDQ   }
#define O_I1      { OP_I1,       SZ_NA    }
#define O_I3      { OP_I3,       SZ_NA    }
#define O_DS      { OP_DS,       SZ_NA    }
#define O_ST4     { OP_ST4,      SZ_NA    }
#define O_ST5     { OP_ST5,      SZ_NA    }
#define O_ST6     { OP_ST6,      SZ_NA    }
#define O_ST7     { OP_ST7,      SZ_NA    }
#define O_ST0     { OP_ST0,      SZ_NA    }
#define O_ST1     { OP_ST1,      SZ_NA    }
#define O_ST2     { OP_ST2,      SZ_NA    }
#define O_ST3     { OP_ST3,      SZ_NA    }
#define O_E       { OP_E,        SZ_NA    }
#define O_AH      { OP_AH,       SZ_NA    }
#define O_M       { OP_M,        SZ_NA    }
#define O_AL      { OP_AL,       SZ_NA    }
#define O_CLr9b   { OP_CLr9b,    SZ_NA    }
#define O_Q       { OP_Q,        SZ_NA    }
#define O_Qq      { OP_Q,        SZ_Q     }
#define O_eAX     { OP_eAX,      SZ_NA    }
#define O_VR      { OP_VR,       SZ_NA    }
#define O_VRx     { OP_VR,       SZ_X     }
#define O_AX      { OP_AX,       SZ_NA    }
#define O_rAX     { OP_rAX,      SZ_NA    }
#define O_Iz      { OP_I,        SZ_Z     }
#define O_rDIr15  { OP_rDIr15,   SZ_NA    }
#define O_Iw      { OP_I,        SZ_W     }
#define O_Iv      { OP_I,        SZ_V     }
#define O_Ap      { OP_A,        SZ_P     }
#define O_CX      { OP_CX,       SZ_NA    }
#define O_Ib      { OP_I,        SZ_B     }
#define O_BHr15b  { OP_BHr15b,   SZ_NA    }

#define O_ZR      { OP_ZR,       SZ_XZ    }
#define O_ZM      { OP_ZM,       SZ_XZ    }
#define O_ZRM     { OP_ZRM,      SZ_XZ    }
#define O_ZV      { OP_ZV,       SZ_XZ    }
#define O_ZVM     { OP_ZVM,      SZ_XZ    }

#define O_KR      { OP_KR,       SZ_W     }
#define O_KRM     { OP_KRM,      SZ_W     }
#define O_KV      { OP_KV,       SZ_W     }

/* A single operand of an entry in the instruction table. 
 * (internal use only)
 */
struct ud_itab_entry_operand 
{
  enum ud_operand_code type;
  enum ud_operand_size size;
};


/* A single entry in an instruction table. 
 *(internal use only)
 */
struct ud_itab_entry 
{
  enum ud_mnemonic_code         mnemonic;
  struct ud_itab_entry_operand  operand1;
  struct ud_itab_entry_operand  operand2;
  struct ud_itab_entry_operand  operand3;
  struct ud_itab_entry_operand  operand4;
  uint32_t                      flags_use;
  uint32_t                      flags_def;
  uint64_t                      impreg_use;
  uint64_t                      impreg_def;
  uint32_t                      prefix;
};

extern const char * ud_lookup_mnemonic( enum ud_mnemonic_code c );

#endif /* UD_DECODE_H */

/* vim:cindent
 * vim:expandtab
 * vim:ts=4
 * vim:sw=4
 */
