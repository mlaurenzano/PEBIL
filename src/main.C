#include <Base.h>
#include <ElfFile.h>
#include <FunctionCounter.h>
#include <BasicBlockCounter.h>

void alignTest(){
    uint32_t align = 0x00000001;
    while (align){
        fprintf(stdout, "testing alignment for %x\n", align);
        for (uint32_t i = 0; i < 2*align; i++){
            if (i % 0x100000 == 0){
                fprintf(stdout, "testing alignment for %d\n", i);                
            }
            nextAlignAddress(i,align);
        }
        align = align << 1;
    }
    exit(-1);
}


void printBriefOptions(){
    fprintf(stderr,"\n");
    fprintf(stderr,"Brief Descriptions for Options:\n");
    fprintf(stderr,"===============================\n");
    fprintf(stderr,"\t--typ : required for all.\n");
    fprintf(stderr,"\t--app : required for all.\n");
    fprintf(stderr,"\t--ver : optional for all. prints informative messages.\n");
    fprintf(stderr,"\t      : a : all parts of the application\n");
    fprintf(stderr,"\t      : e : all elf parts (everything but instructions)\n");
    fprintf(stderr,"\t      : p : program headers\n");
    fprintf(stderr,"\t      : h : section headers\n");
    fprintf(stderr,"\t      : x : all headers\n");
    fprintf(stderr,"\t      : d : disassembly\n");
    fprintf(stderr,"\t      : c : full instruction printing (implies disassembly)\n");
    fprintf(stderr,"\t      : s : string tables\n");
    fprintf(stderr,"\t      : t : symbol tables\n");
    fprintf(stderr,"\t      : r : relocation tables\n");
    fprintf(stderr,"\t      : n : note sections\n");
    fprintf(stderr,"\t      : g : global offset table\n");
    fprintf(stderr,"\t      : h : hash table\n");
    fprintf(stderr,"\t      : v : gnu version needs table\n");
    fprintf(stderr,"\t      : m : gnu version symbol table\n");
    fprintf(stderr,"\t      : y : dynamic table\n");
    fprintf(stderr,"\t      : i : instrumentation reservations\n");
    fprintf(stderr,"\t--lib : optional for all. shared library top directory.\n");
    fprintf(stderr,"\t        default is $X86INST_LIB_HOME\n");
    fprintf(stderr,"\t--ext : optional for all. default is (typ)inst, such as\n");
    fprintf(stderr,"\t        jbbinst for type jbb.\n");
    fprintf(stderr,"\t--dtl : optional for all. detailed .static file with lineno\n");
    fprintf(stderr,"\t        and filenames. default is no details.\n");
    fprintf(stderr,"\t--inp : required for sim/csc.\n");
    fprintf(stderr,"\t--lpi : optional for sim/csc. loop level block inclusion for\n");
    fprintf(stderr,"\t        cache simulation. default is no.\n");
    fprintf(stderr,"\t--phs : optional for sim/csc. phase number. defaults to no phase,\n"); 
    fprintf(stderr,"\t        otherwise, .phase.N. is included in output file names\n");
    fprintf(stderr,"\n");
}

void printUsage(bool shouldExt=true) {
    fprintf(stderr,"\n");
    fprintf(stderr,"usage : x86inst\n");
    fprintf(stderr,"\t--typ (ide|fnc|jbb)\n");
    fprintf(stderr,"\t--app <executable_path>\n");
    fprintf(stderr,"\t--inp <block_unique_ids>    <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--ver [afhpxdtrnghvmy]]");
    fprintf(stderr,"\t[--lib <shared_lib_topdir>]\n");
    fprintf(stderr,"\t[--ext <output_suffix>]\n");
    fprintf(stderr,"\t[--dtl]\n");
    fprintf(stderr,"\t[--lpi]                     <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--phs <phase_no>]          <-- valid for sim/csc\n");
    fprintf(stderr,"\t[--help]\n");
    fprintf(stderr,"\n");
    if(shouldExt){
        printBriefOptions();
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
        } else if (pc == 'h'){
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
        } else {
            printUsage(true);
        }
    }
    PRINT_INFOR("Found print code 0x%08x", printCodes);
    return printCodes;
}


typedef enum {
    unknown_inst_type = 0,
    identical_inst_type,
    frequency_inst_type,
    simulation_inst_type,
    simucntr_inst_type,
    bbtrace_inst_type,
    countblocks_inst_type,
    function_counter_type,
    data_extender_type,
    Total_InstrumentationType
} InstrumentationType;


int main(int argc,char* argv[]){

    char*    execName   = NULL;
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
    uint32_t printCodes = 0x00000000;
    char* rawPrintCodes = NULL;

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
            } else if (!strcmp(argv[i],"dat")){
                instType = data_extender_type;
                extension = "datinst";
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
            } else if (!strcmp(argv[i],"bbt")){
                instType = bbtrace_inst_type;
                extension = "bbtinst";
            } else if (!strcmp(argv[i],"cnt")){
                instType = countblocks_inst_type;
                extension = "cntinst";
            }
        } else if (!strcmp(argv[i],"--help")){
            printUsage(true);
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
            printUsage(true);
        }
        printCodes = processPrintCodes(rawPrintCodes);
    }

    if(!libPath){
        libPath = getenv("X86INST_LIB_HOME");
        if (!libPath){
            PRINT_ERROR("Error : use --lib option or define set the X86INST_LIB_HOME variable"); 
        }
    }
    PRINT_INFOR("The instrumentation libraries will be used from %s",libPath);

    TIMER(double t1 = timer());

    PRINT_INFOR("******** Instrumentation Beginning ********");
    ElfFile elfFile(execName);

    elfFile.parse();
    TIMER(double t2 = timer();PRINT_INFOR("___timer: Instrumentation Step I parse  : %.2f",t2-t1);t1=t2);

    if (verbose){
        elfFile.print(printCodes);
    }

    elfFile.verify();

    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step II Line  : %.2f",t2-t1);t1=t2);
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step III Loop : %.2f",t2-t1);t1=t2);

    ElfFileInst* elfInst = NULL;

    if (instType == identical_inst_type){
        elfFile.dump(extension);
    } else if (instType == function_counter_type){
        elfInst = new FunctionCounter(&elfFile);
    } else if (instType == frequency_inst_type){
        elfInst = new BasicBlockCounter(&elfFile);
    } else {
        PRINT_ERROR("Error : invalid instrumentation type");
    }

    if (elfInst){
        elfInst->instrument();
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step IV Instr : %.2f",t2-t1);t1=t2);
        elfInst->dump(extension);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step V Dump   : %.2f",t2-t1);t1=t2);
        if (verbose){
            elfInst->print(printCodes);
        }
        delete elfInst;
    }

    PRINT_INFOR("******** Instrumentation Successfull ********");

    TIMER(t = timer()-t;PRINT_INFOR("___timer: Total Execution Time          : %.2f",t););

    PRINT_INFOR("\n");
    PRINT_INFOR("******** DONE ******** SUCCESS ***** SUCCESS ***** SUCCESS ********\n");
    return 0;
}
