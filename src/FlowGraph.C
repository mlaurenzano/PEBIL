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

#include <FlowGraph.h>

#include <BasicBlock.h>
#include <BitSet.h>
#include <ElfFileInst.h>
#include <Function.h>
#include <LengauerTarjan.h>
#include <LinkedList.h>
#include <Loop.h>
#include <PriorityQueue.h>
#include <Stack.h>
#include <set>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>

using namespace std;

uint32_t loopXDefUseDist(uint32_t currDist, uint32_t funcSize){
    return currDist + funcSize;
}
uint32_t trueDefUseDist(uint32_t currDist, uint32_t funcSize){
    if (currDist > funcSize){
        return currDist - funcSize;
    }
    return currDist;
}

struct path {
    X86Instruction* ins;
    LinkedList<X86Instruction::ReachingDefinition*>* defs;

    path(X86Instruction * ins, LinkedList<X86Instruction::ReachingDefinition*>* defs)
        : ins(ins), defs(defs)
    {}
};

/* Return true iff
    there is a definition in list1 that would invalidate a location in list2
    That is, the value written by def is read by use.
*/
static bool anyDefsAreUsed(
    LinkedList<X86Instruction::ReachingDefinition*>* defList,
    LinkedList<X86Instruction::ReachingDefinition*>* useList){

    X86Instruction::ReachingDefinition* def, * use;

    LinkedList<X86Instruction::ReachingDefinition*>::Iterator it, it2;
    for (it = defList->begin(); it != defList->end(); it = it.next()) {
        def = *it;
        for (it2 = useList->begin(); it2 != useList->end(); it2 = it2.next()) {
            use = *it2;

            if (use->invalidatedBy(def)){
                /*printf("Instruction:\n");
                def->print();
                printf("\nDefines:\n");
                use->print();
                printf("\n");
                */
                return true;
            }
        }
    }

   return false; 
}

/*
  Return a list of all defines from list1 that are not invalidated
  by the defines in list2.
 
  If no defines are invalidated, return list1.
  If all defines are invalidated return NULL.
  Otherwise, create a new list containing the remaining valid defines.
*/
static LinkedList<X86Instruction::ReachingDefinition*>* removeInvalidated (
    LinkedList<X86Instruction::ReachingDefinition*>* list1,
    LinkedList<X86Instruction::ReachingDefinition*>* list2){

    LinkedList<X86Instruction::ReachingDefinition*>* retval =
        new LinkedList<X86Instruction::ReachingDefinition*>();

    X86Instruction::ReachingDefinition* def1, * def2;

    LinkedList<X86Instruction::ReachingDefinition*>::Iterator it, it2;
    for (it = list1->begin(); it != list1->end(); it = it.next()) {
        def1 = *it;
        bool invalidated = false;
        for (it2 = list2->begin(); it2 != list2->end(); it2 = it2.next()) {
            def2 = *it2;

            if (def1->invalidatedBy(def2)) {
                invalidated = true;
                break;
            }
        }

        if (!invalidated) {
            retval->insert(def1);
        }
    }

    if (retval->empty()) {
        delete retval;
        return NULL;
    }

    if (retval->size() == list1->size()) {
        delete retval;
        return list1;
    }

    return retval;
}

void FlowGraph::computeDefUseDist(){
    LinkedList<LinkedList<X86Instruction::ReachingDefinition*>*> defines =
        LinkedList<LinkedList<X86Instruction::ReachingDefinition*>*>();

    // For each loop
    for (uint32_t i = 0; i < loops.size(); ++i) {
        BasicBlock** allLoopBlocks = new BasicBlock*[loops[i]->getNumberOfBlocks()];
        loops[i]->getAllBlocks(allLoopBlocks);
        uint64_t loopLeader = loops[i]->getHead()->getBaseAddress();
        // For each block
        for (uint32_t j = 0; j < loops[i]->getNumberOfBlocks(); ++j){
            BasicBlock* bb = allLoopBlocks[j];
            bb->setDefXIter(0);
            // For each instruction
            for (uint32_t k = 0; k < bb->getNumberOfInstructions(); ++k){
                X86Instruction* ins = bb->getInstruction(k);

                // Skip the instruction if it can't define anything
                if (!ins->isIntegerOperation() && !ins->isFloatPOperation() && !ins->isMoveOperation()
                    || ins->isConditionCompare()) {
                    continue;
                }
                ASSERT(!ins->usesControlTarget());

                // Get defintions for this instruction: ins
                LinkedList<X86Instruction::ReachingDefinition*>* idefs;
                idefs = ins->getDefs();

                // Skip instruction if it doesn't define anything
                if (idefs == NULL)
                    continue;
                if (idefs->empty()) {
                    delete idefs;
                    continue;
                }

                // Otherwise, save the defs
                defines.insert(idefs);

                PriorityQueue<struct path*, uint32_t> paths = PriorityQueue<struct path*, uint32_t>();
                bool blockTouched[function->getNumberOfBasicBlocks()];
                bzero(&blockTouched, sizeof(bool) * function->getNumberOfBasicBlocks());

                // Initialize worklist with the path from this instruction
                // FIXME why only add paths within current loop?
                if (k == bb->getNumberOfInstructions() - 1){
                    ASSERT(ins->controlFallsThrough());
                    ASSERT(bb->getNumberOfTargets() == 1);
                    if (loops[i]->isBlockIn(bb->getTargetBlock(0)->getIndex())){
                        paths.insert(new path(bb->getTargetBlock(0)->getLeader(), idefs), 1);
                    } 
                } else {
                    paths.insert(new path(bb->getInstruction(k+1), idefs), 1);
                }

                // while there are paths in worklist
                while (!paths.isEmpty()) {

                    // take the shortest path in list
                    uint32_t currDist;
                    struct path* p = paths.deleteMin(&currDist);
                    X86Instruction* cand = p->ins;
                    idefs = p->defs;
                    delete p;

                    LinkedList<X86Instruction::ReachingDefinition*>* i2uses, *i2defs, *newdefs;
                    i2uses = cand->getUses();

                    // Check if any of idefs is used
                    if(i2uses != NULL && anyDefsAreUsed(idefs, i2uses)){

                        while (!i2uses->empty()) { delete i2uses->shift(); } delete i2uses;

                        // Check if use is shortest
                        uint32_t duDist;
                        duDist = trueDefUseDist(currDist, function->getNumberOfInstructions());
                        if (!ins->getDefUseDist() || ins->getDefUseDist() > duDist) {
                            ins->setDefUseDist(duDist);
                        }

                        // If dist has increased beyond size of function, we must be looping?
                        if (currDist > function->getNumberOfInstructions()) {
                            bb->setDefXIter(bb->getDefXIter()+1);
                            break;
                        }

                        // Stop searching along this path
                        continue;
                    }

                    // Check if any defines are overwritten
                    i2defs = cand->getDefs();
                    newdefs = removeInvalidated(idefs, i2defs);

                    while (!i2uses->empty()) { delete i2uses->shift(); } delete i2uses;
                    while (!i2defs->empty()) { delete i2defs->shift(); } delete i2defs;

                    // If all definitions killed, stop searching along this path
                    if (newdefs == NULL)
                        continue;

                    // If some definitions were killed, keep track of the new defs
                    if (newdefs != idefs)
                        defines.insert(newdefs);

                    // end of block that is a branch
                    if (cand->usesControlTarget() && !cand->isCall()){
                        BasicBlock* tgtBlock = function->getBasicBlockAtAddress(cand->getTargetAddress());
                        if (tgtBlock && loops[i]->isBlockIn(tgtBlock->getIndex()) && !blockTouched[tgtBlock->getIndex()]){
                            blockTouched[tgtBlock->getIndex()] = true;
                            if (tgtBlock->getBaseAddress() == loopLeader){
                                paths.insert(new path(tgtBlock->getLeader(), newdefs), loopXDefUseDist(currDist + 1, function->getNumberOfInstructions()));
                            } else {
                                paths.insert(new path(tgtBlock->getLeader(), newdefs), currDist + 1);
                            }
                        }
                    }

                    // non-branching control
                    if (cand->controlFallsThrough()){
                        BasicBlock* tgtBlock = function->getBasicBlockAtAddress(cand->getBaseAddress() + cand->getSizeInBytes());
                        if (tgtBlock && loops[i]->isBlockIn(tgtBlock->getIndex())){
                            X86Instruction* ftTarget = function->getInstructionAtAddress(cand->getBaseAddress() + cand->getSizeInBytes());
                            if (ftTarget){
                                if (ftTarget->isLeader()){
                                    if (!blockTouched[tgtBlock->getIndex()]){
                                        blockTouched[tgtBlock->getIndex()] = true;
                                        if (ftTarget->getBaseAddress() == loopLeader){
                                            paths.insert(new path(ftTarget, newdefs), loopXDefUseDist(currDist + 1, function->getNumberOfInstructions()));
                                        } else {
                                            paths.insert(new path(ftTarget, newdefs), currDist + 1);
                                        }
                                    }
                                } else {
                                    paths.insert(new path(ftTarget, newdefs), currDist + 1);
                                }
                            }
                        }
                    }
                }
                if (!paths.isEmpty()){
                    ins->setDefUseDist(0);
                }
                while (!paths.isEmpty()){
                    delete paths.deleteMin(NULL);
                }
            }
        }
        delete[] allLoopBlocks;
    }

    LinkedList<LinkedList<X86Instruction::ReachingDefinition*>*>::Iterator it1;
    for( it1 = defines.begin(); it1 != defines.end(); it1 = it1.next() ) {
        LinkedList<X86Instruction::ReachingDefinition*>::Iterator it2;
        for( it2 = (*it1)->begin();  it2 != (*it1)->end(); it2 = it2.next() ) {
            delete *it2;
        }
        delete *it1;
    }
}

/*
void FlowGraph::computeDefUseDist(){

    BitSet<uint32_t>* origuses = new BitSet<uint32_t>(X86_ALU_REGS);
    BitSet<uint32_t>* origdefs = new BitSet<uint32_t>(X86_ALU_REGS);
    BitSet<uint32_t>* uses = new BitSet<uint32_t>(X86_ALU_REGS);
    BitSet<uint32_t>* defs = new BitSet<uint32_t>(X86_ALU_REGS);
    
    //    PRINT_INFOR("computing def-use dist for function %s: %d loops", function->getName(), loops.size());
    //    print();
    for (uint32_t i = 0; i < loops.size(); i++){
        BasicBlock** allLoopBlocks = new BasicBlock*[loops[i]->getNumberOfBlocks()];
        loops[i]->getAllBlocks(allLoopBlocks);
        //        PRINT_INFOR("\tcomputing def-use dist for function %s loop %d", function->getName(), i);
        uint64_t loopLeader = loops[i]->getHead()->getBaseAddress();

        for (uint32_t j = 0; j < loops[i]->getNumberOfBlocks(); j++){
            BasicBlock* bb = allLoopBlocks[j];
            bb->setDefXIter(0);
            for (uint32_t k = 0; k < bb->getNumberOfInstructions(); k++){
                X86Instruction* ins = bb->getInstruction(k);

                if ((ins->isIntegerOperation() || ins->isFloatPOperation() || ins->isMoveOperation())
                    && !ins->isConditionCompare()){
                    ASSERT(!ins->usesControlTarget());

                    origuses->clear();
                    origdefs->clear();
                    uses->clear();
                    defs->clear();
                    ins->defsRegisters(origdefs);
                    ins->usesRegisters(origuses);

                    if (!origdefs->empty()){
                        //ins->print();
                        //origdefs->print();
                        //origuses->print();
                        
                        PriorityQueue<struct path*, uint32_t> paths = PriorityQueue<struct path*, uint32_t>();
                        bool blockTouched[function->getNumberOfBasicBlocks()];
                        bzero(&blockTouched, sizeof(bool) * function->getNumberOfBasicBlocks());
                        
                        if (k == bb->getNumberOfInstructions() - 1){
                            ASSERT(ins->controlFallsThrough());
                            ASSERT(bb->getNumberOfTargets() == 1);
                            if (loops[i]->isBlockIn(bb->getTargetBlock(0)->getIndex())){
                                paths.insert(new path(bb->getTargetBlock(0)->getLeader(), NULL), 1);
                            }
                        } else {
                            paths.insert(new path(bb->getInstruction(k+1), NULL), 1);
                        }
                        
                        while (!paths.isEmpty()){
                            uint32_t currDist;
                            struct path * p = paths.deleteMin(&currDist);
                            X86Instruction* cand = p->ins;
                            delete p;
                            uses->clear();
                            cand->usesRegisters(uses);
                            defs->clear();
                            cand->defsRegisters(defs);
                            //PRINT_INFOR("\t\tExamining instructon %llx, %d", cand->getBaseAddress(), currDist);
                            //                            uses->print();
                            (*uses) &= (*origdefs);
                            (*defs) &= (*origdefs);

                            BasicBlock* candblock = function->getBasicBlockAtAddress(cand->getBaseAddress());

                            if (!uses->empty()){
                                //PRINT_INFOR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                                //PRINT_INFOR("Found use of reg at distance %d...", currDist);
                                //cand->print();
                                //PRINT_INFOR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                                // we only want to store the shortest DU dist for now
                                if (!ins->getDefUseDist() || ins->getDefUseDist() > trueDefUseDist(currDist, function->getNumberOfInstructions())){
                                    ins->setDefUseDist(trueDefUseDist(currDist, function->getNumberOfInstructions()));
                                }
                                if (currDist > function->getNumberOfInstructions()){
                                    //PRINT_INFOR("Found defxiter @ %#llx flowing to %#llx for block %#llx", ins->getBaseAddress(), cand->getBaseAddress(), bb->getBaseAddress());
                                    bb->setDefXIter(bb->getDefXIter()+1);
                                    break;
                                }
                            }
                            
                            // this instruction squashes the register(s) we're looking for, so we stop looking on this path
                            if (!defs->empty()){
                                //   defs->print();
                                //PRINT_INFOR("re-def");
                                continue;
                            }
                            
                            // end of block that is a branch
                            if (cand->usesControlTarget() && !cand->isCall()){
                                BasicBlock* tgtBlock = function->getBasicBlockAtAddress(cand->getTargetAddress());
                                if (tgtBlock && loops[i]->isBlockIn(tgtBlock->getIndex()) && !blockTouched[tgtBlock->getIndex()]){
                                    blockTouched[tgtBlock->getIndex()] = true;
                                    if (tgtBlock->getBaseAddress() == loopLeader){
                                        paths.insert(new path(tgtBlock->getLeader(), NULL), loopXDefUseDist(currDist + 1, function->getNumberOfInstructions()));
                                    } else {
                                        paths.insert(new path(tgtBlock->getLeader(), NULL), currDist + 1);
                                    }
                                }
                            }

                            // non-branching control
                            if (cand->controlFallsThrough()){
                                BasicBlock* tgtBlock = function->getBasicBlockAtAddress(cand->getBaseAddress() + cand->getSizeInBytes());
                                if (tgtBlock && loops[i]->isBlockIn(tgtBlock->getIndex())){
                                    X86Instruction* ftTarget = function->getInstructionAtAddress(cand->getBaseAddress() + cand->getSizeInBytes());
                                    if (ftTarget){
                                        if (ftTarget->isLeader()){
                                            if (!blockTouched[tgtBlock->getIndex()]){
                                                blockTouched[tgtBlock->getIndex()] = true;
                                                if (ftTarget->getBaseAddress() == loopLeader){
                                                    paths.insert(new path(ftTarget, NULL), loopXDefUseDist(currDist + 1, function->getNumberOfInstructions()));
                                                } else {
                                                    paths.insert(new path(ftTarget, NULL), currDist + 1);
                                                }
                                            }
                                        } else {
                                            paths.insert(new path(ftTarget, NULL), currDist + 1);
                                        }
                                        
                                    }
                                }
                            }
                            
                        }
                        if (!paths.isEmpty()){
                            ins->setDefUseDist(0);
                        }
                    }

                }
            }
        }

        delete[] allLoopBlocks;
    }
    delete origuses;
    delete origdefs;
    delete uses;
    delete defs;
}
*/

void FlowGraph::interposeBlock(BasicBlock* bb){
    ASSERT(bb->getNumberOfSources() == 1 && bb->getNumberOfTargets() == 1);
    BasicBlock* sourceBlock = bb->getSourceBlock(0);
    BasicBlock* targetBlock = bb->getTargetBlock(0);

    bool linkFound = false;
    for (uint32_t i = 0; i < sourceBlock->getNumberOfTargets(); i++){
        if (sourceBlock->getTargetBlock(i)->getIndex() == targetBlock->getIndex()){
            linkFound = true;
            break;
        }
    }

    if (!linkFound){
        print();
        sourceBlock->print();
        targetBlock->print();        
    }

    ASSERT(linkFound && "There should be a source -> target block relationship between the blocks passed to this function");

    ASSERT(sourceBlock->getBaseAddress() + sourceBlock->getNumberOfBytes() != targetBlock->getBaseAddress() && "Source shouldn't fall through to target");

    bb->setBaseAddress(blocks.back()->getBaseAddress() + blocks.back()->getNumberOfBytes());
    bb->setIndex(basicBlocks.size());
    basicBlocks.append(bb);
    /*
    PRINT_INFOR("now there are %d bbs in function %s", basicBlocks.size(), function->getName());
    PRINT_INFOR("new block has base addres %#llx", bb->getBaseAddress());
    */
    blocks.append(bb);

    sourceBlock->removeTargetBlock(targetBlock);
    sourceBlock->addTargetBlock(bb);
    targetBlock->removeSourceBlock(sourceBlock);
    targetBlock->addSourceBlock(bb);

    X86Instruction* jumpToTarget = bb->getLeader();
    jumpToTarget->initializeAnchor(targetBlock->getLeader());
    jumpToTarget->setBaseAddress(blocks.back()->getBaseAddress() + blocks.back()->getSizeInBytes());
    jumpToTarget->setIndex(0);
    ASSERT(sourceBlock->getExitInstruction() && sourceBlock->getExitInstruction()->getAddressAnchor());
    sourceBlock->getExitInstruction()->getAddressAnchor()->updateLink(jumpToTarget);

    /*
    bb->print();
    bb->printInstructions();
    */
}

bool FlowGraph::isBlockInLoop(uint32_t idx){
    for (uint32_t i = 0; i < loops.size(); i++){
        if (loops[i]->isBlockIn(idx)){
            return true;
        }
    }
    return false;
}

Loop* FlowGraph::getInnermostLoopForBlock(uint32_t idx){
    Loop* loop = NULL;
    for (uint32_t i = 0; i < loops.size(); i++){
        if (loops[i]->isBlockIn(idx)){
            if (loop){
                if (loops[i]->getNumberOfBlocks() < loop->getNumberOfBlocks()){
                    loop = loops[i];
                }
            } else {
                loop = loops[i];
            }
        }
    }
    return loop;
}

Loop* FlowGraph::getOuterMostLoopForLoop(uint32_t idx){
    Loop* input = loops[idx];
    while (input->getIndex() != getOuterLoop(input->getIndex())->getIndex()){
        input = getOuterLoop(input->getIndex());
    }
    return input;
}

Loop* FlowGraph::getOuterLoop(uint32_t idx){
    Loop* input = loops[idx];
    for (uint32_t i = 0; i < loops.size(); i++){
        if (input->isInnerLoopOf(loops[i])){
            return loops[i];
        }
    }
    return input;
}

Loop* FlowGraph::getParentLoop(uint32_t idx){
    Loop* input = loops[idx];
    for (uint32_t i = 0; i < loops.size(); i++){
        if (input->isInnerLoopOf(loops[i])){
            if (getLoopDepth(loops[idx]->getHead()->getIndex()) == getLoopDepth(loops[i]->getHead()->getIndex()) + 1){
                return loops[i];
            }
        }
    }
    return input;
}

uint32_t FlowGraph::getLoopDepth(uint32_t idx){
    Loop* loop = getInnermostLoopForBlock(idx);
    uint32_t depth = 0;

    if (loop){
        depth++;
        for (uint32_t i = 0; i < loops.size(); i++){
            if (loop->getIndex() != i){
                if (loop->isInnerLoopOf(loops[i])){
                    depth++;
                }
            }
        }
    }
    return depth;
}

void FlowGraph::computeLiveness(){
    DEBUG_LIVE_REGS(double t1 = timer();)
    Vector<BitSet<uint32_t>*> uses;
    Vector<BitSet<uint32_t>*> defs;
    Vector<BitSet<uint32_t>*> ins;
    Vector<BitSet<uint32_t>*> outs;
    Vector<BitSet<uint32_t>*> ins_prime;
    Vector<BitSet<uint32_t>*> outs_prime;
    Vector<std::set<uint32_t>*> succs;
    Vector<X86Instruction*> allInstructions;
    uint32_t maxElts = 32;

    // re-index instructions
    uint32_t currIdx = 0;
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = getBasicBlock(i);
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* instruction = bb->getInstruction(j);
            instruction->setIndex(currIdx++);
        }
    }
    PRINT_DEBUG_LIVE_REGS("Flow analysis on function %s (%d instructions)", function->getName(), currIdx);

    // initialize data structures
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = getBasicBlock(i);
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* instruction = bb->getInstruction(j);
            uses.append(instruction->getUseRegs());
            defs.append(instruction->getDefRegs());
            ins.append(new BitSet<uint32_t>(maxElts));
            outs.append(new BitSet<uint32_t>(maxElts));
            ins_prime.append(new BitSet<uint32_t>(maxElts));
            outs_prime.append(new BitSet<uint32_t>(maxElts));

            allInstructions.append(instruction);

            succs.append(new std::set<uint32_t>());
            if (j == bb->getNumberOfInstructions() - 1){
                for (uint32_t k = 0; k < bb->getNumberOfTargets(); k++){
                    succs.back()->insert(bb->getTargetBlock(k)->getLeader()->getIndex());
                }
            } else {
                succs.back()->insert(bb->getInstruction(j+1)->getIndex());
            }
        }
    }
    ASSERT(allInstructions.size() == currIdx);
    ASSERT(uses.size() == currIdx);
    ASSERT(defs.size() == currIdx);
    ASSERT(ins.size() == currIdx);
    ASSERT(outs.size() == currIdx);
    ASSERT(ins_prime.size() == currIdx);
    ASSERT(outs_prime.size() == currIdx);
    ASSERT(succs.size() == currIdx);

    DEBUG_LIVE_REGS(
                    for (uint32_t i = 0; i < allInstructions.size(); i++){
                        allInstructions[i]->print();
                        PRINT_INFO();
                        PRINT_OUT("instruction %d succ list: ", i);
                        for (std::set<uint32_t>::const_iterator it = succs[i]->begin(); it != succs[i]->end(); it++){
                            PRINT_OUT("%d ", (*it));
                        }
                        PRINT_OUT("\n");
                        PRINT_REG_LIST(uses, maxElts, i);
                        PRINT_REG_LIST(defs, maxElts, i);
                    }
                    )

    bool setsSame = false;
    uint32_t iterCount = 0;
    while (!setsSame){
        for (int32_t i = allInstructions.size()-1; i >= 0; i--){
            // ins'[n] = ins[n]
            *(ins_prime[i]) = *(ins[i]);
            
            // outs'[n] = outs[n]
            *(outs_prime[i]) = *(outs[i]);
            
            PRINT_DEBUG_LIVE_REGS("before in[n] = use[n] U (out[n] - def[n])");
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(uses, maxElts, i);
            PRINT_REG_LIST(defs, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);
            
            // out[n] = U(s in succ[n]) in[s]
            //            (outs[i])->clear();
            for (std::set<uint32_t>::const_iterator it = succs[i]->begin(); it != succs[i]->end(); it++){
                *(outs[i]) |= *(ins[(*it)]);
                PRINT_REG_LIST(ins, maxElts, (*it));
            }
            
            PRINT_DEBUG_LIVE_REGS("after out[n] = U(s in succ[n]) in[s]");
            PRINT_REG_LIST(outs, maxElts, i);
            // in[n] = use[n] U (out[n] - def[n])
            BitSet<uint32_t>* tmpbt = new BitSet<uint32_t>(*(outs[i]));
            *(tmpbt) -= *(defs[i]);
            *(ins[i]) |= *(uses[i]);
            *(ins[i]) |= *(tmpbt);
            delete tmpbt;
            
            PRINT_DEBUG_LIVE_REGS("after in[n] = use[n] U (out[n] - def[n])");
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);
            
        }

        // check if in/out have changed this iteration for any n
        setsSame = true;
        for (uint32_t i = 0; i < allInstructions.size() && setsSame; i++){
            if (!(*(ins[i]) == *(ins_prime[i]))){
                PRINT_DEBUG_LIVE_REGS("ins %d different", i);
                setsSame = false;
                break;
            }
            if (!(*(outs[i]) == *(outs_prime[i]))){
                PRINT_DEBUG_LIVE_REGS("outs %d different", i);
                setsSame = false;
                break;
            }
        }

        for (uint32_t i = 0; i < allInstructions.size(); i++){
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(ins_prime, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);
            PRINT_REG_LIST(outs_prime, maxElts, i);
        }
        iterCount++;
    }

    for (uint32_t i = 0; i < allInstructions.size(); i++){
        allInstructions[i]->setLiveIns(ins[i]);
        allInstructions[i]->setLiveOuts(outs[i]);
    }

    for (uint32_t i = 0; i < allInstructions.size(); i++){
        delete uses[i];
        delete defs[i];
        delete ins[i];
        delete outs[i];
        delete ins_prime[i];
        delete outs_prime[i];
        delete succs[i];
    }

    DEBUG_LIVE_REGS(
                    double t2 = timer();
                    PRINT_INFOR("___timer: LiveRegAnalysis function %s -- %d instructions, %d iterations: %.4f seconds", function->getName(), currIdx, iterCount, t2-t1);
                    );
}

bool FlowGraph::verify(){
    if (blocks.size()){
        if (blocks[0]->getBaseAddress() != function->getBaseAddress()){
            PRINT_ERROR("First block of flowGraph should begin at function start");
            return false;
        }
        if (blocks.back()->getBaseAddress()+blocks.back()->getNumberOfBytes() != function->getBaseAddress()+function->getSizeInBytes()){
            PRINT_ERROR("Flowgraph of function %s: last block of flowGraph should end (%#llx) at function end (%#llx)", 
                        function->getName(), blocks.back()->getBaseAddress()+blocks.back()->getNumberOfBytes(),
                        function->getBaseAddress()+function->getSizeInBytes());
            return false;
        }
    }
    for (int32_t i = 0; i < blocks.size()-1; i++){
        if (blocks[i]->getBaseAddress()+blocks[i]->getNumberOfBytes() != blocks[i+1]->getBaseAddress()){
            PRINT_ERROR("Blocks %d and %d in FlowGraph should be adjacent -- %#llx != %#llx", i, i+1, blocks[i]->getBaseAddress()+blocks[i]->getNumberOfBytes(), blocks[i+1]->getBaseAddress());
            return false;
        }
    }
    for (uint32_t i = 0; i < blocks.size(); i++){
        if (!blocks[i]->verify()){
            return false;
        }
    }
    return true;
}

void FlowGraph::addBlock(Block* block){
    if (block->getType() == PebilClassType_BasicBlock){
        basicBlocks.append((BasicBlock*)block);
    }
    blocks.insertSorted(block,compareBaseAddress);
}

void FlowGraph::setBaseAddress(uint64_t newBaseAddr){
    uint64_t currentOffset = 0;
    for (uint32_t i = 0; i < blocks.size(); i++){
        blocks[i]->setBaseAddress(newBaseAddr+currentOffset);
        currentOffset += blocks[i]->getNumberOfBytes();
    }
}

uint32_t FlowGraph::getNumberOfBytes(){
    uint32_t numberOfBytes = 0;
    for (uint32_t i = 0; i < blocks.size(); i++){
        numberOfBytes += blocks[i]->getNumberOfBytes();
    }
    return numberOfBytes;
}

uint32_t FlowGraph::getNumberOfInstructions(){
    uint32_t numberOfInstructions = 0;
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        numberOfInstructions += basicBlocks[i]->getNumberOfInstructions();
    }
    return numberOfInstructions;
}

void FlowGraph::connectGraph(BasicBlock* entry){
    ASSERT(entry);
    entry->setEntry();

    basicBlocks.sort(compareBaseAddress);

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        basicBlocks[i]->setIndex(i);
        for (uint32_t j = 0; j < basicBlocks[i]->getNumberOfInstructions(); j++){
            basicBlocks[i]->getInstruction(j)->setIndex(j);
        }
    }

    uint64_t* addressCache = new uint64_t[basicBlocks.size()];
    uint64_t* targetAddressCache = new uint64_t[basicBlocks.size()];

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        addressCache[i] = basicBlocks[i]->getBaseAddress();
        targetAddressCache[i] = basicBlocks[i]->getTargetAddress();
        PRINT_DEBUG_CFG("caching block addresses %#llx -> %#llx", addressCache[i], targetAddressCache[i]);
    }


    // detect incoming and outgoing edges
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->findExitInstruction()){
            PRINT_DEBUG_CFG("Setting block %d as exit block", i);
            basicBlocks[i]->setExit();
        }
        if (basicBlocks[i]->controlFallsThrough() && i+1 < basicBlocks.size()){
            if (targetAddressCache[i] != addressCache[i+1]){
                PRINT_DEBUG_CFG("Adding adjacent blocks to list %d -> %d", i, i+1);
                basicBlocks[i]->addTargetBlock(basicBlocks[i+1]);
                basicBlocks[i+1]->addSourceBlock(basicBlocks[i]);
            }
        }
        if (function->inRange(targetAddressCache[i])){
            for (uint32_t j = 0; j < basicBlocks.size(); j++){
                if (targetAddressCache[i] == addressCache[j]){
                    PRINT_DEBUG_CFG("Adding jump target to list %d(%llx) -> %d(%llx)", i, addressCache[i], j, targetAddressCache[i]);
                    basicBlocks[j]->addSourceBlock(basicBlocks[i]);
                    basicBlocks[i]->addTargetBlock(basicBlocks[j]);
                }
            }
        }
    }

    delete[] addressCache;
    delete[] targetAddressCache;

    
    // determine which blocks are reachable
    BitSet<BasicBlock*>* edgeSet = newBitSet();
    edgeSet->setall();
    depthFirstSearch(getEntryBlock(),edgeSet,false);

    uint32_t unreachableCount = edgeSet->size(); /* all members with their bit set are unvisited ones **/
    if(unreachableCount){
        BasicBlock** unreachableBlocks = edgeSet->duplicateMembers();
        for(uint32_t i = 0; i < unreachableCount; i++){
            unreachableBlocks[i]->setNoPath();
            PRINT_DEBUG_CFG("\tBlock %d at %#llx is unreachable",unreachableBlocks[i]->getIndex(), unreachableBlocks[i]->getBaseAddress());
        }
        delete[] unreachableBlocks;
    }
    PRINT_DEBUG_CFG("******** Found %d unreachable blocks for function %s",unreachableCount,getFunction()->getName());
    delete edgeSet;

}

void FlowGraph::printLoops(){
    if (loops.size()){
        PRINT_INFOR("Flowgraph @ %#llx has %d loops", basicBlocks[0]->getBaseAddress(), loops.size());
    }
    for (uint32_t i = 0; i < loops.size(); i++){
        loops[i]->print();
    }
}

void FlowGraph::printInnerLoops(){
    for (uint32_t i = 0; i < loops.size(); i++){
        for (uint32_t j = 0; j < loops.size(); j++){
            if (i != j && loops[j]->isInnerLoopOf(loops[i])){
                PRINT_INFOR("Loop %d is inside loop %d", j, i);
            }
            if (i == j){
                ASSERT(loops[i]->isIdenticalLoop(loops[j]));
            } else {
                ASSERT(!loops[i]->isIdenticalLoop(loops[j]));
            }
        }
    }
}

int compareLoopHeaderVaddr(const void* arg1,const void* arg2){
    Loop* lp1 = *((Loop**)arg1);
    Loop* lp2 = *((Loop**)arg2);
    uint64_t vl1 = lp1->getHead()->getBaseAddress();
    uint64_t vl2 = lp2->getHead()->getBaseAddress();

    if(vl1 < vl2)
        return -1;
    if(vl1 > vl2)
        return 1;
    return 0;
}

BasicBlock** FlowGraph::getAllBlocks(){
    BasicBlock** allBlocks = new BasicBlock*[basicBlocks.size()];
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        allBlocks[i] = basicBlocks[i];
    }
    return allBlocks;
}

uint32_t FlowGraph::buildLoops(){

    ASSERT(!loops.size());
    PRINT_DEBUG_LOOP("Considering flowgraph for function %d -- has %d blocks", function->getIndex(),  basicBlocks.size());

    BasicBlock** allBlocks = new BasicBlock*[basicBlocks.size()]; 
    getAllBlocks(basicBlocks.size(), allBlocks);

    LinkedList<BasicBlock*> backEdges;
    BitSet <BasicBlock*>* visitedBitSet = newBitSet();
    BitSet <BasicBlock*>* completedBitSet = newBitSet();

    depthFirstSearch(allBlocks[0], visitedBitSet, true, completedBitSet, &backEdges);

    delete[] allBlocks;
    delete visitedBitSet;
    delete completedBitSet;

    if(backEdges.empty()){
        PRINT_DEBUG_LOOP("\t%d Contains %d loops (back edges) from %d", getIndex(),loops.size(),basicBlocks.size());
        return 0;
    }

    ASSERT(!(backEdges.size() % 2) && "Fatal: Back edge list should be multiple of 2, (from->to)");
    BitSet<BasicBlock*>* inLoop = newBitSet();
    Stack<BasicBlock*> loopStack(basicBlocks.size());
    LinkedList<Loop*> loopList;

    uint32_t numberOfLoops = 0;
    while(!backEdges.empty()){

        BasicBlock* from = backEdges.shift();
        BasicBlock* to = backEdges.shift();
        ASSERT(from && to && "Fatal: Backedge end points are invalid");

        if(from->isDominatedBy(to)){
            /* for each back edge found, perform natural loop finding algorithm 
               from pg. 604 of the Aho/Sethi/Ullman (Dragon) compiler book */

            numberOfLoops++;

            loopStack.clear();
            inLoop->clear();

            inLoop->insert(to->getIndex());
            if (!inLoop->contains(from->getIndex())){
                inLoop->insert(from->getIndex());
                loopStack.push(from);
            }
            while(!loopStack.empty()){
                BasicBlock* top = loopStack.pop();
                uint32_t numberOfSources = top->getNumberOfSources();
                for (uint32_t m = 0; m < numberOfSources; m++){
                    BasicBlock* pred = top->getSourceBlock(m);
                    if (!inLoop->contains(pred->getIndex())){
                        inLoop->insert(pred->getIndex());
                        loopStack.push(pred);
                    }
                }
            }

            Loop* newLoop = new Loop(to, from, this, inLoop);
            loopList.insert(newLoop);

            DEBUG_LOOP(newLoop->print();)
        }
    }

    ASSERT((loopList.size() == numberOfLoops) && 
        "Fatal: Number of loops should match backedges defining them");

    delete inLoop;

    PRINT_DEBUG_LOOP("\t%d Contains %d loops (back edges) from %d", getIndex(),numberOfLoops,basicBlocks.size());

    if (numberOfLoops){
        uint32_t i = 0;
        while (!loopList.empty()){
            loops.append(loopList.shift());
        }
        qsort(&loops,loops.size(),sizeof(Loop*),compareLoopEntry);
        for (i=0; i < loops.size(); i++){
            loops[i]->setIndex(i);
        }
    }
    ASSERT(loops.size() == numberOfLoops);

    DEBUG_LOOP(printInnerLoops());
}

BasicBlock* FlowGraph::getEntryBlock(){
    BasicBlock* entryBlock = NULL;
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->isEntry()){
            ASSERT(!entryBlock && "There should not be multiple entry blocks to the same graph");
            entryBlock = basicBlocks[i];
        }
    }
    return entryBlock;
}

Vector<BasicBlock*>* FlowGraph::getExitBlocks(){
    Vector<BasicBlock*>* exitBlocks = new Vector<BasicBlock*>();
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        if (basicBlocks[i]->isExit()){
            (*exitBlocks).append(basicBlocks[i]);
        } else if (!getFunction()->inRange(basicBlocks[i]->getBaseAddress())){
            (*exitBlocks).append(basicBlocks[i]);
        }        
    }
    return exitBlocks;
}


TextSection* FlowGraph::getTextSection(){
    return function->getTextSection();
}

uint32_t FlowGraph::getIndex() { 
    return function->getIndex(); 
}

uint32_t FlowGraph::getAllBlocks(uint32_t sz, BasicBlock** arr){
    ASSERT(sz == basicBlocks.size());
    for (uint32_t i = 0; i < basicBlocks.size(); i++)
        arr[i] = basicBlocks[i];
    return basicBlocks.size();
}

void FlowGraph::findMemoryFloatOps(){
    if(!basicBlocks.size())
        return;

    for(uint32_t i=0;i<basicBlocks.size();i++){
        basicBlocks[i]->findMemoryFloatOps();
    }
}


void FlowGraph::print(){
    PRINT_INFOR("[G(idx %5d) (#bb %6d) (unq %#12llx)",
            getIndex(),basicBlocks.size(),function->getHashCode().getValue());

    if(!basicBlocks.size()){
        PRINT_INFOR("]");
        return;
    }

    for(uint32_t i=0;i<basicBlocks.size();i++){
        basicBlocks[i]->print();
    }

    PRINT_INFOR("]");

    for (uint32_t i = 0; i < loops.size(); i++){
        //        loops[i]->print();
    }
}

BitSet<BasicBlock*>* FlowGraph::newBitSet() { 

    BasicBlock** blocks = getAllBlocks();
    blockCopies.append(blocks);

    if(basicBlocks.size())
        return new BitSet<BasicBlock*>(basicBlocks.size(),blocks); 
    return NULL;
}

FlowGraph::~FlowGraph(){
    for (uint32_t i = 0; i < blockCopies.size(); i++){
        delete[] blockCopies[i];
    }
    for (uint32_t i = 0; i < loops.size(); i++){
        delete loops[i];
    }
    for (uint32_t i = 0; i < blocks.size(); i++){
        delete blocks[i];
    }
}

void FlowGraph::setImmDominatorBlocks(BasicBlock* root){

    if(!root){
        /** Here find the entry node to the CFG **/
        ASSERT(basicBlocks.size());
        root = basicBlocks[0];
    }
    ASSERT(root);
    ASSERT(root->isEntry() && "Fatal: The root node should be valid and entry to cfg");
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        ASSERT(basicBlocks[i]);
    }

    BasicBlock** allBlocks = getAllBlocks();
    LengauerTarjan dominatorAlg(getNumberOfBasicBlocks(),root,allBlocks);
    dominatorAlg.immediateDominators();
    delete[] allBlocks;
}

void FlowGraph::depthFirstSearch(BasicBlock* root, BitSet<BasicBlock*>* visitedSet, bool visitedMarkOnSet,
                                 BitSet<BasicBlock*>* completedSet, LinkedList<BasicBlock*>* backEdges)
{

    if(visitedMarkOnSet){
        visitedSet->insert(root->getIndex());
    } else {
        visitedSet->remove(root->getIndex());
    }

    uint32_t numberOfTargets = root->getNumberOfTargets();
    for(uint32_t i=0;i<numberOfTargets;i++){
        BasicBlock* target = root->getTargetBlock(i);

        if(visitedMarkOnSet != visitedSet->contains(target->getIndex())){
            depthFirstSearch(target,visitedSet,visitedMarkOnSet,completedSet,backEdges);
        } else if(backEdges && completedSet && 
                  (visitedMarkOnSet != completedSet->contains(target->getIndex()))) 
        {
            backEdges->insert(target);
            backEdges->insert(root);
        }
    }

    if(completedSet){
        if(visitedMarkOnSet){
            completedSet->insert(root->getIndex());
        } else {
            completedSet->remove(root->getIndex());
        }
    }
}

uint32_t FlowGraph::getNumberOfMemoryOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<basicBlocks.size();i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumberOfMemoryOps();
    }
    return ret;
}

uint32_t FlowGraph::getNumberOfFloatOps() {
    uint32_t ret = 0;
    for(uint32_t i=0;i<basicBlocks.size();i++){
        BasicBlock* bb = basicBlocks[i];
        ret += bb->getNumberOfFloatOps();
    }
    return ret;
}
