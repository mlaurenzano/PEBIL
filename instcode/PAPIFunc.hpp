
#ifndef _PAPIFunc_hpp_
#define _PAPIFunc_hpp_

#include <sys/socket.h>
#include <sys/un.h>

#define MAX_HWC 32
typedef long long values_t[MAX_HWC];

typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t functionCount;
    char** functionNames;
    uint64_t* functionTimerAccum;
    uint64_t* functionTimerLast;
    uint32_t* inFunction;
		int events[MAX_HWC];
		values_t* tmpValues;
		values_t* accumValues;
		int num;
} FunctionPAPI;

#endif
