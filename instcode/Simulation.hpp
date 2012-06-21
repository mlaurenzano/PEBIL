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

#ifndef _TracerMethods_h_
#define _TracerMethods_h_

#include <DFPattern.h>
#include <CacheSimulationCommon.hpp>

typedef struct {
    uint64_t       minAddress;
    uint64_t       maxAddress;
} DFPatternRange;

typedef struct {
    void*           basicBlock; // points to BB info?
    DFPatternType   type;
    uint64_t        rangeCnt;
    DFPatternRange* ranges;
} DFPatternInfo;

#endif /* _TracerMethods_h_ */
