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

#ifndef _FlowGraph_h_
#define _FlowGraph_h_

#include <BitSet.h>
#include <Function.h>
#include <LinkedList.h>
#include <Vector.h>

class BasicBlock;
class Block;
class Loop;
class TextSection;

class FlowGraph {
protected:

    Function* function;

    Vector<Block*> blocks; // contains both BasicBlocks and RawBlocks
    Vector<BasicBlock*> basicBlocks; // only BasicBlocks
    Vector<Loop*> loops;

    Vector<BasicBlock**> blockCopies;
public:
    FlowGraph(Function* f) : function(f) {}
    ~FlowGraph();

    void setBaseAddress(uint64_t newBaseAddress);
    void computeLiveness();
    void computeDefUseDist();

    uint32_t getNumberOfInstructions();
    TextSection* getTextSection();
    void print();
    uint32_t getIndex();
    Function* getFunction() { return function; }
    void connectGraph(BasicBlock* entry);
    
    void assignSequenceNumbers();
    
    void findMemoryFloatOps();
    
    BitSet<BasicBlock*>* newBitSet();
    
    BasicBlock* getBasicBlock(uint32_t idx) { return basicBlocks[idx]; }
    Block* getBlock(uint32_t idx) { return blocks[idx]; }
    uint32_t getNumberOfBasicBlocks() { return basicBlocks.size(); }
    uint32_t getNumberOfBlocks() { return blocks.size(); }
    uint32_t getNumberOfBytes();
    
    uint32_t getNumberOfMemoryOps();
    uint32_t getNumberOfFloatOps();
    
    BasicBlock* getEntryBlock();
    Vector<BasicBlock*>* getExitBlocks();

    Loop* getLoop(uint32_t idx) { return loops[idx]; }
    uint32_t getLoopDepth(uint32_t idx);
    uint32_t getNumberOfLoops() { return loops.size(); }
    uint32_t buildLoops();
    void printInnerLoops();
    void printLoops();
    bool isBlockInLoop(uint32_t idx);
    Loop* getInnermostLoopForBlock(uint32_t idx);
    Loop* getOuterMostLoopForLoop(uint32_t idx);
    Loop* getParentLoop(uint32_t idx);
    Loop* getOuterLoop(uint32_t idx);

    void addBlock(Block* block);    
    
    BasicBlock** getAllBlocks();
    uint32_t getAllBlocks(uint32_t sz, BasicBlock** arr);
    
    void depthFirstSearch(BasicBlock* root,BitSet<BasicBlock*>* visitedSet,bool set,
                          BitSet<BasicBlock*>* completedSet=NULL,LinkedList<BasicBlock*>* backEdges=NULL);
    
    void setImmDominatorBlocks(BasicBlock* root=NULL);
    void interposeBlock(BasicBlock* bb);

    bool verify();
    void wedge(uint32_t shamt);
};

#endif /* _FlowGraph_h_ */
