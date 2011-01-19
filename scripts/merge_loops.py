#!/usr/bin/env python

import getopt
import string
import sys

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(-1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        [--file <full_pebil_info>]"
    print_error(err)

def stringify(arr):
    strver = [str(i) for i in arr]
    return string.join(strver, ' ')

def get_outer_loop_head(bbhash, block_info):
    while bbhash != 0 and bbhash != int(block_info[bbhash][29]):
        bbhash = int(block_info[bbhash][29])
    return bbhash


## set up command line args                                                                                                                                        
try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['--file='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

file=''
for i in range(0,len(optlist),1):
    if optlist[i][0] == '--file':
        file = optlist[i][1]
    else:
        print_usage('unknown argument ' + str(optlist[i][0]))
        sys.exit(-1)

if file == '':
    file = sys.stdin
else:
    file = open(file, 'r')
inpdata = file.readlines()
file.close()

inp_hash = {}
for line in inpdata:
    if not line.startswith('#'):
        toks = line.split()
        try:
            bbhash = int(toks[2])
        except ValueError, err:
            print_error(err)
        inp_hash[bbhash] = toks

#print '#<cpu> <sysid>',
#print '<block_unqid> <sequence> <memop> <fpop> <insn> <flname> <line> <fncame> <vaddr> <loopcnt> <loopid> <ldepth> <lploc> <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op> <total_mem_op> <total_mem_bytes> <bytes/op> <loop_head> <parent_loop_head>',
#print '<jbb_block_count>',
#print '<loop_count_this> <loop_count_outmost>',
#print '<sim_block_count> <visit_count> <sample_count>',
#print '<L1hit> <L1miss> <L2hit> <L2miss> <L3hit> <L3miss>',
#print '<duint_cnt> <duint_totlen> <dufp_cnt> <dufp_totlen>'


loop_totals = {}
for k in inp_hash.keys():
    block_data = inp_hash[k]
    # if this block is in a loop
    if int(block_data[12]) != -1:
        loop_head = get_outer_loop_head(k, inp_hash)
        if not loop_totals.has_key(loop_head):
            head = inp_hash[loop_head]
            loop_totals[loop_head] = [head[0], head[1], head[2], head[3], 0, 0, 0, head[7], head[8], head[9], head[10], head[11], head[12], head[13], head[14], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, head[31]] 
        loop_info = loop_totals[loop_head]
        weight = int(block_data[30])
        loop_info[4] += int(block_data[4])*weight
        loop_info[5] += int(block_data[5])*weight
        loop_info[6] += int(block_data[6])*weight
        loop_info[15] += int(block_data[36])*weight
        loop_info[16] += int(block_data[37])*weight
        loop_info[17] += int(block_data[38])*weight
        loop_info[18] += int(block_data[39])*weight
        loop_info[19] += int(block_data[40])*weight
        loop_info[20] += int(block_data[41])*weight
        loop_info[21] += int(block_data[42])*weight
        loop_info[22] += int(block_data[43])*weight
        loop_info[23] += int(block_data[44])*weight
        loop_info[24] += int(block_data[45])*weight
        loop_info[25] += weight
        loop_totals[loop_head] = loop_info
#        print block_data
#        print loop_totals[loop_head]

for k in loop_totals.keys():
    loop_info = loop_totals[k]
    totw = loop_info[25]

    ## get rid of loops that weren't executed
    if totw == 0:
        loop_totals.pop(k)
        continue
    loop_info[4] = round(float(loop_info[4]) / float(totw), 3)
    loop_info[5] = round(float(loop_info[5]) / float(totw), 3)
    loop_info[6] = round(float(loop_info[6]) / float(totw), 3)
    loop_info[15] = round(float(loop_info[15]) / float(totw), 3)
    loop_info[16] = round(float(loop_info[16]) / float(totw), 3)
    loop_info[17] = round(float(loop_info[17]) / float(totw), 3)
    loop_info[18] = round(float(loop_info[18]) / float(totw), 3)
    loop_info[19] = round(float(loop_info[19]) / float(totw), 3)
    loop_info[20] = round(float(loop_info[20]) / float(totw), 3)
    loop_info[21] = round(float(loop_info[21]) / float(totw), 3)
    loop_info[22] = round(float(loop_info[22]) / float(totw), 3)
    loop_info[23] = round(float(loop_info[23]) / float(totw), 3)
    loop_info[24] = round(float(loop_info[24]) / float(totw), 3)
    loop_totals[k] = loop_info
    print stringify(loop_totals[k])
    
