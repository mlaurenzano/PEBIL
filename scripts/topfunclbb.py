#!/usr/bin/env python

ext = ".topfunc"

import os
import sys

def print_usage(msg, args):
    print msg
    print "usage: " + args[0] + " <top#> <lbbfile>"

if len(sys.argv) != 3:
    print_usage("fatal: incorrect number of args", sys.argv)
    sys.exit(-1)

if not os.path.isfile(sys.argv[2]):
    print_usage("fatal: file " + sys.argv[2] + " does not exist", sys.argv)
    sys.exit(-1)

try:
    num_to_print = int(sys.argv[1])
except ValueError:
    print_usage("fatal: argument 1 should be an int", sys.argv)
    sys.exit(-1)

outfile = sys.argv[2] + ext
if os.path.isfile(outfile):
    print_usage("fatal: output file " + str(outfile) + " already exists", sys.argv)
    sys.exit(-1)

file = open(sys.argv[2], 'r')
data = file.readlines()
file.close()

counts = {}
funcs = {}

lineno = 1
total = 0
for line in data:
    # ignore commented lines
    if not line.startswith('#'):
        line.strip()
        toks = line.split()
        if not len(toks) == 10:
            print_usage("fatal: line " + str(lineno) + " of " + str(sys.argv[1]) + " is incorrect", sys.argv)
            sys.exit(-1)

        # if key doesn't exist yet, insert it
        if not counts.has_key(toks[7]):
            counts[toks[7]] = 0
            funcs[toks[7]] = []

        # add current block's count to function total
        counts[toks[7]] = counts[toks[7]] + int(toks[2])
        funcs[toks[7]].append(toks[0])
        total = total + int(toks[2])

values = []
for key in counts.keys():
    values.append([counts[key], key, funcs[key]])

values.sort()
values.reverse()

sys.stderr.write(str(sys.argv[2]) + " has " + str(total) + " block executions\n")
sys.stderr.write("printing blocks from top " + str(num_to_print) + " blocks\n")

top = 0
for v in values:
    if top < num_to_print:
        sys.stderr.write("\t" + str(float(float(100*v[0])/float(total))) + "\t" + str(v[1]) + "\n")
    top = top + 1

output = open(sys.argv[2] + ext, 'w')
sys.stderr.write("writing output file " + str(outfile))

top = 0
for v in values:
    if top < num_to_print:
        for block in v[2]:
            output.write(str(block) + "\tdfTypePattern_Gather\n")
    top = top + 1

output.close()
sys.stderr.write("\n\n\t**** SUCCESS **** SUCCESS **** SUCCESS ****\n")
