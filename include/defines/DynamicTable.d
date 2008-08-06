/*
##################################################################
ELF32		Elf64
Offset	Length	Offset	Length	Name		Description
0       4       0       8       d_tag           Dynamic Array Tags
--union--
4       4       8       8       d_val
4       4       8       8       d_ptr
--union--
##################################################################
*/

#define DYNAMIC_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS_A(uint64_t,d_val,d_un); \
        GET_FIELD_BASIS_A(uint64_t,d_ptr,d_un); \
        GET_FIELD_BASIS(int64_t,d_tag); \
        \
        SET_FIELD_BASIS_A(uint64_t,d_val,d_un); \
        SET_FIELD_BASIS_A(uint64_t,d_ptr,d_un); \
        SET_FIELD_BASIS(int64_t,d_tag); \
        \
        INCREMENT_FIELD_BASIS_A(uint64_t,d_val,d_un); \
        INCREMENT_FIELD_BASIS_A(uint64_t,d_ptr,d_un); \
        INCREMENT_FIELD_BASIS(int64_t,d_tag);
/** END of definitions **/

#define DYNAMIC_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS_A(uint64_t,d_val,d_un); \
        GET_FIELD_CLASS_A(uint64_t,d_ptr,d_un); \
        GET_FIELD_CLASS(int64_t,d_tag); \
        \
        SET_FIELD_CLASS_A(uint64_t,d_val,d_un); \
        SET_FIELD_CLASS_A(uint64_t,d_ptr,d_un); \
        SET_FIELD_CLASS(int64_t,d_tag); \
        \
        INCREMENT_FIELD_CLASS_A(uint64_t,d_val,d_un); \
        INCREMENT_FIELD_CLASS_A(uint64_t,d_ptr,d_un); \
        INCREMENT_FIELD_CLASS(int64_t,d_tag);
/** END of definitions **/

