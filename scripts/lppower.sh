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

run_log_cmd() { echo "$run_msg $1" | tee -a "$log_file" 2>&1; $1 >> "$log_file" 2>&1; }

#run_log_cmd "/home/michaell/PEBIL/scripts/loopBlockSimu.py --application $app --cpu_count $cpu_count --trace_dir $trace_dir --system_id 77"
#head -n1 "$trace_dir/$app`printf _%04d $cpu_count`_sysid77.loops"
#grep -v -P "^#" "$trace_dir/$app`printf _%04d $cpu_count`_sysid77.loops"
run_log_cmd "pebil --typ thr --app $app --trk /dev/null --inp $inp --lnc libcpufreq.so,libpower.so"
cat $inp
#run_log_cmd "pebil --typ thr --app $app --trk /dev/null --inp $app.hand.loops --lnc libcpufreq.so,libpower.so"

i=0
while [ $i -lt $num_runs ]
do
#    run_log_cmd "./prep_hycom.sh"
    run_log_cmd "$app_run_prefix $app.thrinst"
    i=$[$i+1]
    sleep 5
done

joules_total=0.0
for j in `grep joules $log_file | tail -n "$num_usable_runs" | awk '{print $17}'`; do joules_total=`echo "$joules_total + $j" | bc`; done
joules_avg=`echo "$joules_total / $num_usable_runs" | bc`
run_log_cmd "echo JOULES_THR $joules_avg"

grep -i joules "$log_file" | awk '{ print $10 " " $13 " " $15 " " $17 }'

#run_log_cmd "loopBlockSimu.py --application $app --cpu_count $cpu_count --trace_dir . --system_id 77 --throttle_full"
#run_log_cmd "loopBlockSimu.py --application $app --cpu_count $cpu_count --trace_dir . --system_id 64 --throttle_full"
#head "$app`printf _%04d $cpu_count`_sysid77.loops"
run_log_cmd "pebil --typ thr --app $app --trk /dev/null --inp /dev/null --lnc libcpufreq.so,libpower.so"

i=0
while [ $i -lt $num_runs ]
do
    run_log_cmd "./prep_hycom.sh"
    run_log_cmd "$app_run_prefix $app.thrinst"
    i=$[$i+1]
    sleep 5
done

joules_total=0.0
for j in `grep joules $log_file | tail -n "$num_usable_runs" | awk '{print $17}'`; do joules_total=`echo "$joules_total + $j" | bc`; done
joules_avg=`echo "$joules_total / $num_usable_runs" | bc`
run_log_cmd "echo JOULES_FULL $joules_avg"

grep -i joules "$log_file" | awk '{ print $10 " " $13 " " $15 " " $17 }'

echo ""
echo "see $log_file for details"
echo "********** SUCCESS SUCCESS SUCCESS ***********"
exit 0
