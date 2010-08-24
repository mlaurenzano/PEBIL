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

# Counter for a folder
# /pmaclabs/icepic/ti10_round1_icepic_large_0256/processed_trace

# Executable:
# AvgCacheCalc.py
#

# This will calculate the Average of Cache hits for a series of processed metasim files.
# 
# 
# Usage:
# 
# A number of arguments are needed. The arguments determine how to select the set of files to process
# and whether to compute the average across all files or not.
# 
#  Either sysid or taskid is required
# sysid - calculates a single average for all files with the same sysid
#    - use with --sysid option to speciy which sysid to use.
#    - for file icepic_large_0256_0127.sysid44, the sysid is 44
# 
# taskid - prints an average for each file with the same task id (ie 1 set of averages for for each sysid found)
#        - use with --taskid option to specify the task id
#        - for file icepic_large_0256_0127.sysid44, the taskid is 0127
# 
# 
# app		icepic,hycom,..."
# dataset	large, standard..."
# cpu_count	256,1024,... input will be padded with 0s to 4 digits

# Optional:
# dir	- current dir is used if these argument is not given	



# As an example take the folder:
# /pmaclabs/ti10/ti10_round1_icepic_large_0256/processed_trace
# 
# 
# SysID mode:
#mattl@trebek[21]$ ./AvgCacheCalc.py --app icepic --dataset large --cpu_count 1024 --sysid 99 --dir /pmaclabs/ti10/ti10_round1_icepic_large_1024/processed_trace/ 
# # 
# Reading files from:  /pmaclabs/ti10/ti10_round1_icepic_large_1024/processed_trace/
# Averaging for all files like icepic_large_1024*.sysid99
# Number of files:  1024
# Cache 1 average %= 98.365459015 incl(98.365459015)
# Cache 2 average %= 0.000974823792366 incl(98.3654743948)
# Cache 3 average %= 0.0 incl(98.3654743948)
# 
# 
# TaskID:
# mattl@trebek[20]$ ./AvgCacheCalc.py --app icepic --dataset large --cpu_count 1024 --taskid 125 --dir /pmaclabs/ti10/ti10_round1_icepic_large_1024/processed_trace/
# 
# Reading files from:  /pmaclabs/ti10/ti10_round1_icepic_large_1024/processed_trace/
# Averaging for all files like icepic_large_1024_0125*
# Number of files:  32
#   sysid0 99.5021899287 
#   sysid3 98.3544410843 98.4873748354 
#   sysid4 99.0521953314 99.0939555641 
#  sysid21 98.2867244765 98.496093132 
#  sysid22 98.8836107446 99.0731860899 99.5543906444 
#  sysid23 98.086753753 98.4952485239 
#  sysid44 98.8836107446 99.0772427056 99.5790751053 
#  sysid54 96.785672042 99.0781143074 
#  sysid64 98.3544410843 98.4789295449 98.4817196019 
#  sysid67 74.5078816751 
#  sysid68 23.7552154266 
#  sysid69 30.5848561276 
#  sysid70 33.5335710304 
#  sysid71 37.710498373 
#  sysid72 98.2910942185 98.2910942244 98.2910942244 
#  sysid73 98.3544410843 98.4789295449 98.49290069 
#  sysid74 98.3544410843 98.4789295449 98.4887431283 
#  sysid75 98.9182843857 99.0849451175 99.5487031836 
#  sysid77 98.086753753 98.4769519456 98.4956922971 
#  sysid78 98.9182843857 99.0849451175 99.1358601016 
#  sysid81 98.2910942185 98.2910942244 98.2910942244 
#  sysid82 98.2910942185 98.2910942244 98.2910942244 
#  sysid96 98.3544410843 98.4789295449 98.4928364694 
#  sysid97 98.3544410843 98.4789295449 98.492618417 
#  sysid98 98.2910942185 98.2910942244 98.2910942244 
#  sysid99 98.2910942185 98.2910942244 98.2910942244 
# sysid100 98.3544410843 98.4789295449 98.4884141107 
# sysid101 98.3544410843 98.4789295449 98.4884425654 
# sysid102 98.2910942185 98.2910942244 98.2910942244 
# sysid103 98.2910942185 98.2910942244 98.2910942244 
# sysid104 98.086753753 98.4769519456 98.5007917366 
# sysid105 98.086753753 98.4769519456 98.4966562518


import sys, os, glob, re

# function for getting average for a single file
# used by sysid argument
def getAvgSingle(fileNameGetAvg):
	"""calcuates the average cache hits for a file"""
	#print "TESTING:: file:", fileNameGetAvg
	#this part is from counter_mikey.py
	try:
		traceFile = open(fileNameGetAvg, 'r')
		fileLines = traceFile.readlines()
		traceFile.close()
		
	except IOError:
		print "Warning: file" + traceFile, "not found"
		exit()


	#Process file
	#count the lines, track each line in a list
	everyLine = []
	totalMisses = []
	totalAccesses = []
	
	
	myLine = fileLines[0].split()
	cLevel = len(myLine)-3
	#print "TESTING:: cLevel=", cLevel

	for i in range(0,cLevel,1):
		totalMisses.append(0)
		totalAccesses.append(0)
	
	if cLevel < 1 or cLevel > 3:
		print "FATAL: Expected 1, 2 or 3 cache levels"	
		exit()

	idx = 1
	for myLine in fileLines:
		# tokenize the line and verify that we get the correct number of tokens
        	myLine = myLine.split()
        	if cLevel != len(myLine)-3:
               		print "FATAL: expected " + cLevel + " hit rates on line " + str(idx)
			exit()
		# ascribe each token to an aptly-named variable
        	#blockid = long(myLine[0]) ###ASK MIKEY - needed?###
       		#fpCount = long(myLine[1]) ###ASK MIKEY - needed?###
        	memCount = long(myLine[2])
        	inclRate = []
        	for i in range(0,len(totalMisses),1):
			inclRate.append(float(myLine[i+3]))
			#print "myLine[", i+3, "]= ", myLine[i+3]

		# convert each inclusive hit rate to an exclusive rate
		exclRate = [inclRate[0]]
		for i in range(1,len(inclRate),1):
			thisRate = float(inclRate[i])
			prevRate = float(inclRate[i-1])
			if prevRate < 100.0:
                        	exclRate.append(100.0*float(thisRate - prevRate)/(100.0 - prevRate))
                	else:
                       		exclRate.append(float(0.0))

		blockAccesses = []
		blockMisses = []
		blockAccesses.append(memCount)
		## blockHits[n] stores the number of memory accesses that make it to cache level N
       		for i in range(0,len(totalMisses)-1,1):
                	blockMisses.append((blockAccesses[i]*(float(100.0)-exclRate[i]))/float(100.0))
#                	print "block L" + str(i+1) + " misses: " + str(blockMisses[i])
                	blockAccesses.append(blockMisses[i])
#               	print "block L" + str(i+2) + " accesses: " + str(blockAccesses[i+1])

        	blockMisses.append(blockAccesses[cLevel-1]*((100.0-exclRate[cLevel-1])/100.0))
#        	print "block L" + str(cLevel) + " misses: " + str(blockMisses[cLevel-1])

        	for i in range(0,len(totalMisses),1):
                	totalMisses[i] += blockMisses[i]
                	totalAccesses[i] += blockAccesses[i]

	totalHits = 0

	cacheValues = []
	for i in range(0,len(totalMisses),1):
	        levelHits = (totalAccesses[i] - totalMisses[i])
	        totalHits += levelHits
		
		#assign values to tuple and return
		cacheValues.append((levelHits)/(totalAccesses[i])*100)
		cacheValues.append(100*totalHits/totalAccesses[0])
	        #print "Cache " + str(i+1) + " average %= " + str((levelHits)/(totalAccesses[i])*100) + " incl(" + str(100*totalHits/totalAccesses[0]) + ")"
	#print "cacheValues:", cacheValues

	return cacheValues













# function for getting average for a single file
# used by taskid argument
# printType: 1 = taskid prints ex: taskid0001
# printType: 2 = sysid pritns sysid72
def printAvgSingle(fileNamepAvg, printType):
	#print "Avg for file:", fileNamepAvg
	
	fileidx = fileNamepAvg.rfind("/")
	shortfileName = fileNamepAvg[fileidx+1:]
	#print "TESTING: FileName:", shortfileName

	# get the sysid# for printing later
	try:
		sysidname = shortfileName[shortfileName.index('.')+1:]
		taskidname = shortfileName[shortfileName.index('.')-4:shortfileName.index('.')]
		#print "TESTING: sysidname=", sysidname
	except ValueError:
		print "ERROR: Invalid filename no '.' found in filename:", shortfileName
		exit()
	except IndexError: #If file has '.' as last char, this could error
		print "Error: Invalid location of . in filename. this shouldn't happen-", shortfileName
		exit() 
	


	#lifted from counter_mikey.py
	try:
		traceFile = open(fileNamepAvg, 'r')
	except IOError, NameError:
		print "ERROR: can't find that file: " + fileNamepAvg
		exit()

	#Process file
	#count the lines, track each line in a list
	everyLine = []

	fileLines = traceFile.readlines()
	traceFile.close()
	myLine = fileLines[0].split()
	cLevel = len(myLine)-3

	totalMisses = []
	totalAccesses = []
	for i in range(0,cLevel,1):
      		totalMisses.append(0)
        	totalAccesses.append(0)

	####validate cLevel 4,5, or 6 is expected
	#print "TESTING: This file has", cLevel, "cache level(s)"
	##print "Eachline has", len(myLines), "columns"

	if cLevel < 1 or cLevel > 3:
		print "ERROR: Expected 1, 2, or 3 cache levels"	
		exit()


	#### create if, else for cLevel = 4,5,6
	idx = 1
	for myLine in fileLines:

      	 	 # tokenize the line and verify that we get the correct number of tokens
        	myLine = myLine.split()
       		if cLevel != len(myLine)-3:
               		print "ERROR: expected " + cLevel + " hit rates on line " + str(idx)

        	# ascribe each token to an aptly-named variable
        	blockid = long(myLine[0])
        	fpCount = long(myLine[1])
       		memCount = long(myLine[2])
        	inclRate = []
        	for i in range(0,len(totalMisses),1):
              		inclRate.append(float(myLine[i+3]))

        	# convert each inclusive hit rate to an exclusive rate
        	exclRate = [inclRate[0]]
        	for i in range(1,len(inclRate),1):
                	thisRate = float(inclRate[i])
                	prevRate = float(inclRate[i-1])
               		if prevRate < 100.0:
                        	exclRate.append(100.0*float(thisRate - prevRate)/(100.0 - prevRate))
                	else:
                        	exclRate.append(float(0.0))

	#        print str(myLine) + ' -> ',
	#        print str(blockid) + '\t' + str(fpCount) + '\t' + str(memCount),
	#        for i in range(0,len(exclRate),1):
	#                print '\t' + str(exclRate[i]),

	#        print ''
	
	        blockAccesses = []
       	 	blockMisses = []
        	blockAccesses.append(memCount)
	#        print "block L1 accesses: " + str(blockAccesses[0])

       		## blockHits[n] stores the number of memory accesses that make it to cache level N
        	for i in range(0,len(totalMisses)-1,1):
               		blockMisses.append((blockAccesses[i]*(float(100.0)-exclRate[i]))/float(100.0))
#                	print "block L" + str(i+1) + " misses: " + str(blockMisses[i])
                	blockAccesses.append(blockMisses[i])
#                	print "block L" + str(i+2) + " accesses: " + str(blockAccesses[i+1])

        	blockMisses.append(blockAccesses[cLevel-1]*((100.0-exclRate[cLevel-1])/100.0))
#        	print "block L" + str(cLevel) + " misses: " + str(blockMisses[cLevel-1])

        	for i in range(0,len(totalMisses),1):
                	totalMisses[i] += blockMisses[i]
                	totalAccesses[i] += blockAccesses[i]


#	if printType == 1: 
#		print "taskid" + str(taskidname),	
#	if printType == 2:
#		print sysidname.rjust(8),
        print shortfileName,
	
	totalHits = 0
	
	for i in range(0,len(totalMisses),1):
        	levelHits = (totalAccesses[i] - totalMisses[i])
        	totalHits += levelHits
        	#print "Cache " + str(i+1) + " average %= " + str((levelHits)/(totalAccesses[i])*100) + " incl(" + str(100*totalHits/totalAccesses[0]) + ")"
		print str(100*totalHits/totalAccesses[0]).ljust(13),

	print ""	

# used to sort list of files in natural or numeric order
def sort_nicely( l ): 
	""" Sort the given list in the way that humans expect. 
	""" 
	def convert(text):
		if text.isdigit():
			return int(text)
		else:	
			return text

	##convert = lambda text: int(text) if text.isdigit() else text
	alphanum_key = lambda key: [ convert(c) for c in re.split('([0-9]+)', key) ] 
	l.sort( key=alphanum_key ) 





#prints a usage error message and exits
def errorMsg():
	print	
	print "Usage : ./AvgCacheCalc.py\n"
	print "required:"
	print "\t--app		string; eg icepic,hycom,..."
	print "\t--dataset	string; eg large, standard..."
	print "\t--cpu_count	int; eg 256,1024,...\n"
	print "One of these two are required:"	
	print "\t--taskid	int; eg 0001"
	print "\t--sysid	int; 1, 2, or 3 chars - 75\n"
	print "optional"
	print "\t--dir		string; eg /pmaclabs/ti10/ti10_icepic_standard_0128/processed_trace/ [default=.]"

	exit()


diridx = -1
sysidx = -1
taskidx = -1
sysidindexerr = 0
taskidindexerr = 0


try:
	#check for sysid
	sysidx = sys.argv.index("--sysid")
	#print "past sysidx ValueError"
	#print "sysidx=", sysidx
	if sysidx != -1:
		sysid = sys.argv[sysidx+1]
		#print "sysid=", sysid
except IndexError:
	print "TESTING: IndexError:No --sysid argument. ->pass"
	sysidindexerr = 1
	pass
except ValueError:
	#print "TESTING: ValueError --sysid ->pass"	
	# sysid may not be needed, taskid maybe used
	pass		
	
try:
	# check for taskid
	taskidx = sys.argv.index("--taskid")
	task = sys.argv[taskidx+1]

	# pad task with 0s if needed
	while len(task) < 4:
		task = "0"+task
	#print "TESTING:task=", task
				
	#print "taskidx=", taskidx
	#print "taskid=", task
except IndexError:
	print "TESTING: IndexError: No --taskid argument. ->pass"
	taskidindexerr = 1
	pass
except ValueError:
	#print "TESTING: ValueError --taskid ->pass"	
	pass

# if neither sysid or taskid is used, error
if sysidx == -1 and taskidx == -1:
	print "Either --sysid or --taskid required - neither used"
	errorMsg()
# if both sysid and taskid are used, error
if sysidx != -1 and taskidx != -1:
	print "Either --sysid or --taskid required - both used"
	errorMsg()


# check to make sure sys or task value was given
# needed because we skipped this check before
#print "Testing: sysidx and taskidx sysidx=", sysidx," taskidx=", taskidx

if sysidx != -1 and sysidindexerr == 1: # we are using sysid and there was a no value after
	print "No --sysid value given, please provide argument\n"
	errorMsg()

if taskidx != -1 and taskidindexerr == 1: # we are using taskid and there was a no value after
	print "No --taskid value given, please provide argument\n"
	errorMsg()

# check for dir
# not required, uses current dir
try:
	diridx = sys.argv.index("--dir")
except ValueError:
	#print"no dir->ok"
	pass	# if --dir is not used, current dir willbe used
try:
	if diridx == -1:
		dirRead = os.getcwd()  #use currend dir if none given
		#print "TESTING: current dir used dir=", dirRead
	else:
		dirRead = sys.argv[diridx+1]
		#print "TESTING: input used ***WHAT ABOUT SLASH AT END*** dir=", dirRead		
	
	#pad a '/' to the end of the directory
	if dirRead[-1] != '/':
		dirRead = dirRead + '/'
		
except IndexError:
	print "No --dir value given, please provide argument\n"
	errorMsg()
except ValueError:
	print "TESTING:Error with --dir argument, see usage below\n"	
	errorMsg()

try:
	#check for app
	appidx = sys.argv.index("--app")
	appname = sys.argv[appidx+1]
	#print "app=", appname
except IndexError:
	print "No --app value given, please provide argument\n"
	errorMsg()
except ValueError:
	print "Error with --app argument, see usage below\n"	
	errorMsg()
try:
	#check for dataset
	datasetidx = sys.argv.index("--dataset")
	datasetname = sys.argv[datasetidx+1]
	#print "dataset=", datasetname
except IndexError:
	print "No --dataset value given, please provide argument\n"
	errorMsg()
except ValueError:
	print "Error with --dataset argument, see usage below\n"	
	errorMsg()
try:	
	#check for cpu_count
	cpuidx = sys.argv.index("--cpu_count")
	cpu = sys.argv[cpuidx+1]
	#print "cpu=", cpu
	#print "cpu type:", type(cpu)
	cpulen = len(cpu)
	if cpulen > 4: # error if more than 4 digits
		print "ERROR: cpu_count cannot be greater than 4 digits"
		exit()
	if cpulen < 4: # pad with 0 if less than 4 digits
		#print "TESTING: cpulen:", cpulen, "needs to be 4"
		while len(cpu) < 4:
			cpu = "0"+cpu
			cpulen = len(cpu)	
except IndexError:
	print "No --cpu_count value given, please provide argument\n"
	errorMsg()
except ValueError:
	print "Error with --cpu_count argument, see usage below\n"	
	errorMsg()
	

fileName = appname+"_"+datasetname+"_"+cpu


# print "filename:", fileName
# print "dirRead:", dirRead


print
print "Reading files from: ", dirRead

#gets the list of files with a matching sysid value
if taskidx == -1: #use sysid
	print "Averaging for all files like "+fileName+"*.sysid"+sysid
	fileList = glob.glob(dirRead+fileName+"*.sysid"+sysid)
#or gets the list of files with the taskid value
elif sysidx == -1: #use taskid
	print "Averaging for all files like "+fileName+"_"+task+"*"
	#print dirRead+fileName+"_"+task+".*"
	fileList = glob.glob(dirRead+fileName+"_"+task+".*")
	
else:
	print "ERROR: No one should get here either taskid or sysid should have been validated"
	errorMsg()

#for i in range(len(fileList)):
#	print "TESTING:filelist[", i, "]=",fileList[i]



#sort the list of files
#fileList.sort()

#sort numerically
sort_nicely(fileList)
	


#print "fileList[0]:", fileList[0]
#print "fileList type:", type(fileList)

# Catch if there are no files matching the request
if len(fileList) == 0:
	print "ERROR: No files match input...exiting"
	exit()

print "Number of files: ", len(fileList)
#print "This may take a moment"

if taskidx == -1: #use sysid
	dirAvg = []

# goes through each file and collects all the averages
# inclusive and exclusive for Caches 1-3 (if 2 and 3 are present)
	for i in range(0, len(fileList)):
		dirAvg.append(getAvgSingle(fileList[i]))
		printAvgSingle(fileList[i],1)
	print "\n *** Averaged Hit rates ***"
	print fileName+"*.sysid"+sysid,

	numCache = len(dirAvg[0])
	totalCache = [0,0,0,0,0,0]
	
	#print "TESTING:numcache for avg of files is:", numCache
	#print "TESTING:dirAvg[0]=", dirAvg[0]
	#print "TESTING:len(dirAvg[0])=", len(dirAvg[0])
	#print "TESTING:len(dirAvg)=", len(dirAvg)
	#print "TESTING:numCache range= ", range(numCache)

	#calcute averages for the folder
	for i in range(len(dirAvg)):
		#if len(dirAvg[i]) > 4:
			#print "TESTING:dirAvg[",i,"][4]=", dirAvg[i][4]
		for j in range(numCache):
			#print "::j=",j,"dirAvg[",i,"][",j,"]= ", dirAvg[i][j]
			totalCache[j] = totalCache[j]+dirAvg[i][j]
	#print values of the cache
	
	for i in range(0, len(totalCache), 2):
		if totalCache[i+1] !=0:	
			print totalCache[i+1]/len(dirAvg),
			#print "excl", totalCache[i]/len(dirAvg)	
			#print "Cache " + str((i/2)+1) + " average %= " + str(totalCache[i]/len(dirAvg)) + " incl(" + str(totalCache[i+1]/len(dirAvg)) + ")"


elif sysidx == -1: #use taskid
	for i in range(0, len(fileList)):
		printAvgSingle(fileList[i],2)

