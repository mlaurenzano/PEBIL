/*
##################################################################
XCOFF32     XCOFF64     Name     Description
Offset     Length     Offset     Length
0     4     0     4     e_addr.e_symndx+     Symbol table index for function
4     1     8     1     e_lang+     Compiler language ID code
5     1     9     1     e_reason+     Value 0 (exception reason code 0)
Offset     Length     Offset     Length
0     4     0     8     e_addr.e_paddr+     Address of the trap instruction
4     1     8     1     e_lang+     Compiler language ID code
5     1     9     1     e_reason+     Trap exception reason code
+Use "32" or "64" suffix when __XCOFF_HYBRID__ is defined. With e_addr.e_paddr, the suffix is added to e_addr (i.e. e_addr32.e_paddr).
##################################################################
*/

#define SECTRAW_IS_OF_TYPE_DECL(__str) inline bool is ## __str() { return header->is ## __str(); }

#define SECTRAW_MACROS_BASIS(__str) /** __str **/ \
    SECTRAW_IS_OF_TYPE_DECL(TEXT); \
    SECTRAW_IS_OF_TYPE_DECL(DATA); \
    SECTRAW_IS_OF_TYPE_DECL(BSS); \
    SECTRAW_IS_OF_TYPE_DECL(PAD); \
    SECTRAW_IS_OF_TYPE_DECL(LOADER); \
    SECTRAW_IS_OF_TYPE_DECL(DEBUG); \
    SECTRAW_IS_OF_TYPE_DECL(TYPCHK); \
    SECTRAW_IS_OF_TYPE_DECL(EXCEPT); \
    SECTRAW_IS_OF_TYPE_DECL(OVRFLO); \
    SECTRAW_IS_OF_TYPE_DECL(INFO); \
/** END of definitions **/

#define EXCEPTIONRAW_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint8_t,e_lang); \
        GET_FIELD_BASIS(uint8_t,e_reason); \
        GET_FIELD_BASIS_U(uint32_t,e_symndx,e_addr); \
        GET_FIELD_BASIS_U(uint64_t,e_paddr,e_addr); \
/** END of definitions **/

#define EXCEPTIONRAW_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint8_t,e_lang); \
        GET_FIELD_CLASS(uint8_t,e_reason); \
        GET_FIELD_CLASS_U(uint32_t,e_symndx,e_addr); \
        GET_FIELD_CLASS_U(uint64_t,e_paddr,e_addr); \
/** END of definitions **/

