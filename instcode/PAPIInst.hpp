
#ifndef _PAPIInst_hpp_
#define _PAPIInst_hpp_

#include <sys/socket.h>
#include <sys/un.h>

#define MAX_HWC 32
typedef long long values_t[MAX_HWC];

typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t loopCount;
    uint64_t* loopHashes;
    values_t* values;
} PAPIInst;

#endif
