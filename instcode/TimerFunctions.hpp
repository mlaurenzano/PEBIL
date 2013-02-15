
#ifndef _TimerFunctions_hpp_
#define _TimerFunctions_hpp_

typedef struct {
    bool master;
    uint64_t functionCount;
    char** functionNames;
    double* functionTimerAccum;
    double* functionTimerLast;
} FunctionTimers;

#endif
