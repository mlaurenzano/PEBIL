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

#include <Metasim.hpp>

typedef struct {
    bool Initialized;
    bool PerInstruction;
    bool Master;
    uint32_t Size;
    pthread_t threadid;
    pthread_key_t imageid;
    uint64_t* Counters;
    CounterTypes* Types;
    uint64_t* Addresses;
    uint64_t* Hashes;
    uint32_t* Lines;
    char** Files;
    char** Functions;
    char* Application;
    char* Extension;
} CounterArray;

#endif //_CounterFunctions_hpp_
