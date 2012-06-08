/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2012, University of California Regents
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

#ifndef _CounterFunctions_hpp_
#define _CounterFunctions_hpp_

typedef enum {
    CounterType_undefined = 0,
    CounterType_instruction,
    CounterType_basicblock,
    CounterType_loop,
    CounterType_total
} CounterTypes;

static const char* CounterTypeNames[CounterType_total] = {
    "undefined",
    "instruction",
    "basicblock",
    "loop"
};

typedef struct {
    CounterTypes Type;
    bool Initialized;
    uint32_t Size;
    uint64_t* Counters;
    uint64_t* Hashes;
    uint32_t* Lines;
    char** Files;
    char** Functions;
    char* Application;
    char* Extension;
} CounterArray;

#endif
