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

#ifndef _BasicBlock_h_
#define _BasicBlock_h_

#include <Base.h>
#include <BitSet.h>
#include <FlowGraph.h>
#include <Vector.h>

class Function;
class X86Instruction;

class Block : public Base {
protected:
    uint32_t index;
    FlowGraph* flowGraph;

public:
    Block(PebilClassTypes typ, uint32_t idx, FlowGraph* cfg);
    ~Block() {}

    virtual uint32_t getNumberOfBytes() { __SHOULD_NOT_ARRIVE; return 0; }
    virtual void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset) { __SHOULD_NOT_ARRIVE; }
    virtual bool verify() { __SHOULD_NOT_ARRIVE; }
    virtual void setBaseAddress(uint64_t addr) { __SHOULD_NOT_ARRIVE; }
    virtual void print() { __SHOULD_NOT_ARRIVE; }
    virtual void printDisassembly(bool instructionDetail) { __SHOULD_NOT_ARRIVE; }

    FlowGraph* getFlowGraph() { return flowGraph; }
    uint64_t getBaseAddress() { return baseAddress; }
    uint32_t getIndex() { return index; }
    virtual void setIndex(uint32_t idx) { index = idx; }
};

class CodeBlock : public Block {
protected:
    Vector<X86Instruction*> instructions;
    bool byteCountUpdate;
    uint32_t numberOfBytes;
public:
    CodeBlock(uint32_t idx, FlowGraph* cfg);
    ~CodeBlock();

    uint32_t addTailJump(X86Instruction* tgtInstruction);

    Vector<X86Instruction*>* swapInstructions(uint64_t addr, Vector<X86Instruction*>* replacements);
    void printInstructions();
    void setBaseAddress(uint64_t newBaseAddress);
    uint32_t addInstruction(X86Instruction* inst);
    void printDisassembly(bool instructionDetail);
    uint32_t getNumberOfBytes();
    X86Instruction* getInstructionAtAddress(uint64_t addr);

    uint64_t getProgramAddress();

    uint32_t getAllInstructions(X86Instruction** allinsts, uint32_t nexti);
    uint32_t getNumberOfInstructions() { return instructions.size(); }
    X86Instruction* getInstruction(uint32_t idx) { return instructions[idx]; }

    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);

    virtual bool verify() { return true; }
};

class RawBlock : public Block {
private:
    char* rawBytes;
public:
    RawBlock(uint32_t idx, FlowGraph* cfg, char* byt, uint32_t sz, uint64_t addr);
    ~RawBlock();

    char* charStream() { return rawBytes; }
    uint64_t getBaseAddress() { return baseAddress; }
    uint32_t getNumberOfBytes() { return getSizeInBytes(); }

    void dump (BinaryOutputFile* binaryOutputFile, uint32_t offset);
    bool verify() { return true; }
    void printDisassembly(bool instructionDetail);

    void setBaseAddress(uint64_t addr) { baseAddress = addr; }
    void print();
};

class BasicBlock : public CodeBlock {
private:
    const static uint32_t PaddingMask      = 0x1;
    const static uint32_t EntryMask        = 0x2;
    const static uint32_t ExitMask         = 0x4;
    const static uint32_t OnlyCtrlMask     = 0x8;
    const static uint32_t NoPathMask       = 0x10;
    const static uint32_t CmpCtrlSplitMask = 0x20;

protected:
    Vector<BasicBlock*> sourceBlocks;
    Vector<BasicBlock*> targetBlocks;
    BasicBlock* immDominatedBy;

    HashCode hashCode;
    //    MemoryOperation** memoryOps;
    uint32_t flags;
    uint32_t defXIterCount;
public:
    BasicBlock(uint32_t idx, FlowGraph* cfg);
    ~BasicBlock() {}

    X86Instruction* findBestInstPoint(InstLocations* loc, BitSet<uint32_t>* unusableRegs, BitSet<uint32_t>* useRegs, bool attendFlags);
    uint32_t bloat(Vector<InstrumentationPoint*>* instPoints);

    bool containsOnlyControl();
    bool containsCallToRange(uint64_t lowAddr, uint64_t highAddr);

    void print();
    void printSourceBlocks();
    void printTargetBlocks();

    uint32_t addSourceBlock(BasicBlock* srcBlock);
    uint32_t addTargetBlock(BasicBlock* tgtBlock);
    uint32_t removeSourceBlock(BasicBlock* srcBlock);
    uint32_t removeTargetBlock(BasicBlock* tgtBlock);

    std::map<uint32_t, uint32_t>* getOperandLengthCounts();

    uint32_t getNumberOfSources() { return sourceBlocks.size(); }
    uint32_t getNumberOfTargets() { return targetBlocks.size(); }
    uint32_t getNumberOfMemoryOps();
    uint32_t getNumberOfMemoryBytes();
    uint32_t getNumberOfFloatOps();
    uint32_t getNumberOfLoads();
    uint32_t getNumberOfStores();
    uint32_t getNumberOfIntegerOps();
    uint32_t getNumberOfStringOps();
    uint32_t getNumberOfBranches();
    uint32_t getNumberOfShiftRotOps();
    uint32_t getNumberOfSyscalls();
    uint32_t getNumberOfSpecialRegOps();
    uint32_t getNumberOfLogicOps();
    uint32_t getNumberOfScatterGatherOps();
    uint32_t getNumberOfVectorMaskOps();

    uint32_t getNumberOfBinMem();
    uint32_t getNumberOfBinSystem();
    uint32_t getNumberOfBinMove();
    uint32_t getNumberOfBinFloatv();
    uint32_t getNumberOfBinFloat();
    uint32_t getNumberOfBinIntv();
    uint32_t getNumberOfBinInt();
    uint32_t getNumberOfBinInts();
    uint32_t getNumberOfBinBin();
    uint32_t getNumberOfBinBinv();
    uint32_t getNumberOfBinUncond();
    uint32_t getNumberOfBinCond();
    uint32_t getNumberOfBinInvalid();
    uint32_t getNumberOfBinUnknown();
    uint32_t getNumberOfBinFloats();
    uint32_t getNumberOfBinStack();
    uint32_t getNumberOfBinString();
    uint32_t getNumberOfBinCache();
    uint32_t getNumberOfBinOther();
    uint32_t getNumberOfBinSinglev();
    uint32_t getNumberOfBinSingle();
    uint32_t getNumberOfBinDoublev();
    uint32_t getNumberOfBinDouble();
    uint32_t getNumberOfBinSingles();
    uint32_t getNumberOfBinDoubles();
    uint32_t getNumberOfBinWordv();
    uint32_t getNumberOfBinWord();
    uint32_t getNumberOfBinBytev();
    uint32_t getNumberOfBinByte();
    uint32_t getNumberOfBinDwordv();
    uint32_t getNumberOfBinDword();
    uint32_t getNumberOfBinQwordv();
    uint32_t getNumberOfBinQword();

    bool isInLoop();
    bool controlFallsThrough();
    bool findExitInstruction();

    uint64_t findInstrumentationPoint(uint64_t addr, uint32_t size, InstLocations loc);
    bool inRange(uint64_t addr);

    bool verify();
    Function* getFunction() { ASSERT(flowGraph); return flowGraph->getFunction(); }

    uint64_t getTargetAddress();

    X86Instruction* getLeader() { return instructions[0]; }
    X86Instruction* getExitInstruction() { return instructions.back(); }

    bool isPadding()  { return (flags & PaddingMask); }
    bool isEntry()    { return (flags & EntryMask); }
    bool isExit()     { return (flags & ExitMask); }
    bool isOnlyCtrl() { return (flags & OnlyCtrlMask); }
    bool isNoPath()   { return (flags & NoPathMask); }
    bool isCmpCtrlSplit()   { return (flags & CmpCtrlSplitMask); }

    void setPadding()  { flags |= PaddingMask; }
    void setEntry()    { flags |= EntryMask; }
    void setExit()     { flags |= ExitMask; }
    void setOnlyCtrl() { flags |= OnlyCtrlMask; }
    void setNoPath()   { flags |= NoPathMask; }
    void setCmpCtrlSplit() { flags |= CmpCtrlSplitMask; }

    uint32_t getDefXIter();

    void setIndex(uint32_t idx);

    bool isUnreachable() { return isNoPath(); }
    bool isReachable() { return !isNoPath(); }
    bool endsWithCall();
    bool endsWithReturn();
    bool endsWithControl();

    HashCode getHashCode() { return hashCode; }

    bool isDominatedBy(BasicBlock* bb);
    BasicBlock* getSourceBlock(uint32_t idx) { return sourceBlocks[idx]; }
    BasicBlock* getTargetBlock(uint32_t idx) { return targetBlocks[idx]; }

    void findMemoryFloatOps();
    void setImmDominator(BasicBlock* bb) { immDominatedBy = bb; }
    BasicBlock* getImmDominator() { return immDominatedBy; }

};


#endif /* _BasicBlock_h_ */

