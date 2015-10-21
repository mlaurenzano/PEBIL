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
#include <map>

#include <Metasim.hpp>

class InstrumentationPoint;

#define INFO_UNKNOWN "__info_unknown__"

typedef struct {
    uint64_t id;
    uint64_t data;
} ThreadData;
#define ThreadHashShift (17)
#define ThreadHashMod   (0xffff)

struct DynamicInstInternal {
    InstrumentationPoint* Point;
    uint64_t Key;
    bool IsEnabled;

    DynamicInstInternal(){
        Point = NULL;
        Key = 0;
        IsEnabled = true;
    }
};

enum ThreadRegisterMapType {
  ThreadRegisterMapType_None,
  ThreadRegisterMapType_Func,
  ThreadRegisterMapType_Loop
};

class ThreadRegisterMap {
public:
    ThreadRegisterMap();
    ThreadRegisterMap(uint32_t reg);
    uint32_t getThreadRegister(BasicBlock* bb);
    void setThreadRegister(Loop* l, uint32_t reg);
private:
    ThreadRegisterMapType type;
    uint32_t reg;
    std::map<Loop*, uint32_t> loopRegisters;
};

class InstrumentationTool : public ElfFileInst {
private:
    char* extension;
    bool isMaster;

    bool singleArgCheck(void* arg, uint32_t mask, const char* name);
    bool hasThreadEvidence();

    InstrumentationTool* (*maker)(ElfFile*);

    void instrumentEmbeddedElf();
protected:
    uint64_t imageKey;
    uint64_t threadHash;

    Vector<X86Instruction*>* atomicIncrement(uint32_t dest, uint32_t scratch, uint32_t count, uint64_t memaddr, Vector<X86Instruction*>* insns);

    void printStaticFile(const char* extension, Vector<Base*>* allBlocks, Vector<uint32_t>* allBlockIds, Vector<LineInfo*>* allBlockLineInfos, uint32_t bufferSize);
    void printStaticFilePerInstruction(const char* extension, Vector<Base*>* allInstructions, Vector<uint32_t>* allInstructionIds, Vector<LineInfo*>* allInstructionLineInfos, uint32_t bufferSize);

    InstrumentationPoint* insertBlockCounter(uint64_t, Base*);
    InstrumentationPoint* insertBlockCounter(uint64_t, Base*, bool, uint32_t);
    InstrumentationPoint* insertBlockCounter(uint64_t, Base*, bool, uint32_t, uint32_t);
    InstrumentationPoint* insertInlinedTripCounter(uint64_t, X86Instruction*, bool, uint32_t, InstLocations, BitSet<uint32_t>*, uint32_t val);

    void assignStoragePrior(InstrumentationPoint* pt, uint32_t value, uint64_t address, uint8_t tmpreg, uint64_t regbak);
    void assignStoragePrior(InstrumentationPoint* pt, uint32_t value, uint8_t reg);

    Vector<X86Instruction*>* storeThreadData(uint32_t scratch, uint32_t dest);
    Vector<X86Instruction*>* storeThreadData(uint32_t scratch, uint32_t dest, bool storeToStack, uint32_t stackPatch);
    void threadAllEntryPoints(Function* f, uint32_t threadReg);

    std::map<uint64_t, ThreadRegisterMap*>* threadReadyCode(std::set<Base*>& objectsToInst);
    void setThreadingRegister(uint32_t d, X86Instruction* ins, InstLocations loc, bool borrow=false);
    ThreadRegisterMap* instrumentForThreading(Function* func);

    InstrumentationFunction* imageInit;
    InstrumentationFunction* initWrapperC;
    InstrumentationFunction* initWrapperF;

    uint32_t phaseNo;
    bool loopIncl;
    bool printDetail;
    char* inputFile;
    char* dfpFile;
    char* trackFile;
    bool doIntro;

#define PEBIL_OPT_ALL 0xffffffff
#define PEBIL_OPT_NON 0x00000000
#define PEBIL_OPT_PHS 0x00000001
#define PEBIL_OPT_LPI 0x00000002
#define PEBIL_OPT_DTL 0x00000004
#define PEBIL_OPT_INP 0x00000008
#define PEBIL_OPT_DFP 0x00000010
#define PEBIL_OPT_TRK 0x00000020
#define PEBIL_OPT_DOI 0x00000040

    Vector<DynamicInstInternal*> dynamicPoints;
    InstrumentationFunction* dynamicInit;

    uint64_t dynamicPointArray;
    uint64_t dynamicSize;
    bool isThreadedModeFlag;

public:
    InstrumentationTool(ElfFile* elf);
    virtual ~InstrumentationTool() { }

    void init(char* ext);
    void initToolArgs(bool lpi, bool dtl, bool doi, uint32_t phase, char* inp, char* dfp, char* trk);
    void setMaker(InstrumentationTool* (*maker)(ElfFile*)) { this->maker = maker; };

    virtual void declare();
    virtual void instrument();

    void dynamicPoint(InstrumentationPoint* pt, uint64_t key, bool enable);
    uint64_t reserveDynamicPoints();
    void applyDynamicPoints(uint64_t dynArray);

    virtual const char* briefName() { __SHOULD_NOT_ARRIVE; }
    virtual const char* defaultExtension() { __SHOULD_NOT_ARRIVE; }
    const char* getExtension();
    bool verifyArgs();
    virtual uint32_t allowsArgs() { return PEBIL_OPT_ALL; }
    virtual uint32_t requiresArgs() { return PEBIL_OPT_NON; }
    bool isMasterImage();
    void setMasterImage(bool isMaster);
};


#endif /* _InstrumentationTool_h_ */
