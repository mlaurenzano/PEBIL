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

#include <Minimal.h>

// contains commented [psuedo]code that provides minimal/basic functionality

extern "C" {
    InstrumentationTool* MinimalMaker(ElfFile* elf){
        return new Minimal(elf);
    }
}

Minimal::Minimal(ElfFile* elf)
    : InstrumentationTool(elf)
{
}

void Minimal::declare(){
    //InstrumentationTool::declare();

    // declare any shared library that will contain instrumentation functions
    //declareLibrary(SOME_LIB_NAME);

    // declare any instrumentation functions that will be used
    //someFunc = declareFunction(SOME_FUNCTION);
    //ASSERT(someFunc && "Cannot find some function, are you sure it was declared?");
}

void Minimal::instrument(){
    //InstrumentationTool::instrument();
}
