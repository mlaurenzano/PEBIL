
#ifndef _FrequencyConfig_hpp_
#define _FrequencyConfig_hpp_

typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t loopCount;
    uint64_t* loopHashes;
    uint32_t* frequencyMap;
} FrequencyConfig;

#endif
