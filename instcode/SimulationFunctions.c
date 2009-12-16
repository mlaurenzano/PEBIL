#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <InstrumentationCommon.h>

#define NUM_BINS 16
int32_t bins[NUM_BINS];

//#define NO_PROCESS_TRACE

DINT_TYPE processTrace(DINT_TYPE* bufferloc, DINT_TYPE* bufferptr, DINT_TYPE* pointIdx, DINT_TYPE* pointAddrArray){
#ifdef NO_PROCESS_TRACE
    (*bufferptr) = 0;
    return 0;
#else
    register int32_t i;
    register buffer = (*bufferptr);

    DINT_TYPE memloc;
    DINT_TYPE memval;

    //    PRINT_INSTR("raw args: m[%x]=%#x m[%x]=%lld m[%x]=%lld m[%x]=%lld", bufferloc, *bufferloc, bufferptr, *bufferptr, pointIdx, *pointIdx, pointAddrArray, *pointAddrArray);
    for (i = 0; i < buffer; i++){
        //        PRINT_INSTR("trying %d %llx %llx", i, bufferloc, *bufferloc);
        memloc = *(bufferloc+i);
        memval = 0;
        if (memloc){
            memval = *((DINT_TYPE*)memloc);
        }
        bins[memloc % NUM_BINS]++;
    }

    buffer = 0;
    (*bufferptr) = buffer;
    return 0;
#endif
}

int32_t endTrace(DINT_TYPE* bufferloc, DINT_TYPE* bufferptr, DINT_TYPE* pointIdx, DINT_TYPE* pointAddrArray){
    int32_t i;

    PRINT_INSTR("raw args: m[%x]=%#x m[%x]=%lld m[%x]=%lld m[%x]=%lld", bufferloc, *bufferloc, bufferptr, *bufferptr, pointIdx, *pointIdx, pointAddrArray, *pointAddrArray);
    processTrace(bufferloc, bufferptr, pointIdx, pointAddrArray);

    for (i = 0; i < NUM_BINS; i++){
        PRINT_INSTR("bins[%d] = %d", i, bins[i]);
    }
    return 0;
}
