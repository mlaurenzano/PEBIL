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

#ifndef _CacheSimulation_h_
#define _CacheSimulation_h_

#include <InstrumentationTool.h>
#include <SimpleHash.h>


class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;
    InstrumentationFunction* entryFunc;

    SimpleHash<BasicBlock*> blocksToInst;
    SimpleHash<NestedLoopStruct*> nestedLoopGrouping;
    SimpleHash<uint64_t> mapBBToGroupId;

    Vector<Base*> allBlocks;
    Vector<uint32_t> allBlockIds;
    Vector<LineInfo*> allBlockLineInfos;

    void filterBBs();
    void includeLoopBlocks(BasicBlock*);
    void instrumentEntryPoint();
    void instrumentExitPoint();
    void insertBlockCounterAndBufferClear(BasicBlock*,X86Instruction*,uint64_t,uint32_t,SimulationStats&,
        uint64_t, uint64_t);
    void instrumentMemop(Function*,BasicBlock*,X86Instruction*,uint64_t,uint64_t,uint32_t,SimulationStats&,
        uint32_t, uint32_t,uint32_t,uint64_t,uint32_t,uint64_t);
    void initializeLineInfo(SimulationStats&, Function*, BasicBlock*, uint32_t, uint64_t);
    void writeStaticFile();

    inline bool usePIC() { return isThreadedMode() || isMultiImage(); }
public:
    CacheSimulation(ElfFile* elf);
    ~CacheSimulation();

    void declare();
    void instrument();

    const char* briefName() { return "CacheSimulation"; }
    const char* defaultExtension() { return "siminst"; }
    uint32_t allowsArgs() { return PEBIL_OPT_LPI | PEBIL_OPT_DTL | PEBIL_OPT_PHS | PEBIL_OPT_DFP; }
    uint32_t requiresArgs() { return PEBIL_OPT_INP; }
};


#endif /* _CacheSimulation_h_ */
