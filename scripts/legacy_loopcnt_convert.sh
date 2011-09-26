#!/usr/bin/env bash

# converts a legacy loopcnt file into a current one. there is no problem
# if you pass a current loopcnt file to this script... it will not ruin anything
#
# WARNING: this function overwrites the file you pass as an arg

function echo_err() {
    msg=$1
    echo "$msg" >&2
}

function print_usage(){
    if [ "$1" != "" ]; then
        echo_err "!!!!! error: $1"
    fi
    echo_err "     usage: $prg <meta_loopcnt_file>"
    echo_err ""
    exit 1
}

function convert_line(){
    line=$1
    x=`echo $line | grep -Pv "^#"`
    if [ "$x" == "" ]; then
        echo "$line"
    else
        n=`echo $line | wc -w | cut --delimiter=" " --fields=1`
        if [ "$n" != "5" ]; then
            echo_err ""
            echo_err "!!!!! error: malformed loopcnt line: $line"
            exit 1
        fi
        echo "$line" | awk '{ print $5 "\t" $2 "\t" $3 "\t" $4 "\t" $5 }'
    fi
}

loopcnt=$1

if [ "$loopcnt" == "" ]; then
    print_usage "no loopcnt file given"
fi
if [ ! -f "$loopcnt" ]; then
    print_usage "cannot find loopcnt file: $loopcnt"
fi

outf=`mktemp`

while read line; do
    convert_line "$line" >> $outf
done < $loopcnt

echo "overwriting $loopcnt"
mv -f $outf $loopcnt
