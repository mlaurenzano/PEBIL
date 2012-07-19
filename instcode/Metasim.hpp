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


// stuff in this file can be shared with both sides of any tool
#ifndef _Metasim_hpp_
#define _Metasim_hpp_

#include <iostream>
#include <set>

//#define debug(...) __VA_ARGS__
#define debug(...)

typedef uint64_t image_key_t;
typedef pthread_t thread_key_t;

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
    PointType_buffercheck,
    PointType_bufferinc,
    PointType_bufferfill,
    PointType_total
} PointTypes;

#define DYNAMIC_POINT_SIZE_LIMIT 128
typedef struct {
    uint64_t VirtualAddress;
    uint64_t ProgramAddress;
    uint64_t Key;
    uint64_t Flags;
    uint32_t Size;
    uint8_t  OppContent[DYNAMIC_POINT_SIZE_LIMIT];
    bool IsEnabled;
} DynamicInst;
static uint64_t CountDynamicInst = 0;
static DynamicInst* DynamicInst_ = NULL;

static void InitializeDynamicInstrumentation(uint64_t* count, DynamicInst** dyn){
    CountDynamicInst = *count;
    DynamicInst_ = *dyn;
}

static DynamicInst* GetDynamicInstPoint(uint32_t idx){
    assert(idx < CountDynamicInst);
    assert(DynamicInst_ != NULL);
    return &(DynamicInst_[idx]);
}

static void PrintDynamicPoint(DynamicInst* d){
    std::cout
        << "\t"
        << "\t" << "Key 0x" << std::hex << d->Key
        << "\t" << "Vaddr 0x" << std::hex << d->VirtualAddress
        << "\t" << "Oaddr 0x" << std::hex << d->ProgramAddress
        << "\t" << "Size " << std::dec << d->Size
        << "\t" << "Enabled " << (d->IsEnabled? "yes":"no")
        << "\n";
}

static void PrintDynamicPoints(){
    std::cout << "Printing " << std::dec << CountDynamicInst << " dynamic inst points" << "\n";
    for (uint32_t i = 0; i < CountDynamicInst; i++){
        PrintDynamicPoint(&DynamicInst_[i]);
    }
}

static void SetDynamicPointStatus(DynamicInst* d, bool state){

    uint8_t t[DYNAMIC_POINT_SIZE_LIMIT];
    memcpy(t, (uint8_t*)d->VirtualAddress, d->Size);
    memcpy((uint8_t*)d->VirtualAddress, d->OppContent, d->Size);
    memcpy(d->OppContent, t, d->Size);

    d->IsEnabled = state;

    //PrintDynamicPoint(d);
}

static void SetDynamicPoints(std::set<uint64_t>* keys, bool state){
    uint32_t count = 0;
    for (uint32_t i = 0; i < CountDynamicInst; i++){
        if (keys->count(DynamicInst_[i].Key) > 0){
            if (state != DynamicInst_[i].IsEnabled){
                count++;
                SetDynamicPointStatus(&DynamicInst_[i], state);
            }
        }
    }
    std::cout << "Thread " << std::hex << pthread_self() << " switched " << std::dec << count << " to " << (state? "on" : "off") << std::endl;
    debug(PrintDynamicPoints());
}

#endif //_Metasim_hpp_

