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

#include <Base.h>
#include <BasicBlockCounter.h>
#include <CacheSimulation.h>
#include <CallReplace.h>
#include <Classification.h>
#include <DynamicTable.h>
#include <ElfFile.h>
#include <FunctionCounter.h>
#include <FunctionTimer.h>
#include <ThrottleLoop.h>
#include <Vector.h>

#define DEFAULT_FUNC_BLACKLIST "scripts/inputlist/autogen-system.func"

#define SUCCESS_MSG "******** Instrumentation Successfull ********"

void printBriefOptions(bool detail){
    fprintf(stderr,"\n");
    fprintf(stderr,"Brief Descriptions for Options:\n");
    fprintf(stderr,"===============================\n");
    fprintf(stderr,"\t--help : print this help message\n");
    fprintf(stderr,"\t--version : print version number and exit\n");
    fprintf(stderr,"\t--silent : supress all non-critical messages\n");
    fprintf(stderr,"\t--typ : required for all.\n");
    fprintf(stderr,"\t--app : required for all.\n");
    fprintf(stderr,"\t--inf : optional for all. prints informative details about parts of the application binary.\n");
    if (detail){
        fprintf(stderr,"\t      : a : all parts of the application\n");
        fprintf(stderr,"\t      : b : <none>\n");
        fprintf(stderr,"\t      : c : full instruction printing (implies disassembly)\n");
        fprintf(stderr,"\t      : d : disassembly\n");
        fprintf(stderr,"\t      : e : all elf parts (everything but instructions)\n");
        fprintf(stderr,"\t      : f : <none>\n");
        fprintf(stderr,"\t      : g : global offset table\n");
        fprintf(stderr,"\t      : h : section headers\n");
        fprintf(stderr,"\t      : i : instrumentation reservations (on by default)\n");
        fprintf(stderr,"\t      : j : hash table\n");
        fprintf(stderr,"\t      : k : <none>\n");
        fprintf(stderr,"\t      : l : <none>\n");
        fprintf(stderr,"\t      : m : gnu version symbol table\n");
        fprintf(stderr,"\t      : n : note sections\n");
        fprintf(stderr,"\t      : o : loop info\n");
        fprintf(stderr,"\t      : p : program headers\n");
        fprintf(stderr,"\t      : q : <none>\n");
        fprintf(stderr,"\t      : r : relocation tables\n");
        fprintf(stderr,"\t      : s : string tables\n");
        fprintf(stderr,"\t      : t : symbol tables\n");
        fprintf(stderr,"\t      : u : <none>\n");
        fprintf(stderr,"\t      : v : gnu version needs table\n");
        fprintf(stderr,"\t      : w : dwarf debug sections\n");
        fprintf(stderr,"\t      : x : all headers\n");
        fprintf(stderr,"\t      : y : dynamic table\n");
        fprintf(stderr,"\t      : z : <none>\n");   
    }
    fprintf(stderr,"\t--dry : optional for all. processes options only.\n");
    fprintf(stderr,"\t--lib : (deprecated) optional for all. shared library directory.\n");
    fprintf(stderr,"\t        default is $PEBIL_ROOT/lib\n");
    fprintf(stderr,"\t--fbl : optional for all. input file which lists blacklisted functions. if first line of file is '*\\n', this is treated as a whitelist\n");
    fprintf(stderr,"\t        default is %s\n", DEFAULT_FUNC_BLACKLIST);
    fprintf(stderr,"\t--ext : optional for all. default is (typ)inst, such as\n");
    fprintf(stderr,"\t        jbbinst for type jbb.\n");
    fprintf(stderr,"\t--dtl : detailed .static file with lineno and filenames\n");
    fprintf(stderr,"\t        DEPRECATED: ALWAYS ON BY DEFAULT\n");
    fprintf(stderr,"\t--inp : required for sim/csc and thr/crp.\n");
    fprintf(stderr,"\t--lpi : optional for sim/csc. loop level block inclusion for\n");
    fprintf(stderr,"\t        cache simulation. default is no.\n");
    fprintf(stderr,"\t--phs : optional for sim/csc. phase number. defaults to no phase,\n"); 
    fprintf(stderr,"\t        otherwise, .phase.N. is included in output file names\n");
    fprintf(stderr,"\t--trk : required for crp. input file which lists the functions to track\n");
    fprintf(stderr,"\t--lnc : required for crp. list of shared libraries to use, comma seperated\n");
    fprintf(stderr,"\t--dmp : optional for sim/csc. dump the address stream to disk.\n");
    fprintf(stderr,"\t        default is off");
    fprintf(stderr,"\t--dfp : optional for sim/csc. dfpattern file. defaults to no dfpattern file,\n");
    fprintf(stderr,"\t--doi : optional for crp. whether to call intro/exit functions. default is no\n");
    fprintf(stderr,"\t--allow-static : optional for all. allows pebil to try to instrument static-linked binary. default is no\n");
    fprintf(stderr,"\n");
}

void printUsage(bool shouldExt=true, bool optDetail=false) {
    fprintf(stderr,"\n");
    fprintf(stderr,"usage : pebil\n");
    fprintf(stderr,"\t--typ (ide|fnc|jbb|sim|bin|csc|ftm|crp|thr)\n");
    fprintf(stderr,"\t--app <executable_path>\n");
    fprintf(stderr,"\t[--inp <block_unique_ids>]    <-- valid for sim/csc and thr/crp\n");
    fprintf(stderr,"\t[--inf [a-z]*]\n");
    fprintf(stderr,"\t[--lib (deprecated) <shared_lib_dir>]\n");
    fprintf(stderr,"\t\tdefault is $PEBIL_ROOT/lib\n");
    fprintf(stderr,"\t[--ext <output_suffix>]\n");
    fprintf(stderr,"\t[--dtl]\n");
    fprintf(stderr,"\t[--fbl file]\n");
    fprintf(stderr,"\t[--lpi]                     <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--phs <phase_no>]          <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--trk file]                <-- required for crp and thr\n");
    fprintf(stderr,"\t[--lnc <lib_list>]          <-- required for crp and thr\n");
    fprintf(stderr,"\t[--dfp <pattern_file>]      <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--dmp (off|on|nosim)]      <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--doi]                     <-- valid for crp\n");
    fprintf(stderr,"\t[--allow-static]\n");
    fprintf(stderr,"\t[--help]\n");
    fprintf(stderr,"\t[--version]\n");
    fprintf(stderr,"\t[--silent]\n");
    fprintf(stderr,"\n");
    if(shouldExt){
        printBriefOptions(optDetail);
        exit(-1);
    }
}

typedef enum {
    dumpcode_off = 0,
    dumpcode_nosim = 1,
    dumpcode_on = 2,
    Total_DumpCode
} DumpCode;


uint32_t processPrintCodes(char* rawPrintCodes){
    uint32_t printCodes = 0;
    for (uint32_t i = 0; i < strlen(rawPrintCodes); i++){
        char pc = rawPrintCodes[i];
        if (pc == 'a'){
            SET_PRINT_CODE(printCodes,Print_Code_All);
        } else if (pc == 'e'){
            PRINT_ERROR("The flag `%c' not yet implemented", pc);
        } else if (pc == 'f'){
            SET_PRINT_CODE(printCodes,Print_Code_FileHeader);
        } else if (pc == 'p'){
            SET_PRINT_CODE(printCodes,Print_Code_ProgramHeader);
        } else if (pc == 'h'){
            SET_PRINT_CODE(printCodes,Print_Code_SectionHeader);
        } else if (pc == 'x'){
            SET_PRINT_CODE(printCodes,Print_Code_FileHeader);
            SET_PRINT_CODE(printCodes,Print_Code_ProgramHeader);
            SET_PRINT_CODE(printCodes,Print_Code_SectionHeader);
        } else if (pc == 'd'){
            SET_PRINT_CODE(printCodes,Print_Code_Disassemble);
        } else if (pc == 's'){
            SET_PRINT_CODE(printCodes,Print_Code_StringTable);
        } else if (pc == 't'){
            SET_PRINT_CODE(printCodes,Print_Code_SymbolTable);
        } else if (pc == 'r'){
            SET_PRINT_CODE(printCodes,Print_Code_RelocationTable);
        } else if (pc == 'n'){
            SET_PRINT_CODE(printCodes,Print_Code_NoteSection);
        } else if (pc == 'g'){
            SET_PRINT_CODE(printCodes,Print_Code_GlobalOffsetTable);
        } else if (pc == 'j'){
            SET_PRINT_CODE(printCodes,Print_Code_HashTable);
        } else if (pc == 'v'){
            SET_PRINT_CODE(printCodes,Print_Code_GnuVerneedTable);
        } else if (pc == 'm'){
            SET_PRINT_CODE(printCodes,Print_Code_GnuVersymTable);
        } else if (pc == 'y'){
            SET_PRINT_CODE(printCodes,Print_Code_DynamicTable);
        } else if (pc == 'i'){
            SET_PRINT_CODE(printCodes,Print_Code_Instrumentation);
        } else if (pc == 'c'){
            SET_PRINT_CODE(printCodes,Print_Code_Instruction);
            SET_PRINT_CODE(printCodes,Print_Code_Disassemble);
        } else if (pc == 'w'){
            SET_PRINT_CODE(printCodes,Print_Code_DwarfSection);
        } else if (pc == 'o'){
            SET_PRINT_CODE(printCodes,Print_Code_Loops);
        } else {
            printUsage(true, true);
        }
    }
    return printCodes;
}


typedef enum {
    unknown_inst_type = 0,
    identical_inst_type,
    frequency_inst_type,
    simulation_inst_type,
    classification_inst_type,
    function_counter_type,
    func_timer_type,
    call_wrapper_type,
    throttle_loop_type,
    Total_InstrumentationType
} InstrumentationType;

int main(int argc,char* argv[]){
    char*    inptName   = NULL;
    char*    extension  = "";
    char*    libPath    = NULL;
    char*    libArg     = NULL;
    int32_t  phaseNo    = 0;
    uint32_t instType   = unknown_inst_type;
    uint32_t argApp     = 0;
    uint32_t argTyp     = 0;
    uint32_t argExt     = 0;
    uint32_t argPhs     = 0;
    uint32_t argInp     = 0;
    uint32_t argDfp     = 0;
    bool     loopIncl   = false;
    bool     extdPrnt   = true;
    bool     verbose    = false;
    bool     dryRun     = false;
    bool     doIntro    = false;
    uint32_t printCodes = 0x00000000;
    char* rawPrintCodes = NULL;
    char* inputFuncList = NULL;
    char* inputTrackList = NULL;
    char* libList       = NULL;
    bool deleteInpList  = false;
    char*    execName   = NULL;
    char*    appName    = NULL;
    char*    dfpName    = NULL;
    uint32_t dumpCode   = Total_DumpCode;
    bool runSilent      = false;
    bool allowStatic    = false;

    TIMER(double t = timer());
    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i],"--app")){
            if (argApp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            execName = argv[++i];
        } else if (!strcmp(argv[i],"--typ")){
            if (argTyp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            ++i;
            if (i >= argc){
                fprintf(stderr,"\nError : No argument supplied to --typ\n");
                printUsage();
            } 
            if (!strcmp(argv[i],"ide")){
                instType = identical_inst_type;
                extension = "ideinst";
            } else if (!strcmp(argv[i],"fnc")){
                instType = function_counter_type;
                extension = "fncinst";
            } else if (!strcmp(argv[i],"jbb")){
                instType = frequency_inst_type;
                extension = "jbbinst";
            } else if (!strcmp(argv[i],"sim")){
                instType = simulation_inst_type;
                extension = "siminst";
            } else if (!strcmp(argv[i],"csc")){
                instType = simulation_inst_type;
                extension = "cscinst";
            } else if (!strcmp(argv[i],"bin")){
                instType = classification_inst_type;
                extension = "bininst";
            } else if (!strcmp(argv[i],"ftm")){
                instType = func_timer_type;
                extension = "ftminst";
            } else if (!strcmp(argv[i],"crp")){
                instType = call_wrapper_type;
                extension = "crpinst";
            } else if (!strcmp(argv[i],"thr")){
                instType = throttle_loop_type;
                extension = "thrinst";
            }
        } else if (!strcmp(argv[i],"--help")){
            printUsage(true, true);
        } else if (!strcmp(argv[i],"--version")){
            fprintf(stdout, "pebil %s\n", PEBIL_VER);
            exit(0);
        }
    }

    if (!execName){
        fprintf(stderr,"\nError : No executable is specified\n\n");
        printUsage();
    } else {
        // remove the path from the filename
        appName = new char[__MAX_STRING_SIZE];
        uint32_t startApp = 0;
        for (uint32_t i = 0; i < strlen(execName); i++){
            if (execName[i] == '/'){
                startApp = i + 1;
            }
        }
        sprintf(appName, "%s\0", execName + startApp);
    }

    if ((instType <= unknown_inst_type) || 
       (instType >= Total_InstrumentationType)){
        fprintf(stderr,"\nError : Unknown instrumentation type\n");
        printUsage();
    }

    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i],"--app") || !strcmp(argv[i],"--typ")){
            ++i;
        } else if (!strcmp(argv[i],"--ext")){
            if (argExt++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            extension = argv[++i];
        } else if(!strcmp(argv[i],"--phs")){
            if (argPhs++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            if (instType != simulation_inst_type){
                fprintf(stderr,"\nError : Option %s is not valid for typ other than sim/csc\n",argv[i]);
                printUsage();
            }

            ++i;
            char* endptr = NULL;
            phaseNo = strtol(argv[i],&endptr,10);
            if ((endptr == argv[i]) || !phaseNo){
                fprintf(stderr,"\nError : Given phase number is not correct, requires > 0\n\n");
                printUsage();
            }

        } else if (!strcmp(argv[i],"--inp")){
            if (argInp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            if (instType != simulation_inst_type && instType != throttle_loop_type && instType != call_wrapper_type){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i]);
                printUsage();
            }

            inptName = argv[++i];
        } else if (!strcmp(argv[i],"--lpi")){
            loopIncl = true;
            if (instType != simulation_inst_type){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i++]);
                printUsage();
            }
        } else if (!strcmp(argv[i], "--dfp")){
            if (argDfp++){
                fprintf(stderr, "\nError: Duplicate %s option\n", argv[i]);
                printUsage();
            }
            if (instType != simulation_inst_type){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i++]);
                printUsage();
            }
            dfpName = argv[++i];
        } else if (!strcmp(argv[i],"--inf")){
            verbose = true;
            rawPrintCodes = argv[++i];
        } else if (!strcmp(argv[i],"--dtl")){
            extdPrnt = true;
        } else if (!strcmp(argv[i],"--lib")){
            libArg = argv[++i];
        } else if (!strcmp(argv[i],"--dry")){
            dryRun = true;
        } else if (!strcmp(argv[i],"--doi")){
            doIntro = true;
        } else if (!strcmp(argv[i],"--fbl")){
            if (inputFuncList){
                printUsage();
            }
            inputFuncList = argv[++i];
        } else if (!strcmp(argv[i],"--trk")){
            if (inputTrackList){
                printUsage();
            }
            inputTrackList = argv[++i];
        } else if (!strcmp(argv[i],"--lnc")){
            if (libList){
                printUsage();
            }
            libList = argv[++i];
        } else if (!strcmp(argv[i],"--dmp")){
            if (dumpCode != Total_DumpCode){
                fprintf(stderr,"\nError : Option %d already given\n",argv[i]);
                printUsage(argv);
            }
            ++i;
            if (!strcmp(argv[i],"off")){
                dumpCode = dumpcode_off;
            } else if (!strcmp(argv[i],"on")){
                dumpCode = dumpcode_on;
            } else if (!strcmp(argv[i],"nosim")){
                dumpCode = dumpcode_nosim;
            } else {
                fprintf(stderr,"\nError : Option %s given to --dmp is invalid\n",argv[i]);
                printUsage(argv);
            }
        } else if (!strcmp(argv[i],"--silent")){
            runSilent = true;
        } else if (!strcmp(argv[i],"--allow-static")){
            allowStatic = true;
        } else {
            fprintf(stderr,"\nError : Unknown switch at %s\n\n",argv[i]);
            printUsage();
        }
    }

    if (dumpCode == Total_DumpCode){
        dumpCode = dumpcode_off;
    }
    if (dumpCode != dumpcode_off){
        fprintf(stderr, "\tError : --dmp must be off");
        printUsage();
    }

    if ((instType == simulation_inst_type || instType == throttle_loop_type) && !inptName){
        fprintf(stderr,"\nError : Input is required for cache simulation instrumentation\n\n");
        printUsage();
    }

    ASSERT(instType == simulation_inst_type || phaseNo == 0);
    if (instType == simulation_inst_type){
        ASSERT(!phaseNo || phaseNo == 1 && "Error : Support for multiple phases is deprecated");
    }

    if (runSilent){
        pebilOutp = fopen("/dev/null", "w");
        if (!pebilOutp){
            PRINT_WARN(10, "Cannot open file /dev/null for silent output");
            pebilOutp = stdout;
        }
    }
    PRINT_INFOR("application name: %s", appName);

    if (verbose){
        if (!rawPrintCodes){
            fprintf(stderr,"\tError: verbose option used without argument");
            printUsage();
        }
        printCodes = processPrintCodes(rawPrintCodes);
    }

    if (dryRun){
        PRINT_INFOR("--dry option was used, exiting...");
        exit(0);
    }

    uint32_t stepNumber = 0;
    TIMER(double t1 = timer(), t2);

    ElfFile elfFile(execName, appName);
    elfFile.parse();

    if (!inputFuncList){
        deleteInpList = true;
        char* pebilRoot = getenv("PEBIL_ROOT");
        if (!pebilRoot){
            PRINT_ERROR("Set the PEBIL_ROOT variable"); 
        }
        inputFuncList = new char[__MAX_STRING_SIZE];
        sprintf(inputFuncList, "%s/%s", pebilRoot, DEFAULT_FUNC_BLACKLIST);
    }
    PRINT_INFOR("The function blacklist is taken from %s", inputFuncList);

    elfFile.initSectionFilePointers();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Parse  : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    elfFile.generateCFGs();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Disasm/cfg : %.2f seconds",++stepNumber,t2-t1);t1=t2);    

    elfFile.findLoops();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Loop    : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);

    if (verbose){
        elfFile.print(printCodes);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    }

    elfFile.verify();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Verify  : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    ElfFileInst* elfInst = NULL;

    if (instType == identical_inst_type){
        elfFile.dump(extension);
        PRINT_INFOR(SUCCESS_MSG);
        return 0;
    } else if (instType == function_counter_type){
        elfInst = new FunctionCounter(&elfFile, extension, loopIncl, extdPrnt);
    } else if (instType == frequency_inst_type){
        elfInst = new BasicBlockCounter(&elfFile, extension, loopIncl, extdPrnt);
    } else if (instType == simulation_inst_type){
        elfInst = new CacheSimulation(&elfFile, inptName, extension, phaseNo, loopIncl, extdPrnt, dfpName);
    } else if (instType == classification_inst_type){
        elfInst = new Classification(&elfFile, extension, loopIncl, extdPrnt);
    } else if (instType == func_timer_type){
        elfInst = new FunctionTimer(&elfFile, extension, loopIncl, extdPrnt);
    } else if (instType == call_wrapper_type){
        if (!inputTrackList){
            fprintf(stderr, "\nError: option --trk needs to be given with call wrapper inst\n");
            printUsage();
        }
        ASSERT(inputTrackList);
        if (!libList){
            fprintf(stderr, "\nError: option --lnc needs to be given with call wrapper inst\n");
        }
        ASSERT(libList);
        elfInst = new CallReplace(&elfFile, inputTrackList, libList, inptName, extension, loopIncl, extdPrnt, doIntro);
    } else if (instType == throttle_loop_type){
        if (!libList){
            fprintf(stderr, "\nError: option --lnc needs to be given with loop throttle inst\n");
        }
        ASSERT(libList);
        if (!inputTrackList){
            fprintf(stderr, "\nError: option --trk needs to be given with loop throttle inst\n");
            printUsage();
        }
        ASSERT(inputTrackList);
        elfInst = new ThrottleLoop(&elfFile, inptName, inputTrackList, libList, extension, loopIncl, extdPrnt);
    }
    else {
        PRINT_ERROR("Error : invalid instrumentation type");
    }

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(elfInst);
    ASSERT(extension);

    if (phaseNo > 0){
        char* tmp = new char[__MAX_STRING_SIZE];
        sprintf(tmp, "phase.%d.%s", phaseNo, extension);
        extension = tmp;
    }

    elfInst->setInstExtension(extension);
    if (inputFuncList){
        elfInst->setInputFunctions(inputFuncList);
    }
    if (allowStatic){
        elfInst->setAllowStatic();
    }
    
    elfInst->phasedInstrumentation();
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    elfInst->print(Print_Code_Instrumentation);
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Instr   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    
    elfFile.printDynamicLibraries();
    if (verbose){
        elfInst->print(printCodes);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    }
    
    elfInst->dump();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Dump    : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    if (verbose){
        elfInst->print(printCodes);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    }

    // arrrrrrrrrg. i can't figure out why just deleting elfInst doesn't
    // call the CallReplace destructor in this case without the cast. 
    if (instType == call_wrapper_type){
        delete (CallReplace*)elfInst;
    } else if (instType == simulation_inst_type){
        delete (CacheSimulation*)elfInst;
    } else if (instType == throttle_loop_type){
        delete (ThrottleLoop*)elfInst;
    } else {
        delete elfInst;
    }

    if (deleteInpList){
        delete[] inputFuncList;
    }
    delete[] libPath;
    delete[] appName;

    PRINT_INFOR(SUCCESS_MSG);

    TIMER(t = timer()-t;PRINT_INFOR("___timer: Total Execution Time          : %.2f seconds",t););

    PRINT_INFOR("");
    PRINT_INFOR("******** DONE ******** SUCCESS ***** SUCCESS ***** SUCCESS ********");
    PRINT_INFOR("");
    return 0;
}
