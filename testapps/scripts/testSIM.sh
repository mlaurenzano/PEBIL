#!/bin/bash
# Author: Allysonc
# In an attempt to make testing a little more modular, I'm making a 
# script for each tool. I assume there will be a number of parameters
# and everything will be called from the Makefiles by a make "check".
# Hopefully, this means that very little has to be changed in these
# scripts and that nothing is hard-coded.

# Each script should have some of the same type of checks:
# 1. It should check for PEBIL errors during instrumentation
#    (Actually this should probably just kill everything)
# 2. It should check for PEBIL errors while running the instrumented
#    binary (for now let the script die on error?)
# 3. It should check that the binary output doesn't really change. 
#    This might actually be really hard because some apps, like NPBs
#    have timing data that can easily change between runs. Hopefully,
#    a grep string that gets passed in can help filter out this 
#    type of output.
# 4. It should sanity-check PEBIL output and if possible, test for 
#    correctness

#####################################################################
#############################Parameters##############################
#####################################################################
PROGRAM=$1
CD_DIR=$2 # Right now this only makes sense to run it in the same dir
          # as the binary.

if [ -z "${USING_OMP}" ]
then
  USING_OMP=0         # 0 for no OMP, 1 for OMP
fi

# Possible strings:
#   CHECK_BIN_OUTPUT  - Check to see if instrumented binary produces
#                       same output as uninstrumented
#   INST_AND_RUN      - Instrument the program and run both 
#                       instrumented and original binaries. Save 
#                       output for future analysis
if [ -z "${TEST_CHECK}" ]
then
  TEST_CHECK="CHECK_BIN_OUTPUT"
fi

# PROGRAM_SUCCESS is a string that can be grepped for to check if the
# program worked as intended. For example, NPBs output times, MFLOP/s
# and other data that changes from run to run. If instead, we grep 
# for "Verification", we can know if the program ran as intended:
# PROGRAM_SUCCESS="Verification"

# When instrumenting siminst, you need to input which blocks to 
# simulate. For testing basic siminst, input /dev/null by setting 
# this variable to 1. Otherwise, this script will create an input 
# file with getImportantBlocks.py.
if [ -z "${INPUT_DEV_NULL}" ]
then
  INPUT_DEV_NULL=0  
fi

#####################################################################
############################Other Globals############################
#####################################################################
INST_PROGRAM="${PROGRAM}.siminst"
UNINST_OUTFILE="${PROGRAM}.outp"
INST_OUTFILE="${INST_PROGRAM}.outp"
UNINST_TEMPFILE="${PROGRAM}.tmp"
INST_TEMPFILE="${INST_PROGRAM}.tmp"
PEBIL_FILTER="\[Metasim\-r"

#####################################################################
###########################Testing Functions#########################
#####################################################################

# Check for differences in non-PEBIL output. Or, check for success
# with the variable "PROGRAM_SUCCESS".
check_binary_output()
{
  local fileDiff=""

  if [ ! -f ${UNINST_OUTFILE} ]
  then
    echo "Need to have uninstrumented output file: ${UNINST_OUTFILE}"
    echo "Rerun this script with TEST_CHECK=INST_AND_RUN"
    exit
  fi
  if [ ! -f ${INST_OUTFILE} ]
  then
    echo "Need to have instrumented output file: ${INST_OUTFILE}"
    echo "Rerun this script with TEST_CHECK=INST_AND_RUN"
    exit
  fi

  if [ -z "${PROGRAM_SUCCESS}" ]
  then
    cat ${UNINST_OUTFILE} | grep -v "${PEBIL_FILTER}" > ${UNINST_TEMPFILE}
    cat ${INST_OUTFILE} | grep -v "${PEBIL_FILTER}" > ${INST_TEMPFILE}
  else
    cat ${UNINST_OUTFILE} | grep -v "${PEBIL_FILTER}" | grep "${PROGRAM_SUCCESS}" > ${UNINST_TEMPFILE}
    cat ${INST_OUTFILE} | grep -v "${PEBIL_FILTER}" | grep "${PROGRAM_SUCCESS}" > ${INST_TEMPFILE}
  fi

  fileDiff=`diff -q ${UNINST_TEMPFILE} ${INST_TEMPFILE}`
  if [ "${fileDiff}" != "" ]
  then
    echo "${fileDiff}"
    diff ${UNINST_TEMPFILE} ${INST_TEMPFILE}
    exit 1
  fi
}

# Instrument the program with BasicBlockCounter. Run the instrumented
# binary and the original binary. Save the output for future analysis
# Set -e should cause the script to exit if there was a problem with
# instrumentation or running the instrumented application
instrument_and_run()
{
  set -e

  local inputFile="${PROGRAM}.${PROGRAM}.t00000001.lbb"

  if [ "${INPUT_DEV_NULL}" -ne "0" ]
  then
    inputFile="/dev/null"
  else
    getImportantBlocks.py "${PROGRAM}.r"*".jbbinst"
    if [ ! -f ${inputFile} ]
    then
      echo "${PROGRAM} did not return any important blocks."
      echo "Not instrumenting with CacheSimulation"
      exit 0
    fi
  fi

  if [ "${USING_OMP}" -ne "0" ]
  then
    echo "Instrumenting ${PROGRAM} with --threaded and input ${inputFile}"
    pebil --silent --tool CacheSimulation --app ${PROGRAM} --threaded --inp ${inputFile}
  else
    echo "Instrumenting ${PROGRAM} normally with input ${inputFile}"
    pebil --silent --tool CacheSimulation --app ${PROGRAM} --inp ${inputFile}
  fi

  echo "Running ${INST_PROGRAM}"
  ./${INST_PROGRAM} &> ${INST_OUTFILE}
  echo "Running ${PROGRAM}"
  ./${PROGRAM} &> ${UNINST_OUTFILE}

  set +e
}


#####################################################################
############################Main Program#############################
#####################################################################

CALLED_DIR=`pwd`

echo "Called from ${CALLED_DIR} and changing to ${CD_DIR}"
cd ${CD_DIR}

if [ "${TEST_CHECK}" == "INST_AND_RUN" ]
then
  instrument_and_run
elif [ "${TEST_CHECK}" == "CHECK_BIN_OUTPUT" ]
then
  check_binary_output
else
  echo "ERROR: Not a valid test parameter"
  exit
fi

cd ${CALLED_DIR}
echo "Changed back to ${CALLED_DIR}"
