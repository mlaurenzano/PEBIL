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
#include <X86Instruction.h>
#include <map>
#include <string>
#include <vector>

#define INST_LIB_NAME "libtautrace.so"
#define REGISTER_FUNC "tau_register_func"
#define ENTRY_FUNC_CALL "tau_trace_entry"
#define EXIT_FUNC_CALL "tau_trace_exit"

extern "C" {
    InstrumentationTool* TauFunctionTraceMaker(ElfFile* elf){
        return new TauFunctionTrace(elf);
    }
}

TauFunctionTrace::~TauFunctionTrace(){
    if (functionList){
        delete functionList;
    }
}

TauFunctionTrace::TauFunctionTrace(ElfFile* elf)
    : InstrumentationTool(elf)
{
    functionRegister = NULL;

    functionEntry = NULL;
    functionExit = NULL;

    functionList = NULL;
}

void TauFunctionTrace::declare(){
    //InstrumentationTool::declare();

    if (inputFile){
        functionList = new FileList(inputFile);
        functionList->print();
    }

    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    functionRegister = declareFunction(REGISTER_FUNC);
    ASSERT(functionRegister);

    functionEntry = declareFunction(ENTRY_FUNC_CALL);
    ASSERT(functionEntry);

    functionExit = declareFunction(EXIT_FUNC_CALL);
    ASSERT(functionExit);
}

struct FuncInfo {
    std::string name;
    std::string file;
    int line;
    int index;
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

    functionEntry->addArgument(siteIndexAddr);
    functionExit->addArgument(siteIndexAddr);

    std::map<std::string, FuncInfo> functions;
    std::vector<std::string> orderedfuncs;
    uint32_t funcIdx = 0;

    // go over all instructions. when we find a call, instrument it
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* x = getExposedInstruction(i);
        ASSERT(x->getContainer()->isFunction());
        Function* function = (Function*)x->getContainer();

        if (x->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(x->getTargetAddress());
            
            if (functionSymbol){

                if (functionList && functionList->matches(functionSymbol->getSymbolName(), 0)){
                    continue;
                }
                ASSERT(x->getSizeInBytes() == Size__uncond_jump);
                
                std::string c;
                c.append(functionSymbol->getSymbolName());
                if (functions.count(c) == 0){
                    FuncInfo f = FuncInfo();
                    f.name = c;
                    f.file = "";
                    f.line = 0;
                    f.index = funcIdx++;

                    LineInfo* li = NULL;
                    if (lineInfoFinder){
                        li = lineInfoFinder->lookupLineInfo(x->getTargetAddress());
                    }

                    if (li){
                        f.file.append(li->getFileName());
                        f.line = li->GET(lr_line);
                    }

                    functions[c] = f;
                    orderedfuncs.push_back(c);
                }
                uint32_t idx = functions[c].index;

                PRINT_INFOR("Instrumenting call to %s (idx %d)", c.c_str(), idx);

                Base* exitpoint = (Base*)x;
                if (c == "__libc_start_main"){
                    PRINT_INFOR("Special case: instrumenting _fini for __libc_start_main");
                    exitpoint = (Base*)getProgramExitBlock();
                }

                InstrumentationPoint* prior = addInstrumentationPoint(x, functionEntry, InstrumentationMode_tramp, InstLocation_prior);
                InstrumentationPoint* after = addInstrumentationPoint(exitpoint, functionExit, InstrumentationMode_tramp, InstLocation_after);

                assignStoragePrior(prior, idx, getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
                assignStoragePrior(after, idx, getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
            }            
        }
    }

    functionRegister->addArgument(nameAddr);
    functionRegister->addArgument(fileAddr);
    functionRegister->addArgument(lineAddr);
    functionRegister->addArgument(siteIndexAddr);

    // go over every function that was found, insert a registration call at program start
    funcIdx = 0;
    for (std::vector<std::string>::iterator it = orderedfuncs.begin(); it != orderedfuncs.end(); it++){
        std::string name = *it;
        FuncInfo f = functions[name];
        PRINT_INFOR("Adding registration call for %s %d", f.name.c_str(), f.index);
       
        ASSERT(f.name == name);
        ASSERT(f.index == funcIdx);
        funcIdx++;

        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), functionRegister, InstrumentationMode_tramp);

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
            PRINT_INFOR("\t\thave line info: %s %d", cstring2, f.line);
        }

        assignStoragePrior(p, f.line, getInstDataAddress() + lineAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
        assignStoragePrior(p, f.index, getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
    }

}
