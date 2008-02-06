#include <stdio.h>
#include "foo.h"

int bar_global = 128;

int C_PREFIX(bar_helper)(int j,int g){
    return j+g;
}

int C_PREFIX(bar)(int j){
    int k = 0;
    if(j > bar_global){
        bar_global = j;
        j = j * 2;
    }
    j = 0;
    for(k=0;k<3;k++){
        j++;
        j = C_PREFIX(bar_helper)(j,bar_global);
        if( k % 4096){
            printf("What is it %d\n",k);
        }

    }
    return j;
}


