
#include <HybridPhiElfFile.h>
#include <SymbolTable.h>
#include <SectionHeader.h>

IntelOffloadHeader::IntelOffloadHeader(char* buf, uint64_t addr)
    : baseAddress(addr)
{
    memcpy(bytes, buf, INTEL_OFFLOAD_HEADER_SIZE);
}

void IntelOffloadHeader::setElfSize(uint32_t size)
{
    *(uint32_t*)bytes = size;
}

HybridPhiElfFile::HybridPhiElfFile(char* f, char* a)
    : ElfFile(f, a), embeddedElf(NULL), offloadHeader(NULL)
{
    // Do Nothing
}

/*
* An elf file is embedded at __offload_target_image+21
* It is registered by the runtime via:
*   offload_initv:
*       __offload_register_image
*/
ElfFile* HybridPhiElfFile::getEmbeddedElf(){

    if(embeddedElf != NULL )
        return embeddedElf;

    // Locate embedded elf object using the symbol table
    SymbolTable* symtab = getSymbolTable(1);
    if(symtab == NULL)
        return NULL;

    Symbol* sym = symtab->getSymbol("__offload_target_image");
    if(sym == NULL)
        return NULL;

    fprintf(stderr, "Found embedded elf object __offload_target_image at 0x%llx\n", sym->GET(st_value));

    // Get buffer location of file
    // Skip past Intel's 21 byte header
    // Assume the file fills the remainder of the section
    RawSection* sect = findDataSectionAtAddr(sym->GET(st_value));
    char * off_target_img = sect->getStreamAtAddress(sym->GET(st_value));

    offloadHeader = new IntelOffloadHeader(off_target_img, sym->GET(st_value));

    off_target_img += IntelOffloadHeader::INTEL_OFFLOAD_HEADER_SIZE;

    uint64_t img_size = sect->getSectionHeader()->getRawDataSize() - (sym->GET(st_value) - sect->getSectionHeader()->GET(sh_addr));

    ElfFile* elfFile = new ElfFile(off_target_img, img_size);

    // Analyze the elf file
    elfFile->parse();
    elfFile->initSectionFilePointers();
    elfFile->generateCFGs();
    elfFile->findLoops();
    elfFile->verify();
    elfFile->anchorProgramElements();

    return elfFile;
}

