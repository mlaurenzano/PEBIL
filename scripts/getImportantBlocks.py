#!/usr/bin/env python

import getopt
import sys
import os
import re


# global magic numbers/strings
DEFAULT_BLOCK_MIN = 10
BLOCK_IDENTIFIER = 'BLK'
IMAGE_IDENTIFIER = 'IMG'
LOOP_IDENTIFIER = 'LPP'
INPUT_JBB_NAME_REGEX = '(\S+).r(\d\d\d\d\d\d\d\d).t(\d\d\d\d\d\d\d\d).jbbinst'
INPUT_STATIC_NAME_REGEX = '.*.static'
OUTPUT_LBB_NAME = '%(application)s.%(image)s.t%(tasks)08d.lbb'
OUTPUT_OPCOUNT_NAME = 'opcounts.dat'

# util functions
def print_error(err):
    if err != '':
        print 'Error: ' + str(err)
    sys.exit(1)

def print_usage(err=''):
    print "usage : " + sys.argv[0]
    print "[Optional Arguments]:"
    print "        --blockmin <block_minimum_to_include> [default = " + str(DEFAULT_BLOCK_MIN) + "]"
    print "[Positional Arguments]:"
    print "        list of .jbbinst trace files"
    print ""
    print "Example: " + sys.argv[0] + " --blockmin 1000 dynTest.r*.jbbinst"
    print_error(err)

def file_exists(filename):
    if os.path.isfile(filename):
        return True
    return False


# classes describing input file and constituent components
class ImageLine:

    def __init__(self, toks):
        if len(toks) != 7:
            print_usage('invalid image tokens: ' + str(toks))

        if toks[0] != IMAGE_IDENTIFIER:
            print_usage('expecting first token in image line as ' + IMAGE_IDENTIFIER)

        self.type = toks[3]
        if self.type != 'Executable' and self.type != 'SharedLib':
            print_usage('valid types for image are "Executable" and "SharedLib": ' + self.type)

        self.name = toks[4]

        try:
            self.hashcode = int(toks[1], 16)
            self.sequence = int(toks[2])
            self.blockcount = int(toks[5])
            self.loopcount = int(toks[6])
        except ValueError:
            print_usage('problem parsing image line; expecting ints for the following: ' + str([toks[1], toks[2], toks[5], toks[6]]))
            

    def __str__(self):
        return ''

    def id(self):
        return self.name

class CounterLine(object):
    def __init__(self, toks):
        self.threadcounters = {}

    def addthreadcount(self, toks):
        if len(toks) != 2:
            print_usage('invalid thread count tokens: ' + str(toks))
        
        try:
            tseq = int(toks[0])
            tcnt = int(toks[1])
            if self.threadcounters.has_key(tseq):
                print_usage('duplicate thread count for thread ' + tseq + ' in block ' + self.sequence + ' image ' + self.image)
            self.threadcounters[tseq] = tcnt
        except ValueError:
            print_usage('problem parsing thread count line: ' + str(toks))

class BlockLine(CounterLine):

    def __init__(self, toks):
        super(BlockLine, self).__init__(toks)

        if len(toks) != 9:
            print_usage('invalid block tokens: ' + str(toks))

        if toks[0] != BLOCK_IDENTIFIER:
            print_usage('expecting first token in block line as ' + BLOCK_IDENTIFIER)

        if toks[5] != '#':
            print_usage('malformed block line: ' + str(toks))

        self.function = toks[7]

        try:
            self.hashcode = int(toks[2], 16)
            self.sequence = int(toks[1])
            self.image = int(toks[3])
            self.count = int(toks[4])
            self.address = int(toks[8], 16)
        except ValueError:
            print_usage('problem parsing block line; expecting ints for the following: ' + str([toks[1], toks[2], toks[3], toks[4], toks[8]]))

        try:
            t = toks[6].split(':')
            if len(t) != 2:
                raise ValueError
            self.file = t[0]
            self.lineno = int(t[1])
        except ValueError:
            print_usage('problem parsing block line; expecting <file>:<line> instead of the following: ' + str(toks[6]))

            
    def id(self):
        return str(self.image) + str(self.hashcode)

class LoopLine(CounterLine):

    def __init__(self, toks):
        super(LoopLine, self).__init__(toks)

        if len(toks) != 8:
            print_usage('invalid loop tokens: ' + str(toks))

        if toks[0] != LOOP_IDENTIFIER:
            print_usage('expecting first token in loop line as ' + LOOP_IDENTIFIER)

        if toks[4] != '#':
            print_usage('malformed loop line: ' + str(toks))

        self.function = toks[6]

        try:
            self.hashcode = int(toks[1], 16)
            self.image = int(toks[2])
            self.count = int(toks[3])
            self.address = int(toks[7], 16)
        except ValueError:
            print_usage('problem parsing loop line; expecting ints for the following: ' + str([toks[1], toks[2], toks[3], toks[7]]))

        try:
            t = toks[5].split(':')
            if len(t) != 2:
                raise ValueError
            self.file = t[0]
            self.lineno = int(t[1])
        except ValueError:
            print_usage('problem parsing loop line; expecting <file>:<line> instead of the following: ' + str(toks[5]))

    def id(self):
        return str(self.image) + str(self.hashcode)

class StaticBlockLine:
    def __init__(self, toks, image):
        self.hashcode = toks[1]
        self.memOps = int(toks[2])
        self.fpOps = int(toks[3])
        self.insns = int(toks[4])
        self.image = image

    def id(self):
        return str(self.image) + str(self.hashcode)

class StaticFile:
    @staticmethod
    def isStaticFile(f):
        r = re.compile(INPUT_STATIC_NAME_REGEX)
        p = r.match(f)
        if p == None:
            return False
        return True

    def __init__(self, sfile):
        self.sfile = sfile
        if not file_exists(self.sfile):
            print_usage(str(sfile) + ' is not a valid file')

        if not StaticFile.isStaticFile(sfile):
            print_usage('expecting a specific format for file name (' + INPUT_STATIC_NAME_REGEX + '): ' + sfile)

        print 'Reading static file ' + sfile
        self.image = 0
        self.blocks = {}
        f = open(sfile)
        for line in f:
            toks = line.strip().split()
            if len(toks) == 0:
                continue;

#            if toks[1] == "sha1sum":
#                pat = "(................).*"
#                r = re.compile(pat)
#                m = r.match(toks[3])
#                self.image = m.group(1)

            if toks[0].isdigit():
                b = StaticBlockLine(toks, self.image)
                self.blocks[b.id()] = b
                

class JbbTraceFile:
    def __init__(self, tfile):
        self.tfile = tfile
        if not file_exists(self.tfile):
            print_usage(str(f) + ' is not a valid file') # FIXME f? not tfile?

        r = re.compile(INPUT_JBB_NAME_REGEX)
        p = r.match(self.tfile)
        if p == None:
            print_usage('expecting a specific format for file name (' + INPUT_JBB_NAME_REGEX + '): ' + self.tfile)

        try:
            self.application = p.group(1)
            self.mpirank = int(p.group(2))
            self.mpitasks = int(p.group(3))
        except ValueError:
            print_usage('expecting a specific format for file name (' + INPUT_JBB_NAME_REGEX + '): ' + self.tfile)


        self.rimage = {}
        self.images = {}
        self.blocks = {}
        self.loops = {}

        f = open(self.tfile)
        for line in f:

            toks = line.strip().split()
            if len(toks) > 0:

                if toks[0] == IMAGE_IDENTIFIER:
                    c = ImageLine(toks)
                    i = c.id()
                    if self.images.has_key(i):
                        print_usage('duplicate image: ' + str(i))
                    self.images[i] = c
                    self.rimage[c.sequence] = i

                elif toks[0] == BLOCK_IDENTIFIER:
                    c = BlockLine(toks)
                    i = c.id()
                    if self.blocks.has_key(i):
                        print_usage('duplicate block: ' + str(i))
                    self.blocks[i] = c

                elif toks[0] == LOOP_IDENTIFIER:
                    c = LoopLine(toks)
                    i = c.id()
                    if not self.loops.has_key(i):
                        self.loops[i] = []
                    self.loops[i].append(c)
                

    def __str__(self):
        return ''



def main():

    # handle command line
    try:
        optlist, args = getopt.getopt(sys.argv[1:], '', ['blockmin='])
    except getopt.GetoptError, err:
        print_usage(err)

    blockmin = DEFAULT_BLOCK_MIN
    for i in range(0,len(optlist),1):
        if optlist[i][0] == '--blockmin':
            try:
                blockmin = int(optlist[i][1])
                if (blockmin < 1):
                    raise ValueError
            except ValueError:
                print_usage('argument to --blockmin should be a positive int')

    staticFile = args[0]
    if StaticFile.isStaticFile(staticFile):
        staticFile = StaticFile(staticFile)
        args = args[1:]
    else:
        staticFile = None

    if len(args) == 0:
        print_usage('requires a list of jbbinst trace files as positional arguments')


    if staticFile != None:
        outfile = open(OUTPUT_OPCOUNT_NAME, 'w')
        outfile.write("# Rank\ttotInsns\ttotMemops\ttotFpops\n")
    else:
        outfile = None

    # parse input files (all remaining positional args)
    imagelist = {}
    ntasks = 0
    appname = ''
    index = 0
    imagecounts = {}
    total = 0
    blockfiles = {}
    for f in args:
        index += 1
        print 'Processing input file ' + str(index) + ' of ' + str(len(args)) + ': ' + f

        b = JbbTraceFile(f)
        if blockfiles.has_key(b.mpirank):
            print_usage('duplicate mpi rank found in input files: ' + str(b.mpirank))
        blockfiles[b.mpirank] = 1

        if outfile != None:
            blockFile = b
            totInsns = 0
            totMemops = 0
            totFpops = 0
            for block in staticFile.blocks.values():
                try:
                    dynBlock = blockFile.blocks[block.id()]
                except KeyError:
                    continue
                totInsns = totInsns + block.insns * dynBlock.count
                totMemops = totMemops + block.memOps * dynBlock.count
                totFpops = totFpops + block.fpOps * dynBlock.count
            outfile.write(str(blockFile.mpirank) + "\t" + str(totInsns) + "\t" + str(totMemops) + "\t" + str(totFpops) + "\n")
           

        for ki in b.images.keys():
            imagelist[ki] = 1

        if ntasks == 0:
            ntasks = b.mpitasks
        if ntasks != b.mpitasks:
            print_usage('all files should be from a run with the same number of mpi tasks: ' + str(ntasks))
        if ntasks == 0:
            print_usage('number of mpi tasks should be > 0')

        if appname == '':
            appname = b.application
        if appname != b.application:
            print_usage('all files should be from a run with the same number application name: ' + appname)


        # add up block counts from this rank
        for kb in b.blocks.keys():
            bb = b.blocks[kb]
            iid = b.rimage[bb.image]

            if not imagecounts.has_key(iid):
                imagecounts[iid] = {}

            if not imagecounts[iid].has_key(bb.hashcode):
                imagecounts[iid][bb.hashcode] = 0

            imagecounts[iid][bb.hashcode] += bb.count
            total += bb.count


    if outfile != None:
        outfile.close()

    # write to file if block count exceeds minimum
    imagefiles = {}
    for k in imagelist.keys():
        for kb in imagecounts[k].keys():
            if imagecounts[k][kb] >= blockmin:
                if not imagefiles.has_key(k):
                    fname = OUTPUT_LBB_NAME % {'application': appname, 'image': k, 'tasks': ntasks }
                    f = open(fname, 'w')
                    imagefiles[k] = f
                    print 'Writing output file ' + fname
                    f.write('# BlockHash # TotalBlockCount\n')
                imagefiles[k].write(('0x%x' % kb) + ' # ' + str(imagecounts[k][kb]) + '\n')

    # close all ouput files
    for k in imagefiles.keys():
        imagefiles[k].close()

    return 0


# run main if executed
if __name__ == '__main__':
    main()
