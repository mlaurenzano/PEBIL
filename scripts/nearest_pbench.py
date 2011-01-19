#!/usr/bin/env python

import getopt
import string
import sys

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(-1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        [--mloop <mloop_file>] uses stdin if not present"
    print "        --pbench <list:of:pbench:outputs>"
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
    optlist, args = getopt.getopt(sys.argv[1:], '', ['mloop=', 'pbench='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

pbench=''
mloop=''
for i in range(0,len(optlist),1):
    if optlist[i][0] == '--mloop':
        mloop = optlist[i][1]
    elif optlist[i][0] == '--pbench':
        pbench = optlist[i][1]
    else:
        print_usage('unknown argument ' + str(optlist[i][0]))
        sys.exit(-1)

if mloop == '':
    file = sys.stdin
else:
    file = open(mloop, 'r')
inpdata = file.readlines()
file.close()
mloop_data = []
for line in inpdata:
    if not line.startswith('#'):
        mloop_data.append(line.split())

if pbench == '':
    print_usage('missing required argument pbench')

pbench_data = []
ptoks = pbench.split(':')
for p in ptoks:
    pf = open(p, 'r')
    raw = pf.readlines()
    pf.close()

    ndata = []
    for r in raw:
        if not r.startswith('#'):
            ndata.append(r.split())

    pbench_data.append(ndata)

print mloop_data

for mloop in mloop_data:
    toks = 
    
#print pbench_data
