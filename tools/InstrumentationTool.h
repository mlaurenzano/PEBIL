#ifndef _InstrumentationTool_h_
#define _InstrumentationTool_h_

#include <ElfFileInst.h>

#define FILE_UNK "__FILE_UNK__"

class InstrumentationTool : public ElfFileInst {
protected:
    void printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos);

public:
    InstrumentationTool(ElfFile* elf, char* inputFuncList, char* inputFileList);
    ~InstrumentationTool() {}

    virtual void declare() { __SHOULD_NOT_ARRIVE; }
    virtual void instrument() { __SHOULD_NOT_ARRIVE; }

    virtual const char* briefName() { __SHOULD_NOT_ARRIVE; }
};


#endif /* _InstrumentationTool_h_ */
