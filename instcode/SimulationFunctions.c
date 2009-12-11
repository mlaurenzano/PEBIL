#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <InstrumentationCommon.h>

int32_t numBins = 16;
int32_t bins[16];


DINT_TYPE processTrace(DINT_TYPE* bufferloc, DINT_TYPE* bufferptr, DINT_TYPE* pointIdx, DINT_TYPE* pointAddrArray){
    DINT_TYPE memloc = *bufferloc;
    DINT_TYPE memval;
    int buffclear = 0;

    //    PRINT_INSTR("raw args: m[%x]=%#x m[%x]=%lld m[%x]=%lld m[%x]=%lld", bufferloc, *bufferloc, bufferptr, *bufferptr, pointIdx, *pointIdx, pointAddrArray, *pointAddrArray);
    DINT_TYPE pointAddr = *(pointAddrArray + *pointIdx);
    //    PRINT_INSTR("This is inst point %lld from program address %#llx", *pointIdx, pointAddr);
    while (0 < *bufferptr){
        //        PRINT_INSTR("trying %d %llx %llx", *bufferptr, bufferloc, *bufferloc);
        memloc = *bufferloc;
        if (memloc == 0){
            memval = 0;
        } else {
            //            PRINT_INSTR("unravelling addr %#x", memloc);
            memval = *((DINT_TYPE*)memloc);
            bins[memloc % numBins]++;
        }
        (*bufferptr)--;
        bufferloc++;
    }

    return 0;
}

int32_t endTrace(DINT_TYPE* bufferloc, DINT_TYPE* bufferptr, DINT_TYPE* pointIdx, DINT_TYPE* pointAddrArray){
    int32_t i;

    PRINT_INSTR("raw args: m[%x]=%#x m[%x]=%lld m[%x]=%lld m[%x]=%lld", bufferloc, *bufferloc, bufferptr, *bufferptr, pointIdx, *pointIdx, pointAddrArray, *pointAddrArray);
    //    processTrace(bufferloc, bufferptr, pointIdx, pointAddrArray);

    for (i = 0; i < numBins; i++){
        PRINT_INSTR("bins[%d] = %d", i, bins[i]);
    }
    return 0;
}
