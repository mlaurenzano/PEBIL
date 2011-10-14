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

#ifndef _InstrumentationTool_h_
#define _InstrumentationTool_h_

#include <ElfFileInst.h>
#include <Instrumentation.h>
#include <X86Instruction.h>

class InstrumentationPoint;

#define INFO_UNKNOWN "__info_unknown__"

typedef struct 
{
    int64_t pt_vaddr;
    int64_t pt_target;
    int64_t pt_flags;
    int32_t pt_size;
    int32_t pt_blockid;
    unsigned char pt_content[16];
    unsigned char pt_disable[16];
} instpoint_info;

class InstrumentationTool : public ElfFileInst {
protected:
    void printStaticFile(Vector<BasicBlock*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allBlockLineInfos, uint32_t bufferSize);
    void printStaticFilePerInstruction(Vector<X86Instruction*>* allInstructions, Vector<uint32_t>* allInstructionIds, Vector<LineInfo*>* allInstructionLineInfos, uint32_t bufferSize);

    InstrumentationPoint* insertInlinedTripCounter(uint64_t, Base*);

    InstrumentationFunction* initWrapperC;
    InstrumentationFunction* initWrapperF;

    uint32_t phaseNo;
    char* extension;
    bool loopIncl;
    bool printDetail;
public:
    InstrumentationTool(ElfFile* elf, char* ext, uint32_t phase, bool lpi, bool dtl);
    ~InstrumentationTool() { }

    virtual void declare();
    virtual void instrument();
    virtual void usesModifiedProgram() { }

    virtual const char* briefName() { __SHOULD_NOT_ARRIVE; }
};


#endif /* _InstrumentationTool_h_ */
