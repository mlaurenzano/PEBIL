/*
##################################################################
XCOFF32     XCOFF64     Name     Description
Offset     Length     Offset     Length
0     4     0     8     r_vaddr+     Virtual address (position) in section to be relocated
4     4     8     4     r_symndx+     Symbol table index of item that is referenced
8     1     12     1     r_rsize+     Relocation size and information
9     1     13     1     r_rtype+     Relocation type
+Use "32" or "64" suffix when __XCOFF_HYBRID__ is defined.
##################################################################
*/

#define RELOCATION_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(Elf64_Addr,r_offset); \
        GET_FIELD_BASIS(Elf64_Word,r_info);
/** END of definitions **/

#define RELOCATION_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(Elf64_Addr,r_offset); \
        GET_FIELD_CLASS(Elf64_Word,r_info);
/** END of definitions **/
