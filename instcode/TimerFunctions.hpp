
#ifndef _TimerFunctions_hpp_
#define _TimerFunctions_hpp_
#include <sys/time.h>
#include <string>
using namespace std;


typedef struct {
    bool master;
    char* application;
    char* extension;
    uint64_t functionCount;
    char** functionNames;
    uint64_t* functionTimerAccum;
    uint64_t* functionEntryCounts;
    uint32_t* functionShutoff;
    uint64_t* functionTimerLast;
    uint32_t* inFunction;
    uint64_t appTimeStart;
    struct timeval appTimeOfDayStart;
} FunctionTimers;

#endif
#define KILO (1024)
#define MEGA (KILO*KILO)
#define GIGA (MEGA*KILO)
static char ToLowerCase(char c);
static bool ParsePositiveInt32(string token, uint32_t* value);
static bool ParseInt32(string token, uint32_t* value, uint32_t min);
static bool ParsePositiveInt32Hex(string token, uint32_t* value);
static bool ReadEnvUint32(string name, uint32_t* var);
