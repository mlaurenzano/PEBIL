#include <Base.h>
#include <ElfFile.h>
#include <ElfFileInst.h>
#include <FunctionTimer.h>
#include <Vector.h>

int main(int argc,char* argv[]){
    char*    inptName   = NULL;
    char*    extension  = "ftminst";
    char*    libPath    = NULL;
    int32_t  phaseNo    = 0;
    char* inputFuncList = NULL;
    char* inputFileList = NULL;
    bool deleteInpList  = false;
    char*    execName   = NULL;

    uint32_t stepNumber = 0;
    TIMER(double t1 = timer(), t2);

    if (argc != 2){
        PRINT_ERROR("usage: %s <app>", argv[0]);
    }
    TIMER(double t = timer());
    execName = argv[1];

    if(!libPath){
        libPath = getenv("PEBIL_LIB");
        if (!libPath){
            PRINT_ERROR("Use the --lib option or define the PEBIL_LIB variable"); 
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
        sprintf(inputFuncList, "%s/%s", pebilRoot, "scripts/exclusion/system.func");
    }
    PRINT_INFOR("The function blacklist is taken from %s", inputFuncList);

    PRINT_INFOR("******** Instrumentation Beginning ********");
    ElfFile elfFile(execName);

    elfFile.parse();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Parse   : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    elfFile.initSectionFilePointers();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Disasm  : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    elfFile.verify();
    TIMER(t2 = timer();PRINT_INFOR("___timer: Step %d Verify  : %.2f seconds",++stepNumber,t2-t1);t1=t2);

    ElfFileInst* elfInst = NULL;

    elfInst = new FunctionTimer(&elfFile, inputFuncList, inputFileList);

    if (elfInst){
        elfInst->setPathToInstLib(libPath);
        elfInst->phasedInstrumentation();
        PRINT_MEMTRACK_STATS(__LINE__, __FILE__, __FUNCTION__);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Instr   : %.2f seconds",++stepNumber,t2-t1);t1=t2);

        elfInst->dump(extension);
        TIMER(t2 = timer();PRINT_INFOR("___timer: Instrumentation Step %d Dump    : %.2f seconds",++stepNumber,t2-t1);t1=t2);

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
