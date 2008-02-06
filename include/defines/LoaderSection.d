/*
##################################################################
Table 15. Loader Section Header Structure (Defined in loader.h) XCOFF32     XCOFF64     Name     Description
Offset     Length     Offset     Length
0     4     0     4     l_version     Loader section version number
4     4     4     4     l_nsyms     Number of symbol table entries
8     4     8     4     l_nreloc     Number of relocation table entries
12     4     12     4     l_istlen     Length of import file ID string table
16     4     16     4     l_nimpid     Number of import file IDs
20     4     24     8     l_impoff+     Offset to start of import file IDs
24     4     20     4     l_stlen+     Length of string table
28     4     32     8     l_stoff+     Offset to start of string table
N/A     40     8     l_symoff     Offset to start of symbol table
N/A     48     8     l_rldoff     Offset to start of relocation entries
+Use "32" or "64" suffix when __XCOFF_HYBRID__ is defined.
##################################################################
Table 16. Loader Section Symbol Table Entry Structure XCOFF32     XCOFF64     Name     Description
Offset     Length     Offset     Length
0     8     0    0     l_name+     Symbol name or byte offset into string table
0     4     0    0     l_zeroes+     Zero indicates symbol name is referenced from l_offset
4     4     8     4     l_offset+     Byte offset into string table of symbol name
8     4     0     8     l_value+     Address field
12     2     12     2     l_scnum     Section number containing symbol
14     1     14     1     l_smtype     Symbol type, export, import flags
15     1     15     1     l_smclas     Symbol storage class
16     4     16     4     l_ifile     Import file ID; ordinal of import file IDs
20     4     20     4     l_parm     Parameter type-check field
+Use "32" or "64" suffix when __XCOFF_HYBRID__ is defined.
##################################################################
Table 17. Loader Section Relocation Table Entry Structure XCOFF32     XCOFF64     Name     Description
Offset     Length     Offset     Length
0     4     0     8     l_vaddr+     Address field
4     4     12     4     l_symndx+     Loader section symbol table index of referenced item
8     2     8     2     l_rtype     Relocation type
10     2     10     2     l_rsecnm     File section number being relocated
+Use "32" or "64" suffix when __XCOFF_HYBRID__ is defined.
##################################################################
*/

#define LOADERHEADER_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint32_t,l_version); \
        GET_FIELD_BASIS(uint32_t,l_nsyms); \
        GET_FIELD_BASIS(uint32_t,l_nreloc); \
        GET_FIELD_BASIS(uint32_t,l_istlen); \
        GET_FIELD_BASIS(uint32_t,l_nimpid); \
        GET_FIELD_BASIS(uint64_t,l_impoff); \
        GET_FIELD_BASIS(uint32_t,l_stlen); \
        GET_FIELD_BASIS(uint64_t,l_stoff); \
        GET_FIELD_BASIS(uint64_t,l_symoff); \
        GET_FIELD_BASIS(uint64_t,l_rldoff); \
/** END of definitions **/

#define LOADERHEADER_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint32_t,l_version); \
        GET_FIELD_CLASS(uint32_t,l_nsyms); \
        GET_FIELD_CLASS(uint32_t,l_nreloc); \
        GET_FIELD_CLASS(uint32_t,l_istlen); \
        GET_FIELD_CLASS(uint32_t,l_nimpid); \
        GET_FIELD_CLASS(uint64_t,l_impoff); \
        GET_FIELD_CLASS(uint32_t,l_stlen); \
        GET_FIELD_CLASS(uint64_t,l_stoff); \
/** END of definitions **/

#define LOADERSYMBOL_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(char*,l_name); \
        GET_FIELD_BASIS(uint32_t,l_zeroes); \
        GET_FIELD_BASIS(uint32_t,l_offset); \
        GET_FIELD_BASIS(uint64_t,l_value); \
        GET_FIELD_BASIS(uint16_t,l_scnum); \
        GET_FIELD_BASIS(uint8_t,l_smtype); \
        GET_FIELD_BASIS(uint8_t,l_smclas); \
        GET_FIELD_BASIS(uint32_t,l_ifile); \
        GET_FIELD_BASIS(uint32_t,l_parm); \
/** END of definitions **/

#define LOADERSYMBOL_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint32_t,l_offset); \
        GET_FIELD_CLASS(uint64_t,l_value); \
        GET_FIELD_CLASS(uint16_t,l_scnum); \
        GET_FIELD_CLASS(uint8_t,l_smtype); \
        GET_FIELD_CLASS(uint8_t,l_smclas); \
        GET_FIELD_CLASS(uint32_t,l_ifile); \
        GET_FIELD_CLASS(uint32_t,l_parm); \
/** END of definitions **/

#define LOADERRELOC_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint64_t,l_vaddr); \
        GET_FIELD_BASIS(uint32_t,l_symndx); \
        GET_FIELD_BASIS(uint16_t,l_rtype); \
        GET_FIELD_BASIS(uint16_t,l_rsecnm); \
/** END of definitions **/

#define LOADERRELOC_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint64_t,l_vaddr); \
        GET_FIELD_CLASS(uint32_t,l_symndx); \
        GET_FIELD_CLASS(uint16_t,l_rtype); \
        GET_FIELD_CLASS(uint16_t,l_rsecnm); \
/** END of definitions **/

