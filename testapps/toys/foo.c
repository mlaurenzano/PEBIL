/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

