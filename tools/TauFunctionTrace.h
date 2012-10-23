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

#ifndef _TauFunctionTrace_h_
#define _TauFunctionTrace_h_

#include <InstrumentationTool.h>

class TauInstrumentList;

class TauFunctionTrace : public InstrumentationTool {
protected:
    InstrumentationFunction* functionRegister;
    InstrumentationFunction* loopRegister;

    InstrumentationFunction* functionEntry;
    InstrumentationFunction* functionExit;

    TauInstrumentList* instrumentList;
public:
    TauFunctionTrace(ElfFile* elf);
    ~TauFunctionTrace();

    void declare();
    void instrument();

    const char* briefName() { return "TauFunctionTrace"; }
    const char* defaultExtension() { return "tautrc"; }
    uint32_t allowsArgs() { return PEBIL_OPT_INP | PEBIL_OPT_DOI; }
    uint32_t requiresArgs() { return PEBIL_OPT_NON; }
};

class TauInstrumentList : public FileList {
private:
    FileList* functions;
    FileList* loops;

public:
    TauInstrumentList(const char* filename, const char* beginInstr, const char* endInstr, const char* beginExcl, const char* endExcl);
    ~TauInstrumentList();

    bool loopMatches(char* str);
    bool functionMatches(char* str);
};

#endif /* _TauFunctionTrace_h_ */
