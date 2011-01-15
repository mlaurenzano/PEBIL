#!/usr/bin/env bash

trace_file=$1
loop_level=2
if [ "$trace_file" == "" ]; then
    echo "usage: $0 <pebil_fulltrace_file>"
    exit -1
fi

head -n1 $trace_file | awk '{for (i = 1; i < 42; i++) { printf $i " "; } print "<n_dud_int> <tot_dud_int> <n_dud_fp> <tot_dud_fp>"; }'
# extract the test inner loops and the tokens we care about
cat $trace_file | awk '{if ($1 == 0 && $12 != 0 && $13 != -1) { print $_ }}' | tr ":" " "  | awk '{for (i = 1; i < 42; i++) { printf $i " "; }  ni=0; ti=0; nf=0; tf=0; for (i=42; i <= NF; i += 3) { vi=i+1; vf=i+2; ni+=$vi; ti+=($i*$vi); nf+=$vf; tf+=($i*$vf); } print ni " " ti " " nf " " tf; }'

#cat $trace_file | awk '{if ($13 != 0 && $14 != -1) { printf $8 " " $9 " " $3 " " $4 " " $5 " " ; } print " " }'

#for (i = 42; i <= NF; i++){ printf $i " " ; } print " "  }}'

# using the +dud info, compute fp and int-specific reuse distance totals
