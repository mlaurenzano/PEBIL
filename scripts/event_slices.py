#!/usr/bin/env python

import getopt
import sys
import os
import struct
import string

# set to 0 for unlimited
sliceLimit = 0
debug = False
verbose = False

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

def print_verbose(s):
    if verbose == True:
        print s

def print_debug(s):
    if debug == True:
        print 'debug: ' + s

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        --counters <app_slices_file> [--verbose] [--measures <app_measurements_file> --static <static_file>]"
    print ""
    print "example: " + sys.argv[0] + " --counters cg.B.4.slices_0000.step2 --verbose --measures raw_pmon.master --static NPB3.3-MPI/bin/cg.B.4.step2.static"
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

def merge_list_ranges(lst):
    prev = -1
    sub = []
    merged = []
    for item in lst:
        if item-prev == 1 or prev == -1:
            #print 'match ' + str(item)
            # does nothing
            prev = item
        else:
            #print 'no match ' + str(item)
            if len(sub) == 1:
                merged.append(str(sub[0]))
            else:
                merged.append(str(sub[0]) + '-' + str(sub[len(sub)-1]))
            sub = []
        sub.append(item)
        prev = item

    if len(sub) == 1:
        merged.append(str(sub[0]))
    elif len(sub) > 0:
        merged.append(str(sub[0]) + '-' + str(sub[len(sub)-1]))

    return merged

def main():
    global verbose

    try:
        optlist, args = getopt.getopt(sys.argv[1:], '', ['counters=', 'verbose', 'measures=', 'static='])
    except getopt.GetoptError, err:
        print_usage(err)
        sys.exit(1)

    if len(args) > 0:
        print_usage('extra arguments are invalid: ' + str(args))
        sys.exit(1)

    counterfile = ''
    measurefile = ''
    staticfile = ''
    for i in range(0,len(optlist),1):
        if optlist[i][0] == '--counters':
            counterfile = optlist[i][1]
        if optlist[i][0] == '--verbose':
            verbose = True
        if optlist[i][0] == '--measures':
            measurefile = optlist[i][1]
        if optlist[i][0] == '--static':
            staticfile = optlist[i][1]

    if counterfile == '':
        print_usage('missing switch --ctrs')

    if file_exists(counterfile) == False:
        print_error('input file ' + counterfile + ' does not exist')

    do_measure = False
    if measurefile != '' or staticfile != '':
        if not (measurefile != '' and staticfile != ''):
            print_error('--measures and --static must be used simultaneously')
        if file_exists(measurefile) == False:
            print_error('input file ' + measurefile + ' does not exist')
        if file_exists(staticfile) == False:
            print_error('input file ' + staticfile + ' does not exist')
        do_measure = True

    f = open(counterfile, "rb")
    numslices = 0
    diffs = 0
    diff_list = []

    slices = []

    try:
        # read header
        magic = read_unpack(f, uint32)
        print_debug('magic number: ' + str(magic))

        counters = read_unpack(f, uint32)
        print_debug('number of counters: ' + str(counters))

        for i in range(8,32):
            read_unpack(f, uint8)

        prev_counters = []

        while True:
            counter_counts = [read_unpack(f, uint64) for x in range(0,counters,1)]

            counter_diff = diff_counts(prev_counters, counter_counts)

            if counter_diff != 0:
                diffs += 1
            else:
                diff_list.append(numslices)

            if len(prev_counters) == 0:
                slices.append(counter_counts)
            else:
                slices.append([counter_counts[i] - prev_counters[i] for i in range(len(counter_counts))])
            print_verbose('1 ' + string.join([str(x) for x in slices[len(slices)-1]],' '))

            prev_counters = counter_counts
            numslices += 1

    # not really an error... this is kind of a sloppy way of breaking out of the file read loop
    except EOFError:
        f.close()
        print_debug('found ' + str(numslices) + ' slices of size ' + str(len(slices[0])) + ', ' + str(diffs) + ' are different than prev')
        print_debug(merge_list_ranges(diff_list))

    if do_measure == False:
        return

    assert(measurefile != '')
    assert(staticfile != '')

    measures = []
    f = open(measurefile, 'r')
    raw = f.readlines()
    measures = [line.strip().split() for line in raw]

    statics = []
    f = open(staticfile, 'r')
    raw = f.readlines()
    for line in raw:
        line = line.strip().split()
        if len(line) > 0:
            if not (line[0].startswith('#') or line[0].startswith('+')):
                assert(int(line[0]) == len(statics))
                try:
                    hashcode = int(line[1])
                    statics.append(hashcode)
                except ValueError, e:
                    print_error('parser error in static file: ' + line)

    #print statics
    assert(len(statics) == len(slices[0]))

if __name__ == '__main__':
    main()

