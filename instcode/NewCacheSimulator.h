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

#ifndef _NewCacheSimulator_h_
#define _NewCacheSimulator_h_

typedef uint32_t     Attribute_t;

#ifdef METASIM_32_BIT_LIB
    typedef uint32_t     Address_t;
#else
    typedef uint64_t     Address_t;
#endif

typedef struct {
    Attribute_t blockId;
    Attribute_t memOpId;
#ifdef METASIM_32_BIT_LIB
    Attribute_t unused;
#endif
    Address_t   address;
} AddressEntry;

#define lastFreeIdx blockId

void processSamples(AddressEntry*,Attribute_t,Attribute_t);

/*** Define data structures needed for cache 
     simulation functions, such as to keep track of
     hit/miss counts, memory hierarchies and etc. 
     |            |           |             |
     V            V           V             V
***/


#endif
