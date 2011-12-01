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
//#include <DynamicTable.h>
//#include <ElfFile.h>
#include <InstrumentationTool.h>

#define INVALID_LIB_NAME "INVALIDLIBNAME"
#define DEFAULT_FUNC_BLACKLIST "scripts/inputlist/autogen-system.func"

#define SUCCESS_MSG "******** Instrumentation Successfull ********"

void printDone(){
    PRINT_INFOR("");
    PRINT_INFOR("******** DONE ******** SUCCESS ***** SUCCESS ***** SUCCESS ********");
    PRINT_INFOR("");
}

void printUsage(bool shouldExt=true, bool optDetail=false) {
    fprintf(stderr,"\n");
    fprintf(stderr,"usage : pebil\n");
    fprintf(stderr,"\t{tool selection} (use exactly one)\n");
    fprintf(stderr,"\t\t--tool <ClassName> : provide the name of an arbitrary instrumentation tool (eg. `--tool BasicBlockCounter' is identical to `--typ jbb')\n");
    fprintf(stderr,"\t\t--typ <ide|jbb|sim|csc> : selects pre-made instrumentation tool\n");
    fprintf(stderr,"\t{executable selection} (at least one required)\n");
    fprintf(stderr,"\t\t--app <executable/path> : executable to instrument\n");
    fprintf(stderr,"\t\t<path/to/app1> <path/to/app2> (TODO)\n");
    fprintf(stderr,"\t{pebil options} (affect all tools)\n");
    fprintf(stderr,"\t\t[--inf [a-z]*] : print details about application binary\n");
    if (false){
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
    fprintf(stderr,"\t\t[--lib <shared_lib_dir>] : DEPRECATED, kept for compatibility\n");
    fprintf(stderr,"\t\t[--fbl <file/containing/function/list>] : list of functions to exclude from instrumentaiton (default is %s)\n", DEFAULT_FUNC_BLACKLIST);
    fprintf(stderr,"\t\t[--ext <output_suffix>] : override default file extension for instrumented executable\n");
    fprintf(stderr,"\t\t[--lnc <lib1.so,lib2.so>] : list of shared libraries to put in executable's dynamic table\n");
    fprintf(stderr,"\t\t[--allow-static] : allow static-linked executable\n");
    fprintf(stderr,"\t\t[--help] : print help message and exit\n");
    fprintf(stderr,"\t\t[--version] : print version number and exit\n");
    fprintf(stderr,"\t\t[--silent] : print nothing to stdout\n");
    fprintf(stderr,"\t{tool options} (each tool decides if/how to use these)\n");
    fprintf(stderr,"\t\t[--inp <input/file>] : path to an input file\n");
    fprintf(stderr,"\t\t[--dtl] : DEPRECATED (always on), kepy for compatibility\n");
    fprintf(stderr,"\t\t[--lpi] : include loop-level analysis\n");
    fprintf(stderr,"\t\t[--doi] : do special initialization\n");
    fprintf(stderr,"\t\t[--phs <phase_no>] : phase number\n");
    fprintf(stderr,"\t\t[--trk <tracking/file>] : path to a tracking file\n");
    fprintf(stderr,"\t\t[--dfp <pattern/file>] : path to pattern file\n");
    fprintf(stderr,"\t\t[--dmp <off|on|nosim>] : DEPRECATED, kept for compatibility\n");
    fprintf(stderr,"\n");
    if(shouldExt){
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
    Total_InstrumentationType
} InstrumentationType;

/* for backward compatibility with old-style args (--typ)*/
static char* ToolNames[Total_InstrumentationType] = {
    INVALID_LIB_NAME,
    INVALID_LIB_NAME,
    "BasicBlockCounter",
    "CacheSimulation"
};

int main(int argc,char* argv[]){
    char*    inptName   = NULL;
    char*    libPath    = NULL;
    char*    libArg     = NULL;
    char*    extension  = NULL;
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
    char* toolName      = NULL;

    TIMER(double t = timer());
    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i],"--app")){
            if (argApp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            execName = argv[++i];
        } else if (!strcmp(argv[i],"--tool")){
            if (toolName != NULL){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage();
            }
            toolName = argv[++i];
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
            } else if (!strcmp(argv[i],"jbb")){
                instType = frequency_inst_type;
            } else if (!strcmp(argv[i],"sim")){
                instType = simulation_inst_type;
            } else if (!strcmp(argv[i],"csc")){
                instType = simulation_inst_type;
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
        if (toolName == NULL){
            fprintf(stderr,"\nError : Unknown instrumentation type, either --typ or --tool is required\n");
            printUsage();
        }
    }

    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i],"--app") || !strcmp(argv[i],"--typ") || !strcmp(argv[i],"--tool")){
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

            inptName = argv[++i];
        } else if (!strcmp(argv[i],"--lpi")){
            loopIncl = true;
        } else if (!strcmp(argv[i], "--dfp")){
            if (argDfp++){
                fprintf(stderr, "\nError: Duplicate %s option\n", argv[i]);
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
                printUsage();
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
                printUsage();
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

    if (phaseNo > 1){
        PRINT_ERROR("Error : Support for multiple phases is deprecated");
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
    InstrumentationTool* instTool = NULL;
    void *libHandle = NULL;

    if (instType > unknown_inst_type && instType < Total_InstrumentationType){
        toolName = ToolNames[instType];
    }

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

    if (instType == identical_inst_type){
        elfFile.dump(extension);
        PRINT_INFOR("Dumping identical binary from stored executable information");
        printDone();
        return 0;
    }

    char toolLibName[__MAX_STRING_SIZE];
    char toolConstructor[__MAX_STRING_SIZE];
    sprintf(toolLibName, "lib%sTool.so\0", toolName);
    sprintf(toolConstructor, "%sMaker", toolName);
    
    libHandle = dlopen(toolLibName, RTLD_NOW);
    if(libHandle == NULL){
        PRINT_ERROR("cannot open tool library %s, it needs to be in your LD_LIBRARY_PATH", toolLibName);
        exit(1);
    }
    void *maker = dlsym(libHandle, toolConstructor);
    if (maker == NULL){
        PRINT_ERROR("cannot find function %s in %s", toolConstructor, toolLibName);
        exit(1);
    }
    instTool = reinterpret_cast<InstrumentationTool*(*)(ElfFile*)>(maker)(&elfFile);
    ASSERT(!strcmp(toolName, instTool->briefName()) && "name yielded by briefName does not match tool name");
    
    if (libList){
        instTool->setLibraryList(libList);
    }
    
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(instTool);
    
    if (inputFuncList){
        instTool->setInputFunctions(inputFuncList);
    }
    if (allowStatic){
        instTool->setAllowStatic();
    }
    
    instTool->init(extension);
    instTool->initToolArgs(phaseNo, loopIncl, extdPrnt, inptName, dfpName, inputTrackList, doIntro);
    if (!instTool->verifyArgs()){
        fprintf(stderr,"\nError : Argument missing/incorrect\n\n");
        printUsage();
    }

    ASSERT(instTool);
    instTool->phasedInstrumentation();
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    instTool->print(Print_Code_Instrumentation);
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Instr   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    
    elfFile.printDynamicLibraries();
    if (verbose){
        instTool->print(printCodes);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    }
    
    instTool->dump();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Dump    : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    if (verbose){
        instTool->print(printCodes);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    }

    delete instTool;
    dlclose(libHandle);

    if (deleteInpList){
        delete[] inputFuncList;
    }
    delete[] libPath;
    delete[] appName;

    PRINT_INFOR(SUCCESS_MSG);

    TIMER(t = timer()-t;PRINT_INFOR("___timer: Total Execution Time          : %.2f seconds",t););

    printDone();

    if (runSilent){
        fclose(pebilOutp);
    }

    return 0;
}
