#!/usr/bin/env python

# a script that reads in a set of jbb/loop + siminst traces and static files
# and prints out single lines containing all relevant info
#

import getopt
import string
import sys
import os

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(-1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        --application <application>"
    print "        --cpu_count <cpu count>"
    print "        --jbb_trace_dir <trace dir>"
    print "        --sim_trace_dir <trace dir>"
    print "        --unified_static <unified_static_file>"
    print "        --system_id <sysid>"
    print_error(err)

def stringify(arr):
    strver = [str(i) for i in arr]
    return string.join(strver, ' ')

def get_outer_loop_head(bbhash, block_static):
    while bbhash != 0 and bbhash != int(block_static[bbhash][27]):
        bbhash = int(block_static[bbhash][27])
    return bbhash

def file_exists(filename):
    if os.path.isfile(filename):
        return true
    return false


## set up command line args                                                                                                                                        
try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['application=', 'cpu_count=', 'jbb_trace_dir=', 'sim_trace_dir=', 'system_id=', 'unified_static='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

application = ''
cpu_count = ''
jbb_trace_dir = ''
sim_trace_dir = ''
system_id = ''
unified_static = ''
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
    elif optlist[i][0] == '--application':
        application = optlist[i][1]
    elif optlist[i][0] == '--jbb_trace_dir':
        jbb_trace_dir = optlist[i][1]
    elif optlist[i][0] == '--sim_trace_dir':
        sim_trace_dir = optlist[i][1]
    elif optlist[i][0] == '--unified_static':
        unified_static = optlist[i][1]
    else:
        print_usage('unknown argument ' + str(optlist[i][0]))
        sys.exit(-1)

if application == '':
    print_usage('--application required')
    sys.exit(-1)
if cpu_count == '':
    print_usage('--cpu_count required')
    sys.exit(-1)
if jbb_trace_dir == '':
    print_usage('--jbb_trace_dir required')
    sys.exit(-1)
if sim_trace_dir == '':
    print_usage('--sim_trace_dir required')
    sys.exit(-1)
if system_id == '':
    print_usage('--system_id required')
    sys.exit(-1)
if unified_static == '':
    print_usage('--unified_static required')
    sys.exit(-1)

# if we used a file for output maybe we should call it this
#    outf = open(sim_trace_dir + '/%(application)s_%(cpu_count)04d_sysid%(system_id)d.loops' % { 'application':application, 'cpu_count':cpu_count, 'system_id':system_id }, 'w')

# read static data
lsf = open(unified_static)
lsraw = lsf.readlines()
lsf.close()


#<block_unqid> <sequence> <memop> <fpop> <insn> <flname> <line> <fncame> <vaddr> <loopcnt> <loopid> <ldepth> <lploc> <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op> <total_mem_op> <total_mem_bytes> <bytes/op> <loop_head> <parent_loop_head> <dudist1>:<duint1>:<dufp1> <dudist2>:<ducnt2>:<dufp2>...
bbid_map = {}
block_static = {}
for i in range(0,len(lsraw),1):
    line = lsraw[i].strip()
    if not line.startswith('#'):
        toks = line.split()
        if len(toks) < 28:
            print_error('malformed line in loop static file line ' + str(i))
        current_block = int(toks[0])
        block_static[current_block] = toks
        bbid_map[int(toks[1])] = current_block

#print bbid_map
#print block_static

percpu_sim_data = []
percpu_loop_data = []
percpu_jbb_data = []
for i in range(0,cpu_count,1):
    simfiles = [sim_trace_dir + '/%(application)s.meta_%(cpu)04d.siminst' % { 'application':application, 'cpu':i },
                sim_trace_dir + '/%(application)s.phase.1.meta_%(cpu)04d.%(cpustr)04d.siminst' % { 'application':application, 'cpu':i, 'cpustr':cpu_count }]
    sf = ''
    if file_exists(simfiles[0]):
        sf = open(simfiles[0])
    elif file_exists(simfiles[1]):
        sf = open(simfiles[1])
    else:
        print_error('could not find meta.siminst file: ' + simfiles[0] + ' or ' + simfiles[1])
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
                sysid = int(line[1])
                lvl = int(line[3])
                hit = int(line[4])
                miss = int(line[5])
                if not block_hrs[current_block].has_key(sysid):
                    block_hrs[current_block][sysid] = {}
                block_hrs[current_block][sysid][lvl] = [hit, miss]

    percpu_sim_data.append([block_total, block_sim_meta, block_hrs])

    lf = open(jbb_trace_dir + '/%(application)s.meta_%(cpu)04d.loopcnt' % { 'application':application, 'cpu':i })
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

    jf = open(jbb_trace_dir + '/%(application)s.meta_%(cpu)04d.jbbinst' % { 'application':application, 'cpu':i })
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


print '#<cpu> <sysid>',
print '<block_unqid> <sequence> <memop> <fpop> <insn> <flname> <line> <fncame> <vaddr> <loopcnt> <loopid> <ldepth> <lploc> <branch_op> <int_op> <logic_op> <shiftrotate_op> <trapsyscall_op> <specialreg_op> <other_op> <load_op> <store_op> <total_mem_op> <total_mem_op> <total_mem_bytes> <bytes/op> <loop_head> <parent_loop_head>',
print '<jbb_block_count>',
print '<loop_count_this> <loop_count_outmost>',
print '<sim_block_count> <visit_count> <sample_count>',
print '<L1hit> <L1miss> <L2hit> <L2miss> <L3hit> <L3miss>',
print '<dudist1>:<duint1>:<dufp1> <dudist2>:<ducnt2>:<dufp2>...'

bbids = bbid_map.keys()
bbids.sort()
for bb in bbids:
    current_block = bbid_map[bb]
    for i in range(0,cpu_count,1):
        ## static block info, minus def-use data
        print str(i) + ' ' + str(system_id) + ' ',
        print stringify(block_static[current_block][:28]) + ' ',

        ## dynamic jbbinfo info
        jbb_data = ''
        if percpu_jbb_data[i].has_key(current_block):
            jbb_data = percpu_jbb_data[i][current_block][1:]
        else:
            jbb_data = [0]
        if len(jbb_data) != 1:
            print_error('ill formed jbb data')
        print stringify(jbb_data) + ' ',

        ## dynamic loopcnt info
        loop_data = ''
        if percpu_loop_data[i].has_key(current_block):
            loop_data = percpu_loop_data[i][current_block][1:]
        else:
            loop_data = [0]
        if len(loop_data) != 1:
            print_error('ill formed loop data')
        print stringify(loop_data) + ' ',

        loop_data = ''
        loop_head = get_outer_loop_head(current_block, block_static)
        if loop_head != 0 and percpu_loop_data[i].has_key(loop_head):
            loop_data = [percpu_loop_data[i][loop_head][1]]
        else:
            loop_data = [-1]
        if len(loop_data) != 1:
            print_error('ill formed outer loop data')
        print stringify(loop_data) + ' ',
        

        ## dynamic sim info (block specific)
        sim_data = ''
        if percpu_sim_data[i][1].has_key(current_block):
            sim_data = percpu_sim_data[i][1][current_block][1:]
        else:
            sim_data = [0, 0, 0]
        if len(sim_data) != 3:
            print_error('ill formed sim data')
        print stringify(sim_data) + ' ',

        ## dynamic sim info (sysid+block specific)
        sys_data = ''
        if percpu_sim_data[i][2].has_key(current_block):
            sys_data = []
            if not percpu_sim_data[i][2][current_block].has_key(sysid):
                print_error('block ' + str(bb) + ' is missing simulation data for sysid' + str(system_id))
            cache_data = percpu_sim_data[i][2][current_block][system_id]
            for i in [1,2]:
                sys_data.append(cache_data[i][0])
                sys_data.append(cache_data[i][1])
            if cache_data.has_key(3):
                sys_data.append(cache_data[3][0])
                sys_data.append(cache_data[3][1])
            else:
                sys_data.append(-1)
                sys_data.append(-1)
        else:
            sys_data = [0, 0, 0, 0, 0, 0]
        if len(sys_data) != 6:
            print_error('ill formed sys data')
        print stringify(sys_data) + ' ',

        ## def-use info
        print stringify(block_static[current_block][28:]) + ' '
