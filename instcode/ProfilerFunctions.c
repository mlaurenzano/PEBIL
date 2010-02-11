#include <InstrumentationCommon.h>

struct funcInfo* funcInfos = NULL;
int32_t numberOfFunctions = 0;
char** functionNames = NULL;

unsigned long long ticksPerSecond = 0;

__inline__ unsigned long long readtsc(){
    unsigned low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d"(high));
    return ((unsigned long long)low | (((unsigned long long)high) << 32));
}

struct funcInfo
{
    unsigned long long   count;
    unsigned long long   timer_start;
    unsigned long long   timer_total;
    char*                name;
};

int compareFuncInfos(const void* f1, const void* f2){
    struct funcInfo* func1 = (struct funcInfo*)f1;
    struct funcInfo* func2 = (struct funcInfo*)f2;

    if (func1->timer_total > func2->timer_total){
        return 1;
    } else if (func1->timer_total < func2->timer_total){
        return -1;
    }
    return 0;
}

int32_t program_entry(int32_t* numFunctions, char** funcNames){
    int i;
    PRINT_INSTR("PROGRAM ENTRY %d", *numFunctions);

    numberOfFunctions = *numFunctions;
    functionNames = funcNames;

    unsigned long long t1, t2;
    t1 = readtsc();
    sleep(1);
    t2 = readtsc();
    ticksPerSecond = t2-t1;
    ticksPerSecond = 2600000000;
    PRINT_INSTR("%lld ticks per second", ticksPerSecond);
}

int32_t program_exit(){
    PRINT_INSTR("PROGRAM EXIT");
    int32_t i;

    for (i = 0; i < numberOfFunctions; i++){
        funcInfos[i].name = functionNames[i];
    }

    for (i = 0; i < numberOfFunctions; i++){
        if (funcInfos[i].count){
            PRINT_INSTR("funcInfo for func %s: %lld %.6f", funcInfos[i].name, funcInfos[i].count, ((double)((double)funcInfos[i].timer_total/(double)ticksPerSecond)));
        }
    }

    PRINT_INSTR("PROGRAM EXIT");
    qsort((void*)funcInfos, numberOfFunctions, sizeof(struct funcInfo), compareFuncInfos);

    for (i = 0; i < numberOfFunctions; i++){
        if (funcInfos[i].count){
            PRINT_INSTR("funcInfo for func %s: %lld %.6f", funcInfos[i].name, funcInfos[i].count, ((double)((double)funcInfos[i].timer_total/(double)ticksPerSecond)));
        }
    }
}

int32_t function_entry(int64_t* functionIndex){
    int i;

    if (!funcInfos){
        funcInfos = malloc(sizeof(struct funcInfo) * numberOfFunctions);
        bzero(funcInfos, sizeof(struct funcInfo) * numberOfFunctions);
    }

    funcInfos[*functionIndex].timer_start = readtsc();
    if (*functionIndex == 5){
        PRINT_INSTR("%s %d timer started: %lld", functionNames[*functionIndex], *functionIndex, funcInfos[*functionIndex].timer_start);
    }
}

int32_t function_exit(int64_t* functionIndex){
    int64_t tstop = readtsc();
    int64_t tadd = tstop - funcInfos[*functionIndex].timer_start;
    if (*functionIndex == 5){
        PRINT_INSTR("%s %d timer stopped: %lld", functionNames[*functionIndex], *functionIndex, tstop);
    }

    funcInfos[*functionIndex].count++;
    funcInfos[*functionIndex].timer_total += tadd;
}
