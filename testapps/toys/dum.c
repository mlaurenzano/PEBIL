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
