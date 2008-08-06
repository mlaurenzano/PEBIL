#define RELOCATION_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS(uint64_t,r_offset); \
    GET_FIELD_BASIS(uint64_t,r_info); \
        \
    SET_FIELD_BASIS(uint64_t,r_offset); \
    SET_FIELD_BASIS(uint64_t,r_info); \
        \
    INCREMENT_FIELD_BASIS(uint64_t,r_offset); \
    INCREMENT_FIELD_BASIS(uint64_t,r_info);
/** END of definitions **/

#define RELOCATIONADDEND_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS(int64_t,r_addend); \
        \
    SET_FIELD_BASIS(int64_t,r_addend); \
        \
    INCREMENT_FIELD_BASIS(int64_t,r_addend);

#define RELOCATION_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(uint64_t,r_offset); \
    GET_FIELD_CLASS(uint64_t,r_info); \
        \
    SET_FIELD_CLASS(uint64_t,r_offset); \
    SET_FIELD_CLASS(uint64_t,r_info); \
        \
    INCREMENT_FIELD_CLASS(uint64_t,r_offset); \
    INCREMENT_FIELD_CLASS(uint64_t,r_info);
/** END of definitions **/

#define RELOCATIONADDEND_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(int64_t,r_addend); \
        \
    SET_FIELD_CLASS(int64_t,r_addend); \
        \
    INCREMENT_FIELD_CLASS(int64_t,r_addend);
