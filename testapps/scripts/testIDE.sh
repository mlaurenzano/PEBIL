#!/bin/bash

#####################################################################
#############################Parameters##############################
#####################################################################
PROGRAM=$1
CD_DIR=$2 # Right now this only makes sense to run it in the same dir
          # as the binary..

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

#####################################################################
############################Other Globals############################
#####################################################################
INST_PROGRAM="${PROGRAM}.ideinst"
UNINST_OUTFILE="${PROGRAM}.outp"
INST_OUTFILE="${INST_PROGRAM}.outp"
UNINST_TEMPFILE="${PROGRAM}.tmp"
INST_TEMPFILE="${INST_PROGRAM}.tmp"

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
    cat ${UNINST_OUTFILE} > ${UNINST_TEMPFILE}
    cat ${INST_OUTFILE} > ${INST_TEMPFILE}
  else
    cat ${UNINST_OUTFILE} | grep "${PROGRAM_SUCCESS}" > ${UNINST_TEMPFILE}
    cat ${INST_OUTFILE} | grep "${PROGRAM_SUCCESS}" > ${INST_TEMPFILE}
  fi

  fileDiff=`diff -q ${UNINST_TEMPFILE} ${INST_TEMPFILE}`
  if [ "${fileDiff}" != "" ]
  then
    echo "${fileDiff}"
    diff ${UNINST_TEMPFILE} ${INST_TEMPFILE}
    exit
  fi
}

# Instrument the program with BasicBlockCounter. Run the instrumented
# binary and the original binary. Save the output for future analysis
# Set -e should cause the script to exit if there was a problem with
# instrumentation or running the instrumented application
instrument_and_run()
{
  set -e

  echo "Instrumenting ${PROGRAM} normally"
  pebil --silent --typ ide --app ${PROGRAM}

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
