#!/usr/bin/env python

# a script that reads in a set of loopcnt + siminst traces and attempts to produce a list of
# loops that should be acceptable for frequency throttling
# ./loopBlockSimu.py --application cg.C.8 --cpu_count 8 --trace_dir ~/NPB3.3/NPB3.3-MPI/run/ --system_id 77
#

import getopt
import string
import sys


def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(-1)
    
def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        [--file <static_file>]   (defaults to stdin)"
    print_error(err)

def stringify(arr):
    arr = [str(i) for i in arr]
    return string.join(arr, " ")

try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['file='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

pebil_static = ''

for i in range(0,len(optlist),1):
    if optlist[i][0] == '--file':
        pebil_static = optlist[i][1]
    else:
        print_usage('unknown argument ' + str(optlist[i][0]))

# read static data
lsf = ''
if pebil_static == '':
    lsraw = sys.stdin.readlines()
else:
    lsf = open(pebil_static, 'r')
    lsraw = lsf.readlines()
    lsf.close()

# <sequence> <block_unqid> <memop> <fpop> <insn> <line> <fname> # <hex_unq_id> <vaddr>
# +lpi <loopcnt> <loopid> <ldepth> <lploc>
# +cnt <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op>
# +mem <total_mem_op> <total_mem_bytes> <bytes/op>
# +lpc <loop_head> <parent_loop_head>
# +dud <dudist1>:<duint1>:<dufp1> <dudist2>:<ducnt2>:<dufp2>...
bbid_map = {}
current_block = -1
block_static = {}
block_lpi = {}
block_cnt = {}
block_mem = {}
block_lpc = {}
block_dud = {}
block_dxi = {}
block_bin = {}
for i in range(0,len(lsraw),1):
    line = lsraw[i].strip()
    if not line.startswith('#'):
        if line.startswith('+lpi'):
            toks = line.split()
            if len(toks) != 7:
                print_error('malformed line in loop static file line ' + str(i+1))
            block_lpi[current_block] = [int(toks[1]), int(toks[2]), int(toks[3]), int(toks[4])]
            bbhash = int(toks[6], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+lpi) line ' + str(i+1))
        elif line.startswith('+cnt'):
            toks = line.split()
            if len(toks) != 13:
                print_error('malformed line in loop static file line ' + str(i))
            block_cnt[current_block] = [int(toks[1]), int(toks[2]), int(toks[3]), int(toks[4]), int(toks[5]), int(toks[6]), int(toks[7]), int(toks[8]), int(toks[9]), int(toks[10])]
            bbhash = int(toks[12], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+cnt) line ' + str(i+1))
        elif line.startswith('+mem'):
            toks = line.split()
            if len(toks) != 6:
                print_error('malformed line in loop static file line ' + str(i+1))
            block_mem[current_block] = [int(toks[1]), int(toks[2]), float(toks[3])]
            bbhash = int(toks[5], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+mem) line ' + str(i+1))
        elif line.startswith('+lpc'):
            toks = line.split()
            if len(toks) != 5:
                print_error('malformed line in loop static file line ' + str(i+1))
            block_lpc[current_block] = [int(toks[1]), int(toks[2])]
            bbhash = int(toks[4], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+lpc) line ' + str(i+1))
        elif line.startswith('+dxi'):
            toks = line.split()
            if len(toks) != 5:
                print_error('malformed line in loop static file line ' + str(i+1))
            block_dxi[current_block] = [int(toks[1]), int(toks[2])]
            bbhash = int(toks[4], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+dxi) line ' + str(i+1))
        elif line.startswith('+bin'):
            toks = line.split()
            if len(toks) != 30:
                print_error('malformed line in loop static file line ' + str(i+1))
            block_bin[current_block] = [int(toks[1]), int(toks[2]), int(toks[3]), int(toks[4]), int(toks[5]), int(toks[6]), int(toks[7]), int(toks[8]), int(toks[9]), int(toks[10]), int(toks[11]), int(toks[12]), int(toks[13]), int(toks[14]), int(toks[15]), int(toks[16]), int(toks[17]), int(toks[18]), int(toks[19]), int(toks[20]), int(toks[21]), int(toks[22]), int(toks[23]), int(toks[24]), int(toks[25]), int(toks[26]), int(toks[27])]
            bbhash = int(toks[29], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+bin) line ' + str(i+1))
        elif line.startswith('+dud'):
            toks = line.split()
            rawt = []
            for t in toks:
                duds = t.split(':')
                if len(duds) == 3:
                    rawt.append(t)
            dud = []
            for r in rawt:
                dud.append(r)
            block_dud[current_block] = dud
            bbhash = int(toks[len(toks)-1], 16)
            if bbhash != current_block:
                print_error('block hashcode mismatch on (+dud) line ' + str(i+1))
        else:
            toks = line.split()
            if len(toks) != 10:
                print_error('malformed line in loop static file line ' + str(i+1))
            li = toks[5].split(':')
            current_block = int(toks[1])
            block_static[current_block] = [int(toks[0]), int(toks[2]), int(toks[3]), int(toks[4]), li[0], int(li[1]), toks[6], toks[9]]
            bbid_map[int(toks[0])] = current_block
    else:
        print line

# block_static  [bbhash]            = [bbid, memop, fpop, insn, filename, lineno, funcname, vaddr]
# block_lpi     [bbhash]            = [loopcnt, loopid, loopdepth, looploc]
# block_cnt     [bbhash]            = []
# block_mem     [bbhash]            = []
# block_lpc     [bbhash]            = [loop_head, parent_loop_head]
# block_dud     [bbhash]            = [ def_use_dist:num_int:num_fp ... ]
line_info = '#<block_unqid> <sequence> <memop> <fpop> <insn> <flname> <line> <fncame> <vaddr> <loopcnt> <loopid> <ldepth> <lploc> <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op> <total_mem_op> <total_mem_bytes> <bytes/op> <loop_head> <parent_loop_head> <dudist1>:<duint1>:<dufp1> <dudist2>:<ducnt2>:<dufp2>...'
print line_info

bbs = bbid_map.keys()
bbs.sort()
for bbid in bbs:
    current_block = bbid_map[bbid]
    print str(current_block) + " " + stringify(block_static[current_block]) + " " + stringify(block_lpi[current_block]) + " " + stringify(block_cnt[current_block]) + " " + stringify(block_mem[current_block]) + " " + stringify(block_lpc[current_block]) + " " + stringify(block_dud[current_block])

print line_info
