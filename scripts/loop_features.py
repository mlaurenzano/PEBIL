#!/usr/bin/env python

import sys
mloop=sys.argv[1]

if mloop == '':
    file = sys.stdin
else:
    file = open(mloop, 'r')
inpdata = file.readlines()
file.close()
mloop_data = []
lineno = 1
for line in inpdata:
    if not line.startswith('#'):
        toks = line.split()
        if not len(toks) == 27:
            print >> sys.stderr, toks
            print >> sys.stderr, 'error: input file ' + str(mloop) + ' incorrect token count on line ' + str(lineno) + ': ' + str(len(toks))
            sys.exit(1)
        mloop_data.append(toks)
    lineno += 1


    
# 0 77 3659200468418560 71 1.832 0.306 4.71 cg.f 1530 sparse_ 0x401a84 9 6 1 1 1563446.836 254256.108 106543.441 147712.667 106548.479 41164.188 2.483 5.369 0.111 0.344 47012553
tot_instructions = 0.0
for mloop in mloop_data:
    bbcnt  = float(mloop[25])
    insnop = float(mloop[6])
    tot_instructions += (bbcnt * insnop)

#print str(tot_instructions)
tot_energy = 0.0
tot_exec = 0.0

for mloop in mloop_data:
    memop  = float(mloop[4])
    fpop   = float(mloop[5])
    insnop = float(mloop[6])
    L1hit  = float(mloop[15])
    L1miss = float(mloop[16])
    L2hit  = float(mloop[17])
    L2miss = float(mloop[18])
    L3hit  = float(mloop[19])
    L3miss = float(mloop[20])
    duintT = float(mloop[21])
    duintV = float(mloop[22])
    dufpT  = float(mloop[23])
    dufpV  = float(mloop[24])
    bbcnt  = float(mloop[25])
    lpcnt  = float(mloop[26])
    tot_instructions = (bbcnt * insnop)

    if lpcnt == 0.0:
        execlen = 0.0
    else:
        execlen = (bbcnt * insnop) / lpcnt

    if L1hit + L1miss == 0.0:
        L1hr = 0.0
        L2hr = 0.0
        L3hr = 0.0
    else:
        L1hr = (L1hit) / (L1hit + L1miss)
        L2hr = (L1hit + L2hit) / (L1hit + L1miss)
        L3hr = (L1hit + L2hit + L3hit) / (L1hit + L1miss)

    if memop == 0.0:
        fpratio = 16.0
    elif fpop == 0.0:
        fpratio = 1.0 / 16.0
    else:
        fpratio = fpop / memop

    if duintT == 0.0:
        avgintDU = 0.0
    else:
        avgintDU = (duintV / duintT) / insnop
    if dufpT == 0.0:
        avgfpDU = 0.0
    else:
        avgfpDU = (dufpV / dufpT) / insnop

    plain_inp = [round(L1hr,4), round(L2hr,4), round(L3hr,4), round(fpratio,4), round(avgfpDU,2), round(avgintDU,2), fpop, memop, insnop]
    print str(tot_instructions) + ' ' + mloop[2] + ' ' + mloop[9] + ' ' + mloop[7] + ' ' + mloop[8] + ' ' + str(plain_inp)
