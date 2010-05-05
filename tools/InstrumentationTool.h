#ifndef _InstrumentationTool_h_
#define _InstrumentationTool_h_

#include <ElfFileInst.h>
#include <Instrumentation.h>
#include <X86Instruction.h>

#define INFO_UNKNOWN "__info_unknown__"

typedef struct 
{
    int64_t pt_vaddr;
    int64_t pt_target;
    int64_t pt_flags;
    int32_t pt_size;
    int32_t pt_blockid;
    unsigned char pt_content[16];
    unsigned char pt_disable[16];
} instpoint_info;

class InstrumentationTool : public ElfFileInst {
protected:
    void printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<LineInfo*>* allLineInfos);

    InstrumentationFunction* initWrapper;
public:
    InstrumentationTool(ElfFile* elf);
    ~InstrumentationTool() { }

    virtual void declare();
    virtual void instrument();
    virtual void usesModifiedProgram() { }

    virtual const char* briefName() { __SHOULD_NOT_ARRIVE; }
};


#endif /* _InstrumentationTool_h_ */
