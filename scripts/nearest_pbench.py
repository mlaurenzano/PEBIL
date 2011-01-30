#!/usr/bin/env python

import getopt
import string
import sys
import math

geo_weights = [1, 4, 4, 4, 4, 1]
freq_names = ['1596000', '1729000', '1862000', '1995000', '2128000', '2261000', '2394000', '2395000']
default_freq = '2395000'
energy_penalty = 0.00
cache_line_size = 8
exec_min = 5000000

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(-1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        [--mloop <mloop_file>] uses stdin if not present"
    print "        --pbench <list:of:pbench:outputs>"
    print "        --inrlp <pbench.inrlp>"
    print_error(err)

def stringify(arr):
    strver = [str(i) for i in arr]
    return string.join(strver, ' ')

def get_outer_loop_head(bbhash, block_info):
    while bbhash != 0 and bbhash != int(block_info[bbhash][29]):
        bbhash = int(block_info[bbhash][29])
    return bbhash

def normalize_inp(inp):
    L1hr = float(inp[0])
    L2hr = float(inp[1])
    L3hr = float(inp[2])
    fpratio = float(inp[3])
    avgfpDU = float(inp[4])
    avgintDU = float(inp[5])

    n_fpratio = fpratio

    return [L1hr, L2hr, L3hr, n_fpratio, avgfpDU, avgintDU]

def get_mean_inps(all_inps):
    tots = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    for k in all_inps.keys():
        for i in range(0,len(tots),1):
            tots[i] += all_inps[k][i]
    
    for i in range(0,len(tots),1):
        tots[i] /= len(all_inps.keys())

    return tots

def get_stdev_inps(all_inps):
    means = get_mean_inps(all_inps)
    tots = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
    for k in all_inps.keys():
        for i in range(0,len(tots),1):
            tots[i] += math.pow(all_inps[k][i]-means[i],2)
    
    for i in range(0,len(tots),1):
        tots[i] /= len(all_inps.keys())

    for i in range(0,len(tots),1):
        tots[i] = math.sqrt(tots[i])
    return tots


def whitewash_inp(inp, means, stdevs):
    if len(inp) != len(means):
        print_error('whitewash requires 3 same-size arrays...')
    if len(stdevs) != len(means):
        print_error('whitewash requires 3 same-size arrays...')

    washed = []
    [washed.append(x) for x in inp]

#    for i in range(0,len(washed),1):
#        washed[i] -= means[i]
#        washed[i] /= stdevs[i]
    return washed

def dist_geometric(inp1, inp2):
    if len(inp1) != len(inp2):
        print_error('cannot compute geometric distance on 2 diff length arrays')
#    print inp1
#    print inp2
    sum = 0.0
    for i in range(0,len(inp1),1):
        sum += math.pow(geo_weights[i]*(inp1[i]-inp2[i]),2)
    sum = math.sqrt(sum)
#    print sum
    return sum

## set up command line args                                                                                                                                        
try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['mloop=', 'pbench=', 'inrlp='])
except getopt.GetoptError, err:
    print_usage(err)
    sys.exit(-1)

if len(args) > 0:
    print_usage('extra arguments are invalid ' + str(args))
    sys.exit(-1)

pbench = ''
mloop = ''
inrlp = ''
for i in range(0,len(optlist),1):
    if optlist[i][0] == '--mloop':
        mloop = optlist[i][1]
    elif optlist[i][0] == '--pbench':
        pbench = optlist[i][1]
    elif optlist[i][0] == '--inrlp':
        inrlp = optlist[i][1]
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
lineno = 1
for line in inpdata:
    if not line.startswith('#'):
        toks = line.split()
        if not len(toks) == 27:
            print toks
            print_error('input file ' + str(mloop) + ' incorrect token count on line ' + str(lineno) + ': ' + str(len(toks)))
        mloop_data.append(toks)
    lineno += 1

if inrlp == '':
    print_usage('missing required argument inrlp')

ifile = open(inrlp, 'r')
pdata = ifile.readlines()
ifile.close()

pbench_static = {}
lineno = 1
for line in pdata:
    if not line.startswith('#'):
        toks = line.split()
        if len(toks) != 16:
            print_error('input file ' + str(inrlp) + ' incorrect token count on line ' + str(lineno))
        test_name = string.join([toks[0], toks[1], toks[2], toks[3], toks[4], toks[5], toks[6], toks[7]], '_')

        hits = 1.0
        misses = 1.0
        if int(toks[2]) == 1:
            misses = 0.875
        elif int(toks[2]) == 2:
            misses = 0.75
        elif int(toks[2]) == 4:
            misses = 0.50
        elif int(toks[2]) == 8:
            misses = 0.00
        else:
            print_error('unknown stride length (' + toks[2] + ') on line ' + str(lineno))        

        L1hr = 0.0
        L2hr = 0.0
        L3hr = 0.0
        if int(toks[3]) == 16384:
            L1hr = hits
            L2hr = hits
            L3hr = hits
        elif int(toks[3]) == 131072:
            L1hr = misses
            L2hr = hits
            L3hr = hits
        elif int(toks[3]) == 1048576:
            L1hr = misses
            L2hr = misses
            L3hr = hits
        elif int(toks[3]) == 16777216:
            L1hr = misses
            L2hr = misses
            L3hr = misses
        else:
            print_error('unknown array size (' + toks[3] + ') on line ' + str(lineno))
            
        fpratio = float(toks[10]) / float(toks[9])
        avgintDU = 0.0
        if not float(toks[14]) == 0.0:
            avgintDU = (float(toks[15]) / float(toks[14])) / float(toks[11])
        avgfpDU = 0.0
        if not float(toks[12]) == 0.0:
            avgfpDU = (float(toks[13]) / float(toks[12])) / float(toks[11])
        pbench_static[test_name] = [round(L1hr,4), round(L2hr,4), round(L3hr,4), round(fpratio,4), round(avgintDU,4), round(avgfpDU,4), 0.0]

    lineno += 1

#print pbench_static

# "whitewash" inputs
pbench_normals = {}
for p in pbench_static.keys():
    pbench_normals[p] = normalize_inp(pbench_static[p])
#print pbench_normals

pbench_means = get_mean_inps(pbench_normals)
#print pbench_means
pbench_stdevs = get_stdev_inps(pbench_normals)
#print pbench_stdevs
pbench_washed = {}
for p in pbench_normals.keys():
    pbench_washed[p] = whitewash_inp(pbench_normals[p], pbench_means, pbench_stdevs)
#print pbench_washed

if pbench == '':
    print_usage('missing required argument pbench')

pbench_results = {}
pfreqs = pbench.split(':')
lineno = 1
testno = 0
for p in pfreqs:
    pf = open(p, 'r')
    raw = pf.readlines()
    pf.close()

    for r in raw:
        if not r.startswith('#'):
            toks = r.split()
            if not len(toks) == 16:
                print toks
                print_error('input file ' + str(p) + ' incorrect token count on line ' + str(lineno) + ': ' + str(len(toks)))
            test_name = string.join(['ptest', toks[0], toks[1], toks[2], toks[4], toks[5], toks[6], toks[3]], '_')
            if not pbench_results.has_key(test_name):
                pbench_results[test_name] = {}
            pbench_results[test_name][freq_names[testno]] = [toks[9], toks[10], toks[11]]

    lineno += 1
    testno += 1

#print mloop_data

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

    plain_inp = [round(L1hr,4), round(L2hr,4), round(L3hr,4), round(fpratio,4), round(avgfpDU,2), round(avgintDU,2)]
    inpdata = whitewash_inp(normalize_inp(plain_inp), pbench_means, pbench_stdevs)

    mindist = 100000000
    minkey = ''
    for k in pbench_static.keys():
#        print 'GEODIST for ' + str(k)
        dist = dist_geometric(inpdata[:6], pbench_static[k][:6])
        pbench_static[k][6] = dist
        if dist < mindist:
            mindist = dist
            minkey = k

    ## here's where we make the decision about energy optimality
    base_time = float(pbench_results[minkey][default_freq][0])
    base_pwr = float(pbench_results[minkey][default_freq][1])
    base_energy = float(pbench_results[minkey][default_freq][2]) - energy_penalty
    best_freq = default_freq
    for freq in pbench_results[minkey].keys():
        if float(pbench_results[minkey][freq][2]) < float(pbench_results[minkey][best_freq][2]):
            best_freq = freq

    best_ratios = []
    for i in [0,1,2]:
        best_ratios.append(round(float(pbench_results[minkey][best_freq][i]) / float(pbench_results[minkey][default_freq][i]), 4))
    best_ratios.append(round((bbcnt * insnop) / tot_instructions, 4))

    if execlen >= exec_min:
        print string.join([str(mloop[7]), str(mloop[8]), str(freq_names.index(best_freq))], ':'),
        print '#' + stringify(mloop) + ' ' + str(plain_inp) + ' ' + str(minkey) + ' ' + str(pbench_static[minkey]) + ' ' + str([best_freq, freq_names.index(best_freq)]) + ' ' + stringify(best_ratios) + ' ' + str(inpdata) + '<==>' + str(pbench_washed[minkey]) + ' ' + str(execlen) + ' ' + str(lpcnt) + ' ',
        for i in range(0,len(freq_names),1):
            print str(pbench_results[minkey][freq_names[i]]) + ' ',
        print ''

        tot_exec += float(best_ratios[3])
        tot_energy += float(best_ratios[2]*best_ratios[3])
    else:
#        print '#' + string.join([str(mloop[7]), str(mloop[8]), str(freq_names.index(best_freq))], ':'),
#        print '#' + stringify(mloop) + ' ' + str(plain_inp) + ' ' + str(minkey) + ' ' + str(pbench_static[minkey]) + ' ' + str([best_freq, freq_names.index(best_freq)]) + ' ' + stringify(best_ratios) + ' ' + str(inpdata) + '<==>' + str(pbench_washed[minkey]) + ' ' + str(execlen)
#        for i in range(0,len(freq_names),1):
#            print str(pbench_results[minkey][freq_names[i]]) + ' ',
#        print ''
        tot_exec += float(best_ratios[3])
        tot_energy += float(best_ratios[3])
        

print '# estimated pfreq energy usage is ' + str(round((1.0-tot_exec)+tot_energy, 3))
