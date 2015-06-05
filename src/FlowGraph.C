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
#include <X86Instruction.h>
#include <X86InstructionFactory.h>

#include <set>
#include <vector>
#include <stack>

using namespace std;

void FlowGraph::wedge(uint32_t shamt){
    for (uint32_t i = 0; i < blocks.size(); i++){
        blocks[i]->setBaseAddress(blocks[i]->getBaseAddress() + shamt);
    }
}

static uint32_t getRegId(enum ud_type reg) {
    if(IS_16BIT_GPR(reg)) {
        return reg + 32;
    } else if(IS_32BIT_GPR(reg)) {
        return reg + 16;
    }

    return reg;
}

struct RegisterStatePrediction {
    // current instruction and index in block
    X86Instruction* ins;
    uint32_t idx;

    // Register state
    std::pebil_map_type<uint32_t, RuntimeValue> state;

    std::stack<RuntimeValue> stack;

    // copy constructor
    RegisterStatePrediction(RegisterStatePrediction& other) {
        // copy register state
        state = other.state;
        stack = other.stack;
    }

    RegisterStatePrediction() {}
};

// Merge possible value states from newState into oldState
// Return true if changes were made
static bool mergeStates(struct RegisterStatePrediction* oldState, struct RegisterStatePrediction* newState) {
    if(oldState->ins != newState->ins) {
        printf("ERROR? Trying to merge states for different instructions! 0x%llx 0x%llx\n", newState->ins, oldState->ins);
        newState->ins->print();
        oldState->ins->print();
    }
    assert(oldState->ins == newState->ins);
    assert(oldState->idx == newState->idx);

    bool changesMade = false;

    // Update old register states
    for(std::pebil_map_type<uint32_t, RuntimeValue>::iterator it = oldState->state.begin(); it != oldState->state.end(); ++it) {
        uint32_t reg = it->first;
        RuntimeValue v = it->second;

        // If the confidence is already Maybe, it will always be Maybe
        if(v.confidence == Maybe) {
            newState->state.erase(reg);
            continue;
        }

        // Since, it must be Definitely, check against value in new state
        // Drop the confidence if newState has a different value or lower/missing confidence
        std::pebil_map_type<uint32_t, RuntimeValue>::iterator newVit = newState->state.find(reg);
        if(newVit != newState->state.end()) {
            RuntimeValue newV = newVit->second;
            if(newV.confidence == Definitely && v.value == newV.value) {
                // Do nothing
            } else {
                v.confidence = Maybe;
            }
            newState->state.erase(newVit);
        } else {
            v.confidence = Maybe;
        }

        // If the confidence changed, update it
        if(v.confidence == Maybe) {
            changesMade = true;
            oldState->state[reg] = v;
        }
    }

    // Merge any new register states
    for(std::pebil_map_type<uint32_t, RuntimeValue>::iterator it = newState->state.begin(); it != newState->state.end(); ++it) {
        uint32_t reg = it->first;
        RuntimeValue v = it->second;

        // since this value wasn't removed, it must be new to old state and confidence must be Maybe
        assert(oldState->state.count(reg) == 0);
        changesMade = true;
        v.confidence = Maybe;
        oldState->state[reg] = v;
    }

    // Merge stack states FIXME assume no change for now
    return changesMade;
}

static RuntimeValue getValueOfOperand(OperandX86* src, RegisterStatePrediction* item) {
    if(src->getType() == UD_OP_IMM)
        return {Definitely, src->getValue()};
    else if(src->getType() == UD_OP_REG) {
        if(item->state.find(getRegId(src->GET(base))) != item->state.end()) {
            RuntimeValue val = item->state[getRegId(src->GET(base))];
            return val;
        }
    }

    return {Unknown, 0};
}

// Determine the values of vector mask registers for all instructions
// This is a forward moving worklist algorithm using constant value propogation
// to statically predict the values of registers
void FlowGraph::computeVectorMasks(){

    //fprintf(stderr, "Computing vector masks for function %s\n", function->getName());
    std::pebil_map_type<X86Instruction*, struct RegisterStatePrediction*> instruction_states;

    // build maps of addresses -> instructions/blocks
    std::pebil_map_type<uint64_t, X86Instruction*> imap;
    std::pebil_map_type<uint64_t, BasicBlock*> bmap;
    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        BasicBlock* bb = basicBlocks[i];
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* x = bb->getInstruction(j);
            for (uint32_t k = 0; k < x->getSizeInBytes(); k++){
                ASSERT(imap.count(x->getBaseAddress() + k) == 0);
                ASSERT(bmap.count(x->getBaseAddress() + k) == 0);
                imap[x->getBaseAddress() + k] = x;
                bmap[x->getBaseAddress() + k] = bb;
            }
        }
    }

    // The worklist is a queue
    // priorities are unused, should be all 0
    PriorityQueue<RegisterStatePrediction*, int> worklist = PriorityQueue<struct RegisterStatePrediction*, int>();

    // get first instruction
    X86Instruction* firstIns = basicBlocks[0]->getInstruction(0);
    assert(firstIns != NULL);

    // make an initial register state
    struct RegisterStatePrediction* first = new RegisterStatePrediction();
    first->ins = firstIns;
    first->idx = 0;
    first->state[getRegId(UD_R_K0)] = {Definitely, 0xFFFF};
    worklist.insert(first, 0);

    // Initialize worklist with first instruction
    // while worklist has items:
    //   remove i from worklist
    //   i.vectorInfo.k1val = Predict(i, k1)
    //   add targets of i to worklist
    while(!worklist.isEmpty()) {
        int unused;

        // Get input register state
        struct RegisterStatePrediction* item = worklist.deleteMin(&unused);
        X86Instruction* ins = item->ins;

        // Check if there is an existing state and end this path if states are the same
        if(instruction_states.count(ins) > 0) {
            struct RegisterStatePrediction* oldState = instruction_states[ins];
            assert(ins == oldState->ins);
            bool different = mergeStates(oldState, item);
            delete item;
            if(!different) {
                continue;
            }
            item = oldState;
        }
        instruction_states[ins] = item;
        assert(item->ins == ins);

        // Set the mask register value
        // if mask register is k0, hardwired to 0xffff
        if(ins->GET(vector_mask_register) == UD_R_K0) {
            ins->setKRegister({Definitely, 0xffff});

        // otherwise search in input state
        } else if(ins->GET(vector_mask_register) != 0) {
            ins->setKRegister(item->state[ins->GET(vector_mask_register)]);
        }

        // Update state
        RegisterStatePrediction* nextItem = new RegisterStatePrediction(*item);

        // update to registers
        OperandX86* dest = ins->getDestOperand();
        if(dest && dest->getType() == UD_OP_REG) {
            RuntimeValue value;

            // Moves
            if(ins->isMoveOperation() && ins->getSourceOperand(0) != NULL) {
                OperandX86* src = ins->getSourceOperand(0);
                value = getValueOfOperand(src, nextItem);

            // Stack pops
            } else if(ins->isStackPop()) {
                if(!nextItem->stack.empty()) {
                    value = nextItem->stack.top();
                    nextItem->stack.pop();
                } else {
                    value = {Unknown, 0};
                }

            }
            // Other instructions
            else {
                switch(ins->GET(mnemonic)) {
                case UD_Iinc:
                {
                    RuntimeValue oldval = nextItem->state[getRegId(dest->GET(base))];
                    //fprintf(stderr, "Updating with inc: %d, %d\n", oldval.value, oldval.confidence);
                    if(oldval.confidence == Definitely) {
                        value = {Definitely, oldval.value + 1};
                    }
                    break;
                }
                case UD_Ikand: {
                    OperandX86* src = ins->getSourceOperand(1);
                    RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                    RuntimeValue oldVal = nextItem->state[getRegId(dest->GET(base))];
                    if(srcVal.confidence == Definitely && oldVal.confidence == Definitely){
                        value = {Definitely, srcVal.value & oldVal.value};
                    } else {
                        value = {Unknown, 0};
                    }
                    break;
                }
                case UD_Ikandn: {
                    OperandX86* src = ins->getSourceOperand(1);
                    RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                    RuntimeValue oldVal = nextItem->state[getRegId(dest->GET(base))];
                    if(srcVal.confidence == Definitely && oldVal.confidence == Definitely){
                        value = {Definitely, srcVal.value & (~oldVal.value)};
                    } else {
                        value = {Unknown, 0};
                    }
                    break;
                }
                case UD_Ikandnr: {
                    OperandX86* src = ins->getSourceOperand(1);
                    RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                    RuntimeValue oldVal = nextItem->state[getRegId(dest->GET(base))];
                    if(srcVal.confidence == Definitely && oldVal.confidence == Definitely){
                        value = {Definitely, (~srcVal.value) & oldVal.value};
                    } else {
                        value = {Unknown, 0};
                    }
                    break;
                }
                //case UD_Ikconcath:
                //case UD_Ikconcatl:
                //case UD_Ikextract:
                //case UD_Ikmerge2l1h:
                //case UD_Ikmerge2l1l:
                case UD_Iknot: {
                    OperandX86* src = ins->getSourceOperand(1); // FIXME pretending to have two source operands
                    RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                    fprintf(stderr, "Doing knot with src val %d %d\n", srcVal.confidence, srcVal.value);
                    src->print();
                    dest->print();
                    if(srcVal.confidence == Definitely){
                        value = {Definitely, ~srcVal.value};
                    } else {
                        value = {Unknown, 0};
                    }
                    break;
                }
                case UD_Ikor: {
                    OperandX86* src = ins->getSourceOperand(1);
                    RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                    RuntimeValue oldVal = nextItem->state[getRegId(dest->GET(base))];
                    if(srcVal.confidence == Definitely && oldVal.confidence == Definitely){
                        value = {Definitely, srcVal.value | oldVal.value };
                    } else {
                        value = {Unknown, 0};
                    }

                }
                //case UD_Ikortest:
                case UD_Ikxnor: {
                    OperandX86* src = ins->getSourceOperand(1);
                    RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                    RuntimeValue oldVal = nextItem->state[getRegId(dest->GET(base))];
                    if(srcVal.confidence == Definitely && oldVal.confidence == Definitely){
                        value = {Definitely, ~(srcVal.value ^ oldVal.value) };
                    } else if(src->GET(base) == dest->GET(base)){
                        value = {Definitely, 0xFFFF};
                    } else {
                        value = {Unknown, 0};
                    }
                }
                case UD_Ikxor:
                case UD_Ixor:
                {
                    OperandX86* src = ins->getSourceOperand(1);
                    if(src->getType() == UD_OP_REG && dest->GET(base) == src->GET(base)) {
                        value = {Definitely, 0};
                    } else {
                        RuntimeValue srcVal = getValueOfOperand(src, nextItem);
                        RuntimeValue oldVal = nextItem->state[getRegId(dest->GET(base))];
                        if(srcVal.confidence == Definitely && oldVal.confidence == Definitely){
                            value = {Definitely, srcVal.value ^ oldVal.value};
                        } else {
                            value = {Unknown, 0};
                        }
                    }
                    break;
                }
                case UD_Ivcmppd:
                case UD_Ivcmpps:
                {
                    // Check for equality comparisons between the same register
                    Vector<OperandX86*>* sources = ins->getSourceOperands();
                    SwizzleOperation swiz = ins->getSwizzleOperation();
                    if( swiz == 0 && 
                       (*sources)[0]->getType() == UD_OP_REG &&
                       (*sources)[1]->getType() == UD_OP_REG ) {

                        uint8_t cmpOp = (*sources)[2]->getValue();

                        switch(cmpOp) {

                        // eq, le, nlt
                        case 0:
                        case 2:
                        case 5: value = {Definitely, 0xFF}; break;

                        // lt, ne, nle
                        case 1:
                        case 4:
                        case 6: value = {Definitely, 0}; break;

                        case 3: // FIXME Unord
                        case 7: // ord
                        default:
                            value = {Unknown, 0};
                        }
                    } else {
                        value = {Unknown, 0};
                    }
                    break;
                }
                default:
                    value = {Unknown, 0};
                    break;
                }
            }

            //fprintf(stderr, "Writing %d, %d to register: %d\n", value.confidence, value.value, getRegId(dest->GET(base)));
            nextItem->state[getRegId(dest->GET(base))] = value;

        // Updates to stack
        } else if(ins->isStackPush()) {
            //ins->print();
            RuntimeValue value;
            OperandX86* src = ins->getSourceOperand(0);
            if(src == NULL) {
                value = {Unknown, 0};
                //fprintf(stderr, "Pushing unknown onto stack\n");
            } else {
                value = getValueOfOperand(src, nextItem);
            }
            nextItem->stack.push(value);

        }

        // Implicit writes to mask register
        if(ins->GET(mnemonic) >= UD_Ivgatherdpd && ins->GET(mnemonic) <= UD_Ivgatherpf1dps ||
           ins->GET(mnemonic) >= UD_Ivscatterdpd && ins->GET(mnemonic) <= UD_Ivscatterpf1dps ||
           ins->GET(mnemonic) == UD_Ivpscatterdd || ins->GET(mnemonic) == UD_Ivpscatterdq ||
           ins->GET(mnemonic) == UD_Ivpgatherdd || ins->GET(mnemonic) == UD_Ivpgatherdq) {
            nextItem->state[ins->GET(vector_mask_register)].confidence = Maybe;
        }

        // add targets of i to worklist
        bool passed = false;

        // fallthrough targets
        if(ins->controlFallsThrough()) {
            BasicBlock* tgtBlock = bmap[ins->getBaseAddress() + ins->getSizeInBytes()];
            if(tgtBlock) {
                X86Instruction* ftTarget = imap[ins->getBaseAddress() + ins->getSizeInBytes()];
                if(ftTarget) {
                    // fall through to new block
                    if(ftTarget->isLeader()) {
                            nextItem->ins = ftTarget;
                            nextItem->idx = 0;
                            worklist.insert(nextItem, 0);
                            passed = true;
                    // fall through to next instruction
                    } else {
                        nextItem->ins = ftTarget;
                        nextItem->idx = 0;
                        worklist.insert(nextItem, 0);
                        passed = true;
                    }
                }
            }
        }

        // targets of branches/calls within this function
        if(ins->usesControlTarget() && bmap.count(ins->getTargetAddress()) > 0) {
            BasicBlock* tgtBlock = bmap[ins->getTargetAddress()];
            if(tgtBlock) {
                if(!passed) {
                    // pass this item and avoid copying state
                    nextItem->ins = tgtBlock->getLeader();
                    nextItem->idx = 0;
                    worklist.insert(nextItem, 0);
                    passed = true;
                } else {
                    // copy state
                    nextItem = new RegisterStatePrediction(*nextItem);
                    nextItem->ins = tgtBlock->getLeader();
                    nextItem->idx = 0;
                    worklist.insert(nextItem, 0);
                    passed = true;
                }
            }
        }

        if(!passed) {
            delete nextItem;
        }
    }

    for(std::pebil_map_type<X86Instruction*, struct RegisterStatePrediction*>::iterator it = instruction_states.begin(); it != instruction_states.end(); ++it) {
        delete it->second;
    }
}


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

bool flowsInDefUseScope(BasicBlock* tgt, Loop* loop){
    // outside loops, scope includes the entire function
    if (loop == NULL){
        return true;
    }

    // inside loops, only look at loop contents
    if (loop->isBlockIn(tgt->getIndex())){
        return true;
    }

    return false;
}

inline void singleDefUse(FlowGraph* fg, X86Instruction* ins, BasicBlock* bb, Loop* loop,
                         std::pebil_map_type<uint64_t, X86Instruction*>& imap,
                         std::pebil_map_type<uint64_t, BasicBlock*>& bmap,
                         std::pebil_map_type<uint64_t, LinkedList<X86Instruction::ReachingDefinition*>*>& alliuses,
                         std::pebil_map_type<uint64_t, LinkedList<X86Instruction::ReachingDefinition*>*>& allidefs,
                         int k, uint64_t loopLeader, uint32_t fcnt){

    // Get defintions for this instruction: ins
    LinkedList<X86Instruction::ReachingDefinition*>* idefs = ins->getDefs();
    LinkedList<X86Instruction::ReachingDefinition*>* allDefs = idefs;

    // Skip instruction if it doesn't define anything
    if (idefs == NULL) {
        return;
    }

    if (idefs->empty()) {
        delete idefs;
        return;
    }

    set<LinkedList<X86Instruction::ReachingDefinition*>*> allDefLists;
    allDefLists.insert(idefs);

    PriorityQueue<struct path*, uint32_t> paths = PriorityQueue<struct path*, uint32_t>();
    bool blockTouched[fg->getFunction()->getNumberOfBasicBlocks()];
    bzero(&blockTouched, sizeof(bool) * fg->getFunction()->getNumberOfBasicBlocks());

    // Initialize worklist with the path from this instruction
    // Only take paths inside the loop. Since the definitions are in a loop, uses in the loop will be most relevant.
    if (k == bb->getNumberOfInstructions() - 1){
        ASSERT(ins->controlFallsThrough());
        if (bb->getNumberOfTargets() > 0){
            ASSERT(bb->getNumberOfTargets() == 1);
            if (flowsInDefUseScope(bb->getTargetBlock(0), loop)){
                // Path flows to the first instruction of the next block
                paths.insert(new path(bb->getTargetBlock(0)->getLeader(), idefs), 1);
            } 
        }
    } else {
        // path flows to the next instruction in this block
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
        i2uses = alliuses[cand->getBaseAddress()];

        // Check if any of idefs is used
        if(i2uses != NULL && anyDefsAreUsed(idefs, i2uses)){

            // Check if use is shortest
            uint32_t duDist;
            duDist = trueDefUseDist(currDist, fcnt);
            if (!ins->getDefUseDist() || ins->getDefUseDist() > duDist) {
                ins->setDefUseDist(duDist);
            }

            // If dist has increased beyond size of function, we must be looping?
            if (currDist > fcnt) {
                ins->setDefXIter();
                break;
            }

            // Stop searching along this path
            continue;
        }

        // Check if any defines are overwritten
        i2defs = allidefs[cand->getBaseAddress()];
        newdefs = removeInvalidated(idefs, i2defs);

        // If all definitions killed, stop searching along this path
        if (newdefs == NULL)
            continue;

        allDefLists.insert(newdefs);

        // end of block that is a branch
        if (cand->usesControlTarget() && !cand->isCall()){
            BasicBlock* tgtBlock = bmap[cand->getTargetAddress()];
            if (tgtBlock && !blockTouched[tgtBlock->getIndex()] && flowsInDefUseScope(tgtBlock, loop)){
                blockTouched[tgtBlock->getIndex()] = true;
                if (tgtBlock->getBaseAddress() == loopLeader){
                    paths.insert(new path(tgtBlock->getLeader(), newdefs), loopXDefUseDist(currDist + 1, fcnt));
                } else {
                    paths.insert(new path(tgtBlock->getLeader(), newdefs), currDist + 1);
                }
            }
        }

        // non-branching control
        if (cand->controlFallsThrough()){
            BasicBlock* tgtBlock = bmap[cand->getBaseAddress() + cand->getSizeInBytes()];
            if (tgtBlock && flowsInDefUseScope(tgtBlock, loop)){
                X86Instruction* ftTarget = imap[cand->getBaseAddress() + cand->getSizeInBytes()];
                if (ftTarget){
                    if (ftTarget->isLeader()){
                        if (!blockTouched[tgtBlock->getIndex()]){
                            blockTouched[tgtBlock->getIndex()] = true;
                            if (ftTarget->getBaseAddress() == loopLeader){
                                paths.insert(new path(ftTarget, newdefs), loopXDefUseDist(currDist + 1, fcnt));
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

    while (!allDefs->empty()){
        delete allDefs->shift();
    }
    for(set<LinkedList<X86Instruction::ReachingDefinition*>*>::iterator it = allDefLists.begin(); it != allDefLists.end(); ++it){
        delete *it;
    }
}


void FlowGraph::computeDefUseDist(){
    uint32_t fcnt = function->getNumberOfInstructions();
    std::pebil_map_type<uint64_t, X86Instruction*> imap;
    std::pebil_map_type<uint64_t, BasicBlock*> bmap;
    std::pebil_map_type<uint64_t, LinkedList<X86Instruction::ReachingDefinition*>*> alliuses;
    std::pebil_map_type<uint64_t, LinkedList<X86Instruction::ReachingDefinition*>*> allidefs;

    for (uint32_t i = 0; i < basicBlocks.size(); i++){
        BasicBlock* bb = basicBlocks[i];
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* x = bb->getInstruction(j);
            for (uint32_t k = 0; k < x->getSizeInBytes(); k++){
                ASSERT(imap.count(x->getBaseAddress() + k) == 0);
                ASSERT(bmap.count(x->getBaseAddress() + k) == 0);
                imap[x->getBaseAddress() + k] = x;
                bmap[x->getBaseAddress() + k] = bb;
            }

            ASSERT(alliuses.count(x->getBaseAddress()) == 0);
            ASSERT(allidefs.count(x->getBaseAddress()) == 0);
            alliuses[x->getBaseAddress()] = x->getUses();
            allidefs[x->getBaseAddress()] = x->getDefs();
        }
    }

    // For each loop
    for (uint32_t i = 0; i < loops.size(); ++i) {
        BasicBlock** allLoopBlocks = new BasicBlock*[loops[i]->getNumberOfBlocks()];
        loops[i]->getAllBlocks(allLoopBlocks);
        uint64_t loopLeader = loops[i]->getHead()->getBaseAddress();
        // For each block
        for (uint32_t j = 0; j < loops[i]->getNumberOfBlocks(); ++j){
            BasicBlock* bb = allLoopBlocks[j];

            // For each instruction
            for (uint32_t k = 0; k < bb->getNumberOfInstructions(); ++k){
                X86Instruction* ins = bb->getInstruction(k);

                // Skip the instruction if it can't define anything
                if (!ins->isIntegerOperation() && !ins->isFloatPOperation() && !ins->isMoveOperation()) {
                    continue;
                }
                ASSERT(!ins->usesControlTarget());

                singleDefUse(this, ins, bb, loops[i], imap, bmap, alliuses, allidefs, k, loopLeader, fcnt);
            }
        }
        delete[] allLoopBlocks;
    }

    // handle all non-loop blocks
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = basicBlocks[i];
        if (!isBlockInLoop(bb->getIndex())){
            for (uint32_t k = 0; k < bb->getNumberOfInstructions(); ++k){
                X86Instruction* ins = bb->getInstruction(k);
                
                if (!ins->isIntegerOperation() && !ins->isFloatPOperation() && !ins->isMoveOperation()) {
                    continue;
                }
                ASSERT(!ins->usesControlTarget());
                
                singleDefUse(this, ins, bb, NULL, imap, bmap, alliuses, allidefs, k, 0, fcnt);
            }
        }
    }

    for (uint32_t i = 0; i < basicBlocks.size(); i++){

        BasicBlock* bb = basicBlocks[i];
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){

            X86Instruction* x = bb->getInstruction(j);
            uint64_t addr = x->getBaseAddress();

            ASSERT(alliuses.count(addr) == 1);
            LinkedList<X86Instruction::ReachingDefinition*>* l = alliuses[addr];
            alliuses.erase(addr);
            while (!l->empty()){
                delete l->shift();
            }
            delete l;

            ASSERT(allidefs.count(addr) == 1);
            l = allidefs[addr];
            allidefs.erase(addr);
            while (!l->empty()){
                delete l->shift();
            }
            delete l;
        }
    }

    ASSERT(alliuses.size() == 0);
    ASSERT(allidefs.size() == 0);
}

void FlowGraph::interposeBlock(BasicBlock* bb){
    ASSERT(bb->getNumberOfSources() == 1 && bb->getNumberOfTargets() == 1);
    BasicBlock* sourceBlock = bb->getSourceBlock(0);
    BasicBlock* targetBlock = bb->getTargetBlock(0);

    //sourceBlock->print();
    //targetBlock->print();

    bool linkFound = false;
    for (uint32_t i = 0; i < sourceBlock->getNumberOfTargets(); i++){
        if (sourceBlock->getTargetBlock(i)->getIndex() == targetBlock->getIndex()){
            linkFound = true;
            break;
        }
    }

    if (!linkFound){
        function->print();
        print();
        sourceBlock->print();
        targetBlock->print();        
    }

    ASSERT(linkFound && "There should be a source -> target block relationship between the blocks passed to this function");

    ASSERT(sourceBlock->getBaseAddress() + sourceBlock->getNumberOfBytes() != targetBlock->getBaseAddress() && "Source shouldn't fall through to target");

    bb->setBaseAddress(blocks.back()->getBaseAddress() + blocks.back()->getNumberOfBytes());
    bb->setIndex(basicBlocks.size());
    basicBlocks.append(bb);

    //PRINT_INFOR("now there are %d bbs in function %s", basicBlocks.size(), function->getName());
    //PRINT_INFOR("new block has base addres %#llx", bb->getBaseAddress());
    blocks.append(bb);

    sourceBlock->removeTargetBlock(targetBlock);
    sourceBlock->addTargetBlock(bb);
    targetBlock->removeSourceBlock(sourceBlock);
    targetBlock->addSourceBlock(bb);

    X86Instruction* jumpToTarget = bb->getLeader();
    jumpToTarget->setBaseAddress(blocks.back()->getBaseAddress() + blocks.back()->getSizeInBytes());
    jumpToTarget->setIndex(0);

    ASSERT(sourceBlock->getExitInstruction());
    ASSERT(sourceBlock->getExitInstruction()->getAddressAnchor());
    ASSERT(sourceBlock->getExitInstruction()->getTargetAddress() == targetBlock->getBaseAddress());
    sourceBlock->getExitInstruction()->getAddressAnchor()->updateLink(jumpToTarget);

    //bb->print();
    //bb->printInstructions();
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

uint32_t FlowGraph::getLoopDepth(Loop* loop){
    ASSERT(loop);

    if (loop->getDepth()){
        return loop->getDepth();
    }

    uint32_t depth = 1;
    for (uint32_t i = 0; i < loops.size(); i++){
        if (loop->getIndex() != i){
            if (loop->isInnerLoopOf(loops[i])){
                depth++;
            }
        }
    }
    loop->setDepth(depth);

    return depth;
}

uint32_t FlowGraph::getLoopDepth(uint32_t idx){
    Loop* loop = getInnermostLoopForBlock(idx);
    if (loop == NULL){
        return 0;
    }
    return getLoopDepth(loop);
}

void FlowGraph::computeLiveness(){
    DEBUG_LIVE_REGS(double t1 = timer();)

    Vector<std::set<uint32_t>*> succs;
    uint32_t maxElts = 32;
    uint32_t icount = 0;

    // re-index instructions
    uint32_t currIdx = 0;
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = getBasicBlock(i);
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* instruction = bb->getInstruction(j);
            instruction->setIndex(currIdx++);
        }
    }
    icount = currIdx;
    PRINT_DEBUG_LIVE_REGS("Flow analysis on function %s (%d instructions)", function->getName(), currIdx);

    RegisterSet** uses = new RegisterSet*[icount];
    RegisterSet** defs = new RegisterSet*[icount];
    RegisterSet* ins = new RegisterSet[icount];
    RegisterSet* outs = new RegisterSet[icount];
    RegisterSet* ins_prime = new RegisterSet[icount];
    RegisterSet* outs_prime = new RegisterSet[icount];
    X86Instruction** allInstructions = new X86Instruction*[icount];

    // initialize data structures
    currIdx = 0;
    for (uint32_t i = 0; i < getNumberOfBasicBlocks(); i++){
        BasicBlock* bb = getBasicBlock(i);
        for (uint32_t j = 0; j < bb->getNumberOfInstructions(); j++){
            X86Instruction* instruction = bb->getInstruction(j);
            allInstructions[currIdx] = instruction;

            uses[currIdx] = instruction->getRegistersUsed();
            defs[currIdx] = instruction->getRegistersDefined();
            new(&ins[currIdx]) RegisterSet();
            new(&outs[currIdx]) RegisterSet();
            new(&ins_prime[currIdx]) RegisterSet();
            new(&outs_prime[currIdx]) RegisterSet();

            succs.append(new std::set<uint32_t>());
            if (j == bb->getNumberOfInstructions() - 1){
                for (uint32_t k = 0; k < bb->getNumberOfTargets(); k++){
                    succs.back()->insert(bb->getTargetBlock(k)->getLeader()->getIndex());
                }
            } else {
                succs.back()->insert(bb->getInstruction(j+1)->getIndex());
            }
            currIdx++;
        }
    }
    ASSERT(succs.size() == currIdx);

    DEBUG_LIVE_REGS(
                    for (uint32_t i = 0; i < icount; i++){
                        allInstructions[i]->print();
                        PRINT_INFO();
                        PRINT_OUT("instruction %d succ list: ", i);
                        for (std::set<uint32_t>::const_iterator it = succs[i]->begin(); it != succs[i]->end(); it++){
                            PRINT_OUT("%d ", (*it));
                        }
                        PRINT_OUT("\n");
                        uses->print("uses");
                        defs->print("defs");
                        //PRINT_REG_LIST(uses, maxElts, i);
                        //PRINT_REG_LIST(defs, maxElts, i);
                    }
                    )

    bool setsSame = false;
    uint32_t iterCount = 0;
    while (!setsSame){
        for (int32_t i = icount-1; i >= 0; i--){
            // ins'[n] = ins[n]
            ins_prime[i] = ins[i];
            
            // outs'[n] = outs[n]
            outs_prime[i] = outs[i];
            
            PRINT_DEBUG_LIVE_REGS("before in[n] = use[n] U (out[n] - def[n])");
            DEBUG_LIVE_REGS(
                            {
                    ins.print("ins");
                    uses->print("uses");
                    defs->print("defs");
                    outs.print("outs");
                }
            )
            //PRINT_REG_LIST(ins, maxElts, i);
            //PRINT_REG_LIST(uses, maxElts, i);
            //PRINT_REG_LIST(defs, maxElts, i);
            //PRINT_REG_LIST(outs, maxElts, i);
            
            // out[n] = U(s in succ[n]) in[s]
            //            (outs[i])->clear();
            for (std::set<uint32_t>::const_iterator it = succs[i]->begin(); it != succs[i]->end(); it++){
                outs[i] |= ins[(*it)];
                PRINT_REG_LIST(ins, maxElts, (*it));
            }
            
            PRINT_DEBUG_LIVE_REGS("after out[n] = U(s in succ[n]) in[s]");
            PRINT_REG_LIST(outs, maxElts, i);
            // in[n] = use[n] U (out[n] - def[n])
            //BitSet<uint32_t>* tmpbt = new BitSet<uint32_t>(*(outs[i]));
            //*(tmpbt) -= *(defs[i]);
            //*(ins[i]) |= *(uses[i]);
            //*(ins[i]) |= *(tmpbt);
            //delete tmpbt;
            ins[i] = *(uses[i]) | (outs[i] - *(defs[i]));
            
            PRINT_DEBUG_LIVE_REGS("after in[n] = use[n] U (out[n] - def[n])");
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);
            
        }

        // check if in/out have changed this iteration for any n
        setsSame = true;
        for (uint32_t i = 0; i < icount && setsSame; i++){
            if (!(ins[i] == ins_prime[i])){
                PRINT_DEBUG_LIVE_REGS("ins %d different", i);
                setsSame = false;
                break;
            }
            if (!(outs[i] == outs_prime[i])){
                PRINT_DEBUG_LIVE_REGS("outs %d different", i);
                setsSame = false;
                break;
            }
        }

        for (uint32_t i = 0; i < icount; i++){
            PRINT_REG_LIST(ins, maxElts, i);
            PRINT_REG_LIST(ins_prime, maxElts, i);
            PRINT_REG_LIST(outs, maxElts, i);
            PRINT_REG_LIST(outs_prime, maxElts, i);
        }
        iterCount++;
    }

    for (uint32_t i = 0; i < icount; i++){
        allInstructions[i]->setLiveIns(&ins[i]);
        allInstructions[i]->setLiveOuts(&outs[i]);
    }

    for (uint32_t i = 0; i < icount; i++){
        delete uses[i];
        delete defs[i];
        delete succs[i];
    }
    delete[] uses;
    delete[] defs;
    delete[] ins;
    delete[] outs;
    delete[] ins_prime;
    delete[] outs_prime;
    delete[] allInstructions;

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
    return &basicBlocks;
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
    uint32_t excluded = 0;
    while(!backEdges.empty()){

        BasicBlock* from = backEdges.shift();
        BasicBlock* to = backEdges.shift();
        ASSERT(from && to && "Fatal: Backedge end points are invalid");

        if(from->isDominatedBy(to)){
            /* for each back edge found, perform natural loop finding algorithm 
               from pg. 604 of the Aho/Sethi/Ullman (Dragon) compiler book */
            /* note that this algorithm gives us each loop as loop with a single
               head and tail. if we wanted natural loops, we would merge the loops
               which share a head */

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

            if (from->endsWithCall()){
                excluded++;
            } else {
                Loop* newLoop = new Loop(to, from, this, inLoop);
                loopList.insert(newLoop);
                
                DEBUG_LOOP(newLoop->print();)
            }
        }
    }

    ASSERT((loopList.size() == numberOfLoops - excluded) && 
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
    ASSERT(loops.size() == numberOfLoops - excluded);

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
    //blockCopies.append(blocks);

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
    //delete[] allBlocks;
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
