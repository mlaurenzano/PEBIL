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

#ifndef _Metasim_hpp_
#define _Metasim_hpp_

#define METASIM_ID "Metasim"
#define METASIM_VERSION "3.0.0"
#define METASIM_ENV "PEBIL_ROOT"

#define TAB "\t"
#define ENDL "\n"
#define DISPLAY_ERROR cerr << "[" << METASIM_ID << "-r" << getTaskId() << "] " << "Error: "
#define warn cerr << "[" << METASIM_ID << "-r" << getTaskId() << "] " << "Warning: "
#define ErrorExit(__msg, __errno) DISPLAY_ERROR << __msg << endl << flush; exit(__errno);
#define inform cout << "[" << METASIM_ID << "-r" << getTaskId() << "] "

//#define debug(...) __VA_ARGS__
#define debug(...)

enum MetasimErrors {
    MetasimError_None = 0,
    MetasimError_MemoryAlloc,
    MetasimError_NoThread,
    MetasimError_TooManyInsnReads,
    MetasimError_StringParse,
    MetasimError_FileOp,
    MetasimError_Env,
    MetasimError_Total,
};

typedef enum {
    CounterType_undefined = 0,
    CounterType_instruction,
    CounterType_basicblock,
    CounterType_loop,
    CounterType_function,
    CounterType_total
} CounterTypes;

static const char* CounterTypeNames[CounterType_total] = {
    "undefined",
    "instruction",
    "basicblock",
    "loop"
    "function"
};

typedef enum {
    PointType_undefined = 0,
    PointType_blockcount,
    PointType_bufferfill,
    PointType_buffercheck,
    PointType_total
} PointTypes;

#endif //_Metasim_hpp_

