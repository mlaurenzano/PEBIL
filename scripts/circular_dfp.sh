#!/usr/bin/env bash

sfile=$1

if [ "$sfile" == "" ]; then
    echo "usage: $0 <static_file>"
    exit -1
fi

if [ -f $sfile ]; then
    grep -v "^#" $sfile | grep -v "+" | awk '{print $2 " dfTypePattern_Gather"}'
else
    echo "file not found: $sfile"
    exit -1
fi
