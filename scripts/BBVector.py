#!/usr/bin/env python
# prints out a feature vector of all BBs in a set of trace files
# printing is done for each rank and for a single sysid
# this script can only handle single-image single-threaded runs

import getopt
import os
import string
import sys

inform = '******  ' 

def file_exists(filename):
    if os.path.isfile(filename):
        return True
    return False

def print_error(err):
    print 'Error: ' + str(err)
    sys.exit(1)

def print_usage(err):
    print "usage : " + sys.argv[0]
    print "        --app <app_name>"
    print "        --cput <cpu_count>"
    print "        --sysid <sysid>"
    print "        --appdir <app_dir> [defaults to .]"
    print "        --tracedir <trace_dir> [defaults to .]"
    print_error(err)

class BBTrace:
    def __init__(self, toks):
        if len(toks) != 10:
            print_error('cannot parse BB info in static file: ' + line)
    
        try:
            self.seqid = int(toks[0])
            self.bbhash = long(toks[1])
            self.memops = int(toks[2])
            self.fpops = int(toks[3])
            self.insns = int(toks[4])
            self.fname = toks[6]
            self.vaddr = int(toks[9][2:], 16)

        except ValueError, e:
            print_error('malformed value in static file: ' + str(toks))

        self.data = {}

    def getbbhash(self):
        return self.bbhash

    def getbbseq(self):
        return self.seqid

    # right now we don't use the +xyz lines, so this does nothing
    def addstaticplus(self, toks):
        if toks[0] == '+dud':
            tint = 0
            cint = 0
            tfpp = 0
            cfpp = 0
            for i in range(1,len(toks),1):
                if toks[i] == '#':
                    break
                stoks = toks[i].split(':')
                if len(stoks) != 3:
                    print_error('cannot parse +dud line: ' + str(toks))
                try:
                    dist = int(stoks[0])
                    iint = int(stoks[1])
                    ifpp = int(stoks[2])

                    tint += dist * iint
                    cint += iint
                    tfpp += dist * ifpp
                    cfpp += ifpp
                except ValueError, e:
                    print_error('cannot parse +dud line: ' + str(toks))                    

            if cint > 0:
                self.intdud = float(float(tint) / float(cint))
            else:
                self.intdud = 0.0

            if cfpp > 0:
                self.fppdud = float(float(tfpp) / float(cfpp))
            else:
                self.fppdud = 0.0
                     
        return

    # right now we don't use the jbbinst file, so this does nothing
    def addjbbinst(self, lines):
        return

    def addsimdata(self, rank, thread, toks):
        if not self.data.has_key(rank):
            self.data[rank] = {}

        try:
            sysid = int(toks[0])
            level = int(toks[1])
            hits = long(toks[2])
            misses = long(toks[3])

        except ValueError, e:
            print_error('cannot parse simulation data 1')

        if not self.data[rank].has_key(sysid):
            self.data[rank][sysid] = []
        if len(self.data[rank][sysid]) != level - 1:
            #print self.data[rank][sysid]
            print_error('cannot parse simulation data 2: ' +  str(toks))

        self.data[rank][sysid].append([hits, misses])

    def addsiminst(self, rank, lines):
        if len(lines) < 2:
            print_error('cannot parse simulation data 3')

        toks = lines[0].strip().split()
        if toks[0] != 'BLK':
            print_error('cannot parse simulation data 4')

        try:
            seqid = int(toks[1])
            bbhash = long(toks[2][2:], 16)
            thread = int(toks[4])
            self.simbbcount = long(toks[5])
            self.countinsnsim = long(toks[6])
        except ValueError, e:
            print_error('cannot parse simulation data 5')

        for i in range(1,len(lines),1):
            toks = lines[i].strip().split()
            self.addsimdata(rank, thread, toks)

    def sysidappears(self, rank, sysid):
        try:
            x = self.data[rank][sysid]
        except KeyError:
            return False
        return True

    def __str__(self):
        return str(self.seqid) + '\t' + str(self.bbhash)

    def getsimscale(self):
        return float(float(self.simbbcount) * float(self.memops)) / float(self.countinsnsim)

    def getvisitcount(self):
        return self.simbbcount

    def vectorstr(self, rank, sysid, sep=',\t'):
        s = ''
        s += str(self.bbhash) + sep
        s += str(self.intdud) + sep
        s += str(self.fppdud) + sep
        s += str(self.getsimscale() * self.data[rank][sysid][0][1]) + sep
        s += str(self.getsimscale() * self.data[rank][sysid][1][1]) + sep
        s += str(self.getsimscale() * self.data[rank][sysid][2][1]) + sep
        s += str(self.getvisitcount() * self.insns) + sep
        s += str(self.getvisitcount() * self.memops) + sep
        s += str(self.getvisitcount() * self.fpops)
        return s

class ApplicationTrace:
    def __init__(self, appdir, tracedir, app, cput):

        self.blocks = {}
        staticname = "%(appdir)s/%(app)s.jbbinst.static" % {'appdir': appdir, 'app': app}
        if not file_exists(staticname):
            print_error('cannot open static file: ' + staticname)

        print inform + 'Parsing static file ' + staticname
        f = open(staticname, 'r')
        BB = None
        for line in f:
            toks = line.strip().split()
            if len(toks) < 1:
                continue
            if toks[0].startswith('#'):
                continue
            if toks[0].startswith('+'):
                BB.addstaticplus(toks)
                continue
            if len(toks) != 10:
                continue

            BB = BBTrace(toks)
            if self.blocks.has_key(BB.getbbseq()):
                print_error('static file contains the same bbid multiple times? ' + BB.getbbseq())
            self.blocks[BB.getbbseq()] = BB

        f.close()

        for rank in range(0,cput,1):
            fname = "%(tracedir)s/%(app)s.r%(rank)08d.t%(cput)08d.siminst" % {'tracedir': tracedir, 'app': app, 'rank': rank, 'cput': cput}
            if not file_exists(fname):
                print_error('cannot open simulation file: ' + fname)

            print inform + '\tParsing simulation file ' + fname
            f = open(fname, 'r')
            imagemap = {}
            pilelines = []
            currentBB = 0
            for line in f:
                toks = line.strip().split()
                if len(toks) < 1:
                    continue
                if toks[0].startswith('#'):
                    continue
                if toks[0].startswith('+'):
                    continue
                if toks[0] == 'IMG':
                    if len(toks) != 5:
                        print_error('malformed IMG line in ' + fname)
                    try:
                        imagemap[int(toks[2])] = long(toks[1][2:], 16)
                    except ValueError, e:
                        print_error('malformed IMG line in ' + fname)
                    continue

                #print toks
                if toks[0] == 'BLK':
                    if len(toks) != 10:
                        print_error('malformed BLK line in ' + fname)

                    if len(pilelines) > 0:
                        if not self.blocks.has_key(currentBB):
                            print_error('found block seq in siminst that is not in static: ' + str(currentBB))
                        self.blocks[currentBB].addsiminst(rank, pilelines)
                    pilelines = []

                    try:
                        currentBB = int(toks[1])
                    except ValueError, e:
                        print_error('malformed BLK line in ' + fname)
                        

                pilelines.append(line)
                
            if len(pilelines) > 0:
                if not self.blocks.has_key(currentBB):
                    print_error('found block seq in siminst that is not in static: ' + str(currentBB))
                self.blocks[currentBB].addsiminst(rank, pilelines)

            #for k in self.blocks.keys():
            #    print self.blocks[k]



    def getBB(self, bbhash):
        if self.blocks.has_key(bbhash):
            return self.blocks[bbhash]
        return None

    def getallblocks(self):
        return self.blocks.keys()

def main():
    try:
        optlist, args = getopt.getopt(sys.argv[1:], '', ['app=', 'cput=', 'sysid=', 'appdir=', 'tracedir='])
    except getopt.GetoptError, err:
        print_usage(err)
        sys.exit(1)

    if len(args) > 0:
        print_usage('extra arguments are invalid: ' + str(args))
        sys.exit(1)

    app = ''
    cput = ''
    sysid = ''
    appdir = '.'
    tracedir = '.'
    for i in range(0,len(optlist),1):
        if optlist[i][0] == '--app':
            app = optlist[i][1]
        if optlist[i][0] == '--cput':
            try:
                cput = int(optlist[i][1])
            except ValueError, e:
                print_usage(e)
        if optlist[i][0] == '--sysid':
            try:
                sysid = int(optlist[i][1])
            except ValueError, e:
                print_usage(e)            
        if optlist[i][0] == '--appdir':
            appdir = optlist[i][1]
        if optlist[i][0] == '--tracedir':
            tracedir = optlist[i][1]

    if app == '':
        print_usage('missing option --app')
    if cput == '':
        print_usage('missing option --cput')
    if sysid == '':
        print_usage('missing option --sysid')

    apptrace = ApplicationTrace(appdir, tracedir, app, cput)
    for rank in range(0,cput,1):
        outname = "%(tracedir)s/%(app)s.rank%(rank)d.sysid%(sysid)d.bbv" % {'tracedir': tracedir, 'app': app, 'rank': rank, 'sysid': sysid}
        print inform + 'Writing BB Vectors for rank ' + str(rank) + ' sysid ' + str(sysid) + ' to ' + outname
        f = open(outname, 'w')

        for bbid in apptrace.getallblocks():
            bb = apptrace.getBB(bbid)
            if bb.sysidappears(rank, sysid):
                f.write(bb.vectorstr(rank, sysid) + '\n')
            #else:
            #    print 'sim does not have rank ' + str(rank) + ' sysid ' + str(sysid) + ' block ' + str(bbid)

        f.close()

    print inform + 'DONE'

if __name__ == '__main__':
    main()
