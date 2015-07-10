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

int main(int argc){

    __asm("call *%eax");
    
    int i = 0;
    while (i < 1000000000){
        i++;
    }
    fprintf(stdout, "i=%d", i);

    /*
    char f = '1';
    fprintf(stdout, "%c", f);
    int j = i+argc;

    switch (j){
    case 0:
        i++;
        break;
    case 1:
        i--;
    case 2:
        i--;
        break;
    default:
        i += 100;
        break;
    }
    fprintf(stdout, "i=%d", i);
    */
}
