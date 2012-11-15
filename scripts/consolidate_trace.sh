#!/usr/bin/env bash

prog=$0
pmactrace_dir=$1
execname=$2
appname=$3
dataset=$4
ncpus=$5

set -e

function print_err(){
    echo $1 1>&2
}

function print_usage(){
    if [ "$1" != "" ]; then
        print_err $1
    fi
    print_err "usage: $prog </path/to/pmacTRACE/> <execname> <appname> <dataset> <ncpus>"
    exit 1
}

if [ "$pmactrace_dir" == "" -o "$execname" == "" -o "$appname" == "" -o "$ncpus" == "" ]; then
    print_usage "missing argument(s)"
fi

cpustr=`printf "%04d" $ncpus`
appdir="$execname"_"$dataset"_"$cpustr"
tgtdir=$pmactrace_dir/processed/$appdir

if [ -d "$tgtdir" ]; then
    print_err "target directory already exists, consider removing: $tgtdir"
    exit 1
fi

mkdir -p $tgtdir
cp -r $pmactrace_dir/jbbinst/$appdir/* $tgtdir
cp -r $pmactrace_dir/jbbcoll/$appdir/* $tgtdir
cp -r $pmactrace_dir/siminst/$appdir/* $tgtdir
cp -r $pmactrace_dir/simcoll/$appdir/* $tgtdir
