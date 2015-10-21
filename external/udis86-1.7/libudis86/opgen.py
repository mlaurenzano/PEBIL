#!/bin/env python

import os
import sys
import string
from xml.dom import minidom

#
# opgen.py -- generates tables and constants for decoding
#
# - itab.c
# - itab.h
#

#
# special mnemonic types for internal purposes.
#
spl_mnm_types = [   'totaltypes', \
                    'd3vil',      \
                    'na',         \
                    'grp_reg',    \
                    'grp_rm',     \
                    'grp_vendor', \
                    'grp_x87',    \
                    'grp_mode',   \
                    'grp_osize',  \
                    'grp_asize',  \
                    'grp_mod',    \
                    'grp_w',      \
                    'none'        \
                ]

#
# opcode-vendor dictionary
#                
vend_dict = { 
    'AMD'       : '00', 
    'INTEL'     : '01' 
}


#
# opcode-mode dictionary
#

mode_dict   = { 
    '16'        : '00', 
    '32'        : '01', 
    '64'        : '02' 
}

#
# opcode-operand dictionary
#
operand_dict = {
    "Ap"       : [    "OP_A"        , "SZ_P"     ], # seg:offset
    "E"        : [    "OP_E"        , "SZ_NA"    ], # GPR/m stored in ModRm.rm
    "Eb"       : [    "OP_E"        , "SZ_B"     ],
    "Ew"       : [    "OP_E"        , "SZ_W"     ],
    "Ev"       : [    "OP_E"        , "SZ_V"     ],
    "Ed"       : [    "OP_E"        , "SZ_D"     ],
    "Eq"       : [    "OP_E"        , "SZ_Q"     ],
    "Ez"       : [    "OP_E"        , "SZ_Z"     ],
    "Ex"       : [    "OP_E"        , "SZ_MDQ"   ],
    "Ep"       : [    "OP_E"        , "SZ_P"     ],
    "G"        : [    "OP_G"        , "SZ_NA"    ], # GPR stored in ModRm.reg
    "Gb"       : [    "OP_G"        , "SZ_B"     ],
    "Gw"       : [    "OP_G"        , "SZ_W"     ],
    "Gv"       : [    "OP_G"        , "SZ_V"     ],
    "Gvw"      : [    "OP_G"        , "SZ_MDQ"   ],
    "Gd"       : [    "OP_G"        , "SZ_D"     ],
    "Gx"       : [    "OP_G"        , "SZ_MDQ"   ],
    "Gz"       : [    "OP_G"        , "SZ_Z"     ],
    "Gq"       : [    "OP_G"        , "SZ_Q"     ],
    "GVd"      : [    "OP_GV"       , "SZ_D"     ], # GPR stored in VEX.vvvv
    "GVq"      : [    "OP_GV"       , "SZ_Q"     ],
    "M"        : [    "OP_M"        , "SZ_NA"    ], # m in ModRm.rm
    "Mb"       : [    "OP_M"        , "SZ_B"     ],
    "Mw"       : [    "OP_M"        , "SZ_W"     ],
    "Ms"       : [    "OP_M"        , "SZ_W"     ],
    "Md"       : [    "OP_M"        , "SZ_D"     ],
    "Mq"       : [    "OP_M"        , "SZ_Q"     ],
    "Mt"       : [    "OP_M"        , "SZ_T"     ],
    "I1"       : [    "OP_I1"       , "SZ_NA"    ], # Implied constant 1
    "I3"       : [    "OP_I3"       , "SZ_NA"    ], # Implied constant 3
    "Ib"       : [    "OP_I"        , "SZ_B"     ], # Immediate
    "Isb"      : [    "OP_I"        , "SZ_SB"    ],
    "Iw"       : [    "OP_I"        , "SZ_W"     ],
    "Iv"       : [    "OP_I"        , "SZ_V"     ],
    "Iz"       : [    "OP_I"        , "SZ_Z"     ],
    "Jv"       : [    "OP_J"        , "SZ_V"     ], # Immediate for a jump
    "Jz"       : [    "OP_J"        , "SZ_Z"     ],
    "Jb"       : [    "OP_J"        , "SZ_B"     ],
    "R"        : [    "OP_R"        , "SZ_RDQ"   ], # GPR/m in ModRm.rm, same as E but implies size RDQ
    "C"        : [    "OP_C"        , "SZ_NA"    ], # CRG in ModRm.reg
    "D"        : [    "OP_D"        , "SZ_NA"    ], # DBG in ModRm.reg
    "S"        : [    "OP_S"        , "SZ_NA"    ], # SEG in ModRm.reg
    "Ob"       : [    "OP_O"        , "SZ_B"     ], # An offset
    "Ow"       : [    "OP_O"        , "SZ_W"     ],
    "Ov"       : [    "OP_O"        , "SZ_V"     ],
    "X"        : [    "OP_X"        , "SZ_X"     ], # xmm/ymm reg in VEX.vvvv
    "V"        : [    "OP_V"        , "SZ_NA"    ], # xmm/ymm reg in ModRm.reg
    "Vd"       : [    "OP_V"        , "SZ_D"     ],
    "Vq"       : [    "OP_V"        , "SZ_Q"     ],
    "Vx"       : [    "OP_V"        , "SZ_X"     ],
    "W"        : [    "OP_W"        , "SZ_NA"    ], # xmm/ymm/m in ModRm.rm
    "Wd"       : [    "OP_W"        , "SZ_D"     ],
    "Wq"       : [    "OP_W"        , "SZ_Q"     ],
    "Wx"       : [    "OP_W"        , "SZ_X"     ],
    "P"        : [    "OP_P"        , "SZ_NA"    ], # mmx in ModRm.reg
    "Pq"       : [    "OP_P"        , "SZ_Q"     ],
    "Q"        : [    "OP_Q"        , "SZ_NA"    ],
    "Qq"       : [    "OP_Q"        , "SZ_Q"     ], # mmx/m in ModRm.rm
    "VR"       : [    "OP_VR"       , "SZ_NA"    ], # xmm reg in ModRm.rm
    "VRx"      : [    "OP_VR"       , "SZ_X"     ],
    "PR"       : [    "OP_PR"       , "SZ_NA"    ], # mmx reg in ModRm.rm
    "PRq"      : [    "OP_PR"       , "SZ_Q"     ],
    "AL"       : [    "OP_AL"       , "SZ_NA"    ], # specific 8 bit GPR
    "CL"       : [    "OP_CL"       , "SZ_NA"    ],
    "DL"       : [    "OP_DL"       , "SZ_NA"    ],
    "BL"       : [    "OP_BL"       , "SZ_NA"    ],
    "AH"       : [    "OP_AH"       , "SZ_NA"    ],
    "CH"       : [    "OP_CH"       , "SZ_NA"    ],
    "DH"       : [    "OP_DH"       , "SZ_NA"    ],
    "BH"       : [    "OP_BH"       , "SZ_NA"    ],
    "AX"       : [    "OP_AX"       , "SZ_NA"    ], # specific 16 bit GPR
    "CX"       : [    "OP_CX"       , "SZ_NA"    ],
    "DX"       : [    "OP_DX"       , "SZ_NA"    ],
    "BX"       : [    "OP_BX"       , "SZ_NA"    ],
    "SI"       : [    "OP_SI"       , "SZ_NA"    ],
    "DI"       : [    "OP_DI"       , "SZ_NA"    ],
    "SP"       : [    "OP_SP"       , "SZ_NA"    ],
    "BP"       : [    "OP_BP"       , "SZ_NA"    ],
    "eAX"      : [    "OP_eAX"      , "SZ_NA"    ], # specific 32 bit GPR
    "eCX"      : [    "OP_eCX"      , "SZ_NA"    ],
    "eDX"      : [    "OP_eDX"      , "SZ_NA"    ],
    "eBX"      : [    "OP_eBX"      , "SZ_NA"    ],
    "eSI"      : [    "OP_eSI"      , "SZ_NA"    ],
    "eDI"      : [    "OP_eDI"      , "SZ_NA"    ],
    "eSP"      : [    "OP_eSP"      , "SZ_NA"    ],
    "eBP"      : [    "OP_eBP"      , "SZ_NA"    ],
    "rAX"      : [    "OP_rAX"      , "SZ_NA"    ], # specific 64 bit GPR
    "rCX"      : [    "OP_rCX"      , "SZ_NA"    ],
    "rBX"      : [    "OP_rBX"      , "SZ_NA"    ],
    "rDX"      : [    "OP_rDX"      , "SZ_NA"    ],
    "rSI"      : [    "OP_rSI"      , "SZ_NA"    ],
    "rDI"      : [    "OP_rDI"      , "SZ_NA"    ],
    "rSP"      : [    "OP_rSP"      , "SZ_NA"    ],
    "rBP"      : [    "OP_rBP"      , "SZ_NA"    ],
    "ES"       : [    "OP_ES"       , "SZ_NA"    ], # specific segment register
    "CS"       : [    "OP_CS"       , "SZ_NA"    ],
    "DS"       : [    "OP_DS"       , "SZ_NA"    ],
    "SS"       : [    "OP_SS"       , "SZ_NA"    ],
    "GS"       : [    "OP_GS"       , "SZ_NA"    ],
    "FS"       : [    "OP_FS"       , "SZ_NA"    ],
    "ST0"      : [    "OP_ST0"      , "SZ_NA"    ], # specific FP register
    "ST1"      : [    "OP_ST1"      , "SZ_NA"    ],
    "ST2"      : [    "OP_ST2"      , "SZ_NA"    ],
    "ST3"      : [    "OP_ST3"      , "SZ_NA"    ],
    "ST4"      : [    "OP_ST4"      , "SZ_NA"    ],
    "ST5"      : [    "OP_ST5"      , "SZ_NA"    ],
    "ST6"      : [    "OP_ST6"      , "SZ_NA"    ],
    "ST7"      : [    "OP_ST7"      , "SZ_NA"    ],
    "ALr8b"    : [    "OP_ALr8b"    , "SZ_NA"    ], # specific GPR
    "CLr9b"    : [    "OP_CLr9b"    , "SZ_NA"    ],
    "DLr10b"   : [    "OP_DLr10b"   , "SZ_NA"    ],
    "BLr11b"   : [    "OP_BLr11b"   , "SZ_NA"    ],
    "AHr12b"   : [    "OP_AHr12b"   , "SZ_NA"    ],
    "CHr13b"   : [    "OP_CHr13b"   , "SZ_NA"    ],
    "DHr14b"   : [    "OP_DHr14b"   , "SZ_NA"    ],
    "BHr15b"   : [    "OP_BHr15b"   , "SZ_NA"    ],
    "rAXr8"    : [    "OP_rAXr8"    , "SZ_NA"    ],
    "rCXr9"    : [    "OP_rCXr9"    , "SZ_NA"    ],
    "rDXr10"   : [    "OP_rDXr10"   , "SZ_NA"    ],
    "rBXr11"   : [    "OP_rBXr11"   , "SZ_NA"    ],
    "rSPr12"   : [    "OP_rSPr12"   , "SZ_NA"    ],
    "rBPr13"   : [    "OP_rBPr13"   , "SZ_NA"    ],
    "rSIr14"   : [    "OP_rSIr14"   , "SZ_NA"    ],
    "rDIr15"   : [    "OP_rDIr15"   , "SZ_NA"    ],
    "jWP"      : [    "OP_J"        , "SZ_WP"    ], # jump immediate
    "jDP"      : [    "OP_J"        , "SZ_DP"    ],

    "ZR"       : [    "OP_ZR"       , "SZ_XZ"     ], # zmm in R':R:reg
    "ZM"       : [    "OP_ZM"       , "SZ_XZ"     ], # Mem in ModRm.rm
    "ZRM"      : [    "OP_ZRM"      , "SZ_XZ"     ], # zmm/m in X:B:ModRm.rm
    "ZV"       : [    "OP_ZV"       , "SZ_XZ"     ], # zmm in V'vvvv
    "ZVM"      : [    "OP_ZVM"      , "SZ_XZ"     ], # Vector memory addresses

    "KR"       : [    "OP_KR"       , "SZ_W"      ], # K register in ModRm.reg
    "KRM"      : [    "OP_KRM"      , "SZ_W"      ], # K register in ModRm.rm
    "KV"       : [    "OP_KV"       , "SZ_W"      ], # K register in MVEX.vvvv

}

#
# opcode prefix dictionary
# 
pfx_dict = { 
    "aso"      : "P_aso",   
    "oso"      : "P_oso",   
    "rexw"     : "P_rexw", 
    "rexb"     : "P_rexb",  
    "rexx"     : "P_rexx",  
    "rexr"     : "P_rexr",
    "vexlz"    : "P_vexlz",
    "vexlig"   : "P_vexlig",
    "vexix"    : "P_vexix",
    "inv64"    : "P_inv64", 
    "def64"    : "P_def64", 
    "depM"     : "P_depM",
    "cast1"    : "P_c1",    
    "cast2"    : "P_c2",    
    "cast3"    : "P_c3"   
}


#
# globals
#
opr_constants = []
siz_constants = []
tables        = {}
table_sizes   = {}
mnm_list      = ['invalid']
default_opr   = 'O_NONE, O_NONE, O_NONE, O_NONE'

def insert_mnm(m):
    if mnm_list.count(m) == 0:
        mnm_list.append(m)

#
# collect the operand/size constants
# 
for o in operand_dict.keys():
    if not (operand_dict[o][0] in opr_constants):
        opr_constants.append(operand_dict[o][0])
    if not (operand_dict[o][1] in siz_constants):
        siz_constants.append(operand_dict[o][1])

xmlDoc = minidom.parse( "../docs/x86optable.xml" )
tlNode = xmlDoc.firstChild

#
# look for top-level optable node
#
while tlNode and tlNode.localName != "x86optable": tlNode = tlNode.nextSibling

#
# creates a table entry
#
def centry(i, defmap):
    if defmap["type"][0:3] == "grp":
        mnm    = 'UD_I' + defmap["type"].lower()
        opr    = default_opr
        flg_use = "F_none"
        flg_def = "F_none"
        imp_use = "R_none"
        imp_def = "R_none"
        pfx    = defmap["name"].upper()
    elif defmap["type"] == "leaf":
        mnm    = "UD_I" + defmap["name"]
        opr    = defmap["opr"]
        flg    = defmap["flags"]
        flg_use = string.join(['F_' + f.upper() for f in flg['use']], ' | ')
        flg_def = string.join(['F_' + f.upper() for f in flg['def']], ' | ')
        imp    = defmap["implied"]
        imp_use = string.join(['R_' + j.upper() for j in imp['use']], ' | ')
        imp_def = string.join(['R_' + j.upper() for j in imp['def']], ' | ')
        pfx    = defmap["pfx"]
        if len(mnm) == 0: mnm = "UD_Ina"
        if len(opr) == 0: opr = default_opr
        if len(flg_use) == 0: flg_use = "F_none"
        if len(flg_def) == 0: flg_def = "F_none"
        if len(imp_use) == 0: imp_use = "R_none"
        if len(imp_def) == 0: imp_def = "R_none"
        if len(pfx) == 0: pfx = "P_none"
    else:
        mnm    = "UD_Iinvalid"
        opr    = default_opr
        flg_use = "F_none"
        flg_def = "F_none"
        imp_use = "R_none"
        imp_def = "R_none"
        pfx    = "P_none"

    return "  /* %s */  { %-16s %-26s %s %s %s %s %s },\n" % (i, mnm + ',', opr + ',', flg_use + ',', flg_def + ',', imp_use + ',', imp_def + ',', pfx)

#
# makes a new table and adds it to the global
# list of tables
#
def mktab(name, size):
    if not (name in tables.keys()):
        tables[name] = {}
        table_sizes[name] = size

for node in tlNode.childNodes:

    opcodes = []
    iclass  = ''
    vendor  = ''

    # we are only interested in <instruction>
    if node.localName != 'instruction':
        continue

     # we need the mnemonic attribute
    if not ('mnemonic' in node.attributes.keys()):
        print "error: no mnemonic given in <instruction>."
        sys.exit(-1) 

    # check if this instruction was already defined.
    # else add it to the global list of mnemonics
    mnemonic = node.attributes['mnemonic'].value
    #if mnemonic in mnm_list:
    #    print "error: multiple declarations of mnemonic='%s'" % mnemonic;
    #    sys.exit(-1)

    if len(node.childNodes) == 0:
        insert_mnm(mnemonic)

    #
    # collect instruction 
    #   - vendor
    #   - class
    #
    for n in node.childNodes:
        if n.localName == 'vendor':
            vendor = (n.firstChild.data).strip();
        elif n.localName == 'class':
            iclass = n.firstChild.data;

    #
    # for each opcode definition
    #
    for n in node.childNodes:
        if n.localName != 'opcode':
            continue;
        
        opcode = n.firstChild.data.strip();
        parts  = opcode.split(";"); 
        pfx    = []
        opr    = []
        pfx_c  = []

        # get cast attribute, if given
        if 'cast' in n.attributes.keys():
            pfx_c.append( "P_c" + n.attributes['cast'].value )

	# get implicit addressing attribute, if given
	if 'imp_addr' in n.attributes.keys():
            if int( n.attributes['imp_addr'].value ):
                pfx_c.append( "P_ImpAddr" )

        # get mode attribute, if given
        if 'mode' in n.attributes.keys():
            v = (n.attributes['mode'].value).strip()
            modef = v.split();
            for m in modef:
                if not (m in pfx_dict):
                    print "warning: unrecognized mode attribute '%s'" % m
                else:
                     pfx_c.append(pfx_dict[m])

        #
        # split opcode definition into
        #   1. prefixes (pfx)
        #   2. opcode bytes (opc)
        #   3. operands
        #   4. flags use/defs
        #   5. implicit registers use/defs
        #   6. classifications
        #
        if len(parts) == 5:
            pfx = parts[0].split()
            opc = parts[1].split()
            opr = parts[2].split()
            flg = parts[3].split()
            imp = parts[4].split()
        else:
            print "error: invalid opcode definition of 3 %s\n" % mnemonic
            sys.exit(-1)
        # Convert opcodes to upper case
        for i in range(len(opc)):
            opc[i] = opc[i].upper()

        # collect flags use/defs
        flags = {}
        flags['use'] = []
        flags['def'] = []
        for i in range(len(flg)):
            tks = flg[i].split(':')
            if len(tks) != 2 or (tks[0] != 'u' and tks[0] != 'd'):
                print "error: invalid flags declaration %s" % flg[i]
                sys.exit(-1) 
            [usedef, f] = tks
            if usedef == 'u':
                flags['use'].append(f.upper())
            else:
                flags['def'].append(f.upper())

        implied = {}
        implied['use'] = []
        implied['def'] = []
        for i in range(len(imp)):
            tks = imp[i].split(':')
            if len(tks) != 2 or (tks[0] != 'u' and tks[0] != 'd'):
                print "error: invalid implied reg declaration %s" % imp[i]
                sys.exit(-1) 
            [usedef, f] = tks
            if usedef == 'u':
                implied['use'].append(f.upper())
            else:
                implied['def'].append(f.upper())

        #
        # check for special cases of instruction translation
        # and ignore them
        # 
        if mnemonic == 'pause' or \
           ( mnemonic == 'nop' and opc[0] == '90' ) or \
           mnemonic == 'invalid' or \
            mnemonic == 'db' :
            insert_mnm(mnemonic)
            continue

        #
        # Convert prefix
        #
        for p in pfx:
            if not ( p in pfx_dict.keys() ):
                print "error: invalid prefix specification: %s \n" % pfx
                sys.exit(-1) 
            pfx_c.append( pfx_dict[p] )
        if len(pfx) == 0:
            pfx_c.append( "P_none" )
        pfx = "|".join( pfx_c )

        #
        # Convert operands
        #
        opr_c = [ "O_NONE", "O_NONE", "O_NONE", "O_NONE" ]
        for i in range(len(opr)): 
            if not (opr[i] in operand_dict.keys()):
                print "error: invalid operand declaration: %s\n" % opr[i]
                sys.exit(-1)
            opr_c[i] = "O_" + opr[i]
        opr = "%-8s %-8s %-8s %s" % (opr_c[0] + ",", opr_c[1] + ",", opr_c[2] + ",", opr_c[3])

        table_sse    = ''
        table_ssedone = 0
        table_avx    = ''
        table_avxdone = 0
        table_name   = 'itab__1byte'
        table_size   = 256
        table_index  = ''
        
        for op in opc:
            if op[0:4] == 'MVEX':
                table_avx = op[0:4]
            elif op[0:3] == 'SSE':
                table_sse = op
            elif op[0:3] == 'AVX':
                table_avx = op[0:3]
                table_avxdone = 1
            elif op[0:3] == 'VEX':
                table_avx = 'AVX'
                table_name = "itab__avx"
            elif op == '0F' and len(table_sse) and len(table_avx):
                table_name = "itab__" + table_avx + "__pfx_" + table_sse + "__0f"
                table_size = 256
                table_avx = ''
                table_index = op
            elif op == '0F' and len(table_avx):
                table_name = "itab__" + table_avx + "__0f"
                table_size = 256
                table_avx  = ''
            elif op == '0F' and len(table_sse):
                table_name = "itab__pfx_%s__0f" % table_sse
                table_size = 256
                table_sse  = ''
                table_ssedone += 1
            elif op == '0F' and table_ssedone:
                table_index = op
            elif op == '0F':
                table_name = "itab__0f"
                table_size = 256
                table_ssedone += 1
            elif op[0:5] == '/X87=':
                tables[table_name][table_index] = { \
                    'type' : 'grp_x87',  \
                    'name' : "%s__op_%s__x87" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(op[5:7], 16)
                table_size = 64
            elif op[0:4] == '/RM=':
                tables[table_name][table_index] = { \
                    'type' : 'grp_rm',  \
                    'name' : "%s__op_%s__rm" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(op[4:6])
                table_size  = 8
            elif op[0:5] == '/MOD=':
                tables[table_name][table_index] = { \
                    'type' : 'grp_mod',  \
                    'name' : "%s__op_%s__mod" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']
                if len(op) == 8:
                    v = op[5:8]
                else:
                    v = op[5:7]
                mod_dict    = { '!11' : 0, '11' : 1 }
                table_index = "%02X" % int(mod_dict[v])
                table_size  = 2
            elif op[0:2] == '/O':
                tables[table_name][table_index] = { \
                    'type' : 'grp_osize',  \
                    'name' : "%s__op_%s__osize" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(mode_dict[op[2:4]])
                table_size  = 3
            elif op[0:2] == '/A':
                tables[table_name][table_index] = { \
                    'type' : 'grp_asize',  \
                    'name' : "%s__op_%s__asize" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(mode_dict[op[2:4]])
                table_size  = 3
            elif op[0:7] == '/3BYTE=':
                tables[table_name][table_index] = { \
                    'type' : 'grp_reg',  \
                    'name' : "%s__op_%s__3byte_%s__reg" % (table_name, table_index, op[7:9]) \
                }
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(op[7:9], 16)
                table_size  = 256
            elif op[0:2] == '/M':
                tables[table_name][table_index] = { \
                    'type' : 'grp_mode',  \
                    'name' : "%s__op_%s__mode" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(mode_dict[op[2:4]])
                table_size  = 3
            elif op[0:6] == '/3DNOW':
                table_name  = "itab__3dnow"
                table_size  = 256
            elif op[0:3] == "/W=":
                tables[table_name][table_index] = { \
                    'type' : 'grp_w', \
                    'name' : "%s__op_%s__w" % (table_name, table_index) \
                }
                table_name = tables[table_name][table_index]['name']
                table_index = "%02X" % int(op[3:4])
                table_size = 2
            elif op[0:1] == '/':
                tables[table_name][table_index] = { \
                    'type' : 'grp_reg',  \
                    'name' : "%s__op_%s__reg" % (table_name, table_index) \
                }                  
                table_name  = tables[table_name][table_index]['name']    
                table_index = "%02X" % int(op[1:2])
                table_size  = 8
            else:
                table_index = op

            mktab(table_name, table_size)

        if len(vendor):
            tables[table_name][table_index] = { \
                'type' : 'grp_vendor',  \
                'name' : "%s__op_%s__vendor" % (table_name, table_index) \
            }                  
            table_name  = tables[table_name][table_index]['name']    
            table_index = vend_dict[vendor]
            table_size = 2
            mktab(table_name, table_size)

        m = mnemonic
        if table_avxdone > 0:
            m = 'v' + m
        insert_mnm(m)

        tables[table_name][table_index] = { \
            'type'  : 'leaf',   \
            'name'  : m,        \
            'pfx'   : pfx,      \
            'opr'   : opr,      \
            'flags' : flags,    \
            'implied': implied  \
        }

# ---------------------------------------------------------------------
# Generate itab.h
# ---------------------------------------------------------------------

f = open("itab.h", "w")

f.write('''
/* itab.h -- auto generated by opgen.py, do not edit. */

#ifndef UD_ITAB_H
#define UD_ITAB_H

''')

#
# Generate enumeration of size constants
#
siz_constants.sort()
f.write('''
''')

f.write("\nenum ud_itab_vendor_index {\n" )
f.write("  ITAB__VENDOR_INDX__AMD,\n" )
f.write("  ITAB__VENDOR_INDX__INTEL,\n" )
f.write("};\n\n")


f.write("\nenum ud_itab_mode_index {\n" )
f.write("  ITAB__MODE_INDX__16,\n" )
f.write("  ITAB__MODE_INDX__32,\n" )
f.write("  ITAB__MODE_INDX__64\n" )
f.write("};\n\n")


f.write("\nenum ud_itab_mod_index {\n" )
f.write("  ITAB__MOD_INDX__NOT_11,\n" )
f.write("  ITAB__MOD_INDX__11\n" )
f.write("};\n\n")

#
# Generate enumeration of the tables
#
table_names = tables.keys()
table_names.sort();

f.write( "\nenum ud_itab_index {\n" )
for name in table_names:
    f.write("  %s,\n" % name.upper() );
f.write( "};\n\n" ) 

#
# Generate mnemonics list
#
f.write("\nenum ud_mnemonic_code {\n")
mnm_list.sort()
mnm_list = mnm_list + spl_mnm_types
for m in mnm_list:
    f.write("  UD_I%s,\n" % m)
f.write("};\n\n")

#
# Generate struct defs
#
f.write( \
'''

extern const char* ud_mnemonics_str[];;
extern struct ud_itab_entry* ud_itab_list[];

''' )


f.write("#endif\n")

f.close()

# ---------------------------------------------------------------------
# Generate itab.c
# ---------------------------------------------------------------------

f = open("itab.c", "w")

f.write('''
/* itab.c -- auto generated by opgen.py, do not edit. */

#include "types.h"
#include "decode.h"
#include "itab.h"

''')

#
# generate mnemonic list
#
f.write("const char * ud_mnemonics_str[] = {\n")
for m in mnm_list:
    f.write("  \"%s\",\n" % m )
f.write("};\n\n")

#
# generate instruction tables
#

f.write("\n")
for t in table_names:
    f.write("\nstatic struct ud_itab_entry " + t.lower() + "[%d] = {\n" % table_sizes[t]);
    for i in range(int(table_sizes[t])):
        index = "%02X" % i
        if index in tables[t]:
            f.write(centry(index, tables[t][index]))
        else:
            f.write(centry(index,{"type":"invalid"}))
    f.write("};\n");

#
# write the instruction table list
#
f.write( "\n/* the order of this table matches enum ud_itab_index */")
f.write( "\nstruct ud_itab_entry * ud_itab_list[] = {\n" )
for name in table_names:
    f.write( "  %s,\n" % name.lower() )
f.write( "};\n" );

f.close();

# vim:expandtab
# vim:sw=4
# vim:ts=4
