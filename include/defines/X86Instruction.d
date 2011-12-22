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

/*
##################################################################
udis86 ud_operand
  Type	                Name
  enum ud_type          type;
  uint8_t               size;
  union {
        int8_t          sbyte;
        uint8_t         ubyte;
        int16_t         sword;
        uint16_t        uword;
        int32_t         sdword;
        uint32_t        udword;
        int64_t         sqword;
        uint64_t        uqword;

        struct {
                uint16_t seg;
                uint32_t off;
        } ptr;
  } lval;
  enum ud_type          base;
  enum ud_type          index;
  uint8_t               offset;
  uint8_t               scale;

##################################################################
*/

#define OPERAND_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(ud_type,type); \
    GET_FIELD_CLASS(uint8_t,size); \
    GET_FIELD_CLASS(uint8_t,position); \
    GET_FIELD_CLASS_A(int8_t,sbyte,lval); \
    GET_FIELD_CLASS_A(uint8_t,ubyte,lval); \
    GET_FIELD_CLASS_A(int16_t,sword,lval); \
    GET_FIELD_CLASS_A(uint16_t,uword,lval); \
    GET_FIELD_CLASS_A(int32_t,sdword,lval); \
    GET_FIELD_CLASS_A(uint32_t,udword,lval); \
    GET_FIELD_CLASS_A(int64_t,sqword,lval); \
    GET_FIELD_CLASS_A(uint64_t,uqword,lval); \
    GET_FIELD_CLASS(ud_type,base); \
    GET_FIELD_CLASS(ud_type,index); \
    GET_FIELD_CLASS(uint8_t,offset); \
    GET_FIELD_CLASS(uint8_t,scale);


/* we don't give all fields macro access
##################################################################
udis86 ud_compact
  int                   (*inp_hook) (struct ud*);
  uint8_t               inp_curr;
  uint8_t               inp_fill;
  FILE*                 inp_file;
  uint8_t               inp_ctr;
  uint8_t*              inp_buff;
  uint8_t*              inp_buff_end;
  uint8_t               inp_end;
  void                  (*translator)(struct ud*);
  uint64_t              insn_offset;
  char                  insn_hexcode[32];
  char                  insn_buffer[64];
  unsigned int          insn_fill;
  uint8_t               dis_mode;
  uint64_t              pc;
  uint8_t               vendor;
  struct map_entry*     mapen;
  enum ud_mnemonic_code mnemonic;
  struct ud_operand     operand[4];
  uint8_t               error;
  uint8_t               pfx_rex;
  uint8_t               pfx_seg;
  uint8_t               pfx_opr;
  uint8_t               pfx_adr;
  uint8_t               pfx_lock;
  uint8_t               pfx_rep;
  uint8_t               pfx_repe;
  uint8_t               pfx_repne;
  uint8_t               pfx_insn;
  uint8_t               pfx_avx;
  uint8_t               avx_vex[2];
  uint8_t               default64;
  uint8_t               opr_mode;
  uint8_t               adr_mode;
  uint8_t               br_far;
  uint8_t               br_near;
  uint8_t               implicit_addr;
  uint8_t               c1;
  uint8_t               c2;
  uint8_t               c3;
  uint8_t               inp_cache[256];
  uint8_t               inp_sess[64];
  uint32_t              flags_use;
  uint32_t              flags_def;
  struct ud_itab_entry * itab_entry;
##################################################################
*/

#define INSTRUCTION_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(uint64_t,insn_offset); \
    GET_FIELD_CLASS(char*,insn_hexcode); \
    GET_FIELD_CLASS(char*,insn_buffer); \
    GET_FIELD_CLASS(enum ud_mnemonic_code,mnemonic); \
    GET_FIELD_CLASS(struct ud_operand*,operand); \
    GET_FIELD_CLASS(uint8_t,pfx_seg); \
    GET_FIELD_CLASS(uint8_t,pfx_rep); \
    GET_FIELD_CLASS(uint8_t,adr_mode); \
    GET_FIELD_CLASS(uint32_t,flags_use); \
    GET_FIELD_CLASS(uint32_t,flags_def); \
        \
    SET_FIELD_CLASS(uint64_t,insn_offset); 

