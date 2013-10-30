
#ifndef _FrequencyConfig_hpp_
#define _FrequencyConfig_hpp_

#include <sys/socket.h>
#include <sys/un.h>

typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t loopCount;
    uint64_t* loopHashes;
    int throttler;
    uint32_t* frequencyMap;
    uint32_t* frequencyTable;
    uint32_t frequencies;
    float* ipsMap;
    unsigned char extMod;
} FrequencyConfig;

#endif
