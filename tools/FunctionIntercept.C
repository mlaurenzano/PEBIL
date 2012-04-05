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

#include <FunctionIntercept.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <map>
#include <string>

#define INST_LIB_NAME "libsample.so"

extern "C" {
    InstrumentationTool* FunctionInterceptMaker(ElfFile* elf){
        return new FunctionIntercept(elf);
    }
}

FunctionIntercept::FunctionIntercept(ElfFile* elf)
    : InstrumentationTool(elf)
{
    functionRegister = NULL;

    functionEntry = NULL;
    functionExit = NULL;
}

void FunctionIntercept::declare(){
    InstrumentationTool::declare();

    // declare any shared library that will contain instrumentation functions
    declareLibrary(INST_LIB_NAME);

    // declare any instrumentation functions that will be used
    functionRegister = declareFunction("trace_register_func");
    ASSERT(functionRegister);

    functionEntry = declareFunction("traceEntry");
    ASSERT(functionEntry);

    functionExit = declareFunction("traceExit");
    ASSERT(functionExit);
}

void FunctionIntercept::instrument(){
    InstrumentationTool::instrument();

    InstrumentationPoint* p;

    uint64_t nameAddr = reserveDataOffset(sizeof(uint64_t));
    uint64_t siteIndexAddr = reserveDataOffset(sizeof(uint32_t));

    functionEntry->addArgument(siteIndexAddr);
    functionExit->addArgument(siteIndexAddr);

    std::map<std::string, uint32_t> functions;
    uint32_t funcIdx = 0;

    // go over all instructions. when we find a call, instrument it
    for (uint32_t i = 0; i < getNumberOfExposedInstructions(); i++){
        X86Instruction* x = getExposedInstruction(i);
        ASSERT(x->getContainer()->isFunction());
        Function* function = (Function*)x->getContainer();

        if (x->isFunctionCall()){
            Symbol* functionSymbol = getElfFile()->lookupFunctionSymbol(x->getTargetAddress());
            
            if (functionSymbol){
                ASSERT(x->getSizeInBytes() == Size__uncond_jump);
                
                std::string c;
                c.append(functionSymbol->getSymbolName());
                if (functions.count(c) == 0){
                    functions[c] = funcIdx++;
                }
                uint32_t idx = functions[c];

                PRINT_INFOR("Instrumenting call to %s (idx %d)", c.c_str(), idx);

                InstrumentationPoint* prior = addInstrumentationPoint(x, functionEntry, InstrumentationMode_tramp, x->allFlagsDeadIn() ? FlagsProtectionMethod_none : FlagsProtectionMethod_full, InstLocation_prior);
                InstrumentationPoint* after = addInstrumentationPoint(x, functionExit, InstrumentationMode_tramp, x->allFlagsDeadOut() ? FlagsProtectionMethod_none : FlagsProtectionMethod_full, InstLocation_after);

                assignStoragePrior(prior, idx, getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
                assignStoragePrior(after, idx, getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
                
            }            
        }
    }

    functionRegister->addArgument(nameAddr);
    functionRegister->addArgument(siteIndexAddr);

    // go over every function that was found, insert a registration call at program start
    for (std::map<std::string, uint32_t>::iterator it = functions.begin(); it != functions.end(); it++){
        std::string name = it->first;
        const char* cname = name.c_str();
        uint32_t idx = it->second;

        InstrumentationPoint* p = addInstrumentationPoint(getProgramEntryBlock(), functionRegister, InstrumentationMode_tramp);

        uint64_t nameStorage = reserveDataOffset(strlen(cname) + 1);
        //PRINT_INFOR("Adding registration call for %s %d @ %#llx", cname, idx, getInstDataAddress() + nameStorage);

        initializeReservedData(getInstDataAddress() + nameStorage, strlen(cname), (void*)cname);

        assignStoragePrior(p, getInstDataAddress() + nameStorage, getInstDataAddress() + nameAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
        assignStoragePrior(p, idx, getInstDataAddress() + siteIndexAddr, X86_REG_CX, getInstDataAddress() + getRegStorageOffset());
    }

}
