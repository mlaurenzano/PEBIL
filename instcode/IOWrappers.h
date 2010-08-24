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

#ifdef PRELOAD_WRAPPERS
#define _GNU_SOURCE
#include <dlfcn.h>
#endif

#include <stdio.h>
#include <IOEvents.h>

#define __IO_BUFFER_SIZE 0x100000
#define __MAX_MESSAGE_SIZE 0x800
#define __MAX_FILE_NAMES 0x4000


#define GET_RECORD_SIZE(__record) (__record >> 8)
#define GET_RECORD_TYPE(__record) (__record & 0x000000ff)
#define RECORD_HEADER(__type, __size) (((__size << 8) & 0xffffff00) | __type)

typedef enum {
    IORecord_Invalid,
    IORecord_EventInfo,
    IORecord_FileName,
    IORecord_Total_Types
} IORecordType_t;
extern const char* IORecordTypeNames[IORecord_Total_Types];
const char* IORecordTypeNames[IORecord_Total_Types] = {
    "Invalid",
    "EventInfo",
    "FileName"
};

typedef struct {
    uint8_t  class;
    uint8_t  offset_class;
    uint8_t  handle_class;
    uint8_t  mode;
    uint16_t event_type;
    uint16_t handle_id;
    uint64_t unqid;
    uint64_t source;
    uint64_t size;
    uint64_t offset;
    uint64_t start_time;
    uint64_t end_time;
} EventInfo_t;

typedef struct {
    uint8_t handle_class;
    uint8_t access_type;
    uint16_t numchars;
    uint32_t handle;
    uint64_t event_id;
} IOFileName_t;

typedef enum {
    IOOffset_Invalid,
    IOOffset_SET,
    IOOffset_CUR,
    IOOffset_END,
    IOOffset_Total_Types
} IOOffsetClass_t;
extern const char* IOOffsetClassNames[IOOffset_Total_Types];
const char* IOOffsetClassNames[IOOffset_Total_Types] = {
    "Invalid",
    "SET",
    "CUR",
    "END"
};

uint8_t offsetOriginToClass(int origin){
    if (origin == SEEK_SET){
        return IOOffset_SET;
    } else if (origin == SEEK_CUR){
        return IOOffset_CUR;
    } else if (origin == SEEK_END){
        return IOOffset_END;
    }
    return IOOffset_Invalid;
}

typedef struct {
    FILE*    outFile;
    uint32_t size;
    uint32_t freeIdx;
    char     storage[__IO_BUFFER_SIZE];
} TraceBuffer_t;

typedef enum {
    IOHandle_Invalid,
    IOHandle_NAME,
    IOHandle_CLIB,
    IOHandle_POSX,
    IOHandle_MPIO,
    IOHandle_HDF5,
    IOHandle_FLIB,
    IOHandle_Total_Types
} IOHandleClasses;

typedef enum {
    IOFileAccess_Invalid,
    IOFileAccess_ONCE,
    IOFileAccess_OPEN,
    IOFileAccess_SYS,
    IOFileAccess_Total_Types
} IOFileAccess_t;
