#include <NoteSection.h>

#include <BinaryFile.h>
#include <ElfFile.h>

Note::Note(uint32_t idx, uint32_t namsz, uint32_t dessz, uint32_t typ, char* nam, char* des)
    : Base(PebilClassType_Note)
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
    PRINT_INFOR("NoteSection : %d with %d notes",index,notes.size());
    PRINT_INFOR("\tsect : %d",sectionIndex);
    for (uint32_t i = 0; i < notes.size(); i++){
        ASSERT(notes[i] && "notes.size() should indicate the number of elements in the notes array");
        notes[i]->print();
    }
    DEBUG_NOTE(printBytes(0,0,0);)
}

bool NoteSection::verify(){
    for (uint32_t i = 0; i < notes.size(); i++){
        if (!notes[i]){
            PRINT_ERROR("Notes[%d] should exist in the Note Table", i);
            return false;
        }
        if (!notes[i]->verify()){
            return false;
        }
    }
    return true;
}


NoteSection::NoteSection(char* rawPtr, uint32_t size, uint16_t scnIdx, uint32_t idx, ElfFile* elf)
    : RawSection(PebilClassType_NoteSection,rawPtr,size,scnIdx,elf)
{
    index = idx;
}

NoteSection::~NoteSection(){
    for (uint32_t i = 0; i < notes.size(); i++){
        if (notes[i]){
            delete notes[i];
        }
    }
}


uint32_t NoteSection::read(BinaryInputFile* binaryInputFile){

    binaryInputFile->setInPointer(rawDataPtr);
    setFileOffset(binaryInputFile->currentOffset());

    ASSERT(sizeInBytes % Size__32_bit_Note_Section_Entry == 0 && "Notes section should be word-aligned");
    uint32_t* rawData = new uint32_t[sizeInBytes/Size__32_bit_Note_Section_Entry];
    if (!binaryInputFile->copyBytes((char*)rawData, sizeInBytes)){
        PRINT_ERROR("Cannot read notes section from file");
    }


    uint32_t currWord = 0;
    uint32_t tmpNameSize;
    uint32_t tmpDescSize;
    uint32_t tmpType;

    while (currWord * Size__32_bit_Note_Section_Entry < sizeInBytes){
        tmpNameSize = rawData[currWord++];
        tmpDescSize = rawData[currWord++];
        tmpType = rawData[currWord++];

        char* nameptr = (char*)(rawDataPtr + currWord * Size__32_bit_Note_Section_Entry);
        currWord += nextAlignAddress(tmpNameSize, Size__32_bit_Note_Section_Entry) / Size__32_bit_Note_Section_Entry;
        char* descptr = (char*)(rawDataPtr + currWord * Size__32_bit_Note_Section_Entry);
        currWord += nextAlignAddress(tmpDescSize, Size__32_bit_Note_Section_Entry) / Size__32_bit_Note_Section_Entry;

        notes.append(new Note(notes.size(), tmpNameSize, tmpDescSize, tmpType, nameptr, descptr));
    }
    ASSERT(currWord * Size__32_bit_Note_Section_Entry == sizeInBytes && "Number of bytes read from note section is not the same as section size");

    delete[] rawData;
    return sizeInBytes;
}

void NoteSection::dump(BinaryOutputFile* binaryOutputFile, uint32_t offset){
    uint32_t currByte = 0;
    Note* currNote;
    uint32_t tmpEntry;
    uint32_t paddingSize;
    char* tmpName;

    for (uint32_t i = 0; i < notes.size(); i++){
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






