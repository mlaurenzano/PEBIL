/*
##################################################################
File Name Auxiliary Entry Format
0     14     0    0    x_fname     Source file string
0     4     0    0    x_zeroes     Zero, indicating file string in string table (overlays first 4 bytes of x_fname)
4     4     0    0    x_offset     Offset of file string in string table (overlays 5th-8th bytes of x_fname)
14     1     0    0    x_ftype     File string type
15     2     0    0    Reserved     Must contain 0.
17     1     0    0    x_auxtype     Auxiliary symbol type(XCOFF64 only)
##################################################################
csect Auxiliary Entry Format
0     4     0    0     x_scnlen     (See field definition section)
0    0     0     4     x_scnlen_lo     (See field definition section) Low 4 bytes of section length
4     4     4     4     x_parmhash     Offset of parameter type-check hash in .typchk section
8     2     8     2     x_snhash     .typchk section number
10     1     10     1     x_smtyp     Symbol alignment and type 3-bit symbol alignment (log 2) 3-bit symbol type
11     1     11     1     x_smclas     Storage mapping class
12     4     0    0     x_stab     Reserved
16     2     0    0     x_snstab     Reserved
0    0     12     4     x_scnlen_hi     (See field definition section) High 4 bytes of section length
0    0     16     1     (pad)     Reserved
0    0     17     1     x_auxtype     Contains _AUX_CSECT; indicates type of auxiliary entry
##################################################################
Function Auxiliary Entry Format XCOFF32     XCOFF64     Name     Description
0     4     0    0    x_exptr     File offset to exception table entry.
4     4     8     4     x_fsize     Size of function in bytes
8     4     0     8     x_lnnoptr     File pointer to line number
12     4     12     4     x_endndx     Symbol table index of next entry beyond this function
16     1     16     1     (pad)     Unused
0    0    17     1     x_auxtype     Contains _AUX_FCN; Type of auxiliary entry
##################################################################
Block Auxiliary Entry Format (XCOFF64 only)
0     4     0    0    (no name)     Reserved
4     2     0     4     x_lnno     Source line number
6     12     4     13     (no name)     Reserved
0    0    17     1     x_auxtype     Contains _AUX_SYM; Type of auxiliary entry
##################################################################
Section Auxiliary Entry Format (XCOFF32 Only) Offset     Length in Bytes     Name     Description
0     4     0    0    x_scnlen     Section length
4     2     0    0    x_nreloc     Number of relocation entries
6     2     0    0    x_nlinno     Number of line numbers
8     10     0    0    (no name)     Reserved
##################################################################
Exception Auxiliary Entry Format (XCOFF64 only)
0     8     0    0    x_exptr     File offset to exception table entry.
8     4     0    0    x_fsize     Size of function in bytes
12     4     0    0    x_endndx     Symbol table index of next entry beyond this function
16     1     0    0    (pad)     Unused
17     1     0    0    x_auxtype     Contains _AUX_EXCEPT; Type of auxiliary entry
##################################################################
*/

#define AUXILARY_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS_A(char*,x_fname,x_file); \
    GET_FIELD_BASIS_A(uint32_t,x_zeroes,x_file); \
    GET_FIELD_BASIS_A(uint32_t,x_offset,x_file); \
    GET_FIELD_BASIS_A(uint8_t,x_ftype,x_file); \
    \
    GET_FIELD_BASIS_A(uint32_t,x_scnlen,x_csect); \
    GET_FIELD_BASIS_A(uint32_t,x_scnlen_lo,x_csect); \
    GET_FIELD_BASIS_A(uint32_t,x_parmhash,x_csect); \
    GET_FIELD_BASIS_A(uint16_t,x_snhash,x_csect); \
    GET_FIELD_BASIS_A(uint8_t,x_smtyp,x_csect); \
    GET_FIELD_BASIS_A(uint8_t,x_smclas,x_csect); \
    GET_FIELD_BASIS_A(uint32_t,x_stab,x_csect); \
    GET_FIELD_BASIS_A(uint16_t,x_snstab,x_csect); \
    GET_FIELD_BASIS_A(uint32_t,x_scnlen_hi,x_csect); \
    \
    GET_FIELD_BASIS_A(uint32_t,x_exptr,x_fcn); \
    GET_FIELD_BASIS_A(uint32_t,x_fsize,x_fcn); \
    GET_FIELD_BASIS_A(uint64_t,x_lnnoptr,x_fcn); \
    GET_FIELD_BASIS_A(uint32_t,x_endndx,x_fcn); \
    \
    GET_FIELD_BASIS_A(uint32_t,x_lnno,x_misc); \
    \
    GET_FIELD_BASIS_A(uint32_t,x_scnlen,x_scn); \
    GET_FIELD_BASIS_A(uint16_t,x_nreloc,x_scn); \
    GET_FIELD_BASIS_A(uint16_t,x_nlinno,x_scn); \
    \
    GET_FIELD_BASIS_A(uint64_t,x_exptr,x_except); \
    GET_FIELD_BASIS_A(uint32_t,x_fsize,x_except); \
    GET_FIELD_BASIS_A(uint32_t,x_endndx,x_except); \
/** END of definitions **/
