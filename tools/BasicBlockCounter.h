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

#ifndef _BasicBlockCounter_h_
#define _BasicBlockCounter_h_

#include <InstrumentationTool.h>

class BasicBlockCounter : public InstrumentationTool {
protected:
    InstrumentationFunction* entryFunc;
    InstrumentationFunction* exitFunc;

    bool loopCount;
public:
    BasicBlockCounter(ElfFile* elf);
    ~BasicBlockCounter() {}

    void declare();
    void instrument();

    const char* briefName() { return "BasicBlockCounter"; }
    const char* defaultExtension() { return "jbbinst"; }
    uint32_t allowsArgs() { return PEBIL_OPT_LPI | PEBIL_OPT_DTL; }
    uint32_t requiresArgs() { return PEBIL_OPT_NON; }
};

class RareEventCounter : public BasicBlockCounter {
private:
    InstrumentationFunction* entryRare;
    InstrumentationFunction* exitRare;    

    InstrumentationFunction* checkFunc;
    InstrumentationFunction* checkInit;

    uint64_t matchArray;
    uint64_t rareArray;
    uint64_t matchCountAddress;

    void insertPointCheck(Base* point, uint32_t checkIdx, InstLocations loc);

public:
    RareEventCounter(ElfFile* elf);
    ~RareEventCounter() {}

    void declare();
    void instrument();

    const char* briefName() { return "RareEventCounter"; }
    const char* defaultExtension() { if (doIntro) { return "step2"; } else { return "step1"; } }
    uint32_t allowsArgs() { return PEBIL_OPT_DOI; }
};

#endif /* _BasicBlockCounter_h_ */
