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

#ifndef _Classification_h_
#define _Classification_h_

#include <InstrumentationTool.h>

class Classification : public InstrumentationTool {

private:
    InstrumentationFunction* binFunc;
    InstrumentationFunction* exitFunc;
    InstrumentationFunction* entryFunc;

public:
    Classification(ElfFile* elf, char* ext, bool lpi, bool dtl);
    ~Classification();

    void declare();
    void instrument();

    void addInt_Store(Vector<X86Instruction*>& instructions, int x, uint64_t store);

    const char* briefName() { return "Classifier"; }
};


#endif /* _Classification_h_ */
