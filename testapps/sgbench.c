#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <DFPattern.h>
#include <math.h>

#define STACK_ARRAY_SIZE 0x1000
#define MAX_ELEMENT_SIZE 0x100000

uint32_t index_array[MAX_ELEMENT_SIZE/2];
double data_array[MAX_ELEMENT_SIZE];

/*************** Body of the benchmarks *************/
void scatter_body(double* A,double* B, uint32_t* index,uint32_t n,uint32_t* stack_array){
    uint32_t i;
    for(i=0;i<n;i++){
        A[index[i]] = B[i];
		stack_array[i % STACK_ARRAY_SIZE] = i;
    }
}

void gather_body(double* A,double* B, uint32_t* index,uint32_t n,uint32_t* stack_array){
    uint32_t i;
    for(i=0;i<n;i++){
        A[i] = B[index[i]];
    }
}
/*************** end of the bodies of benchs ********/

/*************** Random number generation ***********/
#define RAND_MAX_NUMBER 0x8000
static uint64_t next_number = 1;
void set_random_seed(uint64_t seed){
    next_number = seed;
}
/* RAND_MAX assumed to be 32767 */
uint32_t generate_random(){
    next_number = next_number * 1103515245 + 12345;
    uint64_t value = next_number/(RAND_MAX_NUMBER << 1);
    value = value % RAND_MAX_NUMBER; 
    return (uint32_t)value;
}
/************** End of rand generation **************/

/*************** Initialization of the data *********/
void initialize_data(){
    uint32_t i;
    for(i=0;i<MAX_ELEMENT_SIZE;i++){
        data_array[i] = exp(-1.0*i/(double)MAX_ELEMENT_SIZE);
    }
}

void initialize_index_array(uint32_t* idxs,uint32_t n){
    uint32_t i;
    for(i=0;i<(MAX_ELEMENT_SIZE/2);i++){
        index_array[i] = i;
    }
    for(i=0;i<n;i++){
        idxs[i] = generate_random() % n;
    }
}

/*************** initialization ends here ***********/


DFPatternType convertDFPattenType(char* patternString){
    if(!strcmp(patternString,"dfTypePattern_Gather")){
        return dfTypePattern_Gather;
    } else if(!strcmp(patternString,"dfTypePattern_Scatter")){
        return dfTypePattern_Scatter;
    } else if(!strcmp(patternString,"dfTypePattern_FunctionCallGS")){
        return dfTypePattern_FunctionCallGS;
    }
    return dfTypePattern_undefined;
}

const char* DFPatternTypeNames[] = {
    "dfTypePattern_undefined",
    "dfTypePattern_Other",
    "dfTypePattern_Stream",
    "dfTypePattern_Transpose",
    "dfTypePattern_Random",
    "dfTypePattern_Reduction",
    "dfTypePattern_Stencil",
    "dfTypePattern_Gather",
    "dfTypePattern_Scatter",
    "dfTypePattern_FunctionCallGS",
    "dfTypePattern_Init",
    "dfTypePattern_Default",
    "dfTypePattern_Scalar",
    "dfTypePattern_None"
};

void print_usage(char* s){
    fprintf(stdout,"usage: %s [--ainc <offset_for_a>]\n",s);
    fprintf(stdout,"          [--binc <offset_for_b_after_a>]\n");
    fprintf(stdout,"          [--iinc <offset_for_index>]\n");
    fprintf(stdout,"          [--alen <element_count>]\n");
    fprintf(stdout,"          [--type <dfpattern_type>]\n");
    fprintf(stdout,"          [--help]\n");
}
int main(int argc,char* argv[]){
        
    int32_t       a_offset = 0;
    int32_t       b_offset = 0;
    int32_t       i_offset = 0;
    int32_t       length   = MAX_ELEMENT_SIZE / 2;
    DFPatternType dfp_type = dfTypePattern_Gather;
    int           is_help = 0;

        int32_t i;
    for (i = 1; i < argc; i++){
        if(!strcmp(argv[i],"--ainc")){
            a_offset = atoi(argv[++i]);
        } else if(!strcmp(argv[i],"--binc")){
            b_offset = atoi(argv[++i]);
        } else if(!strcmp(argv[i],"--iinc")){
            i_offset = atoi(argv[++i]);
        } else if(!strcmp(argv[i],"--alen")){
            length = atoi(argv[++i]);
        } else if(!strcmp(argv[i],"--type")){
            dfp_type  = convertDFPattenType(argv[++i]);
        } else if(!strcmp(argv[i],"--help")){
            is_help = 1;
        } else {
            print_usage(argv[0]);
            exit(-1);
        }
    }
    if(is_help){
        print_usage(argv[0]);
        return 0;
    }

    if((a_offset < 0) || (b_offset < 0) || (i_offset < 0) || (length <= 0)){
        fprintf(stderr,"Error: non-negative arguments are needed\n");
        exit(-1);
    }
    if(length > (MAX_ELEMENT_SIZE / 2)){
        fprintf(stderr,"Error: element count is too big to hold all arrays\n");
        exit(-1);
    }
    if(dfp_type == dfTypePattern_undefined){
        fprintf(stderr,"Error: dfpattern type is invalid\n");
        exit(-1);
    }

    if(i_offset > (MAX_ELEMENT_SIZE / 2)){
        fprintf(stderr,"Error: index array offset is not applicable\n");
        exit(-1);

    }
    if((i_offset + length) > (MAX_ELEMENT_SIZE/2)){
        fprintf(stderr,"Error: I array can not be fit");
        exit(-1);
    }
    i_offset = (MAX_ELEMENT_SIZE/2) - (length + i_offset);
    uint32_t* idx_ptr = &(index_array[i_offset]);

    if((a_offset + length) > MAX_ELEMENT_SIZE){
        fprintf(stderr,"Error: A array can not be fit");
        exit(-1);
    }
    double* a_ptr = &(data_array[a_offset]);

    if((a_offset + length + b_offset + length) > MAX_ELEMENT_SIZE){
        fprintf(stderr,"Error: B array can not be fit");
        exit(-1);
    }
    double* b_ptr = &(data_array[a_offset + length + b_offset]);

    initialize_index_array(idx_ptr,length);
    initialize_data();

	uint32_t stack_array[STACK_ARRAY_SIZE];
	for(i=0;i<STACK_ARRAY_SIZE;i++){
		stack_array[i] = i;
	}

    fprintf(stdout,"I = ( 0x%p, 0x%p, %u bytes)\n",idx_ptr,idx_ptr+length,sizeof(uint32_t)*length); 
    fprintf(stdout,"A = ( 0x%p, 0x%p, %u bytes)\n",a_ptr,a_ptr+length,sizeof(double)*length); 
    fprintf(stdout,"B = ( 0x%p, 0x%p, %u bytes)\n",b_ptr,b_ptr+length,sizeof(double)*length); 

    switch(dfp_type){
        case dfTypePattern_Scatter : scatter_body(a_ptr,b_ptr,idx_ptr,length,stack_array); break;
        case dfTypePattern_Gather  : gather_body(a_ptr,b_ptr,idx_ptr,length,stack_array);  break;
        default: break;
    }

    double check_sum1 = 0.0;
    double check_sum2 = 0.0;
    for(i=0;i<length;i++){
        check_sum1 += (a_ptr[i] + b_ptr[i]);
		check_sum2 += stack_array[i % STACK_ARRAY_SIZE];
    }

    fprintf(stdout,"checksums are %f and %f\n",check_sum1,check_sum2);

    return 0;
}

