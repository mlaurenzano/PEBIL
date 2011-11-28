#!/usr/bin/env bash
# computes lookahead average from the output for pebil_full_results.py, and prints the results
# only for cpu 0

trace_file=$1
if [ "$trace_file" == "" ]; then
    cat /dev/stdin > tmpfile
    trace_file=tmpfile
fi

head -n1 $trace_file | awk '{for (i = 1; i < 45; i++) { printf $i " "; } print "<n_dud_int> <tot_dud_int> <n_dud_fp> <tot_dud_fp>"; }'
cat $trace_file | awk '{if ($1 == 0) { print $_ }}' | tr ":" " "  | awk '{for (i = 1; i < 45; i++) { printf $i " "; }  ni=0; ti=0; nf=0; tf=0; for (i=45; i <= NF; i += 3) { vi=i+1; vf=i+2; ni+=$vi; ti+=($i*$vi); nf+=$vf; tf+=($i*$vf); } print ni " " ti " " nf " " tf; }'

rm tmpfile