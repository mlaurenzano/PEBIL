#ifndef _IOTracer_h_
#define _IOTracer_h_

#include <InstrumentationTool.h>

class IOTracer : public InstrumentationTool {
private:
    InstrumentationFunction* programEntry;
    InstrumentationFunction* programExit;
    InstrumentationFunction* functionTrace;

    Vector<char*>* traceFunctions;

public:
    IOTracer(ElfFile* elf, char* inputFuncList, char* inputFileList, char* traceFile);
    ~IOTracer();

    void declare();
    void instrument();

    void traceListContains();
};


#endif /* _IOTracer_h_ */
