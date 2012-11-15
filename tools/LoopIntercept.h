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

#ifndef _LoopIntercept_h_
#define _LoopIntercept_h_

#include <InstrumentationTool.h>

class LoopIntercept : public InstrumentationTool {
private:
    InstrumentationFunction* loopEntry;
    InstrumentationFunction* loopExit;
    InstrumentationFunction* programEntry;
    InstrumentationFunction* programExit;

    Vector<char*>* loopList;

    uint64_t getLoopHash(uint32_t idx);

    bool discoveryMode;
    void discoverAllLoops();

public:
    LoopIntercept(ElfFile* elf);
    ~LoopIntercept();

    void declare();
    void instrument();

    const char* briefName() { return "LoopIntercept"; }
    const char* defaultExtension() { return "lpiinst"; }
    const char* getExtension() { if (discoveryMode) return "loops"; return defaultExtension(); }
    uint32_t allowsArgs() { return PEBIL_OPT_INP; }
};

#endif /* _LoopIntercept_h_ */
