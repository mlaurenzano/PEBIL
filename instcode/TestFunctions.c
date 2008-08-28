#include <stdio.h>

int functioncounter(int numFunctions, int* functionCounts){
    int i;

    fprintf(stdout, "\n*** Instrumentation Summary ****\n");
    fprintf(stdout, "There are %d functions in the code:\n", numFunctions);
    fflush(stdout);

    for (i = 0; i < numFunctions; i++){
        fprintf(stdout, "\tFunction %d  executed %d times\n", i, functionCounts[i]);
        fflush(stdout);
    }
    fflush(stdout);

    return 3;
}

void secondtest(){
    int i;
    for (i = 0; i < 3; i++){
        fprintf(stdout, "shipping loop index %d\n", i);
    }
}
