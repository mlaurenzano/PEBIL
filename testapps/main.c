#include <stdio.h>
#include "foo.h"

#define MIKEY_LOOP

extern int C_PREFIX(foo)(int);
extern int C_PREFIX(bar)(int);
extern int C_PREFIX(dum)(int);

int C_PREFIX(more_arguments)(
int a1,
int a2,
int a3,
int a4,
int a5,
int a6,
int a7,
int a8,
int a9,
int a10)
{
    return (int)(a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + 10);
}

void no_argument(){
    int i = 3;
    int j = 1345;
    printf("%d %d %d\n",i,j,j<<i);

}

#ifdef MIKEY_LOOP
void call_mikey_func(){
    int i, j, k;
    for (i = 0; i < 10000; i++){
        if (i % 2 == 0){
            j--;
            for (k = 0; k < 1000; k++){
                if (k % 2 == 0){
                    j++;
                }
            }
        } else {
            j++;
        }
    }
}
#endif

int main(int argc,char* argv[]){
    int retf,retb,retd;
    unsigned i = 0;
    for(i=0;i<0x10000000;i+=0x1000000){
        retf = C_PREFIX(foo)(i);
        retb = C_PREFIX(bar)(i);
        retd = C_PREFIX(dum)(i);
        printf("%#x %d %d %d\n",i,retf,retb,retd);
        fflush(stdout);
    }
#ifdef MIKEY_LOOP
    call_mikey_func();
#endif

    no_argument();
    printf("more arguments is %d\n",C_PREFIX(more_arguments)(1,2,3,4,5,6,7,8,9,10));
    printf("(Test Application Successfull)\n");
    fflush(stdout);
    return 0;
}

