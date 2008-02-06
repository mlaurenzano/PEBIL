#include <SymbolTable.h>
#include <LineInfoTable.h>
#include <XCoffFile.h>
#include <BinaryFile.h>

LineInfoTable::LineInfoTable(char* ptr,uint32_t s,XCoffFile* xcoff)
        : Base(XCoffClassTypes_line_info),
          lineInfoPointer(ptr),numOfLineInfos(s),
          symbolTable(NULL),xCoffFile(xcoff)
{
    if(getXCoffFile()->is64Bit()){
        sizeInBytes = numOfLineInfos * Size__64_bit_LineInfoTable_Entry;
    } else {
        sizeInBytes = numOfLineInfos * Size__32_bit_LineInfoTable_Entry;
    }
    lineInfos = new LineInfo*[numOfLineInfos];
}

void LineInfo::print(SymbolTable* symbolTable,uint32_t index){
    if(GET(l_lnno)){
        PRINT_INFOR("\tLNN [%3d] (lnn %9d)(adr %#llx)",index,GET(l_lnno),GET(l_paddr));
    } else {
        PRINT_INFOR("\tLNN [%3d] (fcn bgn)(sym %9d)",index,GET(l_symndx));
        if(symbolTable){
            symbolTable->printSymbol(GET(l_symndx));
        }
    }
}

void LineInfoTable::print(){
    PRINT_INFOR("LINEINFOTABLE");
    PRINT_INFOR("\tCount : %d",numOfLineInfos);

    PRINT_INFOR("\tLines :");
    for(uint32_t i = 0;i<numOfLineInfos;i++){
        lineInfos[i]->print(symbolTable,i);
    }
}

uint32_t LineInfoTable::read(BinaryInputFile* binaryInputFile){
    PRINT_DEBUG("Reading the LineInformation table");

    binaryInputFile->setInPointer(lineInfoPointer);
    setFileOffset(binaryInputFile->currentOffset());

    uint32_t currSize = Size__32_bit_LineInfoTable_Entry;
    if(getXCoffFile()->is64Bit()){
        currSize = Size__64_bit_LineInfoTable_Entry;
    }

    uint32_t ret = 0;
    for(uint32_t i = 0;i<numOfLineInfos;i++){
        if(getXCoffFile()->is64Bit()){
            lineInfos[i] = new LineInfo64();
        } else {
            lineInfos[i] = new LineInfo32();
        }
        binaryInputFile->copyBytesIterate(lineInfos[i]->charStream(),currSize);
        ret += currSize;
    }

    ASSERT((sizeInBytes == ret) && "FATAL : Somehow the number of read does not match");

    return sizeInBytes;
}
