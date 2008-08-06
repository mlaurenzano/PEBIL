/*
##################################################################
ELF32		Elf64
Offset	Length	Offset	Length	Name		Description
0	16	0	16	e_ident		mark the file as an object file and provide machine-independent data to interpret the file's contents
16	2	16	2	e_type		object file type
18	2	18	2	e_machine	required architecture for the file
20	4	20	4	e_version	object file version
24	4	24	8	e_entry		virtual address where the system first transfers control
28	4	32	8	e_phoff		program header table's file offset in bytes
32	4	40	8	e_shoff		section header table's file offset in bytes
36	4	48	4	e_flags		processor-specific flags associated with the file
40	2	52	2	e_ehsize	elf header's size in bytes
42	2	54	2	e_phentsize	size of one entry in the file's program header table in bytes
44	2	56	2	e_phnum		number of entries in the file's program header table
46	2	58	2	e_shentsize	size of one entry in the file's section header table in bytes
48	2	60	2	e_shnum		number of entries in the file's section header table
50	2	62	2	e_shstrndx	the section header table idnex of the entry associated with the section name string table
##################################################################
*/

#define FILEHEADER_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS(unsigned char*,e_ident); \
    GET_FIELD_BASIS(uint16_t,e_type); \
    GET_FIELD_BASIS(uint16_t,e_machine); \
    GET_FIELD_BASIS(uint32_t,e_version); \
    GET_FIELD_BASIS(uint64_t,e_entry); \
    GET_FIELD_BASIS(uint64_t,e_phoff); \
    GET_FIELD_BASIS(uint64_t,e_shoff); \
    GET_FIELD_BASIS(uint32_t,e_flags); \
    GET_FIELD_BASIS(uint16_t,e_ehsize); \
    GET_FIELD_BASIS(uint16_t,e_phentsize); \
    GET_FIELD_BASIS(uint16_t,e_phnum); \
    GET_FIELD_BASIS(uint16_t,e_shentsize); \
    GET_FIELD_BASIS(uint16_t,e_shnum); \
    GET_FIELD_BASIS(uint16_t,e_shstrndx); \
        \
    SET_FIELD_BASIS(uint16_t,e_type); \
    SET_FIELD_BASIS(uint16_t,e_machine); \
    SET_FIELD_BASIS(uint32_t,e_version); \
    SET_FIELD_BASIS(uint64_t,e_entry); \
    SET_FIELD_BASIS(uint64_t,e_phoff); \
    SET_FIELD_BASIS(uint64_t,e_shoff); \
    SET_FIELD_BASIS(uint32_t,e_flags); \
    SET_FIELD_BASIS(uint16_t,e_ehsize); \
    SET_FIELD_BASIS(uint16_t,e_phentsize); \
    SET_FIELD_BASIS(uint16_t,e_phnum); \
    SET_FIELD_BASIS(uint16_t,e_shentsize); \
    SET_FIELD_BASIS(uint16_t,e_shnum); \
    SET_FIELD_BASIS(uint16_t,e_shstrndx); \
        \
    INCREMENT_FIELD_BASIS(uint16_t,e_type); \
    INCREMENT_FIELD_BASIS(uint16_t,e_machine); \
    INCREMENT_FIELD_BASIS(uint32_t,e_version); \
    INCREMENT_FIELD_BASIS(uint64_t,e_entry); \
    INCREMENT_FIELD_BASIS(uint64_t,e_phoff); \
    INCREMENT_FIELD_BASIS(uint64_t,e_shoff); \
    INCREMENT_FIELD_BASIS(uint32_t,e_flags); \
    INCREMENT_FIELD_BASIS(uint16_t,e_ehsize); \
    INCREMENT_FIELD_BASIS(uint16_t,e_phentsize); \
    INCREMENT_FIELD_BASIS(uint16_t,e_phnum); \
    INCREMENT_FIELD_BASIS(uint16_t,e_shentsize); \
    INCREMENT_FIELD_BASIS(uint16_t,e_shnum); \
    INCREMENT_FIELD_BASIS(uint16_t,e_shstrndx);

#define FILEHEADER_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(unsigned char*,e_ident); \
    GET_FIELD_CLASS(uint16_t,e_type); \
    GET_FIELD_CLASS(uint16_t,e_machine); \
    GET_FIELD_CLASS(uint32_t,e_version); \
    GET_FIELD_CLASS(uint64_t,e_entry); \
    GET_FIELD_CLASS(uint64_t,e_phoff); \
    GET_FIELD_CLASS(uint64_t,e_shoff); \
    GET_FIELD_CLASS(uint32_t,e_flags); \
    GET_FIELD_CLASS(uint16_t,e_ehsize); \
    GET_FIELD_CLASS(uint16_t,e_phentsize); \
    GET_FIELD_CLASS(uint16_t,e_phnum); \
    GET_FIELD_CLASS(uint16_t,e_shentsize); \
    GET_FIELD_CLASS(uint16_t,e_shnum); \
    GET_FIELD_CLASS(uint16_t,e_shstrndx); \
        \
    SET_FIELD_CLASS(uint16_t,e_type); \
    SET_FIELD_CLASS(uint16_t,e_machine); \
    SET_FIELD_CLASS(uint32_t,e_version); \
    SET_FIELD_CLASS(uint64_t,e_entry); \
    SET_FIELD_CLASS(uint64_t,e_phoff); \
    SET_FIELD_CLASS(uint64_t,e_shoff); \
    SET_FIELD_CLASS(uint32_t,e_flags); \
    SET_FIELD_CLASS(uint16_t,e_ehsize); \
    SET_FIELD_CLASS(uint16_t,e_phentsize); \
    SET_FIELD_CLASS(uint16_t,e_phnum); \
    SET_FIELD_CLASS(uint16_t,e_shentsize); \
    SET_FIELD_CLASS(uint16_t,e_shnum); \
    SET_FIELD_CLASS(uint16_t,e_shstrndx); \
        \
    INCREMENT_FIELD_CLASS(uint16_t,e_type); \
    INCREMENT_FIELD_CLASS(uint16_t,e_machine); \
    INCREMENT_FIELD_CLASS(uint32_t,e_version); \
    INCREMENT_FIELD_CLASS(uint64_t,e_entry); \
    INCREMENT_FIELD_CLASS(uint64_t,e_phoff); \
    INCREMENT_FIELD_CLASS(uint64_t,e_shoff); \
    INCREMENT_FIELD_CLASS(uint32_t,e_flags); \
    INCREMENT_FIELD_CLASS(uint16_t,e_ehsize); \
    INCREMENT_FIELD_CLASS(uint16_t,e_phentsize); \
    INCREMENT_FIELD_CLASS(uint16_t,e_phnum); \
    INCREMENT_FIELD_CLASS(uint16_t,e_shentsize); \
    INCREMENT_FIELD_CLASS(uint16_t,e_shnum); \
    INCREMENT_FIELD_CLASS(uint16_t,e_shstrndx);
