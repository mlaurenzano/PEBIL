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

#ifndef _Loop_h_
#define _Loop_h_

#include <Base.h>
#include <BitSet.h>
#include <Function.h>

extern int compareLoopEntry(const void* arg1,const void* arg2);

class Loop : public Base {
protected:
    uint32_t index;
    FlowGraph* flowGraph;
    BitSet<BasicBlock*>* blocks;
    BasicBlock* head;
    BasicBlock* tail;
public:
    Loop(BasicBlock* h, BasicBlock* t, FlowGraph* cfg, BitSet<BasicBlock*>* newBlocks);
    ~Loop();
    BasicBlock* getHead() { return head; }
    BasicBlock* getTail() { return tail; }
    uint32_t getNumberOfBlocks() { return blocks->size(); }
    uint32_t getAllBlocks(BasicBlock** arr);
    bool isBlockIn(uint32_t idx) { return blocks->contains(idx); }
    bool hasSharedHeader(Loop* loop);
    bool isInnerLoopOf(Loop* loop);
    bool isIdenticalLoop(Loop* loop);
    void print();
    void setIndex(uint32_t idx) { index = idx; }
    uint32_t getIndex() { return index; }
    FlowGraph* getFlowGraph() { return flowGraph; }
    bool containsCall();
};

#endif // _Loop_h_

