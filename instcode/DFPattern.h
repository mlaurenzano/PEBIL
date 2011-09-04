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

#ifndef _DFPattern_h_
#define _DFPattern_h_

#include <stdint.h>

typedef uint8_t DFPatternType;

typedef enum {
    dfTypePattern_undefined = 0,
    dfTypePattern_Other,
    dfTypePattern_Stream,
    dfTypePattern_Transpose,
    dfTypePattern_Random,
    dfTypePattern_Reduction,
    dfTypePattern_Stencil,
    dfTypePattern_Gather,
    dfTypePattern_Scatter,
    dfTypePattern_FunctionCallGS,
    dfTypePattern_Init,
    dfTypePattern_Default,
    dfTypePattern_Scalar,
    dfTypePattern_None,
    dfTypePattern_Total_Types
} DFPatternValues;

typedef enum {
    DFPattern_Inactive = 0,
    DFPattern_Active,
    DFPattern_Total_States
} DFPatternStates;

extern const char* DFPatternTypeNames[];

typedef struct {
    DFPatternType type;
    uint16_t      memopCnt;
} DFPatternSpec;

DFPatternType convertDFPattenType(char* patternString);

#endif
