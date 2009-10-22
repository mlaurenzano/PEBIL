/*
GNUVERNEED
##################################################################
ELF32		ELF64
Offset	Length	Offset	Length	Name		Description
0       2       0       2       vn_version      Version of structure
2       2       2       2       vn_cnt          Number of associated aux entries
4       4       4       4       vn_file         Offset of filename for this dependency
8       4       8       4       vn_aux          Offset in bytes to vernaux array
12      4       12      4       vn_next         Offset in bytes to next verneed entry
##################################################################
*/

#define GNUVERNEED_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint16_t,vn_version); \
        GET_FIELD_BASIS(uint16_t,vn_cnt); \
        GET_FIELD_BASIS(uint32_t,vn_file); \
        GET_FIELD_BASIS(uint32_t,vn_aux); \
        GET_FIELD_BASIS(uint32_t,vn_next); \
                \
        SET_FIELD_BASIS(uint16_t,vn_version); \
        SET_FIELD_BASIS(uint16_t,vn_cnt); \
        SET_FIELD_BASIS(uint32_t,vn_file); \
        SET_FIELD_BASIS(uint32_t,vn_aux); \
        SET_FIELD_BASIS(uint32_t,vn_next); \
                \
        INCREMENT_FIELD_BASIS(uint16_t,vn_version); \
        INCREMENT_FIELD_BASIS(uint16_t,vn_cnt); \
        INCREMENT_FIELD_BASIS(uint32_t,vn_file); \
        INCREMENT_FIELD_BASIS(uint32_t,vn_aux); \
        INCREMENT_FIELD_BASIS(uint32_t,vn_next);

#define GNUVERNEED_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint16_t,vn_version); \
        GET_FIELD_CLASS(uint16_t,vn_cnt); \
        GET_FIELD_CLASS(uint32_t,vn_file); \
        GET_FIELD_CLASS(uint32_t,vn_aux); \
        GET_FIELD_CLASS(uint32_t,vn_next); \
                \
        SET_FIELD_CLASS(uint16_t,vn_version); \
        SET_FIELD_CLASS(uint16_t,vn_cnt); \
        SET_FIELD_CLASS(uint32_t,vn_file); \
        SET_FIELD_CLASS(uint32_t,vn_aux); \
        SET_FIELD_CLASS(uint32_t,vn_next); \
                \
        INCREMENT_FIELD_CLASS(uint16_t,vn_version); \
        INCREMENT_FIELD_CLASS(uint16_t,vn_cnt); \
        INCREMENT_FIELD_CLASS(uint32_t,vn_file); \
        INCREMENT_FIELD_CLASS(uint32_t,vn_aux); \
        INCREMENT_FIELD_CLASS(uint32_t,vn_next);


/*
GNUVERNAUX
##################################################################
ELF32		ELF64
Offset	Length	Offset	Length	Name		Description
0       4       0       4       vna_hash        Hash value of dependency name
4       2       4       2       vna_flags       Dependency specific information
6       2       6       2       vna_other       Unused
8       4       8       4       vna_name        Dependency name string offset
12      4       12      4       vna_next        Offset in bytes to next vernaux entry
##################################################################
*/

#define GNUVERNAUX_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint32_t,vna_hash); \
        GET_FIELD_BASIS(uint16_t,vna_flags); \
        GET_FIELD_BASIS(uint16_t,vna_other); \
        GET_FIELD_BASIS(uint32_t,vna_name); \
        GET_FIELD_BASIS(uint32_t,vna_next); \
                \
        SET_FIELD_BASIS(uint32_t,vna_hash); \
        SET_FIELD_BASIS(uint16_t,vna_flags); \
        SET_FIELD_BASIS(uint16_t,vna_other); \
        SET_FIELD_BASIS(uint32_t,vna_name); \
        SET_FIELD_BASIS(uint32_t,vna_next); \
                \
        INCREMENT_FIELD_BASIS(uint32_t,vna_hash); \
        INCREMENT_FIELD_BASIS(uint16_t,vna_flags); \
        INCREMENT_FIELD_BASIS(uint16_t,vna_other); \
        INCREMENT_FIELD_BASIS(uint32_t,vna_name); \
        INCREMENT_FIELD_BASIS(uint32_t,vna_next);

#define GNUVERNAUX_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint32_t,vna_hash); \
        GET_FIELD_CLASS(uint16_t,vna_flags); \
        GET_FIELD_CLASS(uint16_t,vna_other); \
        GET_FIELD_CLASS(uint32_t,vna_name); \
        GET_FIELD_CLASS(uint32_t,vna_next); \
                \
        SET_FIELD_CLASS(uint32_t,vna_hash); \
        SET_FIELD_CLASS(uint16_t,vna_flags); \
        SET_FIELD_CLASS(uint16_t,vna_other); \
        SET_FIELD_CLASS(uint32_t,vna_name); \
        SET_FIELD_CLASS(uint32_t,vna_next); \
                \
        INCREMENT_FIELD_CLASS(uint32_t,vna_hash); \
        INCREMENT_FIELD_CLASS(uint16_t,vna_flags); \
        INCREMENT_FIELD_CLASS(uint16_t,vna_other); \
        INCREMENT_FIELD_CLASS(uint32_t,vna_name); \
        INCREMENT_FIELD_CLASS(uint32_t,vna_next);
