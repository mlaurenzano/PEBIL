#define RELOCATION_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(uint64_t,r_offset); \
        GET_FIELD_BASIS(uint64_t,r_info);
/** END of definitions **/

#define RELOCATIONADDEND_MACROS_BASIS(__str) /** __str **/ \
        GET_FIELD_BASIS(int64_t,r_addend);

#define RELOCATION_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(uint64_t,r_offset); \
        GET_FIELD_CLASS(uint64_t,r_info);
/** END of definitions **/

#define RELOCATIONADDEND_MACROS_CLASS(__str) /** __str **/ \
        GET_FIELD_CLASS(int64_t,r_addend);
