
#ifndef _LoopTimers_hpp_
#define _LoopTimers_hpp_

typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t loopCount;
    uint64_t* loopHashes;
    uint64_t* loopTimerAccum;
    uint64_t* loopTimerLast;
} LoopTimers;

#endif
