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

#include <Loop.h>

#include <BasicBlock.h>
#include <BitSet.h>
#include <FlowGraph.h>
#include <Function.h>

uint32_t Loop::getNumberOfInstructions(){
    BasicBlock** allBlocks = new BasicBlock*[getNumberOfBlocks()];
    getAllBlocks(allBlocks);
    
    uint32_t icount = 0;
    for (uint32_t i = 0; i < getNumberOfBlocks(); i++){
        icount += allBlocks[i]->getNumberOfInstructions();
    }
    delete[] allBlocks;
    return icount;
}

bool Loop::containsCall(){
    BasicBlock** allBlocks = new BasicBlock*[getNumberOfBlocks()];
    getAllBlocks(allBlocks);
    for (uint32_t i = 0; i < getNumberOfBlocks(); i++){
        for (uint32_t j = 0; j < allBlocks[i]->getNumberOfInstructions(); j++){
            if (allBlocks[i]->getInstruction(j)->isCall()){
                delete[] allBlocks;
                return true;
            }
        }
    }
    delete[] allBlocks;
    return false;
}

int compareLoopEntry(const void* arg1, const void* arg2){
    Loop* lp1 = *((Loop**)arg1);
    Loop* lp2 = *((Loop**)arg2);

    ASSERT(lp1 && lp2 && "Symbols should exist");

    uint64_t vl1 = lp1->getHead()->getBaseAddress();
    uint64_t vl2 = lp2->getHead()->getBaseAddress();

    if(vl1 < vl2)
        return -1;
    else if(vl1 > vl2)
        return 1;
    else { //(vl1 == vl2)
        if (lp1->getHead()->getNumberOfBytes() < lp2->getHead()->getNumberOfBytes()){
            return -1;
        } else if (lp1->getHead()->getNumberOfBytes() > lp2->getHead()->getNumberOfBytes()){
            return 1;
        } else {
            return 0;
        }
    }

    return 0;
}


Loop::~Loop(){
    delete blocks;
}

Loop::Loop(BasicBlock* h, BasicBlock* t, FlowGraph* cfg, BitSet<BasicBlock*>* newBlocks) { 
    index = Invalid_UInteger_ID;
    head = h; 
    tail = t;
    flowGraph = cfg; 
    blocks = cfg->newBitSet();
    blocks->clear();
    for (uint32_t i = 0; i < cfg->getNumberOfBasicBlocks(); i++){
        if (newBlocks->contains(i)){
            blocks->insert(i);
        }
    }
    depth = 0;
}

void Loop::print(){
    PRINT_INFOR("Loop %d of function %s: Head %d (base %#llx), tail %d among %d blocks", 
                getIndex(), flowGraph->getFunction()->getName(), head->getIndex(),
                head->getBaseAddress(), tail->getIndex(), flowGraph->getNumberOfBasicBlocks());
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (blocks->contains(i)){
            BasicBlock* bb = flowGraph->getBasicBlock(i);
            PRINT_INFOR("\tMember Block %d: [%#llx,%#llx)", i, bb->getBaseAddress(), bb->getBaseAddress() + bb->getNumberOfBytes());
        }
    }
}

uint32_t Loop::getAllBlocks(BasicBlock** arr){
    ASSERT(arr != NULL);
    uint32_t arrIdx = 0;
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
    if(blocks->contains(i)){
        arr[arrIdx++] = flowGraph->getBasicBlock(i);
    }
    }
    return blocks->size();
}

bool Loop::hasSharedHeader(Loop* loop){
    return getHead()->getBaseAddress() == loop->getHead()->getBaseAddress();
}

bool Loop::isInnerLoopOf(Loop* loop){
    if (getNumberOfBlocks() > loop->getNumberOfBlocks()){
        return false;
    }
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (isBlockIn(i) && !loop->isBlockIn(i)){
            return false;
        }
    }
    return true;
}

bool Loop::isIdenticalLoop(Loop* loop){
    if (getNumberOfBlocks() != loop->getNumberOfBlocks()){
        return false;
    }
    if (getFlowGraph()->getFunction()->getBaseAddress() != loop->getFlowGraph()->getFunction()->getBaseAddress()){
        return false;
    }
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (isBlockIn(i) != loop->isBlockIn(i)){
            return false;
        }
    }
    return true;
}

