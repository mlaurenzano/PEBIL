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

#include <DeadRegisterSquasher.h>

#include <BasicBlock.h>
#include <Function.h>
#include <Instrumentation.h>
#include <X86Instruction.h>
#include <X86InstructionFactory.h>
#include <LineInformation.h>
#include <Loop.h>
#include <TextSection.h>

#define NOSTRING "__pebil_no_string__"

extern "C" {
    InstrumentationTool* DeadRegisterSquasherMaker(ElfFile* elf){
        return new DeadRegisterSquasher(elf);
    }
}

DeadRegisterSquasher::DeadRegisterSquasher(ElfFile* elf)
    : InstrumentationTool(elf)
{
}

void DeadRegisterSquasher::declare()
{
    InstrumentationTool::declare();
    ASSERT(currentPhase == ElfInstPhase_user_declare && "Instrumentation phase order must be observed"); 
}

void DeadRegisterSquasher::instrument() 
{
    InstrumentationTool::instrument();
    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
    uint32_t temp32;
    uint64_t temp64;

    LineInfoFinder* lineInfoFinder = NULL;
    if (hasLineInformation()){
        lineInfoFinder = getLineInfoFinder();
    }

    uint64_t appName = reserveDataOffset((strlen(getApplicationName()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + appName, strlen(getApplicationName()) + 1, getApplicationName());
    uint64_t instExt = reserveDataOffset((strlen(getExtension()) + 1) * sizeof(char));
    initializeReservedData(getInstDataAddress() + instExt, strlen(getExtension()) + 1, (void*)getExtension());

    uint64_t noDataAddr = getInstDataAddress() + reserveDataOffset(strlen(NOSTRING) + 1);
    char* nostring = new char[strlen(NOSTRING) + 1];
    sprintf(nostring, "%s\0", NOSTRING);
    initializeReservedData(noDataAddr, strlen(NOSTRING) + 1, nostring);


    // For each function
    uint32_t nFuncs = getNumberOfExposedFunctions();
    for(uint32_t i = 0; i < nFuncs; ++i){
        Function * func = getExposedFunction(i);
        //func->digest(getElfFile()->getAddressAnchors());

        uint32_t nInstructions = func->getNumberOfInstructions();
        X86Instruction** instructions = new X86Instruction*[nInstructions];
        func->getAllInstructions(instructions, 0);

        // for each instruction
        for(uint32_t j = 0; j < nInstructions; ++j){
           X86Instruction* instruction = instructions[j];
           InstrumentationSnippet* snip = new InstrumentationSnippet();

            // for each dead register
            for(uint32_t reg = 0; reg < X86_64BIT_GPRS; ++reg){
                if( !instruction->isRegDeadIn(reg)){
                    continue;
                }
               // squash
               if(is64Bit()){
                   snip->addSnippetInstruction(X86InstructionFactory64::emitMoveImmToReg(0x01234567, reg));
               } else {
                   snip->addSnippetInstruction(X86InstructionFactory::emitMoveImmToReg(0x01234567, reg));
               }
            }

            addInstrumentationSnippet(snip);
            InstrumentationPoint* p = addInstrumentationPoint(instruction, snip, InstrumentationMode_trampinline, FlagsProtectionMethod_none, InstLocation_prior);
            if(!p->getInstBaseAddress()){
                PRINT_ERROR("Cannot find an instrumentation point at instruction");
            }

           
        }
        delete[] instructions;
    }
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    delete[] nostring;

    ASSERT(currentPhase == ElfInstPhase_user_reserve && "Instrumentation phase order must be observed"); 
}


