#include <InstrumentationCommon.h>

#define RECORDS_PER_FUNCTION 8
#define STACK_BACKTRACE_SIZE  8
#define NUM_PRINT 10000

int64_t ticksPerSecond;

struct funcInfo* funcInfos = NULL;
int32_t numberOfFunctions = 0;
char** functionNames = NULL;
int32_t* stackError = NULL;
#define HASH_STACK_HEAD hashFunction(funcStack_peep(7), funcStack_peep(6), funcStack_peep(5), funcStack_peep(4), funcStack_peep(3), funcStack_peep(2), funcStack_peep(1), funcStack_peep(0))

struct funcInfo
{
    unsigned long long   count;
    unsigned long long   timer_start;
    unsigned long long   timer_total;
    unsigned long long   hash;
    int32_t              backtrace[STACK_BACKTRACE_SIZE]; // note: backtrace[0] holds the function index
};

#define MAX_STACK_IDX 0x100000
int32_t* funcStack;
int32_t  stackIdx = -1;

#define ONEBYTE_MASK 0x000000ff
__inline__ unsigned long long hashFunction(int32_t n1, int32_t n2, int32_t n3, int32_t n4, int32_t n5, int32_t n6, int32_t n7, int32_t n8){
    unsigned long long hashCode = 0;

    n1 &= ONEBYTE_MASK;
    hashCode |= (n1 << 56);
    n2 &= ONEBYTE_MASK;
    hashCode |= (n2 << 48);
    n3 &= ONEBYTE_MASK;
    hashCode |= (n3 << 40);
    n4 &= ONEBYTE_MASK;
    hashCode |= (n4 << 32);
    n5 &= ONEBYTE_MASK;
    hashCode |= (n5 << 24);
    n6 &= ONEBYTE_MASK;
    hashCode |= (n6 << 16);
    n7 &= ONEBYTE_MASK;
    hashCode |= (n7 <<  8);
    n8 &= ONEBYTE_MASK;
    hashCode |= (n8 <<  0);

    return hashCode;
}

int32_t getRecordIndex(){
    unsigned long long hashCode = HASH_STACK_HEAD;
    unsigned idx = hashCode % (numberOfFunctions * RECORDS_PER_FUNCTION);
    while (funcInfos[idx].hash && hashCode != funcInfos[idx].hash){
        idx++;
        idx = (idx % (numberOfFunctions * RECORDS_PER_FUNCTION));
    }
    return idx;
}

int32_t funcStack_push(int32_t n){
    assert(n < MAX_STACK_IDX);
    funcStack[++stackIdx] = n;
}

int32_t funcStack_pop(){
    assert(stackIdx >= 0);
    return funcStack[stackIdx--];
}

int32_t funcStack_peep(int32_t b){
    if (stackIdx - b < 0){
        return -1;
    }
    return funcStack[stackIdx-b];
}

void funcStack_print(){
    int i;
    for (i = 0; i <= stackIdx; i++){
        fprintf(stdout, "%d ", funcStack[i]);
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

void printFunctionInfo(int i){
    int j;
#ifdef EXCLUDE_TIMER
    PRINT_INSTR(stdout, "%s (%d): %lld executions", functionNames[funcInfos[i].backtrace[0]], funcInfos[i].hash % (numberOfFunctions * RECORDS_PER_FUNCTION), funcInfos[i].count);    
#else
    PRINT_INSTR(stdout, "%s (%d): %lld executions, %.6f seconds", functionNames[funcInfos[i].backtrace[0]], funcInfos[i].hash % (numberOfFunctions * RECORDS_PER_FUNCTION), funcInfos[i].count, ((double)((double)funcInfos[i].timer_total/(double)ticksPerSecond)));
#endif
    for (j = 1; j < STACK_BACKTRACE_SIZE; j++){
        if (funcInfos[i].backtrace[j] >= 0){
            if (stackError[funcInfos[i].backtrace[j]]){
                PRINT_INSTR(stdout, "[%d]\t-e-\t%s", j, functionNames[funcInfos[i].backtrace[j]]);
            } else {
                PRINT_INSTR(stdout, "[%d]\t\t%s", j, functionNames[funcInfos[i].backtrace[j]]);
            }
        }
    }
}

int compareFuncInfos(const void* f1, const void* f2){
    struct funcInfo* func1 = (struct funcInfo*)f1;
    struct funcInfo* func2 = (struct funcInfo*)f2;

    if (func1->timer_total > func2->timer_total){
        return -1;
    } else if (func1->timer_total < func2->timer_total){
        return 1;
    }
    return 0;
}

int32_t program_entry(int32_t* numFunctions, char** funcNames){
    int i;
    numberOfFunctions = *numFunctions;
    functionNames = funcNames;

    ticksPerSecond = CLOCK_RATE_HZ;
    PRINT_DEBUG("%lld ticks per second", ticksPerSecond);

    if (!funcInfos){
        PRINT_DEBUG("allocating %d funcInfos", numberOfFunctions * RECORDS_PER_FUNCTION);

        funcInfos = malloc(sizeof(struct funcInfo) * numberOfFunctions * RECORDS_PER_FUNCTION);
        bzero(funcInfos, sizeof(struct funcInfo) * numberOfFunctions * RECORDS_PER_FUNCTION);

        funcStack = malloc(sizeof(int32_t) * MAX_STACK_IDX);
        bzero(funcStack, sizeof(int32_t) * MAX_STACK_IDX);

        stackError = malloc(sizeof(int32_t) * numberOfFunctions);
        bzero(stackError, sizeof(int32_t) * numberOfFunctions);
    }
    assert(funcInfos);
}

int32_t program_exit(){
    PRINT_INSTR(stdout, "Printing the %d most time-consuming call paths + up to %d stack frames", NUM_PRINT, STACK_BACKTRACE_SIZE);
    int32_t i, j;

    qsort((void*)funcInfos, numberOfFunctions * RECORDS_PER_FUNCTION, sizeof(struct funcInfo), compareFuncInfos);

    int32_t numUsed = 0;
    for (i = 0; i < numberOfFunctions * RECORDS_PER_FUNCTION; i++){
        if (funcInfos[i].count){
            if (numUsed < NUM_PRINT){
                printFunctionInfo(i);
            }
            numUsed++;
        }
    }

    for (i = 0; i < numberOfFunctions; i++){
        if (stackError[i]){
            PRINT_INSTR(stdout, "Function %s timer failed (exit not taken) -- execution count %d", functionNames[i], stackError[i]);
        }
    }

    PRINT_INSTR(stdout, "Hash Table usage = %d of %d entries (%.3f)", numUsed, numberOfFunctions * RECORDS_PER_FUNCTION, ((double)((double)numUsed/((double)(numberOfFunctions * RECORDS_PER_FUNCTION)))));
}

int32_t function_entry(int64_t* functionIndex){
    int i, j;

    funcStack_push(*functionIndex);

    int32_t currentRecord = getRecordIndex();
    if (!funcInfos[currentRecord].hash){
        funcInfos[currentRecord].hash = HASH_STACK_HEAD;

        PRINT_DEBUG("hash[%d](idx %d) = %#llx", currentRecord, *functionIndex, funcInfos[currentRecord].hash);
        for (i = 0; i < STACK_BACKTRACE_SIZE; i++){
            funcInfos[currentRecord].backtrace[i] = funcStack_peep(i);
        }
    }

#ifdef EXCLUDE_TIMER
    funcInfos[currentRecord].timer_start = 0;
#else
    funcInfos[currentRecord].timer_start = readtsc();
#endif
}

int32_t function_exit(int64_t* functionIndex){
    int64_t tstop;
#ifdef EXCLUDE_TIMER
    tstop = 1;
#else
    tstop = readtsc();
#endif

    int32_t currentRecord = getRecordIndex();

    int32_t popped = funcStack_pop();
    if (popped != *functionIndex){
        while (popped != *functionIndex){
            stackError[popped]++;
            popped = funcStack_pop();
        }
        funcStack_push(popped);
        currentRecord = getRecordIndex();
        popped = funcStack_pop();
    }

    int64_t tadd = tstop - funcInfos[currentRecord].timer_start;
    funcInfos[currentRecord].count++;
    funcInfos[currentRecord].timer_total += tadd;
}
