/* -----------------------------------------------------------------------------
 * types.h
 *
 * Copyright (c) 2006, Vivek Mohan <vivek@sig9.com>
 * All rights reserved. See LICENSE
 * -----------------------------------------------------------------------------
 */
#ifndef UD_TYPES_H
#define UD_TYPES_H

#include <stdio.h>

#ifdef _MSC_VER
# define FMT64 "%I64"
  typedef unsigned __int8 uint8_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int32 uint32_t;
  typedef unsigned __int64 uint64_t;
  typedef __int8 int8_t;
  typedef __int16 int16_t;
  typedef __int32 int32_t;
  typedef __int64 int64_t;
#else
# define FMT64 "%ll"
# include <inttypes.h>
#endif

#include "itab.h"

/* -----------------------------------------------------------------------------
 * All possible "types" of objects in udis86. Order is Important!
 * -----------------------------------------------------------------------------
 */
enum ud_type
{
  UD_NONE,

  /* 8 bit GPRs */
  /* 1-20 */
  UD_R_AL,	UD_R_CL,	UD_R_DL,	UD_R_BL,
  UD_R_AH,	UD_R_CH,	UD_R_DH,	UD_R_BH,
  UD_R_SPL,	UD_R_BPL,	UD_R_SIL,	UD_R_DIL,
  UD_R_R8B,	UD_R_R9B,	UD_R_R10B,	UD_R_R11B,
  UD_R_R12B,	UD_R_R13B,	UD_R_R14B,	UD_R_R15B,

  /* 16 bit GPRs */
  /* 21-36 */
  UD_R_AX,	UD_R_CX,	UD_R_DX,	UD_R_BX,
  UD_R_SP,	UD_R_BP,	UD_R_SI,	UD_R_DI,
  UD_R_R8W,	UD_R_R9W,	UD_R_R10W,	UD_R_R11W,
  UD_R_R12W,	UD_R_R13W,	UD_R_R14W,	UD_R_R15W,
	
  /* 32 bit GPRs */
  /* 37-52 */
  UD_R_EAX,	UD_R_ECX,	UD_R_EDX,	UD_R_EBX,
  UD_R_ESP,	UD_R_EBP,	UD_R_ESI,	UD_R_EDI,
  UD_R_R8D,	UD_R_R9D,	UD_R_R10D,	UD_R_R11D,
  UD_R_R12D,	UD_R_R13D,	UD_R_R14D,	UD_R_R15D,
	
  /* 64 bit GPRs */
  /* 52-68 */
  UD_R_RAX,	UD_R_RCX,	UD_R_RDX,	UD_R_RBX,
  UD_R_RSP,	UD_R_RBP,	UD_R_RSI,	UD_R_RDI,
  UD_R_R8,	UD_R_R9,	UD_R_R10,	UD_R_R11,
  UD_R_R12,	UD_R_R13,	UD_R_R14,	UD_R_R15,

  /* segment registers */
  /* 68-74 */
  UD_R_ES,	UD_R_CS,	UD_R_SS,	UD_R_DS,
  UD_R_FS,	UD_R_GS,	

  /* control registers*/
  /* 75-90 */
  UD_R_CR0,	UD_R_CR1,	UD_R_CR2,	UD_R_CR3,
  UD_R_CR4,	UD_R_CR5,	UD_R_CR6,	UD_R_CR7,
  UD_R_CR8,	UD_R_CR9,	UD_R_CR10,	UD_R_CR11,
  UD_R_CR12,	UD_R_CR13,	UD_R_CR14,	UD_R_CR15,
	
  /* debug registers */
  /* 91-106 */
  UD_R_DR0,	UD_R_DR1,	UD_R_DR2,	UD_R_DR3,
  UD_R_DR4,	UD_R_DR5,	UD_R_DR6,	UD_R_DR7,
  UD_R_DR8,	UD_R_DR9,	UD_R_DR10,	UD_R_DR11,
  UD_R_DR12,	UD_R_DR13,	UD_R_DR14,	UD_R_DR15,

  /* mmx registers */
  /* 107-114 */
  UD_R_MM0,	UD_R_MM1,	UD_R_MM2,	UD_R_MM3,
  UD_R_MM4,	UD_R_MM5,	UD_R_MM6,	UD_R_MM7,

  /* x87 registers */
  /* 115-122 */
  UD_R_ST0,	UD_R_ST1,	UD_R_ST2,	UD_R_ST3,
  UD_R_ST4,	UD_R_ST5,	UD_R_ST6,	UD_R_ST7, 

  /* extended multimedia registers */
  /* 123-138 */
  UD_R_XMM0,	UD_R_XMM1,	UD_R_XMM2,	UD_R_XMM3,
  UD_R_XMM4,	UD_R_XMM5,	UD_R_XMM6,	UD_R_XMM7,
  UD_R_XMM8,	UD_R_XMM9,	UD_R_XMM10,	UD_R_XMM11,
  UD_R_XMM12,	UD_R_XMM13,	UD_R_XMM14,	UD_R_XMM15,

  /* 256-bit AVX registers */
  /* 139-154 */
  UD_R_YMM0,	UD_R_YMM1,	UD_R_YMM2,	UD_R_YMM3,
  UD_R_YMM4,	UD_R_YMM5,	UD_R_YMM6,	UD_R_YMM7,
  UD_R_YMM8,	UD_R_YMM9,	UD_R_YMM10,	UD_R_YMM11,
  UD_R_YMM12,	UD_R_YMM13,	UD_R_YMM14,	UD_R_YMM15,

  /* 512-bit AVX registers */
  /* 155 - 186 */
  UD_R_ZMM0,	UD_R_ZMM1,	UD_R_ZMM2,	UD_R_ZMM3,
  UD_R_ZMM4,	UD_R_ZMM5,	UD_R_ZMM6,	UD_R_ZMM7,
  UD_R_ZMM8,	UD_R_ZMM9,	UD_R_ZMM10,	UD_R_ZMM11,
  UD_R_ZMM12,	UD_R_ZMM13,	UD_R_ZMM14,	UD_R_ZMM15,

  UD_R_ZMM16,	UD_R_ZMM17,	UD_R_ZMM18,	UD_R_ZMM19,
  UD_R_ZMM20,	UD_R_ZMM21,	UD_R_ZMM22,	UD_R_ZMM23,
  UD_R_ZMM24,	UD_R_ZMM25,	UD_R_ZMM26,	UD_R_ZMM27,
  UD_R_ZMM28,	UD_R_ZMM29,	UD_R_ZMM30,	UD_R_ZMM31,

  /* 16-bit K registers */
  /* 187 - 194 */
  UD_R_K0,	UD_R_K1,	UD_R_K2,	UD_R_K3,
  UD_R_K4,	UD_R_K5,	UD_R_K6,	UD_R_K7,

  /* 195 */
  UD_R_RIP,

  /* Operand Types */
  UD_OP_REG,	UD_OP_MEM,	UD_OP_PTR,	UD_OP_IMM,	
  UD_OP_JIMM,	UD_OP_CONST
};

/* -----------------------------------------------------------------------------
 * struct ud_operand - Disassembled instruction Operand.
 * -----------------------------------------------------------------------------
 */
struct ud_operand 
{
  enum ud_type		type;
  uint16_t		size;
  uint8_t               position; /* PEBIL */
  union {
	int8_t		sbyte;
	uint8_t		ubyte;
	int16_t		sword;
	uint16_t	uword;
	int32_t		sdword;
	uint32_t	udword;
	int64_t		sqword;
	uint64_t	uqword;

	struct {
		uint16_t seg;
		uint32_t off;
	} ptr;
  } lval;
  enum ud_type		base;
  enum ud_type		index;
  uint8_t		offset; // offset size in bits
  uint8_t		scale;	
};

/* -----------------------------------------------------------------------------
 * struct ud - The udis86 object.
 * -----------------------------------------------------------------------------
 */
struct ud
{
  int 			(*inp_hook) (struct ud*);
  uint8_t		inp_curr;
  uint8_t		inp_fill;
  FILE*			inp_file;
  uint8_t		inp_ctr;
  uint8_t*		inp_buff;
  uint8_t*		inp_buff_end;
  uint8_t		inp_end;
  void			(*translator)(struct ud*);
  uint64_t		insn_offset;
  char			insn_bytes[16];
  char			insn_hexcode[32];
  char			insn_buffer[64];
  unsigned int		insn_fill;
  uint8_t		dis_mode;
  uint64_t		pc;
  uint8_t		vendor;
  struct map_entry*	mapen;
  enum ud_mnemonic_code	mnemonic;
  struct ud_operand	operand[4];
  uint8_t		error;
  uint8_t	 	pfx_rex;
  uint8_t 		pfx_seg;
  uint8_t 		pfx_opr;
  uint8_t 		pfx_adr;
  uint8_t 		pfx_lock;
  uint8_t 		pfx_rep;
  uint8_t 		pfx_repe;
  uint8_t 		pfx_repne;
  uint8_t 		pfx_insn;
  uint8_t               pfx_avx;
  uint8_t               avx_vex[2];
  uint8_t               mvex[3];
  enum ud_type          vector_mask_register;
  uint8_t               conversion;
  uint8_t		default64;
  uint8_t		opr_mode;
  uint8_t		adr_mode;
  uint8_t		br_far;
  uint8_t		br_near;
  uint8_t		implicit_addr;
  uint8_t		c1;
  uint8_t		c2;
  uint8_t		c3;
  uint8_t		c4;
  uint8_t 		inp_cache[256];
  uint8_t		inp_sess[64];
  uint32_t              flags_use;
  uint32_t              flags_def;
    // 16 GPR (l/h/x/e/r) + 6 SEG + 8 X87 + 16 MMX (mmx/xmm/ymm) + 1 RIP
  uint64_t               impreg_use;
  uint64_t               impreg_def;
  struct ud_itab_entry * itab_entry;
};

/* -----------------------------------------------------------------------------
 * Type-definitions
 * -----------------------------------------------------------------------------
 */
typedef enum ud_type 		ud_type_t;
typedef enum ud_mnemonic_code	ud_mnemonic_code_t;

typedef struct ud 		ud_t;
typedef struct ud_operand 	ud_operand_t;

#define UD_SYN_INTEL		ud_translate_intel
#define UD_SYN_ATT		ud_translate_att
#define UD_EOI			-1
#define UD_INP_CACHE_SZ		32
#define UD_VENDOR_AMD		0
#define UD_VENDOR_INTEL		1

#define bail_out(ud,error_code) longjmp( (ud)->bailout, error_code )
#define try_decode(ud) if ( setjmp( (ud)->bailout ) == 0 )
#define catch_error() else

#endif
