#!/usr/bin/env python

# a script that reads in a set of loopcnt + siminst traces and attempts to produce a list of
# loops that should be acceptable for frequency throttling
# ./loopBlockSimu.py --application cg.C.8 --cpu_count 8 --trace_dir ~/NPB3.3/NPB3.3-MPI/run/ --system_id 77
#

import getopt
import string
import sys

block_print_threshold = 1000000
block_exec_threshold   = 3000000
L1hr_threshold       = 0.97
L2hr_threshold       = 0.75

def print_usage(err):
    print 'Error: ' + str(err)
    print "usage : " + sys.argv[0]
    print "        --application <application>"
    print "        --cpu_count <cpu count>"
    print "        --trace_dir <trace dir>"
    print "        --system_id <sysid>"

def compute_hitrate(hitmiss):
    if hitmiss[0] == 0:
        return float(0.0)
    return float(hitmiss[0])/float(hitmiss[0]+hitmiss[1])

def get_outer_loop_head(bbhash, block_lpc):
    while bbhash != 0 and bbhash != block_lpc[bbhash][1]:
        bbhash = block_lpc[bbhash][1]
    return bbhash

def remove_hash_keys(key_list, hash_table):
    for k in key_list:
        hash_table.pop(k)

def total_access(list):
    t = 0
    for e in list:
        t += e
    return t

## set up command line args                                                                                                                                        
try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['application=', 'cpu_count=', 'trace_dir=', 'system_id=', 'stdout'])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

application = ''
cpu_count = ''
trace_dir = ''
system_id = ''
stdout = 0

for i in range(0,len(optlist),1):
    if optlist[i][0] == '--cpu_count':
        try:
            cpu_count = int(optlist[i][1])
        except ValueError, err:
            print_usage(err)
            sys.exit(-1)
    elif optlist[i][0] == '--system_id':
        try:
            system_id = int(optlist[i][1])
        except ValueError, err:
            print_usage(err)
            sys.exit(-1)
    elif optlist[i][0] == '--stdout':
        stdout = 1
    elif optlist[i][0] == '--application':
        application = optlist[i][1]
    elif optlist[i][0] == '--trace_dir':
        trace_dir = optlist[i][1]
    else:
        print_usage('unknown argument ' + str(optlist[i][0]))
        sys.exit(-1)

if application == '':
    print_usage('--application required')
    sys.exit(-1)
if cpu_count == '':
    print_usage('--cpu_count required')
    sys.exit(-1)
if trace_dir == '':
    print_usage('--trace_dir required')
    sys.exit(-1)
if system_id == '':
    print_usage('--system_id required')
    sys.exit(-1)

if stdout == 1:
    outf = sys.stdout
else:
    outf = open(trace_dir + '/%(application)s_%(cpu_count)04d_sysid%(system_id)d.loops' % { 'application':application, 'cpu_count':cpu_count, 'system_id':system_id }, 'w')
# read static data
lsf = open(trace_dir + '/%(application)s.jbbinst.loop.static' % { 'application':application })
lsraw = lsf.readlines()
lsf.close()

bbid_map = {}
current_block = -1
block_static = {}
block_lpi = {}
block_cnt = {}
block_mem = {}
block_lpc = {}
for i in range(0,len(lsraw),1):
    line = lsraw[i].strip()
    if not line.startswith('#'):
        if line.startswith('+lpi'):
            toks = line.split()
            if len(toks) != 6:
                print_error('malformed line in loop static file line ' + str(i))
            block_lpi[current_block] = [int(toks[1]), int(toks[2]), int(toks[3])]
        elif line.startswith('+cnt'):
            toks = line.split()
            if len(toks) != 13:
                print_error('malformed line in loop static file line ' + str(i))
        elif line.startswith('+mem'):
            toks = line.split()
            if len(toks) != 6:
                print_error('malformed line in loop static file line ' + str(i))
        elif line.startswith('+lpc'):
            toks = line.split()
            if len(toks) != 3:
                print_error('malformed line in loop static file line ' + str(i))
            block_lpc[current_block] = [int(toks[1]), int(toks[2])]
        else:
            toks = line.split()
            if len(toks) != 10:
                print_error('malformed line in loop static file line ' + str(i))
            li = toks[5].split(':')
            current_block = int(toks[1])
            block_static[current_block] = [int(toks[0]), int(toks[2]), int(toks[3]), int(toks[4]), li[0], int(li[1]), toks[6], toks[9]]
            bbid_map[int(toks[0])] = current_block

#print str(len(block_lpc.keys()))
#for k in block_static.keys():
#    print str(k) + ' ==> ' + str(block_lpc[k])
#    print block_static[k]
#    print block_lpi[k]

percpu_sim_data = []
percpu_loop_data = []
percpu_jbb_data = []
for i in range(0,cpu_count,1):
    sf = open(trace_dir + '/%(application)s.meta_%(cpu)04d.siminst' % { 'application':application, 'cpu':i })
    sraw = sf.readlines()
    sf.close()

    current_block = -1
    block_total = 0
    block_sim_meta = {}
    block_hrs = {}
    for j in range(0,len(sraw),1):
        line = sraw[j].split()
        if len(line) == 5:
            if line[0].startswith('block'):
                current_block = bbid_map[int(line[1])]
                block_sim_meta[current_block] = [int(line[1]), int(line[2]), int(line[3]), int(line[4])]
                block_total += int(line[2])
                block_hrs[current_block] = {}
        if len(line) == 7:
            if line[0].startswith('sys'):
                sys = int(line[1])
                lvl = int(line[3])
                hit = int(line[4])
                miss = int(line[5])
                if not block_hrs[current_block].has_key(sys):
                    block_hrs[current_block][sys] = {}
                block_hrs[current_block][sys][lvl] = [hit, miss]

    percpu_sim_data.append([block_total, block_sim_meta, block_hrs])

    lf = open(trace_dir + '/%(application)s.meta_%(cpu)04d.loopcnt' % { 'application':application, 'cpu':i })
    lraw = lf.readlines()
    lf.close()

    block_loop_meta = {}
    for j in range(0,len(lraw),1):
        line = lraw[j].strip()
        if not line.startswith('#'):
            toks = line.split()
            if len(toks) != 5:
                print_error('malformed line in loop meta file task ' + str(i) + ' line ' + str(j))
            block_loop_meta[int(toks[4])] = [int(toks[0]), int(toks[1])]
    percpu_loop_data.append(block_loop_meta)

    jf = open(trace_dir + '/%(application)s.meta_%(cpu)04d.jbbinst' % { 'application':application, 'cpu':i })
    jraw = jf.readlines()
    jf.close()

    block_jbb_meta = {}
    for j in range(0,len(jraw),1):
        line = jraw[j].strip()
        if not line.startswith('#'):
            toks = line.split()
            if len(toks) != 5:
                print_error('malformed line in jbb meta file task ' + str(i) + ' line ' + str(j))
            block_jbb_meta[int(toks[4])] = [int(toks[0]), int(toks[1])]
    percpu_jbb_data.append(block_jbb_meta)

for i in range(0,cpu_count,1):
    outf.write('#application:' + application + ' sysid:' + str(system_id) + ' cpu:' + str(i) + ' bbcount:' + str(block_total))
    outf.write(' bbprintmin:' + str(block_print_threshold) + ' bbexecmin:' + str(block_exec_threshold))
    outf.write(' L1hrmax:' + str(L1hr_threshold) + ' L2hrmax:' + str(L2hr_threshold) + '\n')
    [block_total, block_sim_meta, block_hrs] = percpu_sim_data[i]
    block_loop_meta = percpu_loop_data[i]
    block_jbb_meta = percpu_jbb_data[i]
#    print block_jbb_meta

    while len(block_jbb_meta.keys()):
#    for k in block_sim_meta.keys():
        mostfreq_block = block_jbb_meta.keys()[0]
        curr_loop = get_outer_loop_head(block_lpc[mostfreq_block][0], block_lpc)
        blocks_in_loop = []

        current_max = 0
        for k in block_jbb_meta.keys():
            if get_outer_loop_head(block_lpc[k][0], block_lpc) == curr_loop:
                blocks_in_loop.append(k)
                if block_jbb_meta[k][1] > current_max:
                    current_max = block_jbb_meta[k][1]
                    mostfreq_block = k
                
#        print mostfreq_block
#        print blocks_in_loop
        if curr_loop == 0:
            remove_hash_keys(blocks_in_loop, block_jbb_meta)
            continue

        outer_entr = block_loop_meta[curr_loop][1]
        loop_bb_exec = 0
        loop_hr = {1:[0,0], 2:[0,0], 3:[0,0]}
        for bbhash in blocks_in_loop:
            if block_jbb_meta.has_key(bbhash):
                loop_bb_exec += block_jbb_meta[bbhash][1]
            if block_sim_meta.has_key(bbhash):
                loop_hr[1][0] += block_hrs[bbhash][system_id][1][0]
                loop_hr[1][1] += block_hrs[bbhash][system_id][1][1]
                loop_hr[2][0] += block_hrs[bbhash][system_id][2][0]
                loop_hr[2][1] += block_hrs[bbhash][system_id][2][1]
                if block_hrs[bbhash][system_id].has_key(3):
                    loop_hr[3][0] += block_hrs[bbhash][system_id][3][0]
                    loop_hr[3][1] += block_hrs[bbhash][system_id][3][1]
#        print loop_bb_exec
#        print loop_hr

        remove_hash_keys(blocks_in_loop, block_jbb_meta)

        exec_per_entr = int(loop_bb_exec/outer_entr)
        if loop_hr[1][0] > 0 and exec_per_entr >= block_print_threshold:
            L1hr = round(compute_hitrate(loop_hr[1]),4)
            L2hr = round(compute_hitrate(loop_hr[2]),4)
            L3hr = round(compute_hitrate(loop_hr[3]),4)
            prefix = ''
            if L1hr > L1hr_threshold or L2hr > L2hr_threshold:
                prefix = '#'
            if exec_per_entr < block_exec_threshold:
                prefix = '#'

# block_static  [bbhash]            = [bbid, memop, fpop, insn, filename, lineno, funcname, vaddr]
# block_lpi     [bbhash]            = [loopcnt, loopid, loopdepth]
# block_cnt     [bbhash]            = []
# block_mem     [bbhash]            = []
# block_lpc     [bbhash]            = [loop_head, parent_loop_head]
# block_sim_meta[bbhash]            = [bbid, bbcount, bb_buffcnt, memop_buffcnt]
# block_loop_meta    [head_bbhash]       = [lpid, lpcount]
# block_hrs     [bb_hash][sys][lvl] = [hit, miss]
# block_jbb_meta[bbhash]            = [bbid, bbcount]
            k = mostfreq_block
            if block_sim_meta.has_key(mostfreq_block):
                outf.write(prefix + str(block_static[k][4]) + ':' + str(block_static[k][5]) + '\t')
                outf.write('#' + str(k) + '\t' + str(block_static[k][0]) + '\t' + str(loop_bb_exec) + '\t' + str(outer_entr) + '\t')
                outf.write(str(exec_per_entr) + '\t' + str(total_access(loop_hr[1])) + '(' + str(L1hr) + ')\t')
                outf.write(str(total_access(loop_hr[2])) + '(' + str(L2hr) + ')\t' + str(total_access(loop_hr[3])) + '(' + str(L3hr) + ')\t')
                outf.write(str(block_static[k][6]) + '\t' + str(block_static[k][7]) + '\n')
            else:
                sys.stderr.write('probable error??!? found a block ' + str(k) + ' that has a bunch of info but is not in simulation data\n')
                sys.stderr.write(str(exec_per_entr) + '\t' + str(L1hr) + '\t' + str(L2hr) + '\t' + str(L3hr) + '\t\n')
                sys.exit(-1)

if stdout != 1:
    outf.close()
