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

#ifndef _GlobalOffsetTable_h_
#define _GlobalOffsetTable_h_

#include <RawSection.h>

class ElfFile;

#define Size__ProcedureLink_Intermediate 6

class GlobalOffsetTable : public RawSection {
protected:
    uint32_t numberOfEntries;
    uint32_t entrySize;
    uint32_t tableBaseIdx;

    uint64_t* entries;
public:
    GlobalOffsetTable(char* rawPtr, uint32_t size, uint16_t scnIdx, uint64_t gotSymAddr, ElfFile* elf);
    ~GlobalOffsetTable();

    void print();
    uint32_t read(BinaryInputFile* b);

    uint64_t getEntry(uint32_t index);
    uint32_t getEntrySize() { return entrySize; }
    uint64_t getEntryAddress(uint32_t index) { ASSERT(getEntry(index)); return (baseAddress + index*getEntrySize()); }
    uint32_t getNumberOfEntries() { return numberOfEntries; }
    uint32_t minIndex() { return -1*tableBaseIdx; }
    uint32_t maxIndex() { return numberOfEntries-tableBaseIdx; }
    void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);

};

#endif /* _GlobalOffsetTable_h_ */
