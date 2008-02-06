#include <stdio.h>
#include "foo.h"

int dum_global = 128;

int C_PREFIX(dum)(int t){
    t = t / dum_global;
    int r = 0;
    switch(t){
        case 0:
            r = t * 7;
            break;
        case 1:
            r = t * 3;
            break;
        case 3:
            r = t * 5;
            break;
        case 5:
            r = t * 9;
            break;
        case 7:
            r = t * 19;
            break;
        case 8:
            r = t * 27;
            break;
        case 10:
            r = t * 2;
            break;
        case 11:
            r = t * 11;
            break;
        default:
            r = 11;
            break;
    }
    char* ptr = (char*)0x20001000;
    printf("%d\n",ptr);
    return r;
}
