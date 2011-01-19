#!/usr/bin/env bash

#usage="usage: $0 cpu_count program arg1 arg2 ..."

num_usable_runs=5
num_init_runs=2
num_runs=$[$num_usable_runs+$num_init_runs]

cpu_count="$1"
shift;
app="$1"
shift;
inp="$1"
shift;
trace_dir="."

# for MPI runs
app_run_prefix="time mpirun -np $cpu_count"
#app_run_prefix="time"
run_msg="RUNNING ==> "

# collect all command line options into args
args=""
while [ $# -ne 0 ] ; do
    args="$args $1"
    shift
done

log_file="$app.pwrlog_full"
echo "Writing tracing activity log to $log_file"
echo "" > "$log_file"

if [ -f $app.pfreq -a -f $app.nofreq ]; then
    echo "Running $app $num_runs times in sequence with and without frequency throttling to get energy measurements"
else
    echo "Missing $app.pfreq or $app.nofreq, have you run memtrace.sh on your app yet?"
    exit -1
fi

run_log_cmd() { echo "$run_msg $1" | tee -a "$log_file" 2>&1; $1 >> "$log_file" 2>&1; }

i=0
while [ $i -lt $num_runs ]
do
    $prep
    run_log_cmd "$app_run_prefix $app.pfreq"
    i=$[$i+1]
    sleep 5
    grep -i joules "$log_file" | awk '{ print $10 " " $13 " " $15 " " $17 }' | tail -n1
done

joules_total=0.0
for j in `grep joules $log_file | tail -n "$num_usable_runs" | awk '{print $17}'`; do joules_total=`echo "$joules_total + $j" | bc`; done
joules_avg=`echo "$joules_total / $num_usable_runs" | bc`
run_log_cmd "echo JOULES pfreq $joules_avg"


i=0
while [ $i -lt $num_runs ]
do
    $prep
    run_log_cmd "$app_run_prefix $app.nofreq"
    i=$[$i+1]
    sleep 5
    grep -i joules "$log_file" | awk '{ print $10 " " $13 " " $15 " " $17 }' | tail -n1
done

joules_total=0.0
for j in `grep joules $log_file | tail -n "$num_usable_runs" | awk '{print $17}'`; do joules_total=`echo "$joules_total + $j" | bc`; done
joules_avg=`echo "$joules_total / $num_usable_runs" | bc`
run_log_cmd "echo JOULES nofreq $joules_avg"

echo ""
echo "see $log_file for details"
echo "********** SUCCESS SUCCESS SUCCESS ***********"
exit 0
