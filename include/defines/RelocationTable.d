/*
##################################################################
XCOFF32     XCOFF64     Name     Description
Offset     Length     Offset     Length
0     4     0     8   r_vaddr+     Virtual address (position) in section to be relocated
4     4     8     4   r_symndx+     Symbol table index of item that is referenced
8     1     12    1   r_rsize+     Relocation size and information
9     1     13    1   r_rtype+     Relocation type
##################################################################
*/

#define RELOCATION_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint64_t,r_offset); \
        GET_FIELD_BASIS(uint32_t,r_info);
/** END of definitions **/

#define RELOCATIONADDEND_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(int32_t,r_addend);

#define RELOCATION_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint64_t,r_offset); \
        GET_FIELD_CLASS(uint32_t,r_info);
/** END of definitions **/

#define RELOCATIONADDEND_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(int32_t,r_addend);
