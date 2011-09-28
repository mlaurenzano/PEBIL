#!/usr/bin/env bash

# converts a legacy loopcnt file into a current one. there is no problem
# if you pass a current loopcnt file to this script... it will not ruin anything
#
# WARNING: this function overwrites the file you pass as an arg

unqtoken="LOOPCNT_CONVERT_THIS_LINE_MARKER"

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

loopcnt=$1

if [ "$loopcnt" == "" ]; then
    print_usage "no loopcnt file given"
fi
if [ ! -f "$loopcnt" ]; then
    print_usage "cannot find loopcnt file: $loopcnt"
fi

tmp=`mktemp`
outf=`mktemp`

while read line; do
    if [ "${line:0:1}" == "#" ]; then
        echo "$line" >> $tmp
    else
        set $line
        # don't put anything between "set" and this comparison
        if [ "$#" != "5" ]; then
            echo_err ""
            echo_err "!!!!! error: malformed loopcnt line: $line"
            exit 1
        fi
        echo "$unqtoken $line" >> $tmp
    fi
done < $loopcnt

cat $tmp | awk -v unqtoken="$unqtoken" '{ if ($1 == unqtoken){ print $6 "\t" $3 "\t" $4 "\t" $5 "\t" $6 } else { print } }' > $outf

echo "***** inform: overwriting $loopcnt"
mv -f $outf $loopcnt
rm -f $tmp
