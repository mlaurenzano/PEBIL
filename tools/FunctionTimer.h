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

#ifndef _FunctionTimer_h_
#define _FunctionTimer_h_

#include <InstrumentationTool.h>

class FunctionTimer : public InstrumentationTool {
protected:
    InstrumentationFunction* programEntry;
    InstrumentationFunction* programExit;

    InstrumentationFunction* functionEntry;
    InstrumentationFunction* functionExit;

public:
    FunctionTimer(ElfFile* elf);
    ~FunctionTimer() {}

    void declare();
    void instrument();

    const char* briefName() { return "FunctionTimer"; }
    const char* defaultExtension() { return "ftminst"; }
    uint32_t allowsArgs() { return PEBIL_OPT_NON; }
    uint32_t requiresArgs() { return PEBIL_OPT_NON; }
};

class ExternalFunctionTimer : public FunctionTimer {
public:
    ExternalFunctionTimer(ElfFile* elf);
    ~ExternalFunctionTimer() {}

    void declare();
    void instrument();

    const char* briefName() { return "ExternalFunctionTimer"; }
    const char* defaultExtension() { return "eftinst"; }
    uint32_t allowsArgs() { return PEBIL_OPT_INP; }
};

#endif /* _FunctionTimer_h_ */
