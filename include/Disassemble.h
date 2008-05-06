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

#ifndef DIS_ASM_H
#define DIS_ASM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <ansidecl.h>
#include <stdint.h>

typedef int (*fprintf_ftype) PARAMS((PTR, const char*, ...));

enum dis_insn_type {
  dis_noninsn,			/* Not a valid instruction */
  dis_nonbranch,		/* Not a branch instruction */
  dis_branch,			/* Unconditional branch */
  dis_condbranch,		/* Conditional branch */
  dis_jsr,			/* Jump to subroutine */
  dis_condjsr,			/* Conditional jump to subroutine */
  dis_dref,			/* Data reference instruction */
  dis_dref2			/* Two data references in instruction */
};


    //*********************************************************
    //        extracted from bfd.h
    // ********************************************************

    // 64 bit
#define sprintf_vma(s,x) sprintf (s, "%016lx", x)
#define fprintf_vma(f,x) fprintf (f, "%016lx", x)

    /*
#define _bfd_int64_low(x) ((unsigned long) (((x) & 0xffffffff)))
#define _bfd_int64_high(x) ((unsigned long) (((x) >> 32) & 0xffffffff))
#define fprintf_vma(s,x) \
  fprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
#define sprintf_vma(s,x) \
  sprintf ((s), "%08lx%08lx", _bfd_int64_high (x), _bfd_int64_low (x))
    */

enum binary_flavour
{
    binary_target_unknown_flavour,
    binary_target_aout_flavour,
    binary_target_coff_flavour,
    binary_target_ecoff_flavour,
    binary_target_xcoff_flavour,
    binary_target_elf_flavour,
    binary_target_ieee_flavour,
    binary_target_nlm_flavour,
    binary_target_oasys_flavour,
    binary_target_tekhex_flavour,
    binary_target_srec_flavour,
    binary_target_ihex_flavour,
    binary_target_som_flavour,
    binary_target_os9k_flavour,
    binary_target_versados_flavour,
    binary_target_msdos_flavour,
    binary_target_ovax_flavour,
    binary_target_evax_flavour,
    binary_target_mmo_flavour
};


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



/* This struct is passed into the instruction decoding routine, 
   and is passed back out into each callback.  The various fields are used
   for conveying information from your main routine into your callbacks,
   for passing information into the instruction decoders (such as the
   addresses of the callback functions), or for passing information
   back from the instruction decoders to their callers.

   It must be initialized before it is first passed; this can be done
   by hand, or using one of the initialization macros below.  */

typedef struct disassemble_info {
  fprintf_ftype fprintf_func;
  PTR stream;
  PTR application_data;

  /* Target description.  We could replace this with a pointer to the bfd,
     but that would require one.  There currently isn't any such requirement
     so to avoid introducing one we record these explicitly.  */
  /* The binary_flavour.  This can be binary_target_unknown_flavour.  */
  enum binary_flavour flavour;
  /* The mach_arch value.  */
  enum mach_architecture arch;
  /* The mach_mach value.  */
  unsigned long mach;
  /* Endianness (for bi-endian cpus).  Mono-endian cpus can ignore this.  */
  enum byte_endian endian;
  /* An arch/mach-specific bitmask of selected instruction subsets, mainly
     for processors with run-time-switchable instruction sets.  The default,
     zero, means that there is no constraint.  CGEN-based opcodes ports
     may use ISA_foo masks.  */
  unsigned long insn_sets;

  /* For use by the disassembler.
     The top 16 bits are reserved for public use (and are documented here).
     The bottom 16 bits are for the internal use of the disassembler.  */
  unsigned long flags;
#define INSN_HAS_RELOC	0x80000000
  PTR private_data;

  /* Function used to get bytes to disassemble.  MEMADDR is the
     address of the stuff to be disassembled, MYADDR is the address to
     put the bytes in, and LENGTH is the number of bytes to read.
     INFO is a pointer to this struct.
     Returns an errno value or 0 for success.  */
  int (*read_memory_func)
    PARAMS ((uint64_t memaddr, uint8_t *myaddr, unsigned int length,
	     struct disassemble_info *info));

  /* Function which should be called if we get an error that we can't
     recover from.  STATUS is the errno value from read_memory_func and
     MEMADDR is the address that we were trying to read.  INFO is a
     pointer to this struct.  */
  void (*memory_error_func)
    PARAMS ((int status, uint64_t memaddr, struct disassemble_info *info));

  /* Function called to print ADDR.  */
  void (*print_address_func)
    PARAMS ((uint64_t addr, struct disassemble_info *info));

  /* Function called to determine if there is a symbol at the given ADDR.
     If there is, the function returns 1, otherwise it returns 0.
     This is used by ports which support an overlay manager where
     the overlay number is held in the top part of an address.  In
     some circumstances we want to include the overlay number in the
     address, (normally because there is a symbol associated with
     that address), but sometimes we want to mask out the overlay bits.  */
  int (* symbol_at_address_func)
    PARAMS ((uint64_t addr, struct disassemble_info * info));

  /* These are for buffer_read_memory.  */
  uint8_t *buffer;
  uint64_t buffer_vma;
  unsigned int buffer_length;

  /* This variable may be set by the instruction decoder.  It suggests
      the number of bytes objdump should display on a single line.  If
      the instruction decoder sets this, it should always set it to
      the same value in order to get reasonable looking output.  */
  int bytes_per_line;

  /* the next two variables control the way objdump displays the raw data */
  /* For example, if bytes_per_line is 8 and bytes_per_chunk is 4, the */
  /* output will look like this:
     00:   00000000 00000000
     with the chunks displayed according to "display_endian". */
  int bytes_per_chunk;
  enum byte_endian display_endian;

  /* Number of octets per incremented target address 
     Normally one, but some DSPs have byte sizes of 16 or 32 bits.  */
  unsigned int octets_per_byte;

  /* Results from instruction decoders.  Not all decoders yet support
     this information.  This info is set each time an instruction is
     decoded, and is only valid for the last such instruction.

     To determine whether this decoder supports this information, set
     insn_info_valid to 0, decode an instruction, then check it.  */

  char insn_info_valid;		/* Branch info has been set. */
  char branch_delay_insns;	/* How many sequential insn's will run before
				   a branch takes effect.  (0 = normal) */
  char data_size;		/* Size of data reference in insn, in bytes */
  enum dis_insn_type insn_type;	/* Type of instruction */
  uint64_t target;		/* Target address of branch or dref, if known;
				   zero if unknown.  */
  uint64_t target2;		/* Second target address for dref2 */

  /* Command line options specific to the target disassembler.  */
  char * disassembler_options;

} disassemble_info;


/* Standard disassemblers.  Disassemble one instruction at the given
   target address.  Return number of octets processed.  */
typedef int (*disassembler_ftype)
     PARAMS((uint64_t, disassemble_info *));

extern int print_insn_i386		PARAMS ((uint64_t, disassemble_info *));
extern int print_insn_i386_att		PARAMS ((uint64_t, disassemble_info*));
extern int print_insn_i386_intel	PARAMS ((uint64_t, disassemble_info*));


/* Document any target specific options available from the disassembler.  */
extern void disassembler_usage          PARAMS ((FILE *));


/* This block of definitions is for particular callers who read instructions
   into a buffer before calling the instruction decoder.  */

/* Here is a function which callers may wish to use for read_memory_func.
   It gets bytes from a buffer.  */
extern int buffer_read_memory
  PARAMS ((uint64_t, uint8_t *, unsigned int, struct disassemble_info *));

/* This function goes with buffer_read_memory.
   It prints a message using info->fprintf_func and info->stream.  */
extern void perror_memory PARAMS ((int, uint64_t, struct disassemble_info *));


/* Just print the address in hex.  This is included for completeness even
   though both GDB and objdump provide their own (to print symbolic
   addresses).  */
extern void generic_print_address
  PARAMS ((uint64_t, struct disassemble_info *));

/* Always true.  */
extern int generic_symbol_at_address
  PARAMS ((uint64_t, struct disassemble_info *));

/* Macro to initialize a disassemble_info struct.  This should be called
   by all applications creating such a struct.  */
#define INIT_DISASSEMBLE_INFO(INFO, STREAM, FPRINTF_FUNC) \
  (INFO).flavour = binary_target_unknown_flavour, \
  (INFO).arch = mach_arch_unknown, \
  (INFO).mach = 0, \
  (INFO).insn_sets = 0, \
  (INFO).endian = BYTE_ENDIAN_UNKNOWN, \
  (INFO).octets_per_byte = 1, \
  INIT_DISASSEMBLE_INFO_NO_ARCH(INFO, STREAM, FPRINTF_FUNC)

/* Call this macro to initialize only the internal variables for the
   disassembler.  Architecture dependent things such as byte order, or machine
   variant are not touched by this macro.  This makes things much easier for
   GDB which must initialize these things separately.  */

#define INIT_DISASSEMBLE_INFO_NO_ARCH(INFO, STREAM, FPRINTF_FUNC) \
  (INFO).fprintf_func = (fprintf_ftype)(FPRINTF_FUNC), \
  (INFO).stream = (PTR)(STREAM), \
  (INFO).private_data = NULL, \
  (INFO).buffer = NULL, \
  (INFO).buffer_vma = 0, \
  (INFO).buffer_length = 0, \
  (INFO).read_memory_func = buffer_read_memory, \
  (INFO).memory_error_func = perror_memory, \
  (INFO).print_address_func = generic_print_address, \
  (INFO).symbol_at_address_func = generic_symbol_at_address, \
  (INFO).flags = 0, \
  (INFO).bytes_per_line = 0, \
  (INFO).bytes_per_chunk = 0, \
  (INFO).display_endian = BYTE_ENDIAN_UNKNOWN, \
  (INFO).disassembler_options = NULL, \
  (INFO).insn_info_valid = 0

#ifdef __cplusplus
};
#endif

#endif /* ! defined (DIS_ASM_H) */
