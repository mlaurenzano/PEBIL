/*
##################################################################
ELF32		Elf64
Offset	Length	Offset	Length	Name		Description
0	4	0	4	st_name		symbols name (index into object file's symbol string table)
4	4	8	8	st_value	the value of the symbol
8	4	16	8	st_size		the size of the object associated with the symbol
12	1	4	1	st_info		symbol type and binding attributes
13	1	5	1	st_other	holds 0 and has no defined meaning
14	2	6	2	st_shndx	the section header table index of the section this symbol is related to
##################################################################
*/

#define SYMBOL_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS(uint32_t,st_name); \
    GET_FIELD_BASIS(unsigned char,st_info); \
    GET_FIELD_BASIS(unsigned char,st_other); \
    GET_FIELD_BASIS(uint16_t,st_shndx); \
    GET_FIELD_BASIS(uint64_t,st_value); \
    GET_FIELD_BASIS(uint64_t,st_size); \
        \
    SET_FIELD_BASIS(uint32_t,st_name); \
    SET_FIELD_BASIS(unsigned char,st_info); \
    SET_FIELD_BASIS(unsigned char,st_other); \
    SET_FIELD_BASIS(uint16_t,st_shndx); \
    SET_FIELD_BASIS(uint64_t,st_value); \
    SET_FIELD_BASIS(uint64_t,st_size); \
        \
    INCREMENT_FIELD_BASIS(uint32_t,st_name); \
    INCREMENT_FIELD_BASIS(unsigned char,st_info); \
    INCREMENT_FIELD_BASIS(unsigned char,st_other); \
    INCREMENT_FIELD_BASIS(uint16_t,st_shndx); \
    INCREMENT_FIELD_BASIS(uint64_t,st_value); \
    INCREMENT_FIELD_BASIS(uint64_t,st_size);

#define SYMBOL_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(uint32_t,st_name); \
    GET_FIELD_CLASS(unsigned char,st_info); \
    GET_FIELD_CLASS(unsigned char,st_other); \
    GET_FIELD_CLASS(uint16_t,st_shndx); \
    GET_FIELD_CLASS(uint64_t,st_value); \
    GET_FIELD_CLASS(uint64_t,st_size); \
        \
    SET_FIELD_CLASS(uint32_t,st_name); \
    SET_FIELD_CLASS(unsigned char,st_info); \
    SET_FIELD_CLASS(unsigned char,st_other); \
    SET_FIELD_CLASS(uint16_t,st_shndx); \
    SET_FIELD_CLASS(uint64_t,st_value); \
    SET_FIELD_CLASS(uint64_t,st_size); \
        \
    INCREMENT_FIELD_CLASS(uint32_t,st_name); \
    INCREMENT_FIELD_CLASS(unsigned char,st_info); \
    INCREMENT_FIELD_CLASS(unsigned char,st_other); \
    INCREMENT_FIELD_CLASS(uint16_t,st_shndx); \
    INCREMENT_FIELD_CLASS(uint64_t,st_value); \
    INCREMENT_FIELD_CLASS(uint64_t,st_size);
