#include <Base.h>
#include <Instruction.h>
#include <Vector.h>

#define INPUT_MAX_BYTES 0x40000

Vector<Instruction*> instructions = Vector<Instruction*>();

void printBriefOptions(){
    fprintf(stderr,"\n");
    fprintf(stderr,"Brief Descriptions for Options:\n");
    fprintf(stderr,"===============================\n");
    fprintf(stderr,"\t--verbose : optional. prints more details.\n");
    fprintf(stderr,"\t--bytes   : optional. the input bytes to be disassembled are given on the command line.\n");
    fprintf(stderr,"\t--mode    : required. 32/64 bit mode.\n");
    fprintf(stderr,"\t--type    : required. gives the format of the input stream.\n");
    fprintf(stderr,"\t            default is raw.");
    fprintf(stderr,"\t--addr    : optional. gives the start address of the instruction stream, which will help when computing targets.\n");    
    fprintf(stderr,"\n");
}

void printUsage(bool shouldExt) {
    fprintf(stderr,"\n");
    fprintf(stderr,"usage : quickdump\n");
    fprintf(stderr,"\t[--verbose]\n");
    fprintf(stderr,"\t[--bytes [0-9,a-f]*]\n");
    fprintf(stderr,"\t[--mode [32|64]]\n");
    fprintf(stderr,"\t[--type [hex|raw]]\n");
    fprintf(stderr,"\t[--addr 0x[0-9,a-f]*\n");
    fprintf(stderr,"\t[--help]\n");
    fprintf(stderr,"\n");
    if(shouldExt){
        printBriefOptions();
        exit(-1);
    }
}

char* padCharArray(uint32_t* numberOfBytes, char* buff, uint32_t len, char pad){
    char* paddedbuff = new char[*numberOfBytes + len];
    memcpy(paddedbuff, buff, *numberOfBytes);

    for (uint32_t i = *numberOfBytes; i < *numberOfBytes + len; i++){
        paddedbuff[i] = pad;
    }
    delete[] buff;

    return paddedbuff;
}

/*
void printBuffer(uint32_t numberOfBytes, char* inputBytes, uint64_t addr, bool extdPrnt){
    uint32_t currByte = 0;
    Base::disassembler->setPrintFunction((fprintf_ftype)noprint_fprintf, stdout);

    while (currByte < numberOfBytes){
        instructions.append(new Instruction(NULL, addr + currByte, inputBytes + currByte, ByteSource_Application_FreeText, instructions.size(), false));
        currByte += instructions.back()->getSizeInBytes();
    }
    if (currByte != numberOfBytes){
        PRINT_WARN(6, "Tail of input produced an incorrect disassembly: used %d bytes of padding", currByte - numberOfBytes);
        instructions.back()->print();
    }

    Base::disassembler->setPrintFunction((fprintf_ftype)fprintf, stdout);
    for (uint32_t i = 0; i < instructions.size(); i++){
        instructions[i]->binutilsPrint(stdout);
        if (extdPrnt){
            instructions.back()->print();
        }
    }

}
*/
uint8_t* convertAscii(uint32_t* numberOfBytes, uint8_t* buff){

    // remove any whitespace
    Vector<uint8_t> justHex = Vector<uint8_t>();
    for (uint32_t i = 0; i < *numberOfBytes; i++){
        if (!isspace(buff[i])){
            justHex.append(buff[i]);
        }
    }
    *numberOfBytes = justHex.size();
    delete[] buff;
    buff = new uint8_t[*numberOfBytes];
    memcpy(buff, &justHex, *numberOfBytes);

    // perform conversion from ascii hex uint8_ts to raw bytes
    ASSERT(*numberOfBytes % 2 == 0);
    *numberOfBytes = *numberOfBytes / 2;
    uint8_t* hexbuff = new uint8_t[*numberOfBytes];
    for (uint32_t i = 0; i < *numberOfBytes; i++){
        hexbuff[i] = mapCharsToByte(buff[2*i], buff[2*i+1]);
        PRINT_DEBUG("byte mapping %hhx = %c %c", inputBytes[i], buff[2*i], buff[2*i+1]);
    }
    delete[] buff;
    return hexbuff;
}

int main(int argc,char* argv[]){

    bool  extdPrnt   = false;    
    int64_t mode     = -1;
    char* rawInput   = NULL;
    bool cmdlnInput  = false;
    uint64_t addr    = 0;
    bool hexInput    = false;

    TIMER(double t = timer());
    for (int32_t i = 1; i < argc; i++){
        if (!strcmp(argv[i], "--verbose")){
            extdPrnt = true;
        } else if (!strcmp(argv[i], "--bytes")){
            rawInput = argv[++i];
            cmdlnInput = true;
        } else if (!strcmp(argv[i], "--mode")){ 
            char* endptr = NULL;
            mode = strtol(argv[++i], &endptr, 10);
            if ((endptr == argv[i]) || (mode != 32 && mode != 64)){
                fprintf(stderr,"\nError : Given mode %d is not correct, must be 32 or 64\n", mode);
                printUsage(true);
            }
        } else if (!strcmp(argv[i], "--type")){
            char* inputType = argv[++i];
            if (!strcmp(inputType, "hex")){
                hexInput = true;
            } else if (!strcmp(inputType, "raw")){
                hexInput = false;
            } else {
                fprintf(stderr, "\nError: Given an invalid argument to --type: %s\n", inputType);
                printUsage(true);
            }
        } else if (!strcmp(argv[i], "--addr")){
            char* endptr = NULL;
            addr = strtol(argv[++i], &endptr, 16);
            if (endptr == argv[i]){
                fprintf(stderr,"\nError : Given addr %llx is not correct\n", addr);
                printUsage(true);
            }
        } else if (!strcmp(argv[i],"--help")){
            printUsage(true);
        } else {
            printUsage(true);
        }
    }

    if (mode < 0){
        fprintf(stderr,"\nError : Mode not specified\n\n");
        printUsage(true);        
    }
    if (mode == 64){
    } else {
        ASSERT(mode == 32);
    }

    if (hexInput){
        PRINT_DEBUG("Using hex mode");
    } else {
        PRINT_DEBUG("Using raw mode");
    }

    
    uint8_t* bytes = new uint8_t[INPUT_MAX_BYTES];
    bzero(bytes, INPUT_MAX_BYTES);

    if (cmdlnInput){
        ASSERT(rawInput);
        memcpy(bytes, rawInput, strlen(rawInput));
    } else {
        ASSERT(!rawInput);
        char ch;
        uint32_t idx = 0;
        while ((ch=getchar()) != EOF){
            bytes[idx++] = ch;
        }
        printf("\n");
    }

    uint32_t numberOfBytes = strlen((char*)bytes);
    if (hexInput){
        bytes = convertAscii(&numberOfBytes, bytes);
    }

#ifdef UD_DISASM
    ud_t ud_obj;
    ud_init(&ud_obj);
    ud_set_input_buffer(&ud_obj, bytes, strlen((char*)bytes));
    ud_set_mode(&ud_obj, mode);
    ud_set_syntax(&ud_obj, UD_SYN_ATT);
    while (ud_disassemble(&ud_obj)) {
        instructions.append(new Instruction(&ud_obj));
        instructions.back()->print();
        fprintf(stdout, "\n");
    }

    for (uint32_t i = 0; i < instructions.size(); i++){
    }

#endif

    delete[] bytes;
    for (uint32_t i = 0; i < instructions.size(); i++){
        delete instructions[i];
    }

    return 0;
}

