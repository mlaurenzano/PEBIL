#!/usr/bin/env python
#
# This file is part of the pebil project.
#
# Copyright (c) 2010, University of California Regents
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#this script is intended to be run out of $PEBIL_ROOT/scripts

import sys
import os
import string
from xml.dom import minidom

xmldocument = '../instcode/ioevents.xml'
headerfile  = 'IOEvents.h'
sourcefile  = 'IOEvents.c'
wrapperfile = '../scripts/inputlist/iowrappers.inp'
wrapperpost = '_pebil_wrapper'
eventenum   = 'IOEvent'
eventclassenum = 'IOEventClass'

mapClassToNames = {
    'CLIB': ['', '_IO_', '__'],
    'POSX': ['', '__libc_', '__'],
    'MPIO': ['', 'P'],
    'HDF5': []
}

mapTypeToFormat = {
    'FILE*':        '%d',
    'int':          '%d',
    'const char*':  '%s',
    'mode_t':       '%d',
    'const void*':  '%d',
    'size_t':       '%d',
    'off_t':        '%d',
    'struct stat*': '%d',
    'long int':     '%lld',
    'MPI_Comm':     '%d',
    'char*':        '%s',
    'MPI_Info':     '%d',
    'MPI_File':     '%d',
    'void*':        '%d',
    'MPI_Offset':   '%d',
    'MPI_Status*':  '%d',
    'MPI_DataType': '%d'
}

def buildEnumeration(pattern, elist):
    enum = 'typedef enum {\n'
    elist.insert(0,'Invalid')
    for item in elist:
        enum += '\t' + pattern + '_' + item + ',\n'
    sizestr = pattern + '_Total_Types'
    enum += '\t' + sizestr + '\n'
    enum += '} ' + pattern + '_t;\n'

    enum += 'extern const char* ' + pattern + 'Names[' + sizestr + '];\n'
    enum += 'const char* ' + pattern + 'Names[' + sizestr + '] = {\n'
    enum += string.join(['\t"' + item for item in elist], '",\n')
    enum += '"\n};\n\n'

    return enum

def buildSingleFunctionCode(funcdom, fname):
    rettype = str(funcdom.getAttribute('ret'))
    customs = funcdom.getElementsByTagName('custom')
    classname = string.strip(c.firstChild.data)

    entry = ''
    code = ''
    fini = ''
    fcall = ''

    if classname == 'MPIO':
        fcall = 'P' + fname
    else:
        fcall = fname

    entry += '#ifdef WRAPPING_' + classname + '_IO\n'

    # function declaration
    entry += str(rettype) + ' __wrapper_name(' + str(fname) + ')'
    args = funcdom.getElementsByTagName('arg')
    entry += '(' + string.join([str(a.getAttribute('type')) + ' ' + string.strip(a.firstChild.data) for a in args],', ') + ')\n{\n'

    # custom location 0 -- function entry
    for s in customs:
        if cmp(s.getAttribute('location'),'0') == 0:
            entry += '\t' + string.strip(s.firstChild.data) + '\n'
    entry += '\t' + str(rettype) + ' retval;\n'

    code += '#ifdef PRELOAD_WRAPPERS\n'
    code += '\tstatic ' + str(rettype) + ' *(*' + str(fname) + '_ptr)('
    code += string.join([str(a.getAttribute('type')) + ' ' + string.strip(a.firstChild.data) for a in args],', ') + ');\n'
    code += '\t' + str(fname) + '_ptr = dlsym(RTLD_NEXT, "' + fcall + '");\n'
    code += '\tTIMER_EXECUTE(retval = ' + str(fname) + '_ptr(' + string.join([string.strip(a.firstChild.data) for a in args], ', ') + ');)\n'
    code += '#else // PRELOAD_WRAPPERS\n'
    code += '\tTIMER_EXECUTE(retval = ' + str(fcall) + '(' + string.join([string.strip(a.firstChild.data) for a in args], ', ') + ');)\n'
    code += '#endif // PRELOAD_WRAPPERS\n'

    # custom location 1 -- after call, before msg print
    for s in customs:
        if cmp(s.getAttribute('location'),'1') == 0:
            code += '\t' + string.strip(s.firstChild.data) + '\n'

    code += '\tEventInfo_t entry;\n'
    code += '\tbzero(&entry, sizeof(EventInfo_t));\n'
    code += '\tentry.class = ' + eventclassenum + '_' + classname + ';\n'
    code += '\tentry.event_type = IOEvent_' + classname + '_' + fname + ';\n'

    traces = funcdom.getElementsByTagName('trace')
    for t in traces:
        code += '\tentry.' + t.getAttribute('dest') + ' = ' + string.strip(t.firstChild.data) + ';\n';
        if t.getAttribute('dest') == 'handle_id':
            code += '\tentry.handle_class = IOHandle_' + classname  + ';\n'

    for a in args:
        size = a.getAttribute('size')
        if size:
            if size == 'mpi_datatype':
                entry += '\tint datasize;\n'
                entry += '\tMPI_Type_size(' + string.strip(a.firstChild.data) + ', &datasize);\n'

        filetyp = a.getAttribute('file')
        if filetyp:
            if filetyp == 'name':
                code += '\tentry.handle_class = IOHandle_NAME;\n'
                code += '\tentry.handle_id = storeFileName(' + string.strip(a.firstChild.data) + ', 0, IOHandle_NAME, IOFileAccess_ONCE, 1, PEBIL_NULL_COMMUNICATOR);\n'
            elif filetyp == 'handle':
                code += '\tentry.handle_class = IOHandle_' + classname + ';\n'
                code += '\tstoreFileName(' + string.strip(a.firstChild.data) + ', entry.handle_id, entry.handle_class, IOFileAccess_OPEN, 1, PEBIL_NULL_COMMUNICATOR);\n'
            elif filetyp == 'mpi':
                code += '\tentry.handle_class = IOHandle_' + classname + ';\n'
                code += '\tstoreFileName(' + string.strip(a.firstChild.data) + ', entry.handle_id, IOHandle_MPIO, IOFileAccess_OPEN, 1, comm);\n'

        trace = a.getAttribute('trace')
        if trace:
            code += '\tentry.' + trace + ' = ' + string.strip(a.firstChild.data) + ';\n'
            if trace == 'handle_id':
                code += '\tentry.handle_class = IOHandle_' + classname  + ';\n'

    code += '\tstoreEventInfo(&entry);\n'

    # custom location 2 -- before return
    for s in customs:
        if cmp(s.getAttribute('location'),'2') == 0:
            fini += '\t' + string.strip(s.firstChild.data) + '\n'

    fini += '\treturn retval;\n'
    fini += '}\n'

    fini += '#endif // WRAPPING_' + classname + '_IO\n'
    fini += '\n'

    return entry + code + fini

def buildAllFunctionCode(funcdom):
    fnames = string.strip(funcdom.firstChild.data)
    flist = fnames.split(',')

    code = ''
    for f in flist:
        code += buildSingleFunctionCode(funcdom, string.strip(f))
    return code



##############################################################
# get high level info from the xml document
##############################################################
dom = minidom.parse(xmldocument)
classes = dom.getElementsByTagName('ioclass')
classNames = [str(c.getAttribute('class')) for c in classes]

##############################################################
# print header file
##############################################################
header = open(headerfile, 'w')
header.write('// automatically generated by ' + str(sys.argv[0]) + '\n')
header.write('#ifndef _IOEVENTS_H_\n#define _IOEVENTS_H_\n\n')
header.write('#ifdef PRELOAD_WRAPPERS\n#define _GNU_SOURCE\n#include <dlfcn.h>\n#endif\n\n')
header.write('#include <stdio.h>\n\n')
header.write(buildEnumeration(eventclassenum, [string.strip(c.firstChild.data) for c in classes]))
classFuncs = []
for c in classes:
    funcs = c.getElementsByTagName('function')
    for fraw in funcs:
        flist = string.strip(fraw.firstChild.data).split(',')
        for f in flist:
            classFuncs.append(string.strip(c.firstChild.data) + '_' + string.strip(f))
header.write(buildEnumeration(eventenum, classFuncs))
header.write('#endif // _IOEVENTS_H_')
header.close()

##############################################################
# print source file
##############################################################
source = open(sourcefile, 'w')
source.write('// automatically generated by ' + str(sys.argv[0]) + '\n')
source.write('#include <' + headerfile + '>\n\n')
source.write('uint32_t mpiRegSeq = 0x800;\n\n')
for c in classes:
    funcs = c.getElementsByTagName('function')
    for fraw in funcs:
        source.write(buildAllFunctionCode(fraw))
source.close()

##############################################################
# print call->wrapper file (for pebil's --trk)
##############################################################
wrapper = open(wrapperfile, 'w')
wrapper.write('# automatically generated by ' + str(sys.argv[0]) + '\n')
for c in classes:
    cName = string.strip(c.firstChild.data)
    wrapper.write('# ' + cName + ' wrappers\n')
    funcs = c.getElementsByTagName('function')
    prefixes = mapClassToNames[cName]
    for fraw in funcs:
        flist = string.strip(fraw.firstChild.data).split(',')
        for f in flist:
            for p in prefixes:
                wrapper.write(p + string.strip(f) + ':' + string.strip(f) + wrapperpost + '\n')
wrapper.close()
