#include <Base.h>
#include <ElfFile.h>
#include <FunctionCounter.h>

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

void printUsage(char* argv[],bool shouldExt=false) {
    fprintf(stderr,"\n");
    fprintf(stderr,"usage : %s\n",argv[0]);
        //fprintf(stderr,"\t--typ (ide|dat|bbt|cnt|jbb|sim|csc)\n");
        fprintf(stderr,"\t--typ (ide|fnc|dis)\n");
	fprintf(stderr,"\t--app <executable_path>\n");
	fprintf(stderr,"\t--inp <block_unique_ids>    <-- valid for sim/csc\n");
	fprintf(stderr,"\t[--lib <shared_lib_topdir>]\n");
	fprintf(stderr,"\t[--ext <output_suffix>]\n");
    fprintf(stderr,"\t[--dtl]\n");
	fprintf(stderr,"\t[--lpi]                     <-- valid for sim/csc\n");
	fprintf(stderr,"\t[--phs <phase_no>]          <-- valid for sim/csc\n");
	fprintf(stderr,"\t[--help]\n");
    fprintf(stderr,"\n");
	if(shouldExt){
		printBriefOptions();
	}
    exit(-1);
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
    disassembler_type,
    Total_InstrumentationType
} InstrumentationType;


int main(int argc,char* argv[]){

    char*    execName  = NULL;
    char*    inptName  = NULL;
    char*    extension = "";
    char*    libPath   = NULL;
    int32_t  phaseNo   = 0;
    uint32_t instType  = unknown_inst_type;
    uint32_t argApp    = 0;
    uint32_t argTyp    = 0;
    uint32_t argExt    = 0;
    uint32_t argPhs    = 0;
    uint32_t argInp    = 0;
    bool     loopIncl  = false;
    bool     extdPrnt  = false;

    TIMER(double t = timer());
    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i],"--app")){
            if(argApp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage(argv);
            }
            execName = argv[++i];
        } else if(!strcmp(argv[i],"--typ")){
            if (argTyp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage(argv);
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
            } else if (!strcmp(argv[i],"dis")){
                instType = disassembler_type;
                extension = "datinst";
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
        } else if(!strcmp(argv[i],"--help")){
			printUsage(argv,true);
		}
    }

    if (!execName){
        fprintf(stderr,"\nError : No executable is specified\n\n");
        printUsage(argv);
    }

    if ((instType <= unknown_inst_type) || 
       (instType >= Total_InstrumentationType)){
        fprintf(stderr,"\nError : Unknown instrumentation type\n");
        printUsage(argv);
    }

    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i],"--app") || !strcmp(argv[i],"--typ")){
            ++i;
        } else if (!strcmp(argv[i],"--ext")){
            if (argExt++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage(argv);
            }
            extension = argv[++i];
        } else if(!strcmp(argv[i],"--phs")){
            if (argPhs++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage(argv);
            }
            if ((instType != simulation_inst_type) && (instType != simucntr_inst_type)){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i]);
                printUsage(argv);
            }

            ++i;
            char* endptr = NULL;
            phaseNo = strtol(argv[i],&endptr,10);
            if ((endptr == argv[i]) || !phaseNo){
                fprintf(stderr,"\nError : Given phase number is not correct, requires > 0\n\n");
                printUsage(argv);
            }

        } else if (!strcmp(argv[i],"--inp")){
            if (argInp++){
                fprintf(stderr,"\nError : Duplicate %s option\n",argv[i]);
                printUsage(argv);
            }
            if ((instType != simulation_inst_type) && (instType != simucntr_inst_type)){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i]);
                printUsage(argv);
            }

            inptName = argv[++i];
        } else if (!strcmp(argv[i],"--lpi")){
            loopIncl = true;
            if ((instType != simulation_inst_type) && (instType != simucntr_inst_type)){
                fprintf(stderr,"\nError : Option %s is not valid other than simulation\n",argv[i++]);
                printUsage(argv);
            }
        } else if (!strcmp(argv[i],"--dtl")){
            extdPrnt = true;
        } else if (!strcmp(argv[i],"--lib")){
            libPath = argv[++i];
        } else {
            fprintf(stderr,"\nError : Unknown switch at %s\n\n",argv[i]);
            printUsage(argv);
        }
    }

    if (((instType == simulation_inst_type) || 
         (instType == simucntr_inst_type)) && !inptName){
        fprintf(stderr,"\nError : Input is required for cache simulation instrumentation\n\n");
        printUsage(argv);
    }

    ASSERT((instType == simulation_inst_type) || (instType == simucntr_inst_type) || (phaseNo == 0));

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
    elfFile.print();
    elfFile.verify();

    TIMER(double t2 = timer();PRINT_INFOR("___timer: Instrumentation Step I parse  : %.2f",t2-t1);t1=t2);
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step II Line  : %.2f",t2-t1);t1=t2);
    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step III Loop : %.2f",t2-t1);t1=t2);

    if (instType == identical_inst_type){
        elfFile.dump(extension);
    } else if (instType == function_counter_type){
        FunctionCounter* functionCounter = new FunctionCounter(&elfFile);

        functionCounter->instrument();

        functionCounter->print();
        functionCounter->dump(extension);

        delete functionCounter;

    } else if (instType == disassembler_type){
        elfFile.printDisassembledCode();
    } else {
        PRINT_ERROR("Error : invalid instrumentation type");
    }

    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step IV Instr : %.2f",t2-t1);t1=t2);

    TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step V Dump   : %.2f",t2-t1);t1=t2);

    PRINT_INFOR("******** Instrumentation Successfull ********");

    TIMER(t = timer()-t;PRINT_INFOR("___timer: Total Execution Time          : %.2f",t););

    PRINT_INFOR("\n");
    PRINT_INFOR("******** DONE ******** SUCCESS ***** SUCCESS ***** SUCCESS ********\n");
    return 0;
}
