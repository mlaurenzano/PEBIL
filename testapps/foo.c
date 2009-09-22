#include <stdio.h>
#include <string.h>
#include "foo.h"

long long foo_global = 128;

int foo_array[16];

int C_PREFIX(foo)(int);

int C_PREFIX(foo_helper)(){
    int (*ptr) (int) = C_PREFIX(foo);
    return ptr(0);
}
int C_PREFIX(foo)(int i){
    if(!i)
        return 0;
    foo_global = i * 4;
    int j = i * foo_global;
    foo_array[j % 16] = (j ? j : i);
    fprintf(stdout,"%d is Fooooooooooo\n",foo_array[j % 16]);
    if(j < 0x1000000){
        j *= 5;
        printf("foo_global is %d\n",j);
        foo_global = j;
    } else {
        foo_global = foo_global / 2;
        printf("foo_global is %lld\n",foo_global);
    }

    XY(i,j);

    memcpy(&foo_global,&i,sizeof(int));

    return C_PREFIX(foo_helper)();
}

