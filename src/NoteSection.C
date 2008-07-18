#include <NoteSection.h>


Note::Note(uint32_t idx, uint32_t namsz, uint32_t dessz, uint32_t typ, char* nam, char* des)
    : Base(ElfClassTypes_Note)
{
    index = idx;
    namesz = namsz;
    descsz = dessz;
    type = typ;

    name = new char[namesz];
    memcpy(name,nam,namesz);

    desc = new char[descsz];
    memcpy(desc,des,descsz);
}

Note::~Note(){
    if (name){
        delete[] name;
    }
    if (desc){
        delete[] desc;
    }
}

uint32_t Note::getNumberOfDescriptors(){
    return nextAlignAddress(descsz, Size__32_bit_Note_Section_Entry)/Size__32_bit_Note_Section_Entry;
}


uint32_t Note::getDescriptor(uint32_t idx){
    ASSERT(idx >= 0 && idx < getNumberOfDescriptors() && "Descriptor table index out of bounds");
    uint32_t descriptor = 0;
    memcpy(&descriptor, &desc[idx*Size__32_bit_Note_Section_Entry], Size__32_bit_Note_Section_Entry);
    return descriptor;
}

bool Note::verify(){
    if (namesz == 0){
        PRINT_ERROR("Note with namesz=0 is reserved for system use");
        return false;
    }
    if (!name){
        PRINT_ERROR("Name pointer in note section should be non-null");
        return false;
    } if (namesz != strlen(name) + 1 && namesz != nextAlignAddress(strlen(name) + 1, Size__32_bit_Note_Section_Entry)){
        print();
        PRINT_ERROR("Actual name size does not match claimed name size in note section -- %d != %d", namesz, strlen(name) + 1);
        return false;
    }
    return true;
}

void Note::print(){

    PRINT_INFOR("\tnot%5d -- typ:%5d nsz:%5d dsz:%5dB name:%10s",
        index,type,namesz,descsz,name);
    // print the list of descriptors
    for (uint32_t i = 0; i < getNumberOfDescriptors(); i++){
        PRINT_INFOR("\t\tdsc:%d", getDescriptor(i));
    }
}

void NoteSection::print(){
    PRINT_INFOR("NoteSection : %d with %d notes",index,numberOfNotes);
    printBytes(0,0);
    PRINT_INFOR("\tsect : %d",sectionIndex);
    for (uint32_t i = 0; i < numberOfNotes; i++){
        ASSERT(notes[i] && "numberOfNotes should indicate the number of elements in the notes array");
        notes[i]->print();
    }
#ifdef DEBUG_NOTE
    printBytes(0,0);
#endif
}

bool NoteSection::verify(){
    for (uint32_t i = 0; i < numberOfNotes; i++){
        if (!notes[i]){
            PRINT_ERROR("Notes[%d] should exist in the Note Table", i);
        }
        if (!notes[i]->verify()){
            return false;
        }
    }
    return true;
}


NoteSection::NoteSection(char* rawPtr, uint32_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : RawSection(ElfClassTypes_NoteSection,rawPtr,size,scnIdx,elf)
{
    index = idx;

    numberOfNotes = 0;
    notes = NULL;
}

NoteSection::~NoteSection(){
    if (notes){
        for (uint32_t i = 0; i < numberOfNotes; i++){
            if (notes[i]){
                delete notes[i];
            }
        }
        delete[] notes;
    }
}


uint32_t NoteSection::read(BinaryInputFile* binaryInputFile){

    uint32_t currWord = 0;

    uint32_t tmpNameSize;
    uint32_t tmpDescSize;
    uint32_t tmpType;

    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    ASSERT(sizeInBytes % Size__32_bit_Note_Section_Entry == 0 && "Notes section should be word-aligned");
    uint32_t* rawData = new uint32_t[sizeInBytes/Size__32_bit_Note_Section_Entry];
    if (!binaryInputFile->copyBytes((char*)rawData, sizeInBytes)){
        PRINT_ERROR("Cannot read notes section from file");
    }


    // go over the section once to determine how many note structures are in it
    numberOfNotes = 0;
    while (currWord * Size__32_bit_Note_Section_Entry < sizeInBytes){
        tmpNameSize = rawData[currWord++];
        tmpDescSize = rawData[currWord++];
        tmpType = rawData[currWord++];
        
        currWord += nextAlignAddress(tmpNameSize,Size__32_bit_Note_Section_Entry)/Size__32_bit_Note_Section_Entry;
        currWord += nextAlignAddress(tmpDescSize,Size__32_bit_Note_Section_Entry)/Size__32_bit_Note_Section_Entry;
        numberOfNotes++;

        PRINT_INFOR("Found a note with %d %d %d", tmpNameSize, tmpDescSize, tmpType);
    }

    PRINT_INFOR("Found %d notes", numberOfNotes);
    PRINT_INFOR("%d %d", currWord*Size__32_bit_Note_Section_Entry, sizeInBytes);
    ASSERT(currWord*Size__32_bit_Note_Section_Entry == sizeInBytes && "Number of bytes read from note section is not the same as section size");

    notes = new Note*[numberOfNotes];

    currWord = 0;
    numberOfNotes = 0;
    while (currWord * Size__32_bit_Note_Section_Entry < sizeInBytes){
        tmpNameSize = rawData[currWord++];
        tmpDescSize = rawData[currWord++];
        tmpType = rawData[currWord++];

        char* nameptr = (char*)(rawDataPtr + currWord * Size__32_bit_Note_Section_Entry);
        currWord += nextAlignAddress(tmpNameSize,Size__32_bit_Note_Section_Entry)/Size__32_bit_Note_Section_Entry;
        char* descptr = (char*)(rawDataPtr + currWord * Size__32_bit_Note_Section_Entry);
        currWord += nextAlignAddress(tmpDescSize,Size__32_bit_Note_Section_Entry)/Size__32_bit_Note_Section_Entry;

        notes[numberOfNotes] = new Note(numberOfNotes, tmpNameSize, tmpDescSize, tmpType, nameptr, descptr);
        numberOfNotes++;
    }

    delete[] rawData;
    PRINT_INFOR("Finished reading notes section");
    return sizeInBytes;
}

Note* NoteSection::getNote(uint32_t idx){
    ASSERT(idx >= 0 && idx < numberOfNotes && "Note table index out of bounds");
    ASSERT(notes && "Note table should be initialized");

    return notes[idx];
}

void NoteSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    Note* currNote;
    uint32_t tmpEntry;
    uint32_t paddingSize;
    char* tmpName;

    for (uint32_t i = 0; i < numberOfNotes; i++){
        currNote = getNote(i);
        ASSERT(currNote && "Note table should be initialized");

        tmpEntry = currNote->getNameSize();
        binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Note_Section_Entry,offset+currByte);
        currByte += Size__32_bit_Note_Section_Entry;

        tmpEntry = currNote->getDescriptorSize();
        binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Note_Section_Entry,offset+currByte);
        currByte += Size__32_bit_Note_Section_Entry;

        tmpEntry = currNote->getType();
        binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Note_Section_Entry,offset+currByte);
        currByte += Size__32_bit_Note_Section_Entry;


        // print the name
        tmpName = currNote->getName();
        binaryOutputFile->copyBytes(tmpName,currNote->getNameSize(),offset+currByte);
        currByte += currNote->getNameSize();

        // fill the "padding" after the name with zeroes
        paddingSize = nextAlignAddress(currNote->getNameSize(),Size__32_bit_Note_Section_Entry)-currNote->getNameSize();
        tmpName = new char[paddingSize];
        bzero(tmpName,paddingSize);
        binaryOutputFile->copyBytes(tmpName,paddingSize,offset+currByte);
        currByte += paddingSize;
        delete[] tmpName;        

        for (uint32_t j = 0; j < currNote->getNumberOfDescriptors(); j++){
            tmpEntry = currNote->getDescriptor(j);
            binaryOutputFile->copyBytes((char*)&tmpEntry,Size__32_bit_Note_Section_Entry,offset+currByte);
            currByte += Size__32_bit_Note_Section_Entry;
        }
    }
    ASSERT(currByte == sizeInBytes && "Dumped an incorrect number of bytes for NoteSection");
}






