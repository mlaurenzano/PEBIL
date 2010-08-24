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
#include <DFPattern.h>

class CacheSimulation : public InstrumentationTool {
private:
    InstrumentationFunction* simFunc;
    InstrumentationFunction* exitFunc;
    InstrumentationFunction* entryFunc;

    char* bbFile;
    char* dfPatternFile;

    SimpleHash<BasicBlock*> blocksToInst;
    SimpleHash<DFPatternType> dfpSet;

    Vector<DFPatternSpec> dfpBlocks;
    uint32_t dfpTaggedBlockCnt;

    Vector<InstrumentationPoint*> memInstPoints;
    Vector<uint32_t> memInstBlockIds;
    uint64_t instPointInfo;

    void filterBBs();
    void printDFPStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allLineInfos);

public:
    CacheSimulation(ElfFile* elf, char* inputName, char* ext, uint32_t phase, bool lpi, bool dtl, char* dfpFile);
    ~CacheSimulation();

    void declare();
    void instrument();
    void usesModifiedProgram();

    const char* briefName() { return "CacheSimulation"; }
};


#endif /* _CacheSimulation_h_ */
