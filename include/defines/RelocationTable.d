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
