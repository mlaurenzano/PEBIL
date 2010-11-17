#!/usr/bin/env python

# example usage to read a trace file and sort the trace data by basic block execution count:
# ./readProcessedTrace.py --application 450.soplex_pds-50.mps --cpu_count 1 --cpu 0 --sysid 77 --level 3 --input_dir SPEC_CPU2006_trace/processed_trace/ | sort -n -k13
#
# output format is:
# hash_code dyn_fp_cnt dyn_mem_cnt L1hr L2hr [L3hr] func_name line_number static_ft_cnt static_mem_cnt avg_memop_size dynamic_bb_cnt

import getopt
import string
import sys

def print_usage(err):
    print 'Error: ' + str(err)
    print "usage : " + sys.argv[0];
    print "        --application <application>";
    print "        --cpu_count <cpu count>";
    print "        --sysid <sys id>";
    print "        --level <level count>";
    print "        --cpu <cpu>";
    print "        --input_dir <input dir>";

## set up command line args                                                                                                                                        
try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['application=', 'cpu_count=', 'sysid=', 'level=', 'cpu=', 'input_dir='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

application = ''
cpu_count = 0
sysid = 0
level = 0
input_dir = ''

for i in range(0,len(optlist),1):
    if optlist[i][0] == '--cpu_count':
        try:
            cpu_count = int(optlist[i][1])
        except ValueError, err:
            print_usage(err)
            sys.exit(-1)
    elif optlist[i][0] == '--sysid':
        try:
            sysid = int(optlist[i][1])
        except ValueError, err:
            print_usage(err)
            sys.exit(-1)
    elif optlist[i][0] == '--level':
        try:
            level = int(optlist[i][1])
        except ValueError, err:
            print_usage(err)
            sys.exit(-1)
    elif optlist[i][0] == '--cpu':
        try:
            cpu = int(optlist[i][1])
        except ValueError, err:
            print_usage(err)
            sys.exit(-1)
    elif optlist[i][0] == '--application':
        application = optlist[i][1]
    elif optlist[i][0] == '--input_dir':
        input_dir = optlist[i][1]
    else:
        print_usage('unknown argument ' + str(optlist[i][0]))

if application == '':
    print_usage('--application required')
    sys.exit(-1)
if cpu_count == 0:
    print_usage('--cpu_count required')
    sys.exit(-1)
if sysid == 0:
    print_usage('--sysid required')
    sys.exit(-1)
if level == 0:
    print_usage('--level required')
    sys.exit(-1)
if level < 2 or level > 3:
    print_usage('argument to --level should be 2 or 3')
    sys.exit(-1)
if input_dir == '':
    print_usage('--input_dir required')
    sys.exit(-1)

trace_filen = '%(input_dir)s/%(application)s_%(cpu_count)04d_%(cpu)04d.sysid%(sysid)d' % { 'input_dir':input_dir, 'application':application, 'cpu_count':cpu_count, 'cpu':cpu, 'sysid':sysid }
tracef = open(trace_filen, 'r')
trace_data = tracef.readlines()
tracef.close()

bb2func_filen = '%(input_dir)s/%(application)s_%(cpu_count)04d.bb2func' % { 'input_dir':input_dir, 'application':application, 'cpu_count':cpu_count }
bb2funcf = open(bb2func_filen, 'r')
bb2func_data = bb2funcf.readlines()
bb2funcf.close()

static_filen = '%(input_dir)s/%(application)s_%(cpu_count)04d.static' % { 'input_dir':input_dir, 'application':application, 'cpu_count':cpu_count }
staticf = open(static_filen, 'r')
static_data = staticf.readlines()
staticf.close()

bbbytes_filen = '%(input_dir)s/%(application)s_%(cpu_count)04d.bbbytes' % { 'input_dir':input_dir, 'application':application, 'cpu_count':cpu_count }
bbbytesf = open(bbbytes_filen, 'r')
bbbytes_data = bbbytesf.readlines()
bbbytesf.close()

# fill a dictionary with the trace data, keyed on bb hashcode
bb_info = {}
for i in range(0, len(trace_data), 1):
    toks = [d.strip() for d in trace_data[i].split()]
    if len(toks) != 3 + level:
        print_usage('invalid number of tokens on line ' + str(i) + ' of trace file ' + str(trace_filen))
    bb_info[toks[0]] = toks[1:]

# find the bb2func info for all of the blocks that appear in bb_info
for i in range(0, len(bb2func_data), 1):
    toks = [d.strip() for d in bb2func_data[i].split()]
    if len(toks) != 4:
        print_usage('invalid number of tokens on line ' + str(i) + ' of bb2func file ' + str(bb2func_filen))
    if bb_info.has_key(toks[0]):
        bb_info[toks[0]].append(toks[1])
        bb_info[toks[0]].append(toks[3])

# find the static info for all of the blocks that appear in bb_info
for i in range(0, len(static_data), 1):
    toks = [d.strip() for d in static_data[i].split()]
    if len(toks) != 5:
        print_usage('invalid number of tokens on line ' + str(i) + ' of static file ' + str(static_filen))
    if bb_info.has_key(toks[0]):
        bb_info[toks[0]].append(toks[1])
        bb_info[toks[0]].append(toks[2])
        bb_info[toks[0]].append(toks[3])

# find the bbbytes info for all of the blocks that appear in bb_info
for i in range(0, len(bbbytes_data), 1):
    toks = [d.strip() for d in bbbytes_data[i].split()]
    if len(toks) != 2:
        print_usage('invalid number of tokens on line ' + str(i) + ' of bbbytes file ' + str(bbbytes_filen))
    if bb_info.has_key(toks[0]):
        bb_info[toks[0]].append(toks[1])

# determine number of accesses to each block
for k in bb_info.keys():
    try:
        bb_info[k].append(str(int(bb_info[k][1]) / int(bb_info[k][6 + level])))
    except ValueError, err:
        print_usage(err)
        sys.exit(-1)

# print each combined block info
for k in bb_info.keys():
    print str(k) + ' ' + string.join(bb_info[k])
