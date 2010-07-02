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

#ifndef _NoteSection_h_
#define _NoteSection_h_

#include <Base.h>
#include <RawSection.h>
#include <Vector.h>

class BinaryInputFile;
class BinaryOutputFile;

class Note : public Base {
private: 
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
    char* name;
    char* desc;

    uint32_t index;
public:
    Note(uint32_t index, uint32_t namsz, uint32_t dessz, uint32_t typ, char* nam, char* des);
    ~Note();

    bool verify();
    void print();

    uint32_t getNameSize() { return namesz; }
    uint32_t getDescriptorSize() { return descsz; }
    uint32_t getType() { return type; }
    uint32_t getNumberOfDescriptors();

    char* getName() { return name; }
    uint32_t getDescriptor(uint32_t idx);
};


class NoteSection : public RawSection {
private:
    bool verify();

protected:
    Vector<Note*> notes;
    uint32_t index;
public:
    NoteSection(char* rawPtr, uint32_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf);
    ~NoteSection();

    void print();
    uint32_t read(BinaryInputFile* b);

    uint32_t getNumberOfNotes() { return notes.size(); }
    Note* getNote(uint32_t idx) { return notes[idx]; }

    virtual void dump(BinaryOutputFile* binaryOutputFile, uint32_t offset);
};

#endif /* _NoteSection_h_ */
