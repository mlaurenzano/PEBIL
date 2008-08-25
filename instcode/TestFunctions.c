#include <stdio.h>

int smalltest(){
    fprintf(stdout, "You are calling a test function in a shared library. wow.\n");
    fprintf(stdout, "Hello tikir\n");
    fprintf(stdout, "Mikey rules\n");
    fflush(stdout);
    return 3;
}

void secondtest(){
    int i;
    for (i = 0; i < 3; i++){
        fprintf(stdout, "shipping loop index %d\n", i);
    }
}
