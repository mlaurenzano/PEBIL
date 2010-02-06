#ifndef _DFPattern_h_
#define _DFPattern_h_

#include <stdint.h>

typedef uint8_t DFPatternType;

typedef enum {
    dfTypePattern_undefined = 0,
    dfTypePattern_Other,
    dfTypePattern_Stream,
    dfTypePattern_Transpose,
    dfTypePattern_Random,
    dfTypePattern_Reduction,
    dfTypePattern_Stencil,
    dfTypePattern_Gather,
    dfTypePattern_Scatter,
    dfTypePattern_FunctionCallGS,
    dfTypePattern_Init,
    dfTypePattern_Default,
    dfTypePattern_Scalar,
    dfTypePattern_None,
    dfTypePattern_Total_Types
} DFPatternValues;

typedef enum {
    DFPattern_Inactive = 0,
    DFPattern_Active,
    DFPattern_Total_States
} DFPatternStates;

extern const char* DFPatternTypeNames[];

typedef struct {
    DFPatternType type;
    uint16_t      memopCnt;
} DFPatternSpec;

#endif
