#ifndef _CStructures_H_
#define _CStructures_H_

#include <stdint.h>
#include <elf.h>

#define _S_(name) name ## _64

#define ISELFMAGIC(__a,__b,__c,__d) ((ELFMAG0 == (__a)) && (ELFMAG1 == (__b)) && (ELFMAG2 == (__c)) && (ELFMAG3 == (__d)))
#define ISELF32BIT(__a) (ELFCLASS32 == (__a))
#define ISELF64BIT(__a) (ELFCLASS64 == (__a))
#define ELFHDR_GETMAGIC (GET(e_ident)[EI_MAG0] << 24 | GET(e_ident)[EI_MAG1] << 16 | GET(e_ident)[EI_MAG2] << 8 | GET(e_ident)[EI_MAG3])

#define MAX_SHT_HASH_COUNT    1
#define MAX_SHT_DYNAMIC_COUNT 1
#define MAX_SHT_SYMTAB_COUNT  1
#define MAX_SHT_DYNSYM_COUNT  1

#endif // _CStructures_H_
