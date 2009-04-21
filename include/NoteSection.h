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
