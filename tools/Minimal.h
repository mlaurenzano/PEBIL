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

#ifndef _Minimal_h_
#define _Minimal_h_

#include <InstrumentationTool.h>

// contains commented [psuedo]code that provides minimal/basic functionality

class Minimal : public InstrumentationTool {
    // put tool's member var/functions here
private:
    //InstrumentationFunction* someFunc;

public:
    Minimal(ElfFile* elf);
    ~Minimal() {}

    void declare();
    void instrument();

    const char* briefName() { return "Minimal"; }
    const char* defaultExtension() { return "mininst"; }

    uint32_t allowsArgs() { return PEBIL_OPT_NON; }
    uint32_t requiresArgs() { return PEBIL_OPT_NON; }
};


#endif /* _Minimal_h_ */
