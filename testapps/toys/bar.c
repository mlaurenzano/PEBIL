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


