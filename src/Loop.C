/* This program is free software: you can redistribute it and/or modify
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
    for (uint32_t i = 0; i < flowGraph->getNumberOfBasicBlocks(); i++){
        if (isBlockIn(i) != loop->isBlockIn(i)){
            return false;
        }
    }
    return true;
}

