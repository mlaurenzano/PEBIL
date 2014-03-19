/* -----------------------------------------------------------------------------
 * decode.c
 *
 * Copyright (c) 2005, 2006, Vivek Mohan <vivek@sig9.com>
 * All rights reserved. See LICENSE
 * -----------------------------------------------------------------------------
 */

#include <assert.h>
#include <string.h>

#include "types.h"
#include "itab.h"
#include "input.h"
#include "decode.h"

//#define PEBIL_DEBUG(...) fprintf(stdout, "PEBIL_DEBUG: "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout);
#define PEBIL_DEBUG(...)

/* The max number of prefixes to an instruction */
#define MAX_PREFIXES    15

static struct ud_itab_entry ie_invalid = { UD_Iinvalid, O_NONE, O_NONE, O_NONE, O_NONE, F_none, F_none, R_none, R_none, P_none };
static struct ud_itab_entry ie_pause   = { UD_Ipause,   O_NONE, O_NONE, O_NONE, O_NONE, F_none, F_none, R_none, R_none, P_none };
static struct ud_itab_entry ie_nop     = { UD_Inop,     O_NONE, O_NONE, O_NONE, O_NONE, F_none, F_none, R_none, R_none, P_none };

struct modrm {
  unsigned char modrm;
  int position;
  char set;
};

static inline unsigned char get_modrm(struct ud* u, struct modrm* modrm)
{
  if(!modrm->set) {
    modrm->modrm = inp_next(u);
    modrm->position = ud_insn_len(u);
    modrm->set = 1;
  }
  return modrm->modrm;
}

static inline void clear_modrm(struct modrm* modrm)
{
  modrm->set = 0;
}

/* Looks up mnemonic code in the mnemonic string table
 * Returns NULL if the mnemonic code is invalid
 */
const char * ud_lookup_mnemonic( enum ud_mnemonic_code c )
{
    if ( c < UD_Id3vil )
        return ud_mnemonics_str[ c ];
    return NULL;
}


static void decode_vex(struct ud* u)
{
    uint8_t vex = u->avx_vex[0];
    uint8_t ext;
    switch(VEX_PP(vex)){
        case 0:
          ext = 0x00;
          break;
        case 1:
          ext = 0x66;
          break;
        case 2:
          ext = 0xF3;
          break;
        case 3:
          ext = 0xF2;
          break;
        default:
          assert(0);
    }
    u->pfx_avx = ext;

    /* 2-byte form */
    if (u->pfx_insn == 0xC4){
        uint8_t v2 = u->avx_vex[1];
        u->pfx_rex = VEX_REX_DEF(VEX_REXB(v2), VEX_REXX(v2), VEX_REXR(v2), VEX_REXW(vex));
        PEBIL_DEBUG("vex C4 rex prefix %#hhx(b), %#hhx(x), %#hhx(r), %#hhx(w), full %hhu", VEX_REXB(v2), VEX_REXX(v2), VEX_REXR(v2), VEX_REXW(vex), u->pfx_rex);

        /* 1-byte form */
    } else if (u->pfx_insn == 0xC5){
        u->pfx_rex = VEX_REX_DEF(0, 0, VEX_REXR(vex), 0);
        PEBIL_DEBUG("vex C5 rex prefix %#hhx(b), %#hhx(x), %#hhx(r), %#hhx(w), full %hhu", 0, 0, VEX_REXR(vex), 0, u->pfx_rex);
    }


}
static int gen_hex( struct ud *u );

/* Extracts instruction prefixes.
 */
static int get_prefixes( struct ud* u )
{
    unsigned int have_pfx = 1;
    unsigned int i;
    uint8_t curr;

    /* if in error state, bail out */
    if ( u->error ) 
        return -1; 

    PEBIL_DEBUG("getting prefix");
    /* keep going as long as there are prefixes available */
    for ( i = 0; have_pfx ; ++i ) {

        /* Get next byte. */
        inp_next(u); 
        if ( u->error ) 
            return -1;
        curr = inp_curr( u );
        PEBIL_DEBUG("\tgetting prefix for curr %hhx", curr);

        /* rex prefixes in 64bit mode */
        if ( u->dis_mode == 64 && ( curr & 0xF0 ) == 0x40 ) {
            PEBIL_DEBUG("\t\trex!");
            u->pfx_rex = curr;  
        } else {
            PEBIL_DEBUG("\t\tnot rex");
            switch ( curr )  
            {
            case 0x2E : 
                u->pfx_seg = UD_R_CS; 
                u->pfx_rex = 0;
                break;
            case 0x36 :     
                u->pfx_seg = UD_R_SS; 
                u->pfx_rex = 0;
                break;
            case 0x3E : 
                u->pfx_seg = UD_R_DS; 
                u->pfx_rex = 0;
                break;
            case 0x26 : 
                u->pfx_seg = UD_R_ES; 
                u->pfx_rex = 0;
                break;
            case 0x64 : 
                u->pfx_seg = UD_R_FS; 
                u->pfx_rex = 0;
                break;
            case 0x65 : 
                u->pfx_seg = UD_R_GS; 
                u->pfx_rex = 0;
                break;
            case 0x67 : /* adress-size override prefix */ 
                u->pfx_adr = 0x67;
                u->pfx_rex = 0;
                break;
            case 0xF0 : 
                u->pfx_lock = 0xF0;
                u->pfx_rex  = 0;
                break;
            case 0x66: 
                /* the 0x66 sse prefix is only effective if no other sse prefix
                 * has already been specified.
                 */
                if ( !u->pfx_insn ) u->pfx_insn = 0x66;
                u->pfx_opr = 0x66;           
                u->pfx_rex = 0;
                break;
            case 0xF2:
                u->pfx_insn  = 0xF2;
                u->pfx_repne = 0xF2; 
                u->pfx_rex   = 0;
                break;
            case 0xF3:
                u->pfx_insn = 0xF3;
                u->pfx_rep  = 0xF3; 
                u->pfx_repe = 0xF3; 
                u->pfx_rex  = 0;
                break;

                /* PEBIL */
                /* for AVX instructions */
                /* 2-byte vex */
            case 0xC4:
                if (!u->pfx_avx){
                
                    PEBIL_DEBUG("\t\tAVX prefix found %hhx", curr);
                    
                    inp_next(u);
                    u->avx_vex[1]  = inp_curr(u);
                }
                /* 1-byte vex */
            case 0xC5:
                if (!u->pfx_avx){
                    PEBIL_DEBUG("\t\tAVX prefix found %hhx", curr);
                    
                    u->pfx_insn = curr;
                    inp_next(u);
                    u->avx_vex[0]  = inp_curr(u);
                    
                    u->pfx_avx  = 0;
                    u->pfx_rex  = 0;
                    
                    decode_vex(u);
                    inp_next(u);
                    have_pfx = 0;
                    break;
                }
                /* end PEBIL */
            default : 
                /* No more prefixes */
                have_pfx = 0;
                break;
            }
        }

        /* check if we reached max instruction length */
        if ( i + 1 == MAX_INSN_LENGTH ) {
            u->error = 1;
            break;
        }
    }

    /* return status */
    if ( u->error ) 
        return -1; 

    /* rewind back one byte in stream, since the above loop 
     * stops with a non-prefix byte. 
     */
    inp_back(u);

    /* speculatively determine the effective operand mode,
     * based on the prefixes and the current disassembly
     * mode. This may be inaccurate, but useful for mode
     * dependent decoding.
     */
    if ( u->dis_mode == 64 ) {
        u->opr_mode = REX_W( u->pfx_rex ) ? 64 : ( ( u->pfx_opr ) ? 16 : 32 ) ;
        u->adr_mode = ( u->pfx_adr ) ? 32 : 64;
    } else if ( u->dis_mode == 32 ) {
        u->opr_mode = ( u->pfx_opr ) ? 16 : 32;
        u->adr_mode = ( u->pfx_adr ) ? 16 : 32;
    } else if ( u->dis_mode == 16 ) {
        u->opr_mode = ( u->pfx_opr ) ? 32 : 16;
        u->adr_mode = ( u->pfx_adr ) ? 32 : 16;
    }

    return 0;
}


/* Searches the instruction tables for the right entry.
 */
static int search_itab( struct ud * u )
{
    struct ud_itab_entry * e = NULL;
    enum ud_itab_index table;
    uint8_t peek;
    uint8_t did_peek = 0;
    uint8_t curr; 
    uint8_t index;

    table = 0xdeadbeef;

    /* if in state of error, return */
    if ( u->error ) 
        return -1;

    /* get first byte of opcode. */
    inp_next(u); 
    if ( u->error ) 
        return -1;
    curr = inp_curr(u); 

    /* resolve xchg, nop, pause crazyness */
    if ( 0x90 == curr ) {
        if ( !( u->dis_mode == 64 && REX_B( u->pfx_rex ) ) ) {
            if ( u->pfx_rep ) {
                u->pfx_rep = 0;
                e = & ie_pause;
            } else {
                e = & ie_nop;
            }
            goto found_entry;
        }
    }

    /* get top-level table */
    if ( 0x0F == curr ) {
        table = ITAB__0F;
        curr  = inp_next(u);
        if ( u->error )
            return -1;

        /* 2byte opcodes can be modified by 0x66, F3, and F2 prefixes */
        if ( 0x66 == u->pfx_insn ) {
            if ( ud_itab_list[ ITAB__PFX_SSE66__0F ][ curr ].mnemonic != UD_Iinvalid ) {
                table = ITAB__PFX_SSE66__0F;
                u->pfx_opr = 0;
            }            
        } else if ( 0xF2 == u->pfx_insn ) {
            if ( ud_itab_list[ ITAB__PFX_SSEF2__0F ][ curr ].mnemonic != UD_Iinvalid ) {
                table = ITAB__PFX_SSEF2__0F; 
                u->pfx_repne = 0;
            }
        } else if ( 0xF3 == u->pfx_insn ) {
            if ( ud_itab_list[ ITAB__PFX_SSEF3__0F ][ curr ].mnemonic != UD_Iinvalid ) {
                table = ITAB__PFX_SSEF3__0F;
                u->pfx_repe = 0;
                u->pfx_rep  = 0;
            }
        }

        /* PEBIL */
        /* 3byte SSE opcodes */
        if ( 0x38 == curr ) {
            PEBIL_DEBUG("3byte opcode %hhx", curr);
            curr  = inp_next(u);
            PEBIL_DEBUG("\topcode %hhx", curr);
            if ( ud_itab_list[ ITAB__0F__OP___3BYTE_38__REG ][ curr ].mnemonic != UD_Iinvalid ) {
                table = ITAB__0F__OP___3BYTE_38__REG;
            }
            if ( 0x66 == u->pfx_insn ) {
                if ( ud_itab_list[ ITAB__PFX_SSE66__0F__OP___3BYTE_38__REG ][ curr ].mnemonic != UD_Iinvalid ) {
                    table = ITAB__PFX_SSE66__0F__OP___3BYTE_38__REG;
                }
            }
        } else if ( 0x3A == curr ) {
            PEBIL_DEBUG("3byte opcode %hhx", curr);
            curr  = inp_next(u);
            PEBIL_DEBUG("\topcode %hhx", curr);
            if ( ud_itab_list[ ITAB__0F__OP___3BYTE_3A__REG ][ curr ].mnemonic != UD_Iinvalid ) {
                table = ITAB__0F__OP___3BYTE_3A__REG;
            }
            if ( 0x66 == u->pfx_insn ) {
                if ( ud_itab_list[ ITAB__PFX_SSE66__0F__OP___3BYTE_3A__REG ][ curr ].mnemonic != UD_Iinvalid ) {
                    table = ITAB__PFX_SSE66__0F__OP___3BYTE_3A__REG;
                }
            }
        }
        /* end PEBIL */

    } 

    /* PEBIL */
    /* AVX opcodes -- sort of structured on the sse code just above this*/
    else if (P_AVX(u->pfx_insn)){
        PEBIL_DEBUG("found avx? curr %hhx, pfx_avx %hhx", curr, u->pfx_avx);
        if ( u->error )
            return -1;

        int tableid;
        
        if (u->pfx_insn == 0xC5){
            switch (u->pfx_avx){
                case 0x66:
                    tableid = ITAB__AVX__PFX_SSE66__0F;
                    break;
                case 0xF2:
                    tableid = ITAB__AVX__PFX_SSEF2__0F;
                    break;
                case 0xF3:
                    tableid = ITAB__AVX__PFX_SSEF3__0F;
                    break;
                default:
                    tableid = ITAB__AVX__0F;
                    break;
            }
        } else {
            PEBIL_DEBUG("VEX.MMMMM field is %hhx", VEX_M5(u->avx_vex[1]));
            switch(VEX_M5(u->avx_vex[1])){
                /* 0F */
                case 0x01:
                    switch (u->pfx_avx){
                        case 0x66:
                            tableid = ITAB__AVX__PFX_SSE66__0F;
                            break;
                        case 0xF2:
                            tableid = ITAB__AVX__PFX_SSEF2__0F;
                            break;
                        case 0xF3:
                            tableid = ITAB__AVX__PFX_SSEF3__0F;
                            break;
                        default:
                            tableid = ITAB__AVX__0F;
                            break;
                    }
                    break;
                /* 0F 38 */
                case 0x02:
                    switch (u->pfx_avx){
                        case 0x66:
                            tableid = ITAB__AVX__PFX_SSE66__0F__OP_0F__3BYTE_38__REG;
                            break;
                        case 0xF2:
                            assert(0);//tableid = ITAB__AVX_C4__PFX_SSEF2__0F__OP_0F__3BYTE_38__REG;
                            break;
                        case 0xF3:
                            assert(0);//tableid = ITAB__AVX_C4__PFX_SSEF3__0F__OP_0F__3BYTE_38__REG;
                            break;
                        default:
                            assert(0);//tableid = ITAB__AVX_C4__0F__OP_0F__3BYTE_38__REG;
                            break;
                    }
                    break;
                /* 0F 3A */
                case 0x03:
                    switch (u->pfx_avx){
                        case 0x66:
                            tableid = ITAB__AVX__PFX_SSE66__0F__OP_0F__3BYTE_3A__REG;
                            break;
                        case 0xF2:
                            assert(0);//tableid = ITAB__AVX_C4__PFX_SSEF2__0F__OP_0F__3BYTE_3A__REG;
                            break;
                        case 0xF3:
                            assert(0);//tableid = ITAB__AVX_C4__PFX_SSEF3__0F__OP_0F__3BYTE_3A__REG;
                            break;
                        default:
                            assert(0);//tableid = ITAB__AVX_C4__0F__OP_0F__3BYTE_3A__REG;
                            break;
                    }
                    break;
                /* all other values are undefined */
                default:
                    fprintf(stdout, "PEBIL_DEBUG: %#hhx\n", VEX_M5(u->avx_vex[1]));
                    PEBIL_DEBUG("invalid VEX.MMMMM field found: %#hhx", VEX_M5(u->avx_vex[1]));
                    u->error = 1;
                    break;
            }
        }

        if(u->error)
            return -1;

        PEBIL_DEBUG("itab %d %hhx", tableid, curr);
        if ( ud_itab_list[ tableid ][ curr ].mnemonic != UD_Iinvalid ) {
            PEBIL_DEBUG("avx mnemonic found %s", ud_mnemonics_str[ud_itab_list[ tableid ][ curr ].mnemonic]);
            table = tableid;
            u->pfx_opr = 0;
        } else {
            fprintf(stderr, "PEBIL_DEBUG: found UD_Iinvalid: %d, %d\n", tableid, curr);
            gen_hex(u);
            fprintf(stderr, "PEBIL_DEBUG: hex: %hhx %hhx %hhx %hhx ...\n", u->insn_bytes[0], u->insn_bytes[1], u->insn_bytes[2], u->insn_bytes[4]);
            u->error = 1;
            return -1;
        }

        /* check and emit error if vexl constraint is violated */
        if (P_VEXLZ(ud_itab_list[ tableid ][ curr ].prefix) && VEX_L(u->avx_vex[0])){
            PEBIL_DEBUG("VEX.L must be zero");
            u->error = 1;
        } 
    }
    /* end PEBIL */

    /* pick an instruction from the 1byte table */
    else {
        table = ITAB__1BYTE; 
    }

    index = curr;

search:

    e = & ud_itab_list[ table ][ index ];

    /* if mnemonic constant is a standard instruction constant
     * our search is over.
     */
    
    if ( e->mnemonic < UD_Id3vil ) {
        if ( e->mnemonic == UD_Iinvalid ) {
            if ( did_peek ) {
                inp_next( u ); if ( u->error ) return -1;
            }
            goto found_entry;
        }
        goto found_entry;
    }

    table = e->prefix;

    switch ( e->mnemonic )
    {
    case UD_Igrp_reg:
        peek     = inp_peek( u );
        did_peek = 1;
        index    = MODRM_REG( peek );
        break;

    case UD_Igrp_mod:
        peek     = inp_peek( u );
        did_peek = 1;
        index    = MODRM_MOD( peek );
        if ( index == 3 )
           index = ITAB__MOD_INDX__11;
        else 
           index = ITAB__MOD_INDX__NOT_11; 
        break;

    case UD_Igrp_rm:
        curr     = inp_next( u );
        did_peek = 0;
        if ( u->error )
            return -1;
        index    = MODRM_RM( curr );
        break;

    case UD_Igrp_x87:
        curr     = inp_next( u );
        did_peek = 0;
        if ( u->error )
            return -1;
        index    = curr - 0xC0;
        break;

    case UD_Igrp_osize:
        if ( u->opr_mode == 64 ) 
            index = ITAB__MODE_INDX__64;
        else if ( u->opr_mode == 32 ) 
            index = ITAB__MODE_INDX__32;
        else
            index = ITAB__MODE_INDX__16;
        break;
 
    case UD_Igrp_asize:
        if ( u->adr_mode == 64 ) 
            index = ITAB__MODE_INDX__64;
        else if ( u->adr_mode == 32 ) 
            index = ITAB__MODE_INDX__32;
        else
            index = ITAB__MODE_INDX__16;
        break;               

    case UD_Igrp_mode:
        if ( u->dis_mode == 64 ) 
            index = ITAB__MODE_INDX__64;
        else if ( u->dis_mode == 32 ) 
            index = ITAB__MODE_INDX__32;
        else
            index = ITAB__MODE_INDX__16;
        break;

    case UD_Igrp_vendor:
        if ( u->vendor == UD_VENDOR_INTEL ) 
            index = ITAB__VENDOR_INDX__INTEL; 
        else if ( u->vendor == UD_VENDOR_AMD )
            index = ITAB__VENDOR_INDX__AMD;
        else
            assert( !"unrecognized vendor id" );
        break;

    case UD_Id3vil:
        assert( !"invalid instruction mnemonic constant Id3vil" );
        break;

    default:
        assert( !"invalid instruction mnemonic constant" );
        break;
    }

    goto search;

found_entry:

    u->itab_entry = e;
    u->mnemonic = u->itab_entry->mnemonic;

    return 0;
}


static unsigned int resolve_operand_size( const struct ud * u, unsigned int s )
{
    switch ( s ) 
    {
    case SZ_NA:
        if(u->mnemonic == UD_Ilea){ // instructions that use O_M and get operand from adr_mode -- move this elsewhere when instructions are complete
            return u->adr_mode; // FIXME
        }

        if(u->mnemonic == UD_Imov) {
            return u->opr_mode;
        }

        if(u->mnemonic != UD_Ifnop &&
           u->mnemonic != UD_Inop) {
            //fprintf(stderr, "Unknown operand size of instruction %s\n", ud_lookup_mnemonic(u->mnemonic));
        }
        return s;
    case SZ_V:
        return ( u->opr_mode );
    case SZ_Z:  
        return ( u->opr_mode == 16 ) ? 16 : 32;
    case SZ_P:  
        return ( u->opr_mode == 16 ) ? SZ_WP : SZ_DP;
    case SZ_MDQ:
        return ( u->opr_mode == 16 ) ? 32 : u->opr_mode;
    case SZ_RDQ:
        return ( u->dis_mode == 64 ) ? 64 : 32;
    case SZ_X:
        return VEX_L(u->avx_vex[0]) && (!P_VEXLIG(u->itab_entry->prefix)) ? SZ_Y : SZ_X;
    default:
        return s;
    }
}


static int resolve_mnemonic( struct ud* u )
{
  /* far/near flags */
  u->br_far = 0;
  u->br_near = 0;
  /* readjust operand sizes for call/jmp instrcutions */
  if ( u->mnemonic == UD_Icall || u->mnemonic == UD_Ijmp ) {
    /* WP: 16bit pointer */
    if ( u->operand[ 0 ].size == SZ_WP ) {
        u->operand[ 0 ].size = 16;
        u->br_far = 1;
        u->br_near= 0;
    /* DP: 32bit pointer */
    } else if ( u->operand[ 0 ].size == SZ_DP ) {
        u->operand[ 0 ].size = 32;
        u->br_far = 1;
        u->br_near= 0;
    } else {
        u->br_far = 0;
        u->br_near= 1;
    }
  /* resolve 3dnow weirdness. */
  } else if ( u->mnemonic == UD_I3dnow ) {
    u->mnemonic = ud_itab_list[ ITAB__3DNOW ][ inp_curr( u )  ].mnemonic;
  }
  /* SWAPGS is only valid in 64bits mode */
  if ( u->mnemonic == UD_Iswapgs && u->dis_mode != 64 ) {
    u->error = 1;
    return -1;
  }

  return 0;
}


/* -----------------------------------------------------------------------------
 * decode_a()- Decodes operands of the type seg:offset
 * -----------------------------------------------------------------------------
 */
static void 
decode_a(struct ud* u, struct ud_operand *op)
{
  op->position = ud_insn_len(u); /* PEBIL */
  if (u->opr_mode == 16) {  
    /* seg16:off16 */
    op->type = UD_OP_PTR;
    op->size = 32;
    op->lval.ptr.off = inp_uint16(u);
    op->lval.ptr.seg = inp_uint16(u);
  } else {
    /* seg16:off32 */
    op->type = UD_OP_PTR;
    op->size = 48;
    op->lval.ptr.off = inp_uint32(u);
    op->lval.ptr.seg = inp_uint16(u);
  }
}

/* -----------------------------------------------------------------------------
 * decode_gpr() - Returns decoded General Purpose Register 
 * -----------------------------------------------------------------------------
 */
static enum ud_type 
decode_gpr(register struct ud* u, unsigned int s, unsigned char rm)
{
  s = resolve_operand_size(u, s);
        
  switch (s) {
    case 64:
        return UD_R_RAX + rm;
    case SZ_DP:
    case 32:
        return UD_R_EAX + rm;
    case SZ_WP:
    case 16:
        return UD_R_AX  + rm;
    case  8:
        if (u->dis_mode == 64 && u->pfx_rex) {
            if (rm >= 4)
                return UD_R_SPL + (rm-4);
            return UD_R_AL + rm;
        } else return UD_R_AL + rm;
    default:
        return 0;
  }
}

/* -----------------------------------------------------------------------------
 * resolve_gpr64() - 64bit General Purpose Register-Selection. 
 * -----------------------------------------------------------------------------
 */
static enum ud_type 
resolve_gpr64(struct ud* u, enum ud_operand_code gpr_op)
{
  if (gpr_op >= OP_rAXr8 && gpr_op <= OP_rDIr15)
    gpr_op = (gpr_op - OP_rAXr8) | (REX_B(u->pfx_rex) << 3);          
  else  gpr_op = (gpr_op - OP_rAX);

  if (u->opr_mode == 16)
    return gpr_op + UD_R_AX;
  if (u->dis_mode == 32 || 
    (u->opr_mode == 32 && ! (REX_W(u->pfx_rex) || u->default64))) {
    return gpr_op + UD_R_EAX;
  }

  return gpr_op + UD_R_RAX;
}

/* -----------------------------------------------------------------------------
 * resolve_gpr32 () - 32bit General Purpose Register-Selection. 
 * -----------------------------------------------------------------------------
 */
static enum ud_type 
resolve_gpr32(struct ud* u, enum ud_operand_code gpr_op)
{
  gpr_op = gpr_op - OP_eAX;

  if (u->opr_mode == 16) 
    return gpr_op + UD_R_AX;

  return gpr_op +  UD_R_EAX;
}

/* -----------------------------------------------------------------------------
 * resolve_reg() - Resolves the register type 
 * -----------------------------------------------------------------------------
 */
static enum ud_type 
resolve_reg(struct ud* u, unsigned int type, unsigned char i)
{
  switch (type) {
    case T_MMX :    return UD_R_MM0  + (i & 7);
    case T_XMM :    return UD_R_XMM0 + i;
    case T_YMM :    return UD_R_YMM0 + i;
    case T_CRG :    return UD_R_CR0  + i;
    case T_DBG :    return UD_R_DR0  + i;
    case T_SEG :    return UD_R_ES   + (i & 7);
    case T_NONE:
    default:    return UD_NONE;
  }
}


static void decode_vex_vvvv(struct ud* u,
        struct ud_operand* op,
        unsigned int size)
{
    uint8_t vex = u->avx_vex[0];
    unsigned char rtype = VEX_L(vex) ? T_YMM : T_XMM;
    enum ud_type reg = resolve_reg(u, rtype, VEX_VVVV(vex));

    op->type = UD_OP_REG;
    op->base = reg;
    op->size = rtype == T_YMM ? 256 : 128;
    op->position = u->pfx_insn == 0xC4 ? 2 : 1;
}


/* -----------------------------------------------------------------------------
 * clear_operand() - clear operand pointer 
 * -----------------------------------------------------------------------------
 */
static int clear_operand(register struct ud_operand* op){
    memset( op, 0, sizeof( struct ud_operand ) );
    return 0;
}

/* -----------------------------------------------------------------------------
 * decode_imm() - Decodes Immediate values.
 * -----------------------------------------------------------------------------
 */
static void 
decode_imm(struct ud* u, unsigned int s, struct ud_operand *op)
{
  op->position = ud_insn_len(u); /* PEBIL */

  op->size = resolve_operand_size(u, s);
  op->type = UD_OP_IMM;

  switch (op->size) {
    case  8: op->lval.sbyte = inp_uint8(u);   break;
    case 16: op->lval.uword = inp_uint16(u);  break;
    case 32: op->lval.udword = inp_uint32(u); break;
    case 64: op->lval.uqword = inp_uint64(u); break;
    default:
        fprintf(stderr, "Could not determine size for immediate operand for %s with size %d\n", ud_lookup_mnemonic(u->mnemonic), s);
        assert(0);
        return;
  }
}

/* -----------------------------------------------------------------------------
 * decode_modrm_rm() - Decodes ModRM.r/m
 * -----------------------------------------------------------------------------
 */
static void 
decode_modrm_rm(struct ud* u,
        struct modrm* modrm,
        struct ud_operand *op,
        unsigned int size, 
        unsigned char type)
{
  unsigned char modrm_byte = get_modrm(u, modrm);
  op->position = modrm->position;

  unsigned char mod, rm;


  /* get mod, r/m and reg fields */
  mod = MODRM_MOD(modrm_byte);
  rm  = (REX_B(u->pfx_rex) << 3) | MODRM_RM(modrm_byte);

  op->size = resolve_operand_size(u, size);

  /* if mod is 11b, then the UD_R_m specifies a gpr/mmx/sse/control/debug */
  if (mod == 3) {
    op->type = UD_OP_REG;
    if (type ==  T_GPR)
        op->base = decode_gpr(u, op->size, rm);
    else    op->base = resolve_reg(u, type, (REX_B(u->pfx_rex) << 3) | (rm&7));
  } 
  /* else its memory addressing */  
  else {
    op->type = UD_OP_MEM;

    /* 64bit addressing */
    if (u->adr_mode == 64) {

        op->base = UD_R_RAX + rm;

        /* get offset type */
        if (mod == 1)
            op->offset = 8;
        else if (mod == 2)
            op->offset = 32;
        else if (mod == 0 && (rm & 7) == 5) {           
            op->base = UD_R_RIP;
            op->offset = 32;
        } else  op->offset = 0;

        /* Scale-Index-Base (SIB) */
        if ((rm & 7) == 4) {
            inp_next(u);
            
            op->scale = (1 << SIB_S(inp_curr(u))) & ~1;
            op->index = UD_R_RAX + (SIB_I(inp_curr(u)) | (REX_X(u->pfx_rex) << 3));
            op->base  = UD_R_RAX + (SIB_B(inp_curr(u)) | (REX_B(u->pfx_rex) << 3));

            /* special conditions for base reference */
            if (op->index == UD_R_RSP) {
                op->index = UD_NONE;
                op->scale = UD_NONE;
            }

            if (op->base == UD_R_RBP || op->base == UD_R_R13) {
                if (mod == 0) 
                    op->base = UD_NONE;
                if (mod == 1)
                    op->offset = 8;
                else op->offset = 32;
            }
        }
    } 

    /* 32-Bit addressing mode */
    else if (u->adr_mode == 32) {

        /* get base */
        op->base = UD_R_EAX + rm;

        /* get offset type */
        if (mod == 1)
            op->offset = 8;
        else if (mod == 2)
            op->offset = 32;
        else if (mod == 0 && rm == 5) {
            op->base = UD_NONE;
            op->offset = 32;
        } else  op->offset = 0;

        /* Scale-Index-Base (SIB) */
        if ((rm & 7) == 4) {
            inp_next(u);

            op->scale = (1 << SIB_S(inp_curr(u))) & ~1;
            op->index = UD_R_EAX + (SIB_I(inp_curr(u)) | (REX_X(u->pfx_rex) << 3));
            op->base  = UD_R_EAX + (SIB_B(inp_curr(u)) | (REX_B(u->pfx_rex) << 3));

            if (op->index == UD_R_ESP) {
                op->index = UD_NONE;
                op->scale = UD_NONE;
            }

            /* special condition for base reference */
            if (op->base == UD_R_EBP) {
                if (mod == 0)
                    op->base = UD_NONE;
                if (mod == 1)
                    op->offset = 8;
                else op->offset = 32;
            }
        }
    } 

    /* 16bit addressing mode */
    else  {
        switch (rm) {
            case 0: op->base = UD_R_BX; op->index = UD_R_SI; break;
            case 1: op->base = UD_R_BX; op->index = UD_R_DI; break;
            case 2: op->base = UD_R_BP; op->index = UD_R_SI; break;
            case 3: op->base = UD_R_BP; op->index = UD_R_DI; break;
            case 4: op->base = UD_R_SI; break;
            case 5: op->base = UD_R_DI; break;
            case 6: op->base = UD_R_BP; break;
            case 7: op->base = UD_R_BX; break;
        }

        if (mod == 0 && rm == 6) {
            op->offset= 16;
            op->base = UD_NONE;
        }
        else if (mod == 1)
            op->offset = 8;
        else if (mod == 2) 
            op->offset = 16;
    }
  }  

  /* extract offset, if any */
  switch(op->offset) {
    case 8 : op->lval.ubyte  = inp_uint8(u);  break;
    case 16: op->lval.uword  = inp_uint16(u);  break;
    case 32: op->lval.udword = inp_uint32(u); break;
    case 64: op->lval.uqword = inp_uint64(u); break;
    default: break;
  }


}

/* -----------------------------------------------------------------------------
 * decode_modrm_reg() - Decodes ModRM.reg as register
 * -----------------------------------------------------------------------------
 */
static void 
decode_modrm_reg(struct ud* u,
         struct modrm* modrm,
         struct ud_operand *op,
         unsigned char reg_size,
         unsigned char reg_type)
{
  unsigned char modrm_byte = get_modrm(u, modrm);
  op->position = modrm->position;

  unsigned char reg;

  reg = (REX_R(u->pfx_rex) << 3) | MODRM_REG(modrm_byte);

  op->type = UD_OP_REG;
  op->size = resolve_operand_size(u, reg_size);

  if (reg_type == T_GPR) 
      op->base = decode_gpr(u, op->size, reg);
  else op->base = resolve_reg(u, reg_type, reg);
}


/* -----------------------------------------------------------------------------
 * decode_o() - Decodes offset
 * -----------------------------------------------------------------------------
 */
static void 
decode_o(struct ud* u, unsigned int s, struct ud_operand *op)
{
  op->position = ud_insn_len(u); /* PEBIL */

  switch (u->adr_mode) {
    case 64:
        op->offset = 64; 
        op->lval.uqword = inp_uint64(u); 
        break;
    case 32:
        op->offset = 32; 
        op->lval.udword = inp_uint32(u); 
        break;
    case 16:
        op->offset = 16; 
        op->lval.uword  = inp_uint16(u); 
        break;
    default:
        return;
  }
  op->type = UD_OP_MEM;
  op->size = resolve_operand_size(u, s);
}


static int disasm_operand(register struct ud* u,
        struct modrm* modrm,
        struct ud_operand* operand,
        enum ud_operand_code type,
        unsigned int size)
{
  switch(type) {
    case OP_A:
      decode_a(u, operand);
      break;

    case OP_M:
      if(MODRM_MOD(get_modrm(u, modrm))==3) u->error=1;
      // fallthrough
    case OP_E:
      decode_modrm_rm(u, modrm, operand, size, T_GPR);
      break;

    case OP_G:
      decode_modrm_reg(u, modrm, operand, size, T_GPR); 
      break;

    case OP_I:
      decode_imm(u, size, operand);
      break;

    case OP_AL: case OP_CL: case OP_DL: case OP_BL:
    case OP_AH: case OP_CH: case OP_DH: case OP_BH:
      operand->type = UD_OP_REG;
      operand->base = UD_R_AL + (type - OP_AL);
      operand->size = 8;
      break;

    case OP_ALr8b:  case OP_CLr9b:  case OP_DLr10b:  case OP_BLr11b:
    case OP_AHr12b: case OP_CHr13b: case OP_DHr14b:  case OP_BHr15b:
    {
      ud_type_t gpr = (type - OP_ALr8b) + UD_R_AL +
                      (REX_B(u->pfx_rex) << 3);
      if(UD_R_AH <= gpr && u->pfx_rex)
          gpr = gpr + 4;
      operand->type = UD_OP_REG;
      operand->base = gpr;
      break;
    }

    case OP_AX:
      operand->type = UD_OP_REG;
      operand->base = UD_R_AX;
      operand->size = 16;
      break;

    //case OP_CX:
    case OP_DX:
      operand->type = UD_OP_REG;
      operand->base = UD_R_DX;
      operand->size = 16;
      break;

    //case OP_BX:
    //case OP_SI:     case OP_DI:     case OP_SP:      case OP_BP:

    case OP_rAX:    case OP_rCX:    case OP_rDX:     case OP_rBX:
    case OP_rSP:    case OP_rBP:    case OP_rSI:     case OP_rDI:
    case OP_rAXr8:  case OP_rCXr9:  case OP_rDXr10:  case OP_rBXr11:  
    case OP_rSPr12: case OP_rBPr13: case OP_rSIr14:  case OP_rDIr15:
      operand->type = UD_OP_REG;
      operand->base = resolve_gpr64(u, type);
      operand->size = size;
      break;

    case OP_eAX:    case OP_eCX:    case OP_eDX:     case OP_eBX:
    case OP_eSP:    case OP_eBP:    case OP_eSI:     case OP_eDI:
      operand->type = UD_OP_REG;
      operand->base = resolve_gpr32(u, type);
      operand->size = size;
      break;

    case OP_ES:     case OP_CS:     case OP_SS:      case OP_DS:  
    case OP_FS:     case OP_GS:
      operand->type = UD_OP_REG;
      operand->base = type - OP_ES + UD_R_ES;
      operand->size = 16;
      break;

    case OP_ST0:    case OP_ST1:    case OP_ST2:     case OP_ST3:
    case OP_ST4:    case OP_ST5:    case OP_ST6:     case OP_ST7:
      operand->type = UD_OP_REG;
      operand->base = type-OP_ST0 + UD_R_ST0;
      operand->size = 0;
      break;

    case OP_J:
      decode_imm(u, size, operand);
      operand->type = UD_OP_JIMM;
      break;

    case OP_S:
      decode_modrm_reg(u, modrm, operand, size, T_SEG);
      break;

    case OP_O:
      decode_o(u, size, operand);
      break;

    case OP_I1:
      operand->type = UD_OP_CONST;
      operand->lval.udword = 1;
      break;

    case OP_I3: 
      operand->type = UD_OP_CONST;
      operand->lval.sbyte = 3;
      break;

    case OP_V:
      decode_modrm_reg(u, modrm, operand, size, T_XMM);
      break;

    case OP_W:
      decode_modrm_rm(u, modrm, operand, size, T_XMM);
      break;

    case OP_Q:
      decode_modrm_rm(u, modrm, operand, size, T_MMX);
      break;

    case OP_P:
      decode_modrm_reg(u, modrm, operand, size, T_MMX);
      break;

    case OP_R:
      decode_modrm_rm(u, modrm, operand, size, T_GPR);
      break;

    case OP_C:
      decode_modrm_reg(u, modrm, operand, size, T_CRG);
      break;

    case OP_D:
      decode_modrm_reg(u, modrm, operand, size, T_DBG);
      break;

    case OP_VR:
      if(MODRM_MOD(get_modrm(u, modrm)) != 3) u->error = 1;
      decode_modrm_rm(u, modrm, operand, size, T_XMM);
      break;

    case OP_PR:
      if(MODRM_MOD(get_modrm(u, modrm)) != 3) u->error = 1;
      decode_modrm_rm(u, modrm, operand, size, T_MMX);
      break;

    case OP_X:
      decode_vex_vvvv(u, operand, size);
      break;

    default:
      assert(0);

  }
  return 0;
}

static int disasm_operands(register struct ud* u)
{
  struct modrm modrm;
  clear_modrm(&modrm);

  u->operand[0].type = UD_NONE;
  u->operand[1].type = UD_NONE;
  u->operand[2].type = UD_NONE;
  u->operand[3].type = UD_NONE;

  int retval = 0;

  if (u->itab_entry->operand1.type == UD_NONE) return retval;
  retval |= disasm_operand(u, &modrm, &u->operand[0], u->itab_entry->operand1.type, u->itab_entry->operand1.size);

  if( u->itab_entry->operand2.type == UD_NONE) return retval;
  retval |= disasm_operand(u, &modrm, &u->operand[1], u->itab_entry->operand2.type, u->itab_entry->operand2.size);

  if( u->itab_entry->operand3.type == UD_NONE) return retval;
  retval |= disasm_operand(u, &modrm, &u->operand[2], u->itab_entry->operand3.type, u->itab_entry->operand3.size);

  if( u->itab_entry->operand4.type == UD_NONE) return retval;;
  retval |= disasm_operand(u, &modrm, &u->operand[3], u->itab_entry->operand4.type, u->itab_entry->operand4.size);

  return retval;
}


/* -----------------------------------------------------------------------------
 * clear_insn() - clear instruction pointer 
 * -----------------------------------------------------------------------------
 */
static int clear_insn(register struct ud* u)
{
  u->error     = 0;
  u->pfx_seg   = 0;
  u->pfx_opr   = 0;
  u->pfx_adr   = 0;
  u->pfx_lock  = 0;
  u->pfx_repne = 0;
  u->pfx_rep   = 0;
  u->pfx_repe  = 0;
  u->pfx_seg   = 0;
  u->pfx_rex   = 0;
  u->pfx_insn  = 0;
  u->pfx_avx   = 0;
  u->avx_vex[0] = 0;
  u->avx_vex[1] = 0;
  u->mnemonic  = UD_Inone;
  u->itab_entry = NULL;

  clear_operand(&u->operand[ 0 ]);
  clear_operand(&u->operand[ 1 ]);
  clear_operand(&u->operand[ 2 ]);
  clear_operand(&u->operand[ 3 ]);

  return 0;
}

static int do_mode( struct ud* u )
{
  /* if in error state, bail out */
  if ( u->error ) return -1; 

  /* propagate perfix effects */
  if ( u->dis_mode == 64 ) {  /* set 64bit-mode flags */

    /* Check validity of  instruction m64 */
    if ( P_INV64( u->itab_entry->prefix ) ) {
        u->error = 1;
        return -1;
    }

    /* effective rex prefix is the  effective mask for the 
     * instruction hard-coded in the opcode map.
     */
    u->pfx_rex = ( u->pfx_rex & 0x40 ) | 
                 ( u->pfx_rex & REX_PFX_MASK( u->itab_entry->prefix ) ); 

    /* whether this instruction has a default operand size of 
     * 64bit, also hardcoded into the opcode map.
     */
    u->default64 = P_DEF64( u->itab_entry->prefix ); 
    /* calculate effective operand size */
    if ( REX_W( u->pfx_rex ) ) {
        u->opr_mode = 64;
    } else if ( u->pfx_opr ) {
        u->opr_mode = 16;
    } else {
        /* unless the default opr size of instruction is 64,
         * the effective operand size in the absence of rex.w
         * prefix is 32.
         */
        u->opr_mode = ( u->default64 ) ? 64 : 32;
    }

    /* calculate effective address size */
    u->adr_mode = (u->pfx_adr) ? 32 : 64;
  } else if ( u->dis_mode == 32 ) { /* set 32bit-mode flags */
    u->opr_mode = ( u->pfx_opr ) ? 16 : 32;
    u->adr_mode = ( u->pfx_adr ) ? 16 : 32;
  } else if ( u->dis_mode == 16 ) { /* set 16bit-mode flags */
    u->opr_mode = ( u->pfx_opr ) ? 32 : 16;
    u->adr_mode = ( u->pfx_adr ) ? 32 : 16;
  }

  /* These flags determine which operand to apply the operand size
   * cast to.
   */
  u->c1 = ( P_C1( u->itab_entry->prefix ) ) ? 1 : 0;
  u->c2 = ( P_C2( u->itab_entry->prefix ) ) ? 1 : 0;
  u->c3 = ( P_C3( u->itab_entry->prefix ) ) ? 1 : 0;
  u->c4 = ( P_C4( u->itab_entry->prefix ) ) ? 1 : 0;

  /* set flags for implicit addressing */
  u->implicit_addr = P_IMPADDR( u->itab_entry->prefix );

  return 0;
}

static int gen_hex( struct ud *u )
{
    unsigned int i;
    unsigned char *src_ptr = inp_sess( u );
    char* src_hex;
    
    /* bail out if in error stat. */
    if ( u->error ) return -1; 

    /* output buffer pointer */
    src_hex = ( char* ) u->insn_hexcode;

    /* for each byte used to decode instruction */
    for ( i = 0; i < u->inp_ctr; ++i, ++src_ptr) {
        // PEBIL doesn't use this field and this is an expensive op, so skip it
        sprintf( src_hex, "%02x", *src_ptr & 0xFF );
        src_hex += 2;
        u->insn_bytes[i] = (*src_ptr & 0xFF);
    }
    return 0;
}

static int resolve_implied_usedefs( struct ud *u )
{
    u->flags_use = u->itab_entry->flags_use;
    u->flags_def = u->itab_entry->flags_def;
    if (u->flags_def != 0 || u->flags_use != 0){
        PEBIL_DEBUG("flags used: %#x, def: %#x", u->flags_use, u->flags_def);
    }

    u->impreg_use = u->itab_entry->impreg_use;
    u->impreg_def = u->itab_entry->impreg_def;
    if (u->impreg_def != 0 || u->impreg_use != 0){
        PEBIL_DEBUG("implied regs used: %#llx, def: %#llx", u->impreg_use, u->impreg_def);
    }

    /* set use/def of cx for rep prefixes here */
    if (u->pfx_rep || u->pfx_repe || u->pfx_repne){
        u->impreg_use |= R_CX;
        u->impreg_def |= R_CX;
    }

    return 0;
}

/* =============================================================================
 * ud_decode() - Instruction decoder. Returns the number of bytes decoded.
 * =============================================================================
 */
unsigned int ud_decode( struct ud* u )
{
  inp_start(u);

  if ( clear_insn( u ) ) {
    ; /* error */
  } else if ( get_prefixes( u ) != 0 ) {
    ; /* error */
  } else if ( search_itab( u ) != 0 ) {
    ; /* error */
  } else if ( do_mode( u ) != 0 ) {
    ; /* error */
  } else if ( disasm_operands( u ) != 0 ) {
    ; /* error */
  } else if ( resolve_mnemonic( u ) != 0 ) {
    ; /* error */
  } else if ( resolve_implied_usedefs( u ) != 0 ) {
    ; /* error */
  }

  /* Handle decode error. */
  if ( u->error ) {
    /* clear out the decode data. */
    clear_insn( u );
    /* mark the sequence of bytes as invalid. */
    u->itab_entry = & ie_invalid;
    u->mnemonic = u->itab_entry->mnemonic;
  } 

  u->insn_offset = u->pc; /* set offset of instruction */
  u->insn_fill = 0;   /* set translation buffer index to 0 */
  u->pc += u->inp_ctr;    /* move program counter by bytes decoded */

  gen_hex( u );

  /* return number of bytes disassembled. */
  return u->inp_ctr;
}

/* vim:cindent
 * vim:ts=4
 * vim:sw=4
 * vim:expandtab
 */
