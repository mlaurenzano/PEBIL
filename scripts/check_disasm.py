#!/usr/bin/env python

import getopt
import os
import string
import shlex
import subprocess
import sys
import tempfile

check_details = False

def file_exists(filename):
    if os.path.isfile(filename):
        return True
    return False

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        --file <app> [--mode (32|64)]"
    print_error(err)

def run_shell_cmd(textcmd):
    cmd = shlex.split(textcmd)
    sout = tempfile.TemporaryFile()
    p = subprocess.Popen(cmd, stdout=sout, stderr=sout, close_fds=True)
    p.wait()

    if p.returncode != 0:
        print_usage("objdump failed on " + filename + ", it is probably not a valid binary")

    sout.seek(0)
    rawoutput = sout.readlines()
    sout.close()
    return rawoutput

def get_objd_symbol(line):
    toks = line.split('\t')
    if len(toks) != 1:
        return False
    if toks[0].find('File Offset:') < 0:
        return False
    l = toks[0].replace('(','').replace(')','').replace(':','')
    tks = l.split()
    if len(tks) != 5:
        return False
    if (not tks[1].startswith('<')) or (not tks[1].endswith('>')):
        return False
    s = {}
    try:
        s['addr'] = int(tks[0], 16)
        s['offset'] = int(tks[4], 16)
        s['name'] = tks[1][1:len(tks[1])-1]
        s['content'] = []
    except ValueError, e:
        print e
        sys.exit(1)
    return s

def get_objd_instruction(line):
    toks = line.split('\t')
    if len(toks) < 3:
        return False
    if not toks[0].endswith(':'):
        return False
    i = {}
    try:
        addr = int(toks[0][0:len(toks[0])-1], 16)
        i['addr'] = addr
    except ValueError, e:
        print e
        sys.exit(1)
    i['outp'] = string.join(toks[2].strip().split(), ' ')
    i['size'] = len(toks[1].split())
    i['bytes'] = toks[1].strip().replace(' ', '')
    i['source'] = 'objdump'
    return i


def get_exec_instructions(filename):
    rawdump = run_shell_cmd('objdump -d -F --insn-width=24 ' + filename)

    text = {}
    insymbol = False
    for line in rawdump:
        line = line.strip()
        sym = get_objd_symbol(line)
        if sym != False:
            text[sym['name']] = sym
            insymbol = sym['name']
            continue

        ins = get_objd_instruction(line)
        if ins != False:
            text[insymbol]['content'].append(ins)

    return text

def get_udis_instruction(line):
    toks = line.strip().split()
    i = {}
    try:
        i['addr'] = int(toks[0], 16)
        i['bytes'] = toks[1]
        i['size'] = len(toks[1])/2
        i['outp'] = string.join(toks[2:len(toks)],' ').replace(', ', ',')
        i['source'] = 'udis86'
    except ValueError, e:
        print e
        sys.exit(1)
    return i

def get_udis_disasm(mode, filename, offset, size, addr):
    udis = run_shell_cmd('udcli -%d -att -o %x -s %d -c %d %s' % (mode, addr, offset, size, filename))
    if len(udis) != 1:
        print udis
        print_error("error: udis command returned more output than expected")
    return get_udis_instruction(udis[0])

#def get_udis_disasm(mode, filename, size, num):
#    udis = run_shell_cmd('udcli -%d -att -o %x -s %d -c %d %s' % (mode, addr, offset, size, filename))
#    if len(udis) != len(checks):
#        print udis
#        print_error("error: udis command returned more output than expected")
#    return [get_udis_instruction(u) for u in udis]

def int_match(c1, c2):
    try:
        int(c1, 16)
        int(c2, 16)
    except ValueError, e:
        return False

    s1 = int(c1, 16)
    s2 = int(c2, 16)
    #print s1, s2
    if s1 == s2:
        return True

    return False

def compare_instructions(i1, i2):
    errcnt = 0
    if (i1['addr'] != i2['addr']) or (i1['size'] != i2['size']) or (i1['bytes'] != i2['bytes']):
        errcnt += 1

    if check_details:
        t1 = i1['outp'].split(' ')
        t2 = i2['outp'].split(' ')
    #print t1, t2
        
        m1 = t1[0]
        m2 = t2[0]
        if (m1 != m2):
            errcnt += 1
    
        if (len(t1) > 1 and len(t2) > 1):
            o1 = t1[1].replace('*','').replace('$','').replace('0x','').replace('(',',').replace(')',',').split(',')
            o2 = t2[1].replace('*','').replace('$','').replace('0x','').replace('(',',').replace(')',',').split(',')
            df = 0
        if (len(o1) == len(o2)):
            for i in range(len(o1)):
                match = False
                if (o1[i] == o2[i]):
                    match = True
                elif int_match(o1[i], o2[i]):
                    match = True
                if not match:
                    errcnt += 1
        else:
            errcnt += 1
        
    if errcnt > 0:
        print "error in disassembly... see instruction output below"
        print i1
        print i2
        print '-----------------------------------------------------'
        return False
    return True

def main():
    try:
        optlist, args = getopt.getopt(sys.argv[1:], '', ['file=', 'mode='])
    except getopt.GetoptError, err:
        print_usage(err)
        sys.exit(1)

    if len(args) > 0:
        print_usage('extra arguments are invalid: ' + str(args))
        sys.exit(1)

    testfile = ''
    mode = 64
    for i in range(0,len(optlist),1):
        if optlist[i][0] == '--file':
            testfile = optlist[i][1]
        if optlist[i][0] == '--mode':
            try:
                mode = int(optlist[i][1])
            except ValueError, e:
                print_usage(e)

    if testfile == '':
        print_usage('missing option --file')
    if not file_exists(testfile):
        print_usage('must be a valid file: %s' % testfile)

    objdump = get_exec_instructions(testfile)
    errcnt = 0
    icnt = 0
    for o in objdump.keys():
        sym = objdump[o]
        c = 0
        for i in range(len(sym['content'])):
            l = sym['content'][i]
            k = get_udis_disasm(mode, testfile, sym['offset'] + c, l['size'], l['addr'])
            if compare_instructions(l, k) == False:
                errcnt += 1
            icnt += 1
            c += l['size']

    print 'found %d errors out of %d instructions between disassembly and objdump for %s' % (errcnt, icnt, testfile)

    if errcnt > 0:
        return 1
    return 0


if __name__ == '__main__':
    main()
