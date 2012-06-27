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

#ifndef _Function_h_
#define _Function_h_

#include <BitSet.h>
#include <X86Instruction.h>
#include <TextSection.h>
#include <Vector.h>

class BasicBlock;
class BinaryInputFile;
class BinaryOutputFile;
class FlowGraph;
class InstrumentationPoint;
class Symbol;
class TextObject;

class Function : public TextObject {
private:
    const static uint32_t recursivedisasmMask     = 0x1;
    const static uint32_t instrumentationfuncMask = 0x2;
    const static uint32_t jumptableMask           = 0x4;
    const static uint32_t disasmfailMask          = 0x8;
    const static uint32_t relocatedMask           = 0x10;
    const static uint32_t manipulatedMask         = 0x20;

    bool defUse;
    bool leafOpt;
    bool computedLeafOpt;

    BitSet<uint32_t>* deadRegs;
    void findDeadRegs();

protected:
    FlowGraph* flowGraph;
    HashCode hashCode;
    uint64_t badInstruction;
    uint64_t flags;

    Vector<X86Instruction*>* digestRecursive();
public:
    Function(TextSection* text, uint32_t idx, Symbol* sym, uint32_t sz);
    ~Function();

    void wedge(uint32_t shamt);

    void interposeBlock(BasicBlock* bb);
    bool hasLeafOptimization();
    void computeLeafOptimization();

    uint32_t getDeadGPR(uint32_t idx);

    uint32_t getStackSize();

    void computeDefUse();
    bool doneDefUse() { return defUse; }

    bool isRecursiveDisasm()          { return (flags & recursivedisasmMask); }
    bool isInstrumentationFunction()  { return (flags & instrumentationfuncMask); }
    bool isJumpTable()                { return (flags & jumptableMask); }
    bool isDisasmFail()               { return (flags & disasmfailMask); }
    bool isRelocated()                { return (flags & relocatedMask); }
    bool isManipulated()              { return (flags & manipulatedMask); }

    void setRecursiveDisasm()         { flags |= recursivedisasmMask; }
    void setInstrumentationFunction() { flags |= instrumentationfuncMask; }
    void setJumpTable()               { flags |= jumptableMask; }
    void setDisasmFail()              { flags |= disasmfailMask; }
    void setRelocated()               { flags |= relocatedMask; }
    void setManipulated()             { flags |= manipulatedMask; }

    uint64_t getBadInstruction() { return badInstruction; }
    void setBadInstruction(uint64_t addr) { badInstruction = addr; }

    bool hasCompleteDisassembly();
    bool containsCallToRange(uint64_t lowAddr, uint64_t highAddr);

    bool callsSelf();
    bool hasSelfDataReference();
    bool refersToInstruction();
    bool containsReturn();

    uint32_t bloatBasicBlocks(Vector<Vector<InstrumentationPoint*>*>* instPoints);
    uint32_t addSafetyJump(X86Instruction* tgtInstruction);

    void setBaseAddress(uint64_t newBaseAddress);

    Symbol* getFunctionSymbol() { return symbol; }
    uint32_t generateCFG(Vector<X86Instruction*>* instructions, Vector<AddressAnchor*>* addressAnchors);

    FlowGraph* getFlowGraph() { return flowGraph; }
    uint32_t getNumberOfBasicBlocks();
    BasicBlock* getBasicBlock(uint32_t idx);
    X86Instruction* getInstructionAtAddress(uint64_t addr);
    BasicBlock* getBasicBlockAtAddress(uint64_t addr);

    uint32_t getNumberOfInstructions();
    uint32_t getNumberOfBytes();

    uint32_t getAllInstructions(X86Instruction** allinsts, uint32_t nexti);

    void printInstructions();

    uint32_t digest(Vector<AddressAnchor*>* addressAnchors);
    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify();
    void print();

    const char* briefName() { return "Function"; }
    HashCode getHashCode() { return hashCode; }

    Vector<X86Instruction*>* swapInstructions(uint64_t addr, Vector<X86Instruction*>* replacements);
    uint64_t findInstrumentationPoint(uint64_t addr, uint32_t size, InstLocations loc);

    void printDisassembly(bool instructionDetail);
};

#endif /* _Function_h_ */
