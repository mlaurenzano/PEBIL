/* Print i386 instructions for GDB, the GNU debugger.
   Copyright 1988, 1989, 1991, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2001
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
 * 80386 instruction printer by Pace Willisson (pace@prep.ai.mit.edu)
 * July 1988
 *  modified by John Hassey (hassey@dg-rtp.dg.com)
 *  x86-64 support added by Jan Hubicka (jh@suse.cz)
 */

/*
 * The main tables describing the instructions is essentially a copy
 * of the "Opcode Map" chapter (Appendix A) of the Intel 80386
 * Programmers Manual.  Usually, there is a capital letter, followed
 * by a small letter.  The capital letter tell the addressing mode,
 * and the small letter tells about the operand size.  Refer to
 * the Intel manual for details.
 */

#include <Disassemble.h>

int x86inst_intern_read_mem_func(uint64_t memaddr, uint8_t *myaddr, uint32_t length, struct disassemble_info *info){
    // check if memory locations are valid
    uint8_t* dest_addr = myaddr;
    uint8_t* source_addr = info->buffer + (memaddr - info->buffer_vma);

    // check if any bytes in source are part of destination
    if (dest_addr < source_addr && dest_addr + length >= source_addr){
        return 0;
    // check if any bytes in destination are part of source
    } else if (source_addr < dest_addr && source_addr + length >= dest_addr){
        return 0;
    } else {
        //fprintf(stdout, "MEMREAD: %x(%lx) -> %x(%lx), %d\n", (source_addr), *(source_addr), (dest_addr), *(dest_addr), length);
        memcpy(myaddr, source_addr, length);
        return length;
    }
}

fprintf_ftype disasm_fprintf_func = (fprintf_ftype)(fprintf);

void x86inst_set_disassemble_info(struct disassemble_info* dis_info){
    (*dis_info).mach = mach_i386_i386;
    (*dis_info).private_data = NULL;
    (*dis_info).buffer = NULL;
    (*dis_info).buffer_vma = 0;
}

void generic_print_address (uint64_t addr, struct disassemble_info* info)
{
    char buf[30];
    sprintf_vma (buf, addr);
    (*disasm_fprintf_func)(stdout, "0x%s", buf);
}


/*
static int fetch_data PARAMS ((struct disassemble_info *, uint8_t *));
static void ckprefix PARAMS ((void));
static const char *prefix_name PARAMS ((int, int));
int print_insn PARAMS ((uint64_t, disassemble_info *, uint32_t));
static void dofloat PARAMS ((int));
static void OP_ST PARAMS ((int, int));
static void OP_STi  PARAMS ((int, int));
static int putop PARAMS ((const char *, int));
static void oappend PARAMS ((const char *));
static void append_seg PARAMS ((void));
static void OP_indirE PARAMS ((int, int));
static void print_operand_value PARAMS ((char *, int, uint64_t));
static void OP_E PARAMS ((int, int));
static void OP_G PARAMS ((int, int));
static uint64_t get64 PARAMS ((void));
static int64_t get32 PARAMS ((void));
static int64_t get32s PARAMS ((void));
static int get16 PARAMS ((void));
static void set_op PARAMS ((uint64_t, int));
static void OP_REG PARAMS ((int, int));
static void OP_IMREG PARAMS ((int, int));
static void OP_I PARAMS ((int, int));
static void OP_I64 PARAMS ((int, int));
static void OP_sI PARAMS ((int, int));
static void OP_J PARAMS ((int, int));
static void OP_SEG PARAMS ((int, int));
static void OP_DIR PARAMS ((int, int));
static void OP_OFF PARAMS ((int, int));
static void OP_OFF64 PARAMS ((int, int));
static void ptr_reg PARAMS ((int, int));
static void OP_ESreg PARAMS ((int, int));
static void OP_DSreg PARAMS ((int, int));
static void OP_C PARAMS ((int, int));
static void OP_D PARAMS ((int, int));
static void OP_T PARAMS ((int, int));
static void OP_Rd PARAMS ((int, int));
static void OP_MMX PARAMS ((int, int));
static void OP_XMM PARAMS ((int, int));
static void OP_EM PARAMS ((int, int));
static void OP_EX PARAMS ((int, int));
static void OP_MS PARAMS ((int, int));
static void OP_XS PARAMS ((int, int));
static void OP_3DNowSuffix PARAMS ((int, int));
static void OP_SIMD_Suffix PARAMS ((int, int));
static void SIMD_Fixup PARAMS ((int, int));
static void BadOp PARAMS ((void));
*/

/* Make sure that bytes from INFO->PRIVATE_DATA->BUFFER (inclusive)
   to ADDR (exclusive) are valid.  Returns 1 for success, longjmps
   on error.  */
#define FETCH_DATA(info, addr) \
  ((addr) <= ((struct dis_private *) (info->private_data))->max_fetched \
   ? 1 : fetch_data ((info), (addr)))


 int fetch_data (struct disassemble_info* info, uint8_t* addr)
{
    int status;
    struct dis_private *priv = (struct dis_private *) info->private_data;
    uint64_t start = priv->insn_start + (priv->max_fetched - priv->the_buffer);

    status = x86inst_intern_read_mem_func(start, priv->max_fetched, addr - priv->max_fetched, info);
    if (!status){
        fprintf(stderr, "Address 0x%x is out of bounds.\n", start);
        exit(0);
    } else {
        priv->max_fetched = addr;
    }
    return 1;
}

 void ckprefix()
{
    int newrex;
    rex = 0;
    prefixes = 0;
    used_prefixes = 0;
    rex_used = 0;

    while (1){
        FETCH_DATA(the_info, codep + 1);
        newrex = 0;
        switch (*codep){
            /* REX prefixes family.  */
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f:
            //fprintf(stdout, "GOGOGOGO: rex prefix found\n");
	    if (mode_64bit)
                newrex = *codep;
	    else
                return;
            break;
	case 0xf3:
            prefixes |= PREFIX_REPZ;
            break;
	case 0xf2:
            prefixes |= PREFIX_REPNZ;
            break;
	case 0xf0:
            prefixes |= PREFIX_LOCK;
            break;
	case 0x2e:
            prefixes |= PREFIX_CS;
            break;
	case 0x36:
            prefixes |= PREFIX_SS;
            break;
	case 0x3e:
            prefixes |= PREFIX_DS;
            break;
	case 0x26:
            prefixes |= PREFIX_ES;
            break;
	case 0x64:
            prefixes |= PREFIX_FS;
            break;
	case 0x65:
            prefixes |= PREFIX_GS;
            break;
	case 0x66:
            prefixes |= PREFIX_DATA;
            break;
	case 0x67:
            prefixes |= PREFIX_ADDR;
            break;
	case FWAIT_OPCODE:
            /* fwait is really an instruction.  If there are prefixes
               before the fwait, they belong to the fwait, *not* to the
               following instruction.  */
            if (prefixes){
                prefixes |= PREFIX_FWAIT;
                codep++;
                //fprintf(stdout, "GOGOGOGO: fwait prefix\n");
                return;
	    }
            prefixes = PREFIX_FWAIT;
            break;
	default:
            //fprintf(stdout, "GOGOGOGO: no prefix found\n");
            return;
	}
        /* Rex is ignored when followed by another prefix.  */
        if (rex){
            oappend (prefix_name (rex, 0));
            oappend (" ");
        }
        rex = newrex;
        //fprintf(stdout, "GOGOGOGO: have a prefix\n");
        codep++;
    }
}

/* Return the name of the prefix byte PREF, or NULL if PREF is not a
   prefix byte.  */

 const char* prefix_name(int pref, int sizeflag)
{
    switch (pref){
        /* REX prefixes family.  */
    case 0x40:
        return "rex";
    case 0x41:
        return "rexZ";
    case 0x42:
        return "rexY";
    case 0x43:
        return "rexYZ";
    case 0x44:
        return "rexX";
    case 0x45:
        return "rexXZ";
    case 0x46:
        return "rexXY";
    case 0x47:
        return "rexXYZ";
    case 0x48:
        return "rex64";
    case 0x49:
        return "rex64Z";
    case 0x4a:
        return "rex64Y";
    case 0x4b:
        return "rex64YZ";
    case 0x4c:
        return "rex64X";
    case 0x4d:
        return "rex64XZ";
    case 0x4e:
        return "rex64XY";
    case 0x4f:
        return "rex64XYZ";
    case 0xf3:
        return "repz";
    case 0xf2:
        return "repnz";
    case 0xf0:
        return "lock";
    case 0x2e:
        return "cs";
    case 0x36:
        return "ss";
    case 0x3e:
        return "ds";
    case 0x26:
        return "es";
    case 0x64:
        return "fs";
    case 0x65:
        return "gs";
    case 0x66:
        return (sizeflag & DFLAG) ? "data16" : "data32";
    case 0x67:
        if (mode_64bit)
            return (sizeflag & AFLAG) ? "addr32" : "addr64";
        else
            return ((sizeflag & AFLAG) && !mode_64bit) ? "addr16" : "addr32";
    case FWAIT_OPCODE:
        return "fwait";
    default:
        return NULL;
    }
}

static char op1out[100], op2out[100], op3out[100];
static int op_ad, op_index[3];
static uint64_t op_address[3];
static uint64_t op_riprel[3];
static uint64_t start_pc;

/*
 *   On the 386's of 1988, the maximum length of an instruction is 15 bytes.
 *   (see topic "Redundant prefixes" in the "Differences from 8086"
 *   section of the "Virtual 8086 Mode" chapter.)
 * 'pc' should be the address of this instruction, it will
 *   be used to print the target address if this is a relative jump or call
 * The function returns the length of this instruction in bytes.
 */

static char intel_syntax;
static char open_char;
static char close_char;
static char separator_char;
static char scale_char;


int print_insn (uint64_t pc, disassemble_info* info, uint32_t is64bit){
    const struct dis386 *dp;
    int i;
    int two_source_ops;
    char *first, *second, *third;
    int needcomma;
    unsigned char uses_SSE_prefix;
    int sizeflag;
    const char *p;
    struct dis_private priv;

    mode_64bit = (info->mach == mach_x86_64_intel_syntax || info->mach == mach_x86_64);
    intel_syntax = (info->mach == mach_i386_i386_intel_syntax || info->mach == mach_x86_64_intel_syntax);

    if (info->mach == mach_i386_i386 || info->mach == mach_x86_64 || info->mach == mach_i386_i386_intel_syntax || info->mach == mach_x86_64_intel_syntax){
        priv.orig_sizeflag = AFLAG | DFLAG;
    } else if (info->mach == mach_i386_i8086){
        priv.orig_sizeflag = 0;
    } else { 
        fprintf(stderr, "ERROR: mach field of info struct not set correctly\n");
        abort();
    }

    if (intel_syntax) {
        names64 = intel_names64;
        names32 = intel_names32;
        names16 = intel_names16;
        names8 = intel_names8;
        names8rex = intel_names8rex;
        names_seg = intel_names_seg;
        index16 = intel_index16;
        open_char = '[';
        close_char = ']';
        separator_char = '+';
        scale_char = '*';

    } else {
        names64 = att_names64;
        names32 = att_names32;
        names16 = att_names16;
        names8 = att_names8;
        names8rex = att_names8rex;
        names_seg = att_names_seg;
        index16 = att_index16;
        open_char = '(';
        close_char =  ')';
        separator_char = ',';
        scale_char = ',';
    }

    info->private_data = (PTR) &priv;
    priv.max_fetched = priv.the_buffer;
    priv.insn_start = pc;

    obuf[0] = 0;
    op1out[0] = 0;
    op2out[0] = 0;
    op3out[0] = 0;

    op_index[0] = op_index[1] = op_index[2] = -1;
    
    the_info = info;
    start_pc = pc;
    start_codep = priv.the_buffer;
    codep = priv.the_buffer;

    obufp = obuf;
    ckprefix();

    insn_codep = codep;
    sizeflag = priv.orig_sizeflag;

    FETCH_DATA(info, codep + 1);
    //fprintf(stdout, "fetched data, codep=%lx %lx %lx %lx %lx %lx %lx %lx\n", *codep, *(codep+1), *(codep+2), *(codep+3), *(codep+4), *(codep+5), *(codep+6), *(codep+7));

    two_source_ops = (*codep == 0x62) || (*codep == 0xc8);

    if ((prefixes & PREFIX_FWAIT) && ((*codep < 0xd8) || (*codep > 0xdf))){
        const char *name;

        /* fwait not followed by floating point instruction.  Print the
           first prefix, which is probably fwait itself.  */
        name = prefix_name (priv.the_buffer[0], priv.orig_sizeflag);
        if (name == NULL)
            name = INTERNAL_DISASSEMBLER_ERROR;
        (*disasm_fprintf_func) (stdout, "%s", name);
        return 1;
    }

    if (*codep == 0x0f){
        FETCH_DATA(info, codep + 2);
        dp = &dis386_twobyte[*++codep];
        need_modrm = twobyte_has_modrm[*codep];
        uses_SSE_prefix = twobyte_uses_SSE_prefix[*codep];
    } else {
        dp = &dis386[*codep];
        need_modrm = onebyte_has_modrm[*codep];
        uses_SSE_prefix = 0;
    }
    codep++;

    if (!uses_SSE_prefix && (prefixes & PREFIX_REPZ)){
        oappend ("repz ");
        used_prefixes |= PREFIX_REPZ;
    }
    if (!uses_SSE_prefix && (prefixes & PREFIX_REPNZ)){
        oappend ("repnz ");
        used_prefixes |= PREFIX_REPNZ;
    }
    if (prefixes & PREFIX_LOCK){
        oappend ("lock ");
        used_prefixes |= PREFIX_LOCK;
    }

    if (prefixes & PREFIX_ADDR){
        sizeflag ^= AFLAG;
        if (dp->bytemode3 != loop_jcxz_mode || intel_syntax){
            if ((sizeflag & AFLAG) || mode_64bit)
                oappend ("addr32 ");
            else
                oappend ("addr16 ");
            used_prefixes |= PREFIX_ADDR;
	}
    }

    if (!uses_SSE_prefix && (prefixes & PREFIX_DATA)){
        sizeflag ^= DFLAG;
        if (dp->bytemode3 == cond_jump_mode && dp->bytemode1 == v_mode && !intel_syntax){
            if (sizeflag & DFLAG)
                oappend ("data32 ");
            else
                oappend ("data16 ");
            used_prefixes |= PREFIX_DATA;
	}
    }

    if (need_modrm){
        FETCH_DATA(info, codep + 1);
        mod = (*codep >> 6) & 3;
        reg = (*codep >> 3) & 7;
        rm = *codep & 7;
    }

    if (dp->name == NULL && dp->bytemode1 == FLOATCODE){
        dofloat (sizeflag);
    } else {
        int index;
        if (dp->name == NULL){
            switch (dp->bytemode1){
	    case USE_GROUPS:
                dp = &grps[dp->bytemode2][reg];
                break;
                
	    case USE_PREFIX_USER_TABLE:
                index = 0;
                used_prefixes |= (prefixes & PREFIX_REPZ);
                if (prefixes & PREFIX_REPZ)
                    index = 1;
                else {
                    used_prefixes |= (prefixes & PREFIX_DATA);
                    if (prefixes & PREFIX_DATA)
                        index = 2;
                    else {
                        used_prefixes |= (prefixes & PREFIX_REPNZ);
                        if (prefixes & PREFIX_REPNZ)
                            index = 3;
		    }
		}
                dp = &prefix_user_table[dp->bytemode2][index];
                break;
                
	    case X86_64_SPECIAL:
                dp = &x86_64_table[dp->bytemode2][mode_64bit];
                break;
                
	    default:
                oappend (INTERNAL_DISASSEMBLER_ERROR);
                break;
	    }
	}
        
        if (putop (dp->name, sizeflag) == 0){
            obufp = op1out;
            op_ad = 2;
            if (dp->op1)
                (*dp->op1) (dp->bytemode1, sizeflag);
            
            obufp = op2out;
            op_ad = 1;
            if (dp->op2)
                (*dp->op2) (dp->bytemode2, sizeflag);
            
            obufp = op3out;
            op_ad = 0;
            if (dp->op3)
                (*dp->op3) (dp->bytemode3, sizeflag);
	}
    }

    /* See if any prefixes were not used.  If so, print the first one
       separately.  If we don't do this, we'll wind up printing an
       instruction stream which does not precisely correspond to the
       bytes we are disassembling.  */
    if ((prefixes & ~used_prefixes) != 0){
        const char *name;
        
        name = prefix_name (priv.the_buffer[0], priv.orig_sizeflag);
        if (name == NULL)
            name = INTERNAL_DISASSEMBLER_ERROR;
        (*disasm_fprintf_func) (stdout, "%s", name);
        return 1;
    }
    if (rex & ~rex_used){
        const char *name;
        name = prefix_name (rex | 0x40, priv.orig_sizeflag);
        if (name == NULL)
            name = INTERNAL_DISASSEMBLER_ERROR;
        (*disasm_fprintf_func) (stdout, "%s ", name);
    }

    obufp = obuf + strlen (obuf);
    for (i = strlen (obuf); i < 6; i++)
        oappend (" ");
    oappend (" ");
    (*disasm_fprintf_func) (stdout, "%s", obuf);

    /* The enter and bound instructions are printed with operands in the same
       order as the intel book; everything else is printed in reverse order.  */
    if (intel_syntax || two_source_ops){
        first = op1out;
        second = op2out;
        third = op3out;
        op_ad = op_index[0];
        op_index[0] = op_index[2];
        op_index[2] = op_ad;
    }
    else {
        first = op3out;
        second = op2out;
        third = op1out;
    }
    needcomma = 0;
    if (*first) {
        if (op_index[0] != -1 && !op_riprel[0])
            generic_print_address((uint64_t) op_address[op_index[0]], info);
        else
            (*disasm_fprintf_func) (stdout, "%s", first);
        needcomma = 1;
    }
    if (*second) {
        if (needcomma)
            (*disasm_fprintf_func) (stdout, ",");
        if (op_index[1] != -1 && !op_riprel[1])
            generic_print_address((uint64_t) op_address[op_index[1]], info);
        else
            (*disasm_fprintf_func) (stdout, "%s", second);
        needcomma = 1;
    }
    if (*third){
        if (needcomma)
            (*disasm_fprintf_func) (stdout, ",");
        if (op_index[2] != -1 && !op_riprel[2])
            generic_print_address((uint64_t) op_address[op_index[2]], info);
        else
            (*disasm_fprintf_func) (stdout, "%s", third);
    }
    for (i = 0; i < 3; i++){
        if (op_index[i] != -1 && op_riprel[i]){
            (*disasm_fprintf_func) (stdout, "        # ");
            generic_print_address((uint64_t) (start_pc + codep - start_codep
                                                     + op_address[op_index[i]]), info);
        }
    }
    return codep - priv.the_buffer;
}


void dofloat (int sizeflag){
    const struct dis386 *dp;
    unsigned char floatop;

    floatop = codep[-1];
    
    if (mod != 3){
        putop (float_mem[(floatop - 0xd8) * 8 + reg], sizeflag);
        obufp = op1out;
        if (floatop == 0xdb)
            OP_E (x_mode, sizeflag);
        else if (floatop == 0xdd)
            OP_E (d_mode, sizeflag);
        else
            OP_E (v_mode, sizeflag);
        return;
    }
    /* Skip mod/rm byte.  */
    MODRM_CHECK;
    codep++;

    dp = &float_reg[floatop - 0xd8][reg];
    if (dp->name == NULL){
        putop (fgrps[dp->bytemode1][rm], sizeflag);
        
        /* Instruction fnstsw is only one with strange arg.  */
        if (floatop == 0xdf && codep[-1] == 0xe0)
            strcpy (op1out, names16[0]);
    }
    else {
        putop (dp->name, sizeflag);
        
        obufp = op1out;
        if (dp->op1)
            (*dp->op1) (dp->bytemode1, sizeflag);
        obufp = op2out;
        if (dp->op2)
            (*dp->op2) (dp->bytemode2, sizeflag);
    }
}

void OP_ST (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    oappend ("%st");
}

void OP_STi (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    sprintf (scratchbuf, "%%st(%d)", rm);
    oappend (scratchbuf + intel_syntax);
}

/* Capital letters in template are macros.  */
int putop (const char* template, int sizeflag)
{
    const char *p;
    int alt;
    
    for (p = template; *p; p++){
        switch (*p){
	default:
            *obufp++ = *p;
            break;
	case '{':
            alt = 0;
            if (intel_syntax)
                alt += 1;
            if (mode_64bit)
                alt += 2;
            while (alt != 0){
                while (*++p != '|'){
                    if (*p == '}'){
                        /* Alternative not valid.  */
                        strcpy (obuf, "(bad)");
                        obufp = obuf + 5;
                        return 1;
		    }
                    else if (*p == '\0')
                        abort();
		}
                alt--;
	    }
            break;
	case '|':
            while (*++p != '}'){
                if (*p == '\0')
                    abort();
	    }
            break;
	case '}':
            break;
	case 'A':
            if (intel_syntax)
                break;
            if (mod != 3 || (sizeflag & SUFFIX_ALWAYS))
                *obufp++ = 'b';
            break;
	case 'B':
            if (intel_syntax)
                break;
            if (sizeflag & SUFFIX_ALWAYS)
                *obufp++ = 'b';
            break;
	case 'E':		/* For jcxz/jecxz */
            if (mode_64bit){
                if (sizeflag & AFLAG)
                    *obufp++ = 'r';
                else
                    *obufp++ = 'e';
	    }
            else
                if (sizeflag & AFLAG)
                    *obufp++ = 'e';
            used_prefixes |= (prefixes & PREFIX_ADDR);
            break;
	case 'F':
            if (intel_syntax)
                break;
            if ((prefixes & PREFIX_ADDR) || (sizeflag & SUFFIX_ALWAYS)){
                if (sizeflag & AFLAG)
                    *obufp++ = mode_64bit ? 'q' : 'l';
                else
                    *obufp++ = mode_64bit ? 'l' : 'w';
                used_prefixes |= (prefixes & PREFIX_ADDR);
	    }
            break;
	case 'H':
            if (intel_syntax)
                break;
            if ((prefixes & (PREFIX_CS | PREFIX_DS)) == PREFIX_CS || (prefixes & (PREFIX_CS | PREFIX_DS)) == PREFIX_DS){
                used_prefixes |= prefixes & (PREFIX_CS | PREFIX_DS);
                *obufp++ = ',';
                *obufp++ = 'p';
                if (prefixes & PREFIX_DS)
                    *obufp++ = 't';
                else
                    *obufp++ = 'n';
	    }
            break;
	case 'L':
            if (intel_syntax)
                break;
            if (sizeflag & SUFFIX_ALWAYS)
                *obufp++ = 'l';
            break;
	case 'N':
            if ((prefixes & PREFIX_FWAIT) == 0)
                *obufp++ = 'n';
            else
                used_prefixes |= PREFIX_FWAIT;
            break;
	case 'O':
            USED_REX (REX_MODE64);
            if (rex & REX_MODE64)
                *obufp++ = 'o';
            else
                *obufp++ = 'd';
            break;
	case 'T':
            if (intel_syntax)
                break;
            if (mode_64bit){
                *obufp++ = 'q';
                break;
	    }
            /* Fall through.  */
	case 'P':
            if (intel_syntax)
                break;
            if ((prefixes & PREFIX_DATA) || (rex & REX_MODE64) || (sizeflag & SUFFIX_ALWAYS)){
                USED_REX (REX_MODE64);
                if (rex & REX_MODE64)
                    *obufp++ = 'q';
                else {
                    if (sizeflag & DFLAG)
                        *obufp++ = 'l';
                    else
                        *obufp++ = 'w';
                    used_prefixes |= (prefixes & PREFIX_DATA);
		}
	    }
            break;
	case 'U':
            if (intel_syntax)
                break;
            if (mode_64bit){
                *obufp++ = 'q';
                break;
	    }
            /* Fall through.  */
	case 'Q':
            if (intel_syntax)
                break;
            USED_REX (REX_MODE64);
            if (mod != 3 || (sizeflag & SUFFIX_ALWAYS)){
                if (rex & REX_MODE64)
                    *obufp++ = 'q';
                else {
                    if (sizeflag & DFLAG)
                        *obufp++ = 'l';
                    else
                        *obufp++ = 'w';
                    used_prefixes |= (prefixes & PREFIX_DATA);
		}
	    }
            break;
	case 'R':
            USED_REX (REX_MODE64);
            if (intel_syntax){
                if (rex & REX_MODE64){
                    *obufp++ = 'q';
                    *obufp++ = 't';
		}
                else if (sizeflag & DFLAG){
                    *obufp++ = 'd';
                    *obufp++ = 'q';
		}
                else {
                    *obufp++ = 'w';
                    *obufp++ = 'd';
                }
	    }
            else {
                if (rex & REX_MODE64)
                    *obufp++ = 'q';
                else if (sizeflag & DFLAG)
                    *obufp++ = 'l';
                else
                    *obufp++ = 'w';
	    }
            if (!(rex & REX_MODE64))
                used_prefixes |= (prefixes & PREFIX_DATA);
            break;
	case 'S':
            if (intel_syntax)
                break;
            if (sizeflag & SUFFIX_ALWAYS){
                if (rex & REX_MODE64)
                    *obufp++ = 'q';
                else{
                    if (sizeflag & DFLAG)
                        *obufp++ = 'l';
                    else
                        *obufp++ = 'w';
                    used_prefixes |= (prefixes & PREFIX_DATA);
		}
	    }
            break;
	case 'X':
            if (prefixes & PREFIX_DATA)
                *obufp++ = 'd';
            else
                *obufp++ = 's';
            used_prefixes |= (prefixes & PREFIX_DATA);
            break;
	case 'Y':
            if (intel_syntax)
                break;
            if (rex & REX_MODE64){
                USED_REX (REX_MODE64);
                *obufp++ = 'q';
	    }
            break;
            /* implicit operand size 'l' for i386 or 'q' for x86-64 */
	case 'W':
            /* operand size flag for cwtl, cbtw */
            USED_REX (0);
            if (rex)
                *obufp++ = 'l';
            else if (sizeflag & DFLAG)
                *obufp++ = 'w';
            else
                *obufp++ = 'b';
            if (intel_syntax){
                if (rex){
                    *obufp++ = 'q';
                    *obufp++ = 'e';
		}
                if (sizeflag & DFLAG){
                    *obufp++ = 'd';
                    *obufp++ = 'e';
		}
                else {
                    *obufp++ = 'w';
		}
	    }
            if (!rex)
                used_prefixes |= (prefixes & PREFIX_DATA);
            break;
	}
    }
    *obufp = 0;
    return 0;
}

void oappend (const char* s)
{
    strcpy (obufp, s);
    obufp += strlen (s);
}

void append_seg(){
    if (prefixes & PREFIX_CS){
        used_prefixes |= PREFIX_CS;
        oappend ("%cs:" + intel_syntax);
    }
    if (prefixes & PREFIX_DS){
        used_prefixes |= PREFIX_DS;
        oappend ("%ds:" + intel_syntax);
    }
    if (prefixes & PREFIX_SS){
        used_prefixes |= PREFIX_SS;
        oappend ("%ss:" + intel_syntax);
    }
    if (prefixes & PREFIX_ES){
        used_prefixes |= PREFIX_ES;
        oappend ("%es:" + intel_syntax);
    }
    if (prefixes & PREFIX_FS){
        used_prefixes |= PREFIX_FS;
        oappend ("%fs:" + intel_syntax);
    }
    if (prefixes & PREFIX_GS){
        used_prefixes |= PREFIX_GS;
        oappend ("%gs:" + intel_syntax);
    }
}

void OP_indirE (int bytemode, int sizeflag){
    if (!intel_syntax)
        oappend ("*");
    OP_E (bytemode, sizeflag);
}

void print_operand_value (char* buf, int hex, uint64_t disp){
    if (mode_64bit){
        if (hex){
            char tmp[30];
            int i;
            buf[0] = '0';
            buf[1] = 'x';
            sprintf_vma (tmp, disp);
            for (i = 0; tmp[i] == '0' && tmp[i + 1]; i++);
            strcpy (buf + 2, tmp + i);
	}
        else {
            int64_t v = disp;
            char tmp[30];
            int i;
            if (v < 0){
                *(buf++) = '-';
                v = -disp;
                /* Check for possible overflow on 0x8000000000000000.  */
                if (v < 0){
                    strcpy (buf, "9223372036854775808");
                    return;
		}
	    }
            if (!v){
                strcpy (buf, "0");
                return;
	    }
            
            i = 0;
            tmp[29] = 0;
            while (v){
                tmp[28 - i] = (v % 10) + '0';
                v /= 10;
                i++;
	    }
            strcpy (buf, tmp + 29 - i);
	}
    }
    else{
        if (hex)
            sprintf (buf, "0x%x", (unsigned int) disp);
        else
            sprintf (buf, "%d", (int) disp);
    }
}

void OP_E (int bytemode, int sizeflag){
    uint64_t disp;
    int add = 0;
    int riprel = 0;
    USED_REX (REX_EXTZ);
    if (rex & REX_EXTZ)
        add += 8;
    
    /* Skip mod/rm byte.  */
    MODRM_CHECK;
    codep++;

    if (mod == 3){
        switch (bytemode){
	case b_mode:
            USED_REX (0);
            if (rex)
                oappend (names8rex[rm + add]);
            else
                oappend (names8[rm + add]);
            break;
	case w_mode:
            oappend (names16[rm + add]);
            break;
	case d_mode:
            oappend (names32[rm + add]);
            break;
	case q_mode:
            oappend (names64[rm + add]);
            break;
	case m_mode:
            if (mode_64bit)
                oappend (names64[rm + add]);
            else
                oappend (names32[rm + add]);
            break;
	case v_mode:
            USED_REX (REX_MODE64);
            if (rex & REX_MODE64)
                oappend (names64[rm + add]);
            else if (sizeflag & DFLAG)
                oappend (names32[rm + add]);
            else
                oappend (names16[rm + add]);
            used_prefixes |= (prefixes & PREFIX_DATA);
            break;
	case 0:
            if (!(codep[-2] == 0xAE && codep[-1] == 0xF8 /* sfence */)
                && !(codep[-2] == 0xAE && codep[-1] == 0xF0 /* mfence */)
                && !(codep[-2] == 0xAE && codep[-1] == 0xe8 /* lfence */))
                BadOp();	/* bad sfence,lea,lds,les,lfs,lgs,lss modrm */
            break;
	default:
            oappend (INTERNAL_DISASSEMBLER_ERROR);
            break;
	}
        return;
    }
    
    disp = 0;
    append_seg();
    
    /* 32 bit address mode */
    if ((sizeflag & AFLAG) || mode_64bit){
        int havesib;
        int havebase;
        int base;
        int index = 0;
        int scale = 0;
        
        havesib = 0;
        havebase = 1;
        base = rm;

        if (base == 4){
            havesib = 1;
            FETCH_DATA(the_info, codep + 1);
            scale = (*codep >> 6) & 3;
            index = (*codep >> 3) & 7;
            base = *codep & 7;
            USED_REX (REX_EXTY);
            USED_REX (REX_EXTZ);
            if (rex & REX_EXTY)
                index += 8;
            if (rex & REX_EXTZ)
                base += 8;
            codep++;
	}

        switch (mod){
	case 0:
            if ((base & 7) == 5){
                havebase = 0;
                if (mode_64bit && !havesib && (sizeflag & AFLAG))
                    riprel = 1;
                disp = get32s();
	    }
            break;
	case 1:
            FETCH_DATA(the_info, codep + 1);
            disp = *codep++;
            if ((disp & 0x80) != 0)
                disp -= 0x100;
            break;
	case 2:
            disp = get32s();
            break;
	}

        if (!intel_syntax){
            if (mod != 0 || (base & 7) == 5){
                print_operand_value (scratchbuf, !riprel, disp);
                oappend (scratchbuf);
                if (riprel){
                    set_op (disp, 1);
                    oappend ("(%rip)");
                }
            }
        }        
        if (havebase || (havesib && (index != 4 || scale != 0))){
            if (intel_syntax){
                switch (bytemode){
                case b_mode:
                    oappend ("BYTE PTR ");
                    break;
                case w_mode:
                    oappend ("WORD PTR ");
                    break;
                case v_mode:
                    oappend ("DWORD PTR ");
                    break;
                case d_mode:
                    oappend ("QWORD PTR ");
                    break;
                case m_mode:
                    if (mode_64bit)
                        oappend ("DWORD PTR ");
                    else
                        oappend ("QWORD PTR ");
                    break;
                case x_mode:
                    oappend ("XWORD PTR ");
                    break;
                default:
                    break;
                }
            }
            *obufp++ = open_char;
            if (intel_syntax && riprel)
                oappend ("rip + ");
            *obufp = '\0';
            USED_REX (REX_EXTZ);
            if (!havesib && (rex & REX_EXTZ))
                base += 8;
            if (havebase)
                oappend (mode_64bit && (sizeflag & AFLAG)
                         ? names64[base] : names32[base]);
            if (havesib){
                if (index != 4){
                    if (intel_syntax){
                        if (havebase){
                            *obufp++ = separator_char;
                            *obufp = '\0';
                        }
                        sprintf (scratchbuf, "%s",
                                 mode_64bit && (sizeflag & AFLAG)
                                 ? names64[index] : names32[index]);
                    }
                    else
                        sprintf (scratchbuf, ",%s",
                                 mode_64bit && (sizeflag & AFLAG)
                                 ? names64[index] : names32[index]);
                    oappend (scratchbuf);
		}
                if (!intel_syntax || (intel_syntax && bytemode != b_mode && bytemode != w_mode && bytemode != v_mode)){
                    *obufp++ = scale_char;
                    *obufp = '\0';
                    sprintf (scratchbuf, "%d", 1 << scale);
                    oappend (scratchbuf);
                }
	    }
            if (intel_syntax)
                if (mod != 0 || (base & 7) == 5){
                    /* Don't print zero displacements.  */
                    if (disp != 0){
                        if ((int64_t) disp > 0){
                            *obufp++ = '+';
                            *obufp = '\0';
                        }
                        
                        print_operand_value (scratchbuf, 0, disp);
                        oappend (scratchbuf);
                    }
                }
            
            *obufp++ = close_char;
            *obufp = '\0';
        }
        else if (intel_syntax){
            if (mod != 0 || (base & 7) == 5){
                if (prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS
                                | PREFIX_ES | PREFIX_FS | PREFIX_GS))
                    ;
                else{
                    oappend (names_seg[ds_reg - es_reg]);
                    oappend (":");
		}
                print_operand_value (scratchbuf, 1, disp);
                oappend (scratchbuf);
            }
        }
    }
    else { /* 16 bit address mode */
        switch (mod){
	case 0:
            if ((rm & 7) == 6){
                disp = get16();
                if ((disp & 0x8000) != 0)
                    disp -= 0x10000;
	    }
            break;
	case 1:
            FETCH_DATA(the_info, codep + 1);
            disp = *codep++;
            if ((disp & 0x80) != 0)
                disp -= 0x100;
            break;
	case 2:
            disp = get16();
            if ((disp & 0x8000) != 0)
                disp -= 0x10000;
            break;
	}

        if (!intel_syntax)
            if (mod != 0 || (rm & 7) == 6){
                print_operand_value (scratchbuf, 0, disp);
                oappend (scratchbuf);
            }

        if (mod != 0 || (rm & 7) != 6){
            *obufp++ = open_char;
            *obufp = '\0';
            oappend (index16[rm + add]);
            *obufp++ = close_char;
            *obufp = '\0';
	}
    }
}

void OP_G (int bytemode, int sizeflag)
{
    int add = 0;
    USED_REX (REX_EXTX);
    if (rex & REX_EXTX)
        add += 8;
    switch (bytemode){
    case b_mode:
        USED_REX (0);
        if (rex)
            oappend (names8rex[reg + add]);
        else
            oappend (names8[reg + add]);
        break;
    case w_mode:
        oappend (names16[reg + add]);
        break;
    case d_mode:
        oappend (names32[reg + add]);
        break;
    case q_mode:
        oappend (names64[reg + add]);
        break;
    case v_mode:
        USED_REX (REX_MODE64);
        if (rex & REX_MODE64)
            oappend (names64[reg + add]);
        else if (sizeflag & DFLAG)
            oappend (names32[reg + add]);
        else
            oappend (names16[reg + add]);
        used_prefixes |= (prefixes & PREFIX_DATA);
        break;
    default:
        oappend (INTERNAL_DISASSEMBLER_ERROR);
        break;
    }
}

uint64_t get64()
{
    uint64_t x;
#ifdef BFD64
    unsigned int a;
    unsigned int b;
    
    FETCH_DATA(the_info, codep + 8);
    a = *codep++ & 0xff;
    a |= (*codep++ & 0xff) << 8;
    a |= (*codep++ & 0xff) << 16;
    a |= (*codep++ & 0xff) << 24;
    b = *codep++ & 0xff;
    b |= (*codep++ & 0xff) << 8;
    b |= (*codep++ & 0xff) << 16;
    b |= (*codep++ & 0xff) << 24;
    x = a + ((uint64_t) b << 32);
#else
    abort();
    x = 0;
#endif
    return x;
}

 int64_t get32()
{
    int64_t x = 0;
    
    FETCH_DATA(the_info, codep + 4);
    x = *codep++ & (int64_t) 0xff;
    x |= (*codep++ & (int64_t) 0xff) << 8;
    x |= (*codep++ & (int64_t) 0xff) << 16;
    x |= (*codep++ & (int64_t) 0xff) << 24;
    return x;
}

int64_t get32s()
{
    int64_t x = 0;
    
    FETCH_DATA(the_info, codep + 4);
    x = *codep++ & (int64_t) 0xff;
    x |= (*codep++ & (int64_t) 0xff) << 8;
    x |= (*codep++ & (int64_t) 0xff) << 16;
    x |= (*codep++ & (int64_t) 0xff) << 24;
  
    x = (x ^ ((int64_t) 1 << 31)) - ((int64_t) 1 << 31);
    
    return x;
}

int get16()
{
    int x = 0;
    
    FETCH_DATA(the_info, codep + 2);
    x = *codep++ & 0xff;
    x |= (*codep++ & 0xff) << 8;
    return x;
}

void set_op (uint64_t op, int riprel)
{
    op_index[op_ad] = op_ad;
    if (mode_64bit){
        op_address[op_ad] = op;
        op_riprel[op_ad] = riprel;
    }
    else {
        /* Mask to get a 32-bit address.  */
        op_address[op_ad] = op & 0xffffffff;
        op_riprel[op_ad] = riprel & 0xffffffff;
    }
}

void OP_REG (int code, int sizeflag)
{
    const char *s;
    int add = 0;
    USED_REX (REX_EXTZ);
    if (rex & REX_EXTZ)
        add = 8;
    
    switch (code){
    case indir_dx_reg:
        if (intel_syntax)
            s = "[dx]";
        else
            s = "(%dx)";
        break;
    case ax_reg: case cx_reg: case dx_reg: case bx_reg:
    case sp_reg: case bp_reg: case si_reg: case di_reg:
        s = names16[code - ax_reg + add];
        break;
    case es_reg: case ss_reg: case cs_reg:
    case ds_reg: case fs_reg: case gs_reg:
        s = names_seg[code - es_reg + add];
        break;
    case al_reg: case ah_reg: case cl_reg: case ch_reg:
    case dl_reg: case dh_reg: case bl_reg: case bh_reg:
        USED_REX (0);
        if (rex)
            s = names8rex[code - al_reg + add];
        else
            s = names8[code - al_reg];
        break;
    case rAX_reg: case rCX_reg: case rDX_reg: case rBX_reg:
    case rSP_reg: case rBP_reg: case rSI_reg: case rDI_reg:
        if (mode_64bit){
            s = names64[code - rAX_reg + add];
            break;
	}
        code += eAX_reg - rAX_reg;
        /* Fall through.  */
    case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
    case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
        USED_REX (REX_MODE64);
        if (rex & REX_MODE64)
            s = names64[code - eAX_reg + add];
        else if (sizeflag & DFLAG)
            s = names32[code - eAX_reg + add];
        else
            s = names16[code - eAX_reg + add];
        used_prefixes |= (prefixes & PREFIX_DATA);
        break;
    default:
        s = INTERNAL_DISASSEMBLER_ERROR;
        break;
    }
    oappend (s);
}

void OP_IMREG (int code, int sizeflag)
{
    const char *s;

    switch (code){
    case indir_dx_reg:
        if (intel_syntax)
            s = "[dx]";
        else
            s = "(%dx)";
        break;
    case ax_reg: case cx_reg: case dx_reg: case bx_reg:
    case sp_reg: case bp_reg: case si_reg: case di_reg:
        s = names16[code - ax_reg];
        break;
    case es_reg: case ss_reg: case cs_reg:
    case ds_reg: case fs_reg: case gs_reg:
        s = names_seg[code - es_reg];
        break;
    case al_reg: case ah_reg: case cl_reg: case ch_reg:
    case dl_reg: case dh_reg: case bl_reg: case bh_reg:
        USED_REX (0);
        if (rex)
            s = names8rex[code - al_reg];
        else
            s = names8[code - al_reg];
        break;
    case eAX_reg: case eCX_reg: case eDX_reg: case eBX_reg:
    case eSP_reg: case eBP_reg: case eSI_reg: case eDI_reg:
        USED_REX (REX_MODE64);
        if (rex & REX_MODE64)
            s = names64[code - eAX_reg];
        else if (sizeflag & DFLAG)
            s = names32[code - eAX_reg];
        else
            s = names16[code - eAX_reg];
        used_prefixes |= (prefixes & PREFIX_DATA);
        break;
    default:
        s = INTERNAL_DISASSEMBLER_ERROR;
        break;
    }
    oappend (s);
}

void OP_I (int bytemode, int sizeflag)
{
    int64_t op;
    int64_t mask = -1;
    
    switch (bytemode){
    case b_mode:
        FETCH_DATA(the_info, codep + 1);
        op = *codep++;
        mask = 0xff;
        break;
    case q_mode:
        if (mode_64bit){
            op = get32s();
            break;
	}
        /* Fall through.  */
    case v_mode:
        USED_REX (REX_MODE64);
        if (rex & REX_MODE64)
            op = get32s();
        else if (sizeflag & DFLAG){
            op = get32();
            mask = 0xffffffff;
	}
        else{
            op = get16();
            mask = 0xfffff;
	}
        used_prefixes |= (prefixes & PREFIX_DATA);
        break;
    case w_mode:
        mask = 0xfffff;
        op = get16();
        break;
    default:
        oappend (INTERNAL_DISASSEMBLER_ERROR);
        return;
    }
    
    op &= mask;
    scratchbuf[0] = '$';
    print_operand_value (scratchbuf + 1, 1, op);
    oappend (scratchbuf + intel_syntax);
    scratchbuf[0] = '\0';
}

void OP_I64 (int bytemode, int sizeflag)
{
    int64_t op;
    int64_t mask = -1;
    
    if (!mode_64bit){
        OP_I (bytemode, sizeflag);
        return;
    }
    
    switch (bytemode){
    case b_mode:
        FETCH_DATA(the_info, codep + 1);
        op = *codep++;
        mask = 0xff;
        break;
    case v_mode:
        USED_REX (REX_MODE64);
        if (rex & REX_MODE64)
            op = get64();
        else if (sizeflag & DFLAG){
            op = get32();
            mask = 0xffffffff;
        }
        else {
            op = get16();
            mask = 0xfffff;
        }
        used_prefixes |= (prefixes & PREFIX_DATA);
        break;
    case w_mode:
        mask = 0xfffff;
        op = get16();
        break;
    default:
        oappend (INTERNAL_DISASSEMBLER_ERROR);
        return;
    }
    
    op &= mask;
    scratchbuf[0] = '$';
    print_operand_value (scratchbuf + 1, 1, op);
    oappend (scratchbuf + intel_syntax);
    scratchbuf[0] = '\0';
}

 void OP_sI (int bytemode, int sizeflag)
{
    int64_t op;
    int64_t mask = -1;

    switch (bytemode){
    case b_mode:
        FETCH_DATA(the_info, codep + 1);
        op = *codep++;
        if ((op & 0x80) != 0)
            op -= 0x100;
        mask = 0xffffffff;
        break;
    case v_mode:
        USED_REX (REX_MODE64);
        if (rex & REX_MODE64)
            op = get32s();
        else if (sizeflag & DFLAG){
            op = get32s();
            mask = 0xffffffff;
	}
        else {
            mask = 0xffffffff;
            op = get16();
            if ((op & 0x8000) != 0)
                op -= 0x10000;
	}
        used_prefixes |= (prefixes & PREFIX_DATA);
        break;
    case w_mode:
        op = get16();
        mask = 0xffffffff;
        if ((op & 0x8000) != 0)
            op -= 0x10000;
        break;
    default:
        oappend (INTERNAL_DISASSEMBLER_ERROR);
        return;
    }

    scratchbuf[0] = '$';
    print_operand_value (scratchbuf + 1, 1, op);
    oappend (scratchbuf + intel_syntax);
}

void OP_J (int bytemode, int sizeflag)
{
    uint64_t disp;
    uint64_t mask = -1;

    switch (bytemode){
    case b_mode:
        FETCH_DATA(the_info, codep + 1);
        disp = *codep++;
        if ((disp & 0x80) != 0)
            disp -= 0x100;
        break;
    case v_mode:
        if (sizeflag & DFLAG)
            disp = get32s();
        else {
            disp = get16();
            /* For some reason, a data16 prefix on a jump instruction
               means that the pc is masked to 16 bits after the
               displacement is added!  */
            mask = 0xffff;
	}
        break;
    default:
        oappend (INTERNAL_DISASSEMBLER_ERROR);
        return;
    }
    disp = (start_pc + codep - start_codep + disp) & mask;
    set_op (disp, 0);
    print_operand_value (scratchbuf, 1, disp);
    oappend (scratchbuf);
}

void OP_SEG (dummy, sizeflag)
     int dummy ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    oappend (names_seg[reg]);
}

void OP_DIR (dummy, sizeflag)
     int dummy ATTRIBUTE_UNUSED;
     int sizeflag;
{
    int seg, offset;
    
    if (sizeflag & DFLAG){
        offset = get32();
        seg = get16();
    }
    else {
        offset = get16();
        seg = get16();
    }
    used_prefixes |= (prefixes & PREFIX_DATA);
    if (intel_syntax)
        sprintf (scratchbuf, "0x%x,0x%x", seg, offset);
    else
        sprintf (scratchbuf, "$0x%x,$0x%x", seg, offset);
    oappend (scratchbuf);
}

 void OP_OFF (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag;
{
    uint64_t off;

    append_seg();

    if ((sizeflag & AFLAG) || mode_64bit)
        off = get32();
    else
        off = get16();
    
    if (intel_syntax){
        if (!(prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS | PREFIX_ES | PREFIX_FS | PREFIX_GS))){
            oappend (names_seg[ds_reg - es_reg]);
            oappend (":");
	}
    }
    print_operand_value (scratchbuf, 1, off);
    oappend (scratchbuf);
}

void OP_OFF64 (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    uint64_t off;
    
    if (!mode_64bit){
        OP_OFF (bytemode, sizeflag);
        return;
    }

    append_seg();
    
    off = get64();
    
    if (intel_syntax){
        if (!(prefixes & (PREFIX_CS | PREFIX_SS | PREFIX_DS | PREFIX_ES | PREFIX_FS | PREFIX_GS))){
            oappend (names_seg[ds_reg - es_reg]);
            oappend (":");
	}
    }
    print_operand_value (scratchbuf, 1, off);
    oappend (scratchbuf);
}

void ptr_reg (int code, int sizeflag)
{
    const char *s;
    if (intel_syntax)
        oappend ("[");
    else
        oappend ("(");
    
    USED_REX (REX_MODE64);
    if (rex & REX_MODE64){
        if (!(sizeflag & AFLAG))
            s = names32[code - eAX_reg];
        else
            s = names64[code - eAX_reg];
    }
    else if (sizeflag & AFLAG)
        s = names32[code - eAX_reg];
    else
        s = names16[code - eAX_reg];
    oappend (s);
    if (intel_syntax)
        oappend ("]");
    else
        oappend (")");
}

void OP_ESreg (int code, int sizeflag)
{
    oappend ("%es:" + intel_syntax);
    ptr_reg (code, sizeflag);
}

void OP_DSreg (int code, int sizeflag)
{
    if ((prefixes
         & (PREFIX_CS
            | PREFIX_DS
            | PREFIX_SS
            | PREFIX_ES
            | PREFIX_FS
            | PREFIX_GS)) == 0)
        prefixes |= PREFIX_DS;
    append_seg();
    ptr_reg (code, sizeflag);
}

 void OP_C (dummy, sizeflag)
     int dummy ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    int add = 0;
    USED_REX (REX_EXTX);
    if (rex & REX_EXTX)
        add = 8;
    sprintf (scratchbuf, "%%cr%d", reg + add);
    oappend (scratchbuf + intel_syntax);
}

 void OP_D (dummy, sizeflag)
     int dummy ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    int add = 0;
    USED_REX (REX_EXTX);
    if (rex & REX_EXTX)
        add = 8;
    if (intel_syntax)
        sprintf (scratchbuf, "db%d", reg + add);
    else
        sprintf (scratchbuf, "%%db%d", reg + add);
    oappend (scratchbuf);
}

void OP_T (dummy, sizeflag)
     int dummy ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    sprintf (scratchbuf, "%%tr%d", reg);
    oappend (scratchbuf + intel_syntax);
}

void OP_Rd (int bytemode, int sizeflag)
{
    if (mod == 3)
        OP_E (bytemode, sizeflag);
    else
        BadOp();
}

void OP_MMX (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    int add = 0;
    USED_REX (REX_EXTX);
    if (rex & REX_EXTX)
        add = 8;
    used_prefixes |= (prefixes & PREFIX_DATA);
    if (prefixes & PREFIX_DATA)
        sprintf (scratchbuf, "%%xmm%d", reg + add);
    else
        sprintf (scratchbuf, "%%mm%d", reg + add);
    oappend (scratchbuf + intel_syntax);
}

void OP_XMM (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    int add = 0;
    USED_REX (REX_EXTX);
    if (rex & REX_EXTX)
        add = 8;
    sprintf (scratchbuf, "%%xmm%d", reg + add);
    oappend (scratchbuf + intel_syntax);
}

void OP_EM (int bytemode, int sizeflag)
{
    int add = 0;
    if (mod != 3){
        OP_E (bytemode, sizeflag);
        return;
    }
    USED_REX (REX_EXTZ);
    if (rex & REX_EXTZ)
        add = 8;
    
    /* Skip mod/rm byte.  */
    MODRM_CHECK;
    codep++;
    used_prefixes |= (prefixes & PREFIX_DATA);
    if (prefixes & PREFIX_DATA)
        sprintf (scratchbuf, "%%xmm%d", rm + add);
    else
        sprintf (scratchbuf, "%%mm%d", rm + add);
    oappend (scratchbuf + intel_syntax);
}

void OP_EX (int bytemode, int sizeflag)
{
    int add = 0;
    if (mod != 3){
        OP_E (bytemode, sizeflag);
        return;
    }
    USED_REX (REX_EXTZ);
    if (rex & REX_EXTZ)
        add = 8;
    
    /* Skip mod/rm byte.  */
    MODRM_CHECK;
    codep++;
    sprintf (scratchbuf, "%%xmm%d", rm + add);
    oappend (scratchbuf + intel_syntax);
}

void OP_MS (int bytemode, int sizeflag)
{
    if (mod == 3)
        OP_EM (bytemode, sizeflag);
    else
        BadOp();
}

void OP_XS (int bytemode, int sizeflag)
{
    if (mod == 3)
        OP_EX (bytemode, sizeflag);
    else
        BadOp();
}

void OP_3DNowSuffix (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    const char *mnemonic;

    FETCH_DATA(the_info, codep + 1);
    /* AMD 3DNow! instructions are specified by an opcode suffix in the
       place where an 8-bit immediate would normally go.  ie. the last
       byte of the instruction.  */
    obufp = obuf + strlen (obuf);
    mnemonic = Suffix3DNow[*codep++ & 0xff];
    if (mnemonic)
        oappend (mnemonic);
    else {
        /* Since a variable sized modrm/sib chunk is between the start
           of the opcode (0x0f0f) and the opcode suffix, we need to do
           all the modrm processing first, and don't know until now that
           we have a bad opcode.  This necessitates some cleaning up.  */
        op1out[0] = '\0';
        op2out[0] = '\0';
        BadOp();
    }
}

void OP_SIMD_Suffix (bytemode, sizeflag)
     int bytemode ATTRIBUTE_UNUSED;
     int sizeflag ATTRIBUTE_UNUSED;
{
    unsigned int cmp_type;

    FETCH_DATA(the_info, codep + 1);
    obufp = obuf + strlen (obuf);
    cmp_type = *codep++ & 0xff;
    if (cmp_type < 8) {
        char suffix1 = 'p', suffix2 = 's';
        used_prefixes |= (prefixes & PREFIX_REPZ);
        if (prefixes & PREFIX_REPZ)
            suffix1 = 's';
        else {
            used_prefixes |= (prefixes & PREFIX_DATA);
            if (prefixes & PREFIX_DATA)
                suffix2 = 'd';
            else {
                used_prefixes |= (prefixes & PREFIX_REPNZ);
                if (prefixes & PREFIX_REPNZ)
                    suffix1 = 's', suffix2 = 'd';
	    }
	}
        sprintf (scratchbuf, "cmp%s%c%c",
                 simd_cmp_op[cmp_type], suffix1, suffix2);
        used_prefixes |= (prefixes & PREFIX_REPZ);
        oappend (scratchbuf);
    }
    else {
        /* We have a bad extension byte.  Clean up.  */
        op1out[0] = '\0';
        op2out[0] = '\0';
        BadOp();
    }
}

void SIMD_Fixup (extrachar, sizeflag)
     int extrachar;
     int sizeflag ATTRIBUTE_UNUSED;
{
    /* Change movlps/movhps to movhlps/movlhps for 2 register operand
       forms of these instructions.  */
    if (mod == 3){
        char *p = obuf + strlen (obuf);
        *(p + 1) = '\0';
        *p       = *(p - 1);
        *(p - 1) = *(p - 2);
        *(p - 2) = *(p - 3);
        *(p - 3) = extrachar;
    }
}

void BadOp (void)
{
    /* Throw away prefixes and 1st. opcode byte.  */
    codep = insn_codep + 1;
    oappend ("(bad)");
}
