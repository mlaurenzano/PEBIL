#!/usr/bin/env python

import getopt
import sys
import os
import struct

debug = False
#debug = True

uint64 = 'Q'
uint32 = 'I'
uint16 = 'H'
uint8 = 'B'
int64 = 'q'
int32 = 'i'
int16 = 'h'
int8 = 'b'

data_sizes = {}
data_sizes[uint64] = 8
data_sizes[int64] = 8
data_sizes[uint32] = 4
data_sizes[int32] = 4
data_sizes[uint16] = 2
data_sizes[int16] = 2
data_sizes[uint8] = 1
data_sizes[int8] = 1

def print_debug(s):
    if debug == True:
        print s

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        --file <counter_dump_file>"
    print_error(err)

def file_exists(filename):
    if os.path.isfile(filename):
        return True
    return False

def read_unpack(fp, fmt):
    ret = 0
    try:
        ret = struct.unpack(fmt, fp.read(data_sizes[fmt]))[0]
    except struct.error, e:
        raise EOFError
    return ret

def diff_counts(a, b):
    if len(a) != len(b):
        return -1
    for i in range(0,len(a),1):
        if a[i] != b[i]:
            return i+1
    return 0

try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['file='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

inputfile = ''
for i in range(0,len(optlist),1):
    if optlist[i][0] == '--file':
        inputfile = optlist[i][1]

if inputfile == '':
    print_usage('missing switch --file')

print "using input file " + inputfile

if file_exists(inputfile) == False:
    print_error('input file ' + inputfile + ' does not exist')

f = open(inputfile, "rb")
slices = 0
diffs = 0
diff_list = []
try:

    # read header
    magic = read_unpack(f, uint32)
    print_debug(magic)

    blocks = read_unpack(f, uint32)
    print_debug(blocks)

    loops = read_unpack(f, uint32)
    print_debug(loops)

    for i in range(12,32):
        read_unpack(f, uint8)

    prev_blocks = []
    prev_loops = []

    while True:
        block_counts = [read_unpack(f, uint64) for x in range(0,blocks,1)]
        loop_counts = [read_unpack(f, uint64) for x in range(0,loops,1)]

        print_debug(block_counts)
        print_debug(loop_counts)

        block_diff = diff_counts(prev_blocks, block_counts)
        loop_diff = diff_counts(prev_loops, loop_counts)

        if block_diff != 0:
            print_debug(block_diff)
        if loop_diff != 0:
            print_debug(loop_diff)

        if block_diff != 0 or loop_diff != 0:
            diffs += 1
        else:
            diff_list.append(slices)

        prev_blocks = block_counts
        prev_loops = loop_counts

        slices += 1

except EOFError:
    f.close()
    print 'found ' + str(slices) + ' slices, ' + str(diffs) + ' are different than prev'
    #print diff_list
