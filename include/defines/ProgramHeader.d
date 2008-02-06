/*
##################################################################
ELF32		ELF64
Offset	Length	Offset	Length	Name		Description
0	4	0	4	p_type		what kind of segment this is
4	4	4	4	p_offset	file offset from beginning of file to this segment
8	4	8	8	p_vaddr		vitual address of the first byte of segment in mem
12	4	16	8	p_paddr		segment's physical address (unspecified for executables and shared objs)
16	4	24	8	p_filesz	number of bytes in the file image of the segment
20	4	32	8	p_memsz		number of bytes in the memory image of the segment
24	4	40	8	p_flags		flags for this segment
28	4	48	8	p_align		segment alignment in memory
##################################################################
*/
#define PROGRAMHEADER_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS(Elf64_Word, p_type); \
    GET_FIELD_BASIS(Elf64_Word, p_offset); \
    GET_FIELD_BASIS(Elf64_Off,  p_vaddr); \
    GET_FIELD_BASIS(Elf64_Addr, p_paddr); \
    GET_FIELD_BASIS(Elf64_Addr, p_filesz); \
    GET_FIELD_BASIS(Elf64_Xword,p_memsz); \
    GET_FIELD_BASIS(Elf64_Xword,p_flags); \
    GET_FIELD_BASIS(Elf64_Xword,p_align);

#define PROGRAMHEADER_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(Elf64_Word, p_type); \
    GET_FIELD_CLASS(Elf64_Word, p_offset); \
    GET_FIELD_CLASS(Elf64_Off,  p_vaddr); \
    GET_FIELD_CLASS(Elf64_Addr, p_paddr); \
    GET_FIELD_CLASS(Elf64_Addr, p_filesz); \
    GET_FIELD_CLASS(Elf64_Xword,p_memsz); \
    GET_FIELD_CLASS(Elf64_Xword,p_flags); \
    GET_FIELD_CLASS(Elf64_Xword,p_align);
