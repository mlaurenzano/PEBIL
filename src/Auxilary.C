#include <Instruction.h>
#include <DemangleWrapper.h>
#include <SymbolTable.h>
#include <StringTable.h>
#include <SectHeader.h>
#include <RawSection.h>
#include <Auxilary.h>
#include <BinaryFile.h>

const char* Auxilary::getTypeName(){
    switch(getAuxilaryType()) {
        case Type__Auxilary_Symbol_No_Type : return "NO_TYPE";
        case Type__Auxilary_Symbol_Section : return "SECTION";
        case Type__Auxilary_Symbol_Exception : return "EXCEPTION";
        case Type__Auxilary_Symbol_Function : return "FUNCTION";
        case Type__Auxilary_Symbol_Block : return "BLOCK";
        case Type__Auxilary_Symbol_File : return "FILE";
        case Type__Auxilary_Symbol_CSect : return "CSECT";
        default: 
            PRINT_DEBUG("Some auxilary symbol that is not known");
            ASSERT(false);
            return "UNK";
    }
    return NULL;
}

void Auxilary::print(StringTable* stringTable,
                     DebugSection* debugRawSect,bool followAux)
{
    PRINT_INFOR("\tAUX\t%10s [%7d]",getTypeName(),getIndex());
}

void AuxilarySection::print(StringTable* stringTable,
           DebugSection* debugRawSect,
           bool followAux)
{
    PRINT_INFOR("\t^\t%10s [%7d] (length %d) (#reloc %d) (#line %d)",
                    getTypeName(),getIndex(),
                    GET_A(x_scnlen,x_scn),
                    GET_A(x_nreloc,x_scn),
                    GET_A(x_nlinno,x_scn));
}
void AuxilaryException::print(StringTable* stringTable,
           DebugSection* debugRawSect,
           bool followAux)
{
    PRINT_INFOR("\tAUX\t%10s [%7d] (exptr %lld) (fsize %d) (nextsym %d)",
                    getTypeName(),getIndex(),
                    GET_A(x_exptr,x_except),
                    GET_A(x_fsize,x_except),
                    GET_A(x_endndx,x_except));
}

void AuxilaryException::changeExptrCopy(uint64_t exptr,char* buff){
    AUXENT_64 newEntry;
    newEntry = entry;
    newEntry.x_except.x_exptr = exptr;
    memcpy(buff,&newEntry,Size__NN_bit_SymbolTable_Entry);
}

void AuxilaryBlock::print(StringTable* stringTable,
           DebugSection* debugRawSect,
           bool followAux)
{
    PRINT_INFOR("\tAUX\t%10s [%7d] (lineno %d)",
                    getTypeName(),getIndex(),
                    GET_A(x_lnno,x_misc));
}

void AuxilaryFile::print(StringTable* stringTable,
           DebugSection* debugRawSect,
           bool followAux)
{
    char* name = GET_A(x_fname,x_file);
    uint32_t zeroes = GET_A(x_zeroes,x_file);
    if(!zeroes){
        name = stringTable->getString(GET_A(x_offset,x_file));
    }
    PRINT_INFOR("\tAUX\t%10s [%7d] (fname %s)",
                    getTypeName(),getIndex(),
                    name);
}

void AuxilaryFunction::print(StringTable* stringTable,
           DebugSection* debugRawSect,
           bool followAux)
{
    PRINT_INFOR("\tAUX\t%10s [%7d] (exptr %d) (fsize %d) (linptr %lld) (nextsym %d)",
                    getTypeName(),getIndex(),
                    GET_A(x_exptr,x_fcn),
                    GET_A(x_fsize,x_fcn),
                    GET_A(x_lnnoptr,x_fcn),
                    GET_A(x_endndx,x_fcn));
}

void AuxilaryFunction32::changeExptrLnnoptrCopy(uint32_t exptr,uint64_t lnnoptr,char* buff){
    AUXENT newEntry;
    newEntry = entry;
    newEntry.x_sym.x_exptr = exptr;
    newEntry.x_sym.x_fcnary.x_fcn.x_lnnoptr = lnnoptr;
    memcpy(buff,&newEntry,Size__NN_bit_SymbolTable_Entry);
}

void AuxilaryFunction64::changeExptrLnnoptrCopy(uint32_t exptr,uint64_t lnnoptr,char* buff){
    AUXENT_64 newEntry;
    newEntry = entry;
    newEntry.x_fcn.x_lnnoptr = lnnoptr;
    memcpy(buff,&newEntry,Size__NN_bit_SymbolTable_Entry);
}

void AuxilaryCSect::print(StringTable* stringTable,
           DebugSection* debugRawSect,
           bool followAux)
{
    uint64_t length = getLength();

    bool isInSymTable = false;

    uint8_t smtype = GET_A(x_smtyp,x_csect);
    uint32_t lengthMod = smtype & 0x7;
    uint32_t alignment = (smtype >> 3) & 0x1f;
    alignment = 1 << alignment;

    char* ptr = "";
    switch(lengthMod){
        case XTY_SD: 
            ptr = "SD"; 
            break;
        case XTY_LD: 
            isInSymTable = true; 
            ptr = "LD"; 
            break;
        case XTY_CM: 
            ptr = "CM"; 
            break;
        case XTY_ER: 
            ptr = "ER"; 
            ASSERT((length == 0) && "FATAL : For this type the length should be 0");
            break;
        default:
            break;
    }

    PRINT_INFOR("\tAUX\t%10s [%7d] (scnlen %lld) (parmhash %d) (snhash %d) (type %#x) (cls %d) %s (%d,%d)",
                    getTypeName(),getIndex(),
                    length,GET_A(x_parmhash,x_csect),GET_A(x_snhash,x_csect),
                    GET_A(x_smtyp,x_csect),GET_A(x_smclas,x_csect),ptr,lengthMod,alignment);
}

uint64_t AuxilaryCSect32::getLength(){
    return GET_A(x_scnlen,x_csect);
}

uint64_t AuxilaryCSect64::getLength(){
    uint64_t lo = GET_A(x_scnlen_lo,x_csect);
    uint64_t hi = GET_A(x_scnlen_hi,x_csect);
    return ((hi << 32) | lo);
}
