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

#include <TauFunctionTrace.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <LineInformation.h>
#include <Loop.h>
#include <X86Instruction.h>
#include <map>
#include <string>
#include <vector>

#ifdef HAVE_UNORDERED_MAP
#include <unordered_map>
#endif

#define REGISTER_FUNC "tau_register_func"
#define REGISTER_LOOP "tau_register_loop"
#define ENTRY_FUNC_CALL "tau_trace_entry"
#define EXIT_FUNC_CALL "tau_trace_exit"

#define TAU_INST_LOOPS_TOKEN "loops routine"

extern "C" {
    InstrumentationTool* TauFunctionTraceMaker(ElfFile* elf){
        return new TauFunctionTrace(elf);
    }
}

TauFunctionTrace::~TauFunctionTrace(){
    if (instrumentList){
        delete instrumentList;
    }
}

TauFunctionTrace::TauFunctionTrace(ElfFile* elf)
    : InstrumentationTool(elf)
{
    functionRegister = NULL;
    loopRegister = NULL;

    functionEntry = NULL;
    functionExit = NULL;

    instrumentList = NULL;
}

void TauFunctionTrace::declare(){
    //InstrumentationTool::declare();

    if (inputFile){
        instrumentList = new TauInstrumentList(inputFile, "BEGIN_INSTRUMENT_SECTION", "END_INSTRUMENT_SECTION", "BEGIN_EXCLUDE_LIST", "END_EXCLUDE_LIST");
    }

    // declare any instrumentation functions that will be used
    functionRegister = declareFunction(REGISTER_FUNC);
    ASSERT(functionRegister);

    loopRegister = declareFunction(REGISTER_LOOP);
    ASSERT(loopRegister);

    functionEntry = declareFunction(ENTRY_FUNC_CALL);
    ASSERT(functionEntry);

    functionExit = declareFunction(EXIT_FUNC_CALL);
    ASSERT(functionExit);
}

struct LoopPoint {
    FlowGraph* flowgraph;
    BasicBlock* source;
    BasicBlock* target;
    bool entry;
    bool interpose;
};

typedef enum {
    ControlType_Undefined = 0,
    ControlType_Function,
    ControlType_Loop,
    ControlType_TotalTypes
} ControlTypes;

struct ControlInfo {
    // used for any type
    ControlTypes type;
    uint64_t baseaddr;
    std::string name;
    std::string file;
    int line;
    int index;
    Vector<LoopPoint*>* info;
};

void TauFunctionTrace::instrument(){
    //InstrumentationTool::instrument();

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    InstrumentationPoint* p;

    uint64_t nameAddr = reserveDataOffset(sizeof(uint64_t));
    uint64_t fileAddr = reserveDataOffset(sizeof(uint64_t));
    uint64_t lineAddr = reserveDataOffset(sizeof(uint32_t));
    uint64_t siteIndexAddr = reserveDataOffset(sizeof(uint32_t));

    uint32_t site = functionEntry->addConstantArgument();
    ASSERT(site == functionExit->addConstantArgument());

    std::pebil_map_type<uint64_t, ControlInfo> functions;
    std::vector<uint64_t> orderedfuncs;

    uint32_t sequenceId = 0;
    if (doIntro){
        // go over all functions and intrument entries/exits
        for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
            Function* function = getExposedFunction(i);
            uint64_t addr = function->getBaseAddress();
            if (instrumentList && !instrumentList->functionMatches(function->getName())){
                continue;
            }
            if (!strcmp(function->getName(), "_fini")){
                continue;
            }

            BasicBlock* entryBlock = function->getFlowGraph()->getEntryBlock();
            Vector<BasicBlock*>* exitPoints = function->getFlowGraph()->getExitBlocks();

            std::string c;
            c.append(function->getName());
            if (c == "_start"){
                exitPoints->append(getProgramExitBlock());
                PRINT_INFOR("Special case: inserting exit for _start inside _fini since control generally doesn't reach its exit");
            }

            ASSERT(functions.count(addr) == 0 && "Multiple functions have the same base address?");

            PRINT_INFOR("[FUNCTION index=%d] internal instrumentation: %s", sequenceId, function->getName());

            ControlInfo f = ControlInfo();
            f.name = c;
            f.file = "";
            f.line = 0;
            f.index = sequenceId++;
            f.baseaddr = addr;
            f.type = ControlType_Function;

            LineInfo* li = NULL;
            if (lineInfoFinder){
                li = lineInfoFinder->lookupLineInfo(addr);
            }

            if (li){
                f.file.append(li->getFileName());
                f.line = li->GET(lr_line);
            }

            functions[addr] = f;
            orderedfuncs.push_back(addr);

            InstrumentationPoint* prior = addInstrumentationPoint(entryBlock->getLeader(), functionEntry, InstrumentationMode_tramp, InstLocation_prior);
            prior->setPriority(InstPriority_custom3);
            assignStoragePrior(prior, f.index, site);

            for (uint32_t j = 0; j < exitPoints->size(); j++){
                InstrumentationPoint* after = addInstrumentationPoint((*exitPoints)[j]->getExitInstruction(), functionExit, InstrumentationMode_tramp, InstLocation_prior);
                after->setPriority(InstPriority_custom5);

                if (c == "_start" && j == exitPoints->size() - 1){
                    after->setPriority(InstPriority_custom6);
                }
                assignStoragePrior(after, f.index, site);
            }

            delete exitPoints;
        }

    } else {
        // go over all instructions. when we find a call, instrument it
        for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
            X86Instruction* x = getExposedInstruction(i);
            ASSERT(x->getContainer()->isFunction());
            Function* function = (Function*)x->getContainer();

            if (x->isFunctionCall()){
                uint64_t addr = x->getTargetAddress();
                Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(addr);
            
                if (functionSymbol){

                    if (instrumentList && !instrumentList->functionMatches(functionSymbol->getSymbolName())){
                        continue;
                    }
                    ASSERT(x->getSizeInBytes() == Size__uncond_jump);
                
                    std::string c;
                    c.append(functionSymbol->getSymbolName());
                    if (functions.count(addr) == 0){
                        PRINT_INFOR("[FUNCTION index=%d] call site instrumentation: %#lx(%s) -> %s", sequenceId, addr, function->getName(), functionSymbol->getSymbolName());

                        ControlInfo f = ControlInfo();
                        f.name = c;
                        f.file = "";
                        f.line = 0;
                        f.index = sequenceId++;
                        f.baseaddr = addr;
                        f.type = ControlType_Function;

                        LineInfo* li = NULL;
                        if (lineInfoFinder){
                            li = lineInfoFinder->lookupLineInfo(addr);
                        }

                        if (li){
                            f.file.append(li->getFileName());
                            f.line = li->GET(lr_line);
                        }

                        functions[addr] = f;
                        orderedfuncs.push_back(addr);
                    }
                    uint32_t idx = functions[addr].index;

                    Base* exitpoint = (Base*)x;
                    if (c == "__libc_start_main"){
                        PRINT_INFOR("Special case: inserting exit for __libc_start_main inside _fini since this call generally doesn't return");
                        exitpoint = (Base*)getProgramExitBlock();
                    }

                    InstrumentationPoint* prior = addInstrumentationPoint(x, functionEntry, InstrumentationMode_tramp, InstLocation_prior);
                    InstrumentationPoint* after = addInstrumentationPoint(exitpoint, functionExit, InstrumentationMode_tramp, InstLocation_after);

                    assignStoragePrior(prior, idx, site);
                    assignStoragePrior(after, idx, site);
                }
            }
        }
    }


    // set up argument passing for function registration
    functionRegister->addArgument(nameAddr);
    functionRegister->addArgument(fileAddr);
    functionRegister->addArgument(lineAddr);
    uint32_t siteReg = functionRegister->addConstantArgument();


    // go over every function that was found, insert a registration call at program start
    for (std::vector<uint64_t>::iterator it = orderedfuncs.begin(); it != orderedfuncs.end(); it++){
        uint64_t addr = *it;
        ControlInfo f = functions[addr];
       
        ASSERT(f.baseaddr == addr);

        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), functionRegister, InstrumentationMode_tramp);
        p->setPriority(InstPriority_custom1);

        const char* cstring = f.name.c_str();
        uint64_t storage = reserveDataOffset(strlen(cstring) + 1);
        initializeReservedData(getInstDataAddress() + storage, strlen(cstring), (void*)cstring);
        assignStoragePrior(p, getInstDataAddress() + storage, getInstDataAddress() + nameAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());

        const char* cstring2 = f.file.c_str();
        if (f.file == ""){
            assignStoragePrior(p, NULL, getInstDataAddress() + fileAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());            
        } else {
            storage = reserveDataOffset(strlen(cstring2) + 1);
            initializeReservedData(getInstDataAddress() + storage, strlen(cstring2), (void*)cstring2);
            assignStoragePrior(p, getInstDataAddress() + storage, getInstDataAddress() + fileAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
        }

        assignStoragePrior(p, f.line, getInstDataAddress() + lineAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
        assignStoragePrior(p, f.index, siteReg);
    }


    if (!instrumentList){
        return;
    }

    // instrument loops
    std::pebil_map_type<uint64_t, ControlInfo> loops;
    std::vector<uint64_t> orderedloops;

    loopRegister->addArgument(nameAddr);
    loopRegister->addArgument(fileAddr);
    loopRegister->addArgument(lineAddr);
    ASSERT(siteReg == loopRegister->addConstantArgument());

    for (uint32_t i = 0; i < getNumberOfExposedFunctions(); i++){
        Function* function = getExposedFunction(i);
        FlowGraph* flowgraph = function->getFlowGraph();

        if (!instrumentList->loopMatches(function->getName())){
            continue;
        }

        for (uint32_t j = 0; j < flowgraph->getNumberOfLoops(); j++){
            Loop* loop = flowgraph->getLoop(j);
            uint32_t depth = flowgraph->getLoopDepth(loop);
            BasicBlock* head = loop->getHead();
            uint64_t addr = head->getBaseAddress();

            // only want outer-most (depth == 1) loops
            if (depth != 1){
                continue;
            }

            BasicBlock** allLoopBlocks = new BasicBlock*[loop->getNumberOfBlocks()];
            loop->getAllBlocks(allLoopBlocks);

            // reject any loop that contains an indirect branch since it is difficult to guarantee that we will find all exits
            bool badLoop = false;
            for (uint32_t k = 0; k < loop->getNumberOfBlocks() && !badLoop; k++){
                BasicBlock* bb = allLoopBlocks[k];
                if (bb->getExitInstruction()->isIndirectBranch()){
                    badLoop = true;
                }
            }

            if (badLoop){
                PRINT_WARN(20, "Loop at %#lx in %s contains an indirect branch so we can't guarantee that all exits will be found. skipping!", addr, function->getName());
                delete[] allLoopBlocks;
                continue;
            }

            std::string c;
            c.append(function->getName());

            uint32_t entryc = 0;

            Vector<LoopPoint*>* points = NULL;

            // if addr already exists, it means that two loops share a head and we are going to merge them logically here
            if (loops.count(addr) == 0){

                ControlInfo f = ControlInfo();
                f.name = c;
                f.file = "";
                f.line = 0;
                f.index = sequenceId++;
                f.baseaddr = addr;
                f.type = ControlType_Loop;

                points = new Vector<LoopPoint*>();
                f.info = points;

                LineInfo* li = NULL;
                if (lineInfoFinder){
                    li = lineInfoFinder->lookupLineInfo(addr);
                }
                if (li){
                    f.file.append(li->getFileName());
                    f.line = li->GET(lr_line);
                }

                loops[addr] = f;
                orderedloops.push_back(addr);

                // find entries into this loop
                for (uint32_t k = 0; k < head->getNumberOfSources(); k++){
                    BasicBlock* source = head->getSourceBlock(k);

                    if (!loop->isBlockIn(source->getIndex())){
                        LoopPoint* lp = new LoopPoint();
                        points->append(lp);

                        lp->flowgraph = flowgraph;
                        lp->source = source;
                        lp->target = NULL;
                        lp->entry = true;
                        lp->interpose = false;

                        if (source->getBaseAddress() + source->getNumberOfBytes() != head->getBaseAddress()){
                            lp->interpose = true;
                            lp->target = head;
                        }
                        entryc++;
                    }
                }
            }

            ControlInfo f = loops[addr];
            points = f.info;

            // find exits from this loop
            uint32_t exitc = 0;
            for (uint32_t k = 0; k < loop->getNumberOfBlocks(); k++){
                BasicBlock* bb = allLoopBlocks[k];
                if (bb->endsWithReturn()){
                    LoopPoint* lp = new LoopPoint();
                    points->append(lp);

                    lp->flowgraph = flowgraph;
                    lp->source = bb;
                    lp->target = NULL;
                    lp->entry = false;
                    lp->interpose = false;
                    exitc++;
                }

                for (uint32_t m = 0; m < bb->getNumberOfTargets(); m++){
                    BasicBlock* target = bb->getTargetBlock(m);
                    if (!loop->isBlockIn(target->getIndex())){
                        LoopPoint* lp = new LoopPoint();
                        points->append(lp);

                        lp->flowgraph = flowgraph;
                        lp->source = bb;
                        lp->target = NULL;
                        lp->entry = false;
                        lp->interpose = false;

                        if (target->getBaseAddress() != bb->getBaseAddress() + bb->getNumberOfBytes()){
                            lp->interpose = true;
                            lp->target = target;
                        }
                        exitc++;
                    }
                }
            }

            PRINT_INFOR("[LOOP index=%d] loop instrumentation %#lx(%s) has %d entries and %d exits", sequenceId-1, addr, function->getName(), entryc, exitc);

            delete[] allLoopBlocks;
        }
    }

    // go over every loop that was found, insert a registration call at program start
    // [source_addr -> [target_addr -> interposed]]
    std::pebil_map_type<uint64_t, std::pebil_map_type<uint64_t, BasicBlock*> > idone;
    for (std::vector<uint64_t>::iterator it = orderedloops.begin(); it != orderedloops.end(); it++){
        uint64_t addr = *it;
        ControlInfo f = loops[addr];
       
        ASSERT(f.baseaddr == addr);

        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), loopRegister, InstrumentationMode_tramp);
        p->setPriority(InstPriority_custom2);

        const char* cstring = f.name.c_str();
        uint64_t storage = reserveDataOffset(strlen(cstring) + 1);
        initializeReservedData(getInstDataAddress() + storage, strlen(cstring), (void*)cstring);
        assignStoragePrior(p, getInstDataAddress() + storage, getInstDataAddress() + nameAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());

        const char* cstring2 = f.file.c_str();
        if (f.file == ""){
            assignStoragePrior(p, NULL, getInstDataAddress() + fileAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());            
        } else {
            storage = reserveDataOffset(strlen(cstring2) + 1);
            initializeReservedData(getInstDataAddress() + storage, strlen(cstring2), (void*)cstring2);
            assignStoragePrior(p, getInstDataAddress() + storage, getInstDataAddress() + fileAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
        }

        assignStoragePrior(p, f.line, getInstDataAddress() + lineAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
        assignStoragePrior(p, f.index, siteReg);

        // now add instrumentation for each loop entry/exit
        Vector<LoopPoint*>* v = (Vector<LoopPoint*>*)f.info;
        for (uint32_t i = 0; i < v->size(); i++){
            LoopPoint* lp = (*v)[i];
            ASSERT(lp->flowgraph && lp->source);

            BasicBlock* bb = lp->source;
            if (lp->interpose){
                ASSERT(lp->target);
                if (idone.count(lp->source->getBaseAddress()) == 0){
                    idone[lp->source->getBaseAddress()] = std::pebil_map_type<uint64_t, BasicBlock*>();
                }
                if (idone[lp->source->getBaseAddress()].count(lp->target->getBaseAddress()) == 0){
                    idone[lp->source->getBaseAddress()][lp->target->getBaseAddress()] = initInterposeBlock(lp->flowgraph, lp->source->getIndex(), lp->target->getIndex());
                }

                bb = idone[lp->source->getBaseAddress()][lp->target->getBaseAddress()];

            } else {
                ASSERT(lp->target == NULL);
            }

            Base* pt = (Base*)bb;
            InstLocations loc = InstLocation_prior;

            // if exit block falls through, we must place the instrumentation point at the very end of the block
            if (!lp->entry && !lp->interpose){
                pt = (Base*)bb->getExitInstruction();
                if (!bb->getExitInstruction()->isReturn()){
                    loc = InstLocation_after;
                }
            }

            InstrumentationFunction* inf = functionExit;
            if (lp->entry){
                inf = functionEntry;
            }

            InstrumentationPoint* p = addInstrumentationPoint(pt, inf, InstrumentationMode_tramp, loc);
            p->setPriority(InstPriority_custom4);
            assignStoragePrior(p, f.index, site);

            delete lp;
        }
        delete v;
    }

}


Vector<char*>* TauRegexToC(const char* t, bool isQuoted){
    uint32_t len = strlen(t);
    if (isQuoted){
        if (t[0] != '"' || t[len-1] != '"'){
            PRINT_ERROR("Format of instrumentation directive is '%s=\"<tau_instr_regex>\"', malformed RHS found: %s", TAU_INST_LOOPS_TOKEN, t);
        }
    }

    uint32_t counthash = 0;
    for (uint32_t j = 0; j < len; j++){
        if (t[j] == '#'){
            counthash++;
        }
    }

    int extra = 0;
    if (!isQuoted){
        extra = 2;
    }

    char* loopregex = new char[len + 1 + counthash + extra];
    counthash = 0;
    for (uint32_t j = 0; j < len; j++){
        if (t[j] == '#'){
            loopregex[j + counthash + (extra/2)] = '.';
            loopregex[j + counthash + 1 + (extra/2)] = '*';
            counthash++;
        } else {
            loopregex[j + counthash + (extra/2)] = t[j];
        }
    }
    loopregex[0] = '^';
    loopregex[len + counthash - 1 + extra] = '$';
    loopregex[len + counthash + extra] = '\0';

    Vector<char*>* v = new Vector<char*>();
    v->append(loopregex);

    return v;
}

TauInstrumentList::TauInstrumentList(const char* filename, const char* beginInstr, const char* endInstr, const char* beginExcl, const char* endExcl){
    init(filename, 0, '=', '/');

    functions = new FileList();
    loops = new FileList();

    functions->setFileName(filename);
    loops->setFileName(filename);

    functions->setSeparator('?');
    loops->setSeparator('=');

    bool instrState = false;
    bool exclState = false;

    for (uint32_t i = 0; i < fileTokens.size(); i++){
        Vector<char*>* toks = fileTokens[i];
        if (toks->size() == 0){
            continue;
        }
        char* t = toks->front();

        if (!strcmp(t, beginInstr)){
            if (instrState || exclState){
                PRINT_ERROR("Error parsing TAU loop tracking file: unexpected token %s", beginInstr);
            }

            instrState = true;
            continue;
        } else if (!strcmp(t, endInstr)){
            if (!instrState || exclState){
                PRINT_ERROR("Error parsing TAU loop tracking file: unexpected token %s", endInstr);
            }

            instrState = false;
            continue;
        } else if (!strcmp(t, beginExcl)){
            if (exclState || instrState){
                PRINT_ERROR("Error parsing TAU loop tracking file: unexpected token %s", beginExcl);
            }

            exclState = true;
            continue;
        } else if (!strcmp(t, endExcl)){
            if (!exclState || instrState){
                PRINT_ERROR("Error parsing TAU loop tracking file: unexpected token %s", endExcl);
            }

            exclState = false;
            continue;
        }

        if (instrState){
            if (strcmp(TAU_INST_LOOPS_TOKEN, t)){
                PRINT_ERROR("Format of instrumentation directive is '%s=\"<tau_instr_regex>\"', invalid LHS found: %s", TAU_INST_LOOPS_TOKEN, t);
            }
            if (toks->size() == 1){
                PRINT_ERROR("Format of instrumentation directive is '%s=\"<tau_instr_regex>\"', no RHS found: %s", TAU_INST_LOOPS_TOKEN, t);
            }
            if (toks->size() > 2){
                PRINT_ERROR("Format of instrumentation directive is '%s=\"<tau_instr_regex>\"', too many '=' found", TAU_INST_LOOPS_TOKEN);
            }

            loops->appendLine(TauRegexToC(toks->back(), true));
            continue;
        }

        if (exclState){
            if (toks->size() != 1){
                PRINT_ERROR("Format of exclusion directive is '\"<tau_instr_regex>\"', found: %s", t);
            }

            functions->appendLine(TauRegexToC(toks->back(), false));
            continue;
        }
    }
    ASSERT(instrState == false && "Cannot leave unclosed INSTRUMENT section in TAU instrument file");
    ASSERT(exclState == false && "Cannot leave unclosed EXCLUDE section in TAU instrument file");

    while (fileTokens.size()){
        Vector<char*>* v = fileTokens.remove(0);
        while (v->size()){
            char* c = v->remove(0);
            delete[] c;
        }
        ASSERT(v->size() == 0);
        delete v;
    }
    ASSERT(fileTokens.size() == 0);

    functions->print();
    loops->print();
}

bool TauInstrumentList::loopMatches(char* str){
    if (loops->matches(str, 0) && !functions->matches(str, 0)){
        return true;
    }
    return false;
}

bool TauInstrumentList::functionMatches(char* str){
    if (functions->matches(str, 0)){
        return false;
    }
    return true;
}

TauInstrumentList::~TauInstrumentList(){
    if (functions){
        delete functions;
    }

    if (loops){
        delete loops;
    }
}
