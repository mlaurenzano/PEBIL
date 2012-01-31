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
#include <InstrumentationTool.h>
#include <Vector.h>
#include <getopt.h>

#define DEFAULT_FUNC_BLACKLIST "scripts/inputlist/autogen-system.func"

void printUsage(const char* msg = NULL){
    if (msg){
        fprintf(stderr, "error: %s", msg);
        fprintf(stderr,"\n");
    }
    fprintf(stderr,"usage : pebil\n");
    fprintf(stderr,"\t{tool selection} (use exactly one)\n");
    fprintf(stderr,"\t\t--tool <ClassName> : provide the name of an arbitrary instrumentation tool (eg. `--tool BasicBlockCounter' is identical to `--typ jbb')\n");
    fprintf(stderr,"\t\t--typ <ide|jbb|sim|csc> : selects pre-made instrumentation tool\n");
    fprintf(stderr,"\t{executable selection} (at least one required)\n");
    fprintf(stderr,"\t\t--app <executable/path> : executable to instrument\n");
    fprintf(stderr,"\t\t[--] <path/to/app1> <path/to/app2>\n");
    fprintf(stderr,"\t{pebil options} (affect all tools)\n");
    fprintf(stderr,"\t\t[--tlib <tool_shlib_name>] : supply a name for tool library (overrides the name gleaned from --tool)\n");
    fprintf(stderr,"\t\t[--inf <a-z*>] : print details about application binary\n");
    if (false){
        fprintf(stderr,"\t      : a : all parts of the application\n");
        fprintf(stderr,"\t      : b : <none>\n");
        fprintf(stderr,"\t      : c : full instruction printing (implies disassembly)\n");
        fprintf(stderr,"\t      : d : disassembly\n");
        fprintf(stderr,"\t      : e : <none>\n");
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
    fprintf(stderr,"\t\t[--lib <shared_lib_dir>] : DEPRECATED link to instrumentation libs from this directory\n");
    fprintf(stderr,"\t\t[--fbl <file/containing/function/list>] : list of functions to exclude from instrumentaiton (default is %s)\n", DEFAULT_FUNC_BLACKLIST);
    fprintf(stderr,"\t\t[--ext <output_suffix>] : override default file extension for instrumented executable\n");
    fprintf(stderr,"\t\t[--lnc <lib1.so,lib2.so>] : list of shared libraries to put in executable's dynamic table\n");
    fprintf(stderr,"\t\t[--allowstatic] : allow static-linked executable\n");
    fprintf(stderr,"\t\t[--help] : print help message and exit\n");
    fprintf(stderr,"\t\t[--version] : print version number and exit\n");
    fprintf(stderr,"\t\t[--silent] : print nothing to stdout\n");
    fprintf(stderr,"\t\t[--dry] : quit before processing any executables\n");
    fprintf(stderr,"\t{tool options} (each tool decides if/how to use these)\n");
    fprintf(stderr,"\t\t[--inp <input/file>] : path to an input file\n");
    fprintf(stderr,"\t\t[--dtl] : DEPRECATED (always on) print details in static files\n");
    fprintf(stderr,"\t\t[--lpi] : DEPRECATED (always on) perform loop inclusion\n");
    fprintf(stderr,"\t\t[--doi] : do special initialization\n");
    fprintf(stderr,"\t\t[--phs <phase_no>] : phase number\n");
    fprintf(stderr,"\t\t[--trk <tracking/file>] : path to a tracking file\n");
    fprintf(stderr,"\t\t[--dfp <pattern/file>] : path to pattern file\n");
    fprintf(stderr,"\t\t[--dmp <off|on|nosim>] : DEPRECATED, kept for compatibility\n");
    fprintf(stderr,"\n");
    exit(1);
}

void printSuccess(){
    PRINT_INFOR("******** Instrumentation Successfull ********");
}

void printDone(){
    PRINT_INFOR("");
    PRINT_INFOR("******** DONE ******** SUCCESS ***** SUCCESS ***** SUCCESS ********");
    PRINT_INFOR("");
}

typedef enum {
    dumpcode_off = 0,
    dumpcode_nosim = 1,
    dumpcode_on = 2,
    Total_DumpCode
} DumpCode;

uint32_t processDumpCode(char* rawDumpCode){
    uint32_t dumpCode = Total_DumpCode;
    if (!strcmp(rawDumpCode, "off")){
        dumpCode = dumpcode_off;
    } else if (!strcmp(rawDumpCode, "on")){
        dumpCode = dumpcode_on;
    } else if (!strcmp(rawDumpCode, "nosim")){
        dumpCode = dumpcode_nosim;
    } else {
        printUsage("argument passed to --dmp is invalid");
    }
    return dumpCode;
}

uint32_t processPrintCodes(char* rawPrintCodes){
    uint32_t printCodes = 0;
    for (uint32_t i = 0; i < strlen(rawPrintCodes); i++){
        char pc = rawPrintCodes[i];

        if (pc == '*'){
            printUsage("unexpected print code given");
            __SHOULD_NOT_ARRIVE;
        }
#define YIELD_PRINT_CODE(__char, __name) else if (pc == __char) { SET_PRINT_CODE(printCodes, __name); }
        YIELD_PRINT_CODE('f', Print_Code_FileHeader)
        YIELD_PRINT_CODE('p', Print_Code_ProgramHeader)
        YIELD_PRINT_CODE('h', Print_Code_SectionHeader)
        YIELD_PRINT_CODE('d', Print_Code_Disassemble)
        YIELD_PRINT_CODE('s', Print_Code_StringTable)
        YIELD_PRINT_CODE('t', Print_Code_SymbolTable)
        YIELD_PRINT_CODE('r', Print_Code_RelocationTable)
        YIELD_PRINT_CODE('n', Print_Code_NoteSection)
        YIELD_PRINT_CODE('g', Print_Code_GlobalOffsetTable)
        YIELD_PRINT_CODE('j', Print_Code_HashTable)
        YIELD_PRINT_CODE('v', Print_Code_GnuVerneedTable)
        YIELD_PRINT_CODE('m', Print_Code_GnuVersymTable)
        YIELD_PRINT_CODE('y', Print_Code_DynamicTable)
        YIELD_PRINT_CODE('i', Print_Code_Instrumentation)
        YIELD_PRINT_CODE('w', Print_Code_DwarfSection)
        YIELD_PRINT_CODE('o', Print_Code_Loops)
        YIELD_PRINT_CODE('a', Print_Code_All)
        YIELD_PRINT_CODE('x', Print_Code_SectionHeader | Print_Code_FileHeader | Print_Code_ProgramHeader)
        YIELD_PRINT_CODE('c', Print_Code_Disassemble | Print_Code_Instruction)
        else {
            printUsage("unknown print code given");
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

static char* ToolNames[Total_InstrumentationType] = {
    "",
    "INVALID",
    "BasicBlockCounter",
    "CacheSimulation"
};

int main(int argc,char* argv[]){

#define DEFINE_FLAG(__name) int __name ## _flag = 0
    DEFINE_FLAG(help); // int help_flag = 0;
    DEFINE_FLAG(allowstatic);
    DEFINE_FLAG(silent);
    DEFINE_FLAG(dry);
    DEFINE_FLAG(version);
    DEFINE_FLAG(lpi);
    DEFINE_FLAG(dtl);
    DEFINE_FLAG(doi);

#define DEFINE_ARG(__name) char* __name ## _arg = NULL
    DEFINE_ARG(typ); // char* typ_arg = NULL;
    DEFINE_ARG(tool);
    DEFINE_ARG(inp);
    DEFINE_ARG(tlib);
    DEFINE_ARG(trk);
    DEFINE_ARG(lnc);
    DEFINE_ARG(inf);
    DEFINE_ARG(app);
    DEFINE_ARG(lib);
    DEFINE_ARG(ext);
    DEFINE_ARG(fbl);
    DEFINE_ARG(dmp);
    DEFINE_ARG(phs);
    DEFINE_ARG(dfp);

#define FLAG_OPTION(__name, __char) {#__name, no_argument, &__name ## _flag, __char}
#define ARG_OPTION(__name, __char) {#__name, required_argument, 0, __char}
    static struct option pebil_options[] = {
        /* These options set a flag. */
        FLAG_OPTION(help, 'h'), FLAG_OPTION(allowstatic, 'w'), FLAG_OPTION(silent, 's'), FLAG_OPTION(dry, 'r'),
        FLAG_OPTION(version, 'V'), FLAG_OPTION(lpi, 'p'), FLAG_OPTION(dtl, 'd'), FLAG_OPTION(doi, 'i'),

        /* These options take an argument
           We distinguish them by their indices. */
        ARG_OPTION(typ, 'y'), ARG_OPTION(tool, 't'), ARG_OPTION(tlib, 'o'), ARG_OPTION(inp, 'p'), ARG_OPTION(trk, 'k'), 
        ARG_OPTION(lnc, 'n'), ARG_OPTION(inf, 'z'), ARG_OPTION(app, 'a'), ARG_OPTION(lib, 'l'), 
        ARG_OPTION(ext, 'x'), ARG_OPTION(fbl, 'b'), ARG_OPTION(dmp, 'm'), ARG_OPTION(phs, 'f'), ARG_OPTION(dfp, 'g'),
        {0,              0,                 0,              0},
    };

    int c;
    while (true){
        int option_index = 0;
        c = getopt_long(argc, argv, "", pebil_options, &option_index);
        if (c == -1){
            break;
        }

        /* flags are handled here */
        if (c == 0){
            if (pebil_options[option_index].flag == 0){
                abort();
            }
        } 

        /* unknown options here */
        else if (c == '?'){
            printUsage();
        }

        /* arguments handled here */
#define SET_ARGPTR(__name, __char) else if (c == __char) { __name ## _arg = optarg; }
        SET_ARGPTR(typ, 'y')
        SET_ARGPTR(tool, 't')
        SET_ARGPTR(tlib, 'o')
        SET_ARGPTR(inp, 'p')
        SET_ARGPTR(trk, 'k')
        SET_ARGPTR(lnc, 'n')
        SET_ARGPTR(inf, 'z')
        SET_ARGPTR(app, 'a')
        SET_ARGPTR(lib, 'l')
        SET_ARGPTR(ext, 'x')
        SET_ARGPTR(fbl, 'b')
        SET_ARGPTR(dmp, 'm')
        SET_ARGPTR(phs, 'f')
        SET_ARGPTR(dfp, 'g')

        /* dont think this should happen, but handle it anyway */
        else {
            printUsage("unexpected argument");
        }
    }

    // keep this around for a little while in case we need to debug new option-handling code
    /*
#define VERIFY_FLAG(__name) if (__name ## _flag) { PRINT_INFOR("--%s is set", #__name); } else { PRINT_INFOR("--%s is not set", #__name); }
    VERIFY_FLAG(help);
    VERIFY_FLAG(allowstatic);
    VERIFY_FLAG(silent);
    VERIFY_FLAG(dry);
    VERIFY_FLAG(version);
    VERIFY_FLAG(lpi);
    VERIFY_FLAG(dtl);
    VERIFY_FLAG(doi);

#define VERIFY_ARG(__name) if (__name ## _arg){ PRINT_INFOR("--%s is %s", #__name , __name ## _arg); } else { PRINT_INFOR("--%s is null", #__name); }
    VERIFY_ARG(typ);
    VERIFY_ARG(tool);
    VERIFY_ARG(inp);
    VERIFY_ARG(trk);
    VERIFY_ARG(lnc);
    VERIFY_ARG(inf);
    VERIFY_ARG(app);
    VERIFY_ARG(lib);
    VERIFY_ARG(ext);
    VERIFY_ARG(fbl);
    VERIFY_ARG(dmp);
    VERIFY_ARG(phs);
    VERIFY_ARG(dfp);
    */

    /* now we can do stuff with options/args */


    // --version: print version number and exit
    if (version_flag){
        fprintf(stdout, "pebil %s\n", PEBIL_VER);
        return 0;
    }

    // --help: print help message and exit
    if (help_flag){
        printUsage();
        __SHOULD_NOT_ARRIVE
    }

    // --silent: set all printing to go to /dev/null
    if (silent_flag){
        pebilOutp = fopen("/dev/null", "w");
        if (!pebilOutp){
            PRINT_WARN(10, "Cannot open file /dev/null for silent output");
            pebilOutp = stdout;
        }
    }

    // --inf: turn argument into a single int to pass around to ElfFile and subclasses
    uint32_t printCodes = 0;
    if (inf_arg){
        printCodes = processPrintCodes(inf_arg);
    }

    // --dtl: always turn it on
    if (!dtl_flag){
        dtl_flag = 1;
    }

    // --typ: figure out which tool to use
    uint32_t instType = unknown_inst_type;
    if (typ_arg){
        if (!strcmp(typ_arg, "ide")){
            instType = identical_inst_type;
            if (!ext_arg){
                ext_arg = "ideinst";
            }
        } else if (!strcmp(typ_arg, "jbb")){
            instType = frequency_inst_type;
        } else if (!strcmp(typ_arg, "sim")){
            instType = simulation_inst_type;
        } else if (!strcmp(typ_arg, "csc")){
            instType = simulation_inst_type;
        } else {
            printUsage("invalid argument to --typ");
            __SHOULD_NOT_ARRIVE;
        }
        PRINT_INFOR("Converting `--typ %s' to `--tool %s'", typ_arg, ToolNames[instType]);
        tool_arg = ToolNames[instType];
    }

    // --tool: make sure --tool or --typ was passed
    if (!tool_arg){
        printUsage("one of the following options is required: --typ, --tool");
        __SHOULD_NOT_ARRIVE;
    }
        
    // --app: use the argument as the application name
    Vector<char*> applications = Vector<char*>();
    if (app_arg){
        applications.append(app_arg);
    }

    // anything extra (not an arg or flag) on the command line is considered an application name
    if (optind < argc){
        while (optind < argc){
            applications.append(argv[optind++]);
        }
    }
             
    if (!applications.size()){
        printUsage("application missing: use --app or list applications after options");
    }

    // --fbl: set up function black list either by this option or by $PEBIL_ROOT/DEFAULT_FUNCTION_BLACKLIST
    char* functionBlackList = NULL;
    if (!fbl_arg){
        char* pebilRoot = getenv("PEBIL_ROOT");
        if (!pebilRoot){
            printUsage("set the PEBIL_ROOT env variable or pass --fbl on the command line");
            __SHOULD_NOT_ARRIVE;
        }
        functionBlackList = new char[__MAX_STRING_SIZE];
        sprintf(functionBlackList, "%s/%s", pebilRoot, DEFAULT_FUNC_BLACKLIST);
    } else {
        functionBlackList = fbl_arg;
    }
    PRINT_INFOR("The function blacklist is taken from %s", functionBlackList);

    // --dmp: convert arg to dump code
    uint32_t dumpCode = dumpcode_off;
    if (dmp_arg){
        dumpCode = processDumpCode(dmp_arg);
    }
    if (dumpCode != dumpcode_off){
        printUsage("`off' is the only allowed option to --dmp, others are not implemented");
        __SHOULD_NOT_ARRIVE;
    }

    // --dry: stop doing stuff and exit!
    if (dry_flag){
        PRINT_INFOR("--dry option was used, exiting before file processing");
        return 0;
    }

    TIMER(double tt = timer(), t1 = tt, t2, tapp);

    void* libHandle = NULL;
    void* maker = NULL;
    char toolLibName[__MAX_STRING_SIZE];
    char toolConstructor[__MAX_STRING_SIZE];
    
    if (tlib_arg){
        sprintf(toolLibName, "lib%s.so\0", tlib_arg);
    } else {
        sprintf(toolLibName, "lib%sTool.so\0", tool_arg);
    }
    sprintf(toolConstructor, "%sMaker", tool_arg);
    PRINT_INFOR("Using library %s and generator %s for dynamic class loading", toolLibName, toolConstructor);

    // if we are using a real tool (not --typ ide), set up dynamic class loading
    if (instType != identical_inst_type){
        libHandle = dlopen(toolLibName, RTLD_NOW);
        char* dlErr = dlerror();
        if (dlErr){
            PRINT_ERROR("Error from dlopen: %s", dlErr);
            return 1;
        }
        ASSERT(dlErr == NULL);
        if(libHandle == NULL){
            PRINT_ERROR("cannot open tool library %s, it needs to be in your LD_LIBRARY_PATH", toolLibName);
            return 1;
        }
        maker = dlsym(libHandle, toolConstructor);
        dlErr = dlerror();
        if (dlErr){
            PRINT_ERROR("Error from dlsym: %s", dlErr);
            return 1;
        }
        ASSERT(dlErr == NULL);
        if (maker == NULL){
            PRINT_ERROR("cannot find function %s in %s, see tools/Minimal.* for an intro to tool creation", toolConstructor, toolLibName);
            return 1;
        }
    }

    // go over every given application
    for (uint32_t i = 0; i < applications.size(); i++){

        uint32_t stepNumber = 0;
        char* execName = applications[i];

        // remove the path from the filename
        char* appName = new char[__MAX_STRING_SIZE];
        uint32_t startApp = 0;
        for (uint32_t i = 0; i < strlen(execName); i++){
            if (execName[i] == '/'){
                startApp = i + 1;
            }
        }
        sprintf(appName, "%s\0", execName + startApp);
        
        ElfFile elfFile(execName, appName);

        TIMER(t1 = timer(); tapp = t1);
        elfFile.parse();
        elfFile.initSectionFilePointers();
        TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Parse  : %.2f seconds",++stepNumber,t2-t1);t1=t2);
        
        elfFile.generateCFGs();
        TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Disasm/cfg : %.2f seconds",++stepNumber,t2-t1);t1=t2);    
        
        elfFile.findLoops();
        TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Loop    : %.2f seconds",++stepNumber,t2-t1);t1=t2);
        
        PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
            
        if (inf_arg){
            elfFile.print(printCodes);
            TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
        }
        
        elfFile.verify();
        TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Verify  : %.2f seconds",++stepNumber,t2-t1);t1=t2);

        if (instType == identical_inst_type){
            elfFile.dump(ext_arg);
            PRINT_INFOR("Dumping identical binary from stored executable information");
            printSuccess();
        } else {
            ASSERT(libHandle && maker);
            InstrumentationTool* instTool = reinterpret_cast<InstrumentationTool*(*)(ElfFile*)>(maker)(&elfFile);
            ASSERT(!strcmp(tool_arg, instTool->briefName()) && "name yielded by briefName does not match tool name");

            instTool->initToolArgs(lpi_flag == 0 ? false : true,
                                   dtl_flag == 0 ? false : true,
                                   doi_flag == 0 ? false : true,
                                   0, inp_arg, dfp_arg, trk_arg);

            char ext[__MAX_STRING_SIZE];
            if (ext_arg){
                sprintf(ext, "%s\0", ext_arg);
            } else {
                sprintf(ext, "%s\0", instTool->defaultExtension());
            }

            // --phs: convert arg/emmptiness into an integer value
            uint32_t phaseNo = 0;
            if (phs_arg){
                char* endptr = NULL;
                phaseNo = strtol(phs_arg, &endptr, 10);
                if ((endptr == phs_arg) || !phaseNo){
                    printUsage("argument to --phs must be an int > 0");
                }
                // prepend phase string to extension
                char tmp[__MAX_STRING_SIZE];
                sprintf(tmp, "phase.%d.%s\0", phaseNo, ext);
                sprintf(ext, "%s\0", tmp);
            }

            if (lnc_arg){
                instTool->setLibraryList(lnc_arg);
            }
            
            ASSERT(functionBlackList);
            instTool->setInputFunctions(functionBlackList);
            
            if (allowstatic_flag){
                instTool->setAllowStatic();
            }
            
            instTool->init(ext);
            instTool->initToolArgs(lpi_flag == 0 ? false : true,
                                   dtl_flag == 0 ? false : true,
                                   doi_flag == 0 ? false : true,
                                   phaseNo, inp_arg, dfp_arg, trk_arg);
            if (!instTool->verifyArgs()){
                printUsage("argument missing/incorrect");
            }
            
            ASSERT(instTool);
            instTool->phasedInstrumentation();
            PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
            instTool->print(Print_Code_Instrumentation);
            TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Instr   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
            
            elfFile.printDynamicLibraries();
            if (inf_arg){
                instTool->print(printCodes);
                TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
            }
            
            instTool->dump();
            TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Dump    : %.2f seconds",++stepNumber,t2-t1);t1=t2);
            if (inf_arg){
                instTool->print(printCodes);
                TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Print   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
            }
            
            printSuccess();
            TIMER(t2 = timer();PRINT_INFOR("___timer: Application %s Total : %.2f seconds", execName, t2-tapp););
            
            delete instTool;
        }

        delete[] appName;
    }

    TIMER(t2 = timer();PRINT_INFOR("___timer: Total Execution Time          : %.2f seconds",t2-tt););
    printDone();
    if (warnCount){
        PRINT_WARN(10000000, "!!!!!!!!!!!!!!! Completed with %lld warnings", warnCount);
    }

    if (!fbl_arg){
        delete[] functionBlackList;
    }

    if (silent_flag){
        fclose(pebilOutp);
    }

    if (instType == identical_inst_type){
        return 0;
    }

    dlclose(libHandle);
    return 0;
}
