
#ifndef _TimerFunctions_hpp_
#define _TimerFunctions_hpp_

typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t functionCount;
    char** functionNames;
    uint64_t* functionTimerAccum;
    uint64_t* functionTimerLast;
    uint32_t* inFunction;
} FunctionTimers;

#endif
