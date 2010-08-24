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

#ifndef _LengauerTarjan_h_
#define _LengauerTarjan_h_

#include <LinkedList.h>

class BasicBlock;

class LengauerTarjan {
private:
    uint32_t              reachableCount;
    uint32_t              nodeCount;
    uint32_t              rootLoc;

    uint32_t              *dom;
    uint32_t              *parent; 
    uint32_t              *ancestor;
    uint32_t              *child;
    uint32_t              *vertex;
    uint32_t              *label;
    uint32_t              *semi;
    uint32_t              *size;

    LinkedList<uint32_t>* bucket;

    BasicBlock**     locToBasicBlock;
    uint32_t*        basicBlockToLoc;

    void       depthFirstSearch(uint32_t vertexV,uint32_t* dfsNo);
    void       COMPRESS(uint32_t vertexV);
    uint32_t   EVAL(uint32_t vertexV);
    void       LINK(uint32_t vertexV,uint32_t vertexW);

public:

    LengauerTarjan(uint32_t blockCount, BasicBlock* root, BasicBlock** blocks); 
    ~LengauerTarjan();
    void immediateDominators();
};

#endif
