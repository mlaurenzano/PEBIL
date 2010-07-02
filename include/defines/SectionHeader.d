/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
##################################################################
ELF32		ELF64
Offset	Length	Offset	Length	Name		Description
0       4       0	4	sh_name         name of the section
4       4       4	4	sh_type         categorizes the section's contents and semantics
8       4       8	8	sh_flags        describe misc attributes
12      4       16	8	sh_addr         the address for this section's first byte to reside
16      4       24	8	sh_offset       byte offset from the beginning of the file to the first byte in the section
20      4       32	8	sh_size         section's size in bytes
24      4       40	4	sh_link         section header table index link
28      4       44	4	sh_info         extra information
32      4       48	8	sh_addralign    alignment restraint on this section
36      4       56	8	sh_entsize      size of extra table of fixed size entries (eg a symbol table)
##################################################################
*/

#define SECTIONHEADER_MACROS_BASIS(__str) /** __str **/ \
    GET_FIELD_BASIS(uint32_t,sh_name); \
    GET_FIELD_BASIS(uint32_t,sh_type); \
    GET_FIELD_BASIS(uint64_t,sh_flags); \
    GET_FIELD_BASIS(uint64_t,sh_addr); \
    GET_FIELD_BASIS(uint64_t,sh_offset); \
    GET_FIELD_BASIS(uint64_t,sh_size); \
    GET_FIELD_BASIS(uint32_t,sh_link); \
    GET_FIELD_BASIS(uint32_t,sh_info); \
    GET_FIELD_BASIS(uint64_t,sh_addralign); \
    GET_FIELD_BASIS(uint64_t,sh_entsize); \
        \
    SET_FIELD_BASIS(uint32_t,sh_name); \
    SET_FIELD_BASIS(uint32_t,sh_type); \
    SET_FIELD_BASIS(uint64_t,sh_flags); \
    SET_FIELD_BASIS(uint64_t,sh_addr); \
    SET_FIELD_BASIS(uint64_t,sh_offset); \
    SET_FIELD_BASIS(uint64_t,sh_size); \
    SET_FIELD_BASIS(uint32_t,sh_link); \
    SET_FIELD_BASIS(uint32_t,sh_info); \
    SET_FIELD_BASIS(uint64_t,sh_addralign); \
    SET_FIELD_BASIS(uint64_t,sh_entsize); \
        \
    INCREMENT_FIELD_BASIS(uint32_t,sh_name); \
    INCREMENT_FIELD_BASIS(uint32_t,sh_type); \
    INCREMENT_FIELD_BASIS(uint64_t,sh_flags); \
    INCREMENT_FIELD_BASIS(uint64_t,sh_addr); \
    INCREMENT_FIELD_BASIS(uint64_t,sh_offset); \
    INCREMENT_FIELD_BASIS(uint64_t,sh_size); \
    INCREMENT_FIELD_BASIS(uint32_t,sh_link); \
    INCREMENT_FIELD_BASIS(uint32_t,sh_info); \
    INCREMENT_FIELD_BASIS(uint64_t,sh_addralign); \
    INCREMENT_FIELD_BASIS(uint64_t,sh_entsize); 

#define SECTIONHEADER_MACROS_CLASS(__str) /** __str **/ \
    GET_FIELD_CLASS(uint32_t,sh_name); \
    GET_FIELD_CLASS(uint32_t,sh_type); \
    GET_FIELD_CLASS(uint64_t,sh_flags); \
    GET_FIELD_CLASS(uint64_t,sh_addr); \
    GET_FIELD_CLASS(uint64_t,sh_offset); \
    GET_FIELD_CLASS(uint64_t,sh_size); \
    GET_FIELD_CLASS(uint32_t,sh_link); \
    GET_FIELD_CLASS(uint32_t,sh_info); \
    GET_FIELD_CLASS(uint64_t,sh_addralign); \
    GET_FIELD_CLASS(uint64_t,sh_entsize);\
        \
    SET_FIELD_CLASS(uint32_t,sh_name); \
    SET_FIELD_CLASS(uint32_t,sh_type); \
    SET_FIELD_CLASS(uint64_t,sh_flags); \
    SET_FIELD_CLASS(uint64_t,sh_addr); \
    SET_FIELD_CLASS(uint64_t,sh_offset); \
    SET_FIELD_CLASS(uint64_t,sh_size); \
    SET_FIELD_CLASS(uint32_t,sh_link); \
    SET_FIELD_CLASS(uint32_t,sh_info); \
    SET_FIELD_CLASS(uint64_t,sh_addralign); \
    SET_FIELD_CLASS(uint64_t,sh_entsize); \
        \
    INCREMENT_FIELD_CLASS(uint32_t,sh_name); \
    INCREMENT_FIELD_CLASS(uint32_t,sh_type); \
    INCREMENT_FIELD_CLASS(uint64_t,sh_flags); \
    INCREMENT_FIELD_CLASS(uint64_t,sh_addr); \
    INCREMENT_FIELD_CLASS(uint64_t,sh_offset); \
    INCREMENT_FIELD_CLASS(uint64_t,sh_size); \
    INCREMENT_FIELD_CLASS(uint32_t,sh_link); \
    INCREMENT_FIELD_CLASS(uint32_t,sh_info); \
    INCREMENT_FIELD_CLASS(uint64_t,sh_addralign); \
    INCREMENT_FIELD_CLASS(uint64_t,sh_entsize); 
