#include <Base.h>
#include <BasicBlockCounter.h>
#include <CacheSimulation.h>
#include <CallReplace.h>
#include <ElfFile.h>
#include <FunctionCounter.h>
#include <FunctionTimer.h>
#include <Vector.h>

#define DEFAULT_FUNC_BLACKLIST "scripts/inputlist/none.func"

void printBriefOptions(bool detail){
    fprintf(stderr,"\n");
    fprintf(stderr,"Brief Descriptions for Options:\n");
    fprintf(stderr,"===============================\n");
    fprintf(stderr,"\t--typ : required for all.\n");
    fprintf(stderr,"\t--app : required for all.\n");
    fprintf(stderr,"\t--ver : optional for all. prints informative details about parts of the application binary.\n");
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
    fprintf(stderr,"\t--lib : optional for all. shared library directory.\n");
    fprintf(stderr,"\t        default is $PEBIL_LIB\n");
    fprintf(stderr,"\t--fbl : optional for all. input file which lists blacklisted functions\n");
    fprintf(stderr,"\t        default is %s\n", DEFAULT_FUNC_BLACKLIST);
    fprintf(stderr,"\t--ext : optional for all. default is (typ)inst, such as\n");
    fprintf(stderr,"\t        jbbinst for type jbb.\n");
    fprintf(stderr,"\t--dtl : optional for all. detailed .static file with lineno\n");
    fprintf(stderr,"\t        and filenames. default is no details.\n");
    fprintf(stderr,"\t--inp : required for sim/csc.\n");
    fprintf(stderr,"\t--lpi : optional for sim/csc. loop level block inclusion for\n");
    fprintf(stderr,"\t        cache simulation. default is no.\n");
    fprintf(stderr,"\t--phs : optional for sim/csc. phase number. defaults to no phase,\n"); 
    fprintf(stderr,"\t        otherwise, .phase.N. is included in output file names\n");
    fprintf(stderr,"\t--trk : required for crp. input file which lists the functions to track\n");
    fprintf(stderr,"\t--lnc : required for crp. list of shared libraries to use, comma seperated\n");
    fprintf(stderr,"\n");
}

void printUsage(bool shouldExt=true, bool optDetail=false) {
    fprintf(stderr,"\n");
    fprintf(stderr,"usage : pebil\n");
    fprintf(stderr,"\t--typ (ide|fnc|jbb|sim|ftm|crp)\n");
    fprintf(stderr,"\t--app <executable_path>\n");
    fprintf(stderr,"\t--inp <block_unique_ids>    <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--ver [a-z]*]\n");
    fprintf(stderr,"\t[--lib <shared_lib_dir>]\n");
    fprintf(stderr,"\t\tdefault is $PEBIL_LIB\n");
    fprintf(stderr,"\t[--ext <output_suffix>]\n");
    fprintf(stderr,"\t[--dtl]\n");
    fprintf(stderr,"\t[--fbl file]\n");
    fprintf(stderr,"\t[--lpi]                     <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--phs <phase_no>]          <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--trk file]                <-- required for crp\n");
    fprintf(stderr,"\t[--lnc <lib_list>]          <-- required for crp\n");
    fprintf(stderr,"\t[--help]\n");
    fprintf(stderr,"\n");
    if(shouldExt){
        printBriefOptions(optDetail);
        exit(-1);
    }
}

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
    simucntr_inst_type,
    function_counter_type,
    func_timer_type,
    call_wrapper_type,
    Total_InstrumentationType
} InstrumentationType;


int main(int argc,char* argv[]){
    int overflow = 0xbeefbeef;

    char*    inptName   = NULL;
    char*    extension  = "";
    char*    libPath    = NULL;
    int32_t  phaseNo    = 0;
    uint32_t instType   = unknown_inst_type;
    uint32_t argApp     = 0;
    uint32_t argTyp     = 0;
    uint32_t argExt     = 0;
    uint32_t argPhs     = 0;
    uint32_t argInp     = 0;
    bool     loopIncl   = false;
    bool     extdPrnt   = false;
    bool     verbose    = false;
    bool     dryRun     = false;
    uint32_t printCodes = 0x00000000;
    char* rawPrintCodes = NULL;
    char* inputFuncList = NULL;
    char* inputTrackList = NULL;
    char* libList       = NULL;
    bool deleteInpList  = false;
    char*    execName   = NULL;

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
                instType = simucntr_inst_type;
                extension = "cscinst";
            } else if (!strcmp(argv[i],"ftm")){
                instType = func_timer_type;
                extension = "ftminst";
            } else if (!strcmp(argv[i],"crp")){
                instType = call_wrapper_type;
                extension = "crpinst";
            }
        } else if (!strcmp(argv[i],"--help")){
            printUsage(true, true);
        }
    }

    if (!execName){
        fprintf(stderr,"\nError : No executable is specified\n\n");
        printUsage();
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
            if ((instType != simulation_inst_type) && (instType != simucntr_inst_type)){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i]);
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
            if ((instType != simulation_inst_type) && (instType != simucntr_inst_type)){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i]);
                printUsage();
            }

            inptName = argv[++i];
        } else if (!strcmp(argv[i],"--lpi")){
            loopIncl = true;
            if ((instType != simulation_inst_type) && (instType != simucntr_inst_type)){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i++]);
                printUsage();
            }
        } else if (!strcmp(argv[i],"--ver")){
            verbose = true;
            rawPrintCodes = argv[++i];
        } else if (!strcmp(argv[i],"--dtl")){
            extdPrnt = true;
        } else if (!strcmp(argv[i],"--lib")){
            libPath = argv[++i];
        } else if (!strcmp(argv[i],"--dry")){
            dryRun = true;
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
        } else {
            fprintf(stderr,"\nError : Unknown switch at %s\n\n",argv[i]);
            printUsage();
        }
    }

    if (((instType == simulation_inst_type) || 
         (instType == simucntr_inst_type)) && !inptName){
        fprintf(stderr,"\nError : Input is required for cache simulation instrumentation\n\n");
        printUsage();
    }

    ASSERT((instType == simulation_inst_type) || (instType == simucntr_inst_type) || (phaseNo == 0));

    if (verbose){
        if (!rawPrintCodes){
            fprintf(stderr,"\tError: verbose option used without argument");
            printUsage();
        }
        printCodes = processPrintCodes(rawPrintCodes);
    }

    if(!libPath){
        libPath = getenv("PEBIL_LIB");
        if (!libPath){
            PRINT_ERROR("Use the -lib option or define the PEBIL_LIB variable"); 
        }
    }
    PRINT_INFOR("The instrumentation libraries will be used from %s",libPath);

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

    if (dryRun){
        PRINT_INFOR("--dry option was used, exiting...");
        exit(0);
    }

    uint32_t stepNumber = 0;
    TIMER(double t1 = timer(), t2);

    PRINT_INFOR("******** Instrumentation Beginning ********");
    ElfFile elfFile(execName);

    elfFile.parse();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Parse   : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    elfFile.initSectionFilePointers();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Disasm  : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    elfFile.generateCFGs();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d GenCFG  : %.2f seconds",++stepNumber,t2-t1);t1=t2);    

    if (extdPrnt){
        elfFile.findLoops();
        TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Loop    : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    }

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
        return 0;
    } else if (instType == function_counter_type){
        elfInst = new FunctionCounter(&elfFile);
    } else if (instType == frequency_inst_type){
        elfInst = new BasicBlockCounter(&elfFile);
    } else if (instType == simulation_inst_type){
        elfInst = new CacheSimulation(&elfFile, inptName);
    } else if (instType == func_timer_type){
        elfInst = new FunctionTimer(&elfFile);
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
        elfInst = new CallReplace(&elfFile, inputTrackList, libList);
    }
    else {
        PRINT_ERROR("Error : invalid instrumentation type");
    }

    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    ASSERT(elfInst);
    ASSERT(libPath);
    ASSERT(extension);

    elfInst->setPathToInstLib(libPath);
    elfInst->setInstExtension(extension);
    if (inputFuncList){
        elfInst->setInputFunctions(inputFuncList);
    }
    
    elfInst->phasedInstrumentation();
    PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
    elfInst->print(Print_Code_Instrumentation);
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Instr   : %.2f seconds",++stepNumber,t2-t1);t1=t2);
    
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

    // arrrrrg. i can't figure out why just deleting elfInst doesn't
    // call the CallReplace destructor in this case without the cast
    if (instType == call_wrapper_type){
        delete (CallReplace*)elfInst;
        //        delete (CallReplace*)elfInst;
    } else if (instType == simulation_inst_type){
        delete (CacheSimulation*)elfInst;
    } else {
        delete elfInst;
    }

    if (deleteInpList){
        delete[] inputFuncList;
    }
    PRINT_INFOR("******** Instrumentation Successfull ********");

    TIMER(t = timer()-t;PRINT_INFOR("___timer: Total Execution Time          : %.2f seconds",t););

    PRINT_INFOR("\n");
    PRINT_INFOR("******** DONE ******** SUCCESS ***** SUCCESS ***** SUCCESS ********\n");
    return 0;
}
