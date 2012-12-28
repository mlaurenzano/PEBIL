#!/usr/bin/env bash

app=${1}

printmsg="*** BINSEARCH ***  "

################# can adjust these as necessary #########################
# the pebil instrumentation command
instr="${PEBIL_ROOT}/bin/pebil --tool BasicBlockCounter -- ${app}"
# the format of the command you want to run
cmd="${app}.jbbinst"
# can adjust down to run faster, or up for codes with more points
limit=524288


set -e

function rebuild(){
    PEBIL_SWAP_MOD=${1} PEBIL_SWAP_OFF=${2} make -C ${PEBIL_ROOT}/src cleandebug all install
}

function print_usage(){
    echo "usage: ${0} <path_to_app>"
}

function print_info(){
    echo "${printmsg} ${*}"
}

if [ "${app}" == "" ]; then
    print_usage
    exit 1
fi

if [ "${PEBIL_ROOT}" == "" ]; then
    echo "Must have the env variable PEBIL_ROOT set"
    exit 1
fi

off=0
mod=2
while [ ${mod} -le ${limit} ]; do
    print_info "checking ${off} % ${mod}"

    rebuild ${mod} ${off}
    ${instr}

    set +e
    ${cmd}
    ret=${?}
    set -e

    if [ ${ret} -eq 0 ]; then
        off=$[${off}+$[${mod}/2]]
    else
        mod=$[${mod}*2]
    fi

    if [ ${off} -ge ${mod} ]; then
        print_info "Found no errors for modulo ${mod}... quitting"
        exit 1
        break
    fi

done


print_info "Final search result: ${off}"
exit 0