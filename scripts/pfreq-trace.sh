#!/usr/bin/env bash

#usage="usage: $0 cpu_count program arg1 arg2 ..."

cpu_count="$1"
shift;
app="$1"
shift;

pbench_dir=/home/michaell/power_profile
pbench_inrlp=$pbench_dir/pbench.inrlp
pbench_pwrs=$pbench_dir/1.60GHz:$pbench_dir/1.73GHz:$pbench_dir/1.86GHz:$pbench_dir/2.00GHz:$pbench_dir/2.13GHz:$pbench_dir/2.26GHz:$pbench_dir/2.39GHz:$pbench_dir/2.40GHz

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

if [ -f ./init_$app.sh ]; then
    prep="./init_app.sh"
fi

log_file="$app.tracelog"
echo "Writing tracing activity log to $app.tracelog"
echo "" > "$log_file"

run_log_cmd() { echo "$run_msg $1" | tee -a "$log_file" 2>&1; $1 >> "$log_file" 2>&1; }

#$prep
#run_log_cmd "$app_run_prefix $app $args"
run_log_cmd "pebil --typ jbb --app $app"
unify_static.py --file $app.jbbinst.static > $app.unified.static

$prep
run_log_cmd "$app_run_prefix $app.jbbinst $args"
#run_log_cmd "selectSimBlocks.pl --block_info $app.jbbinst.static --exec_name `basename $app` --application $app --cpu_count $cpu_count"
#run_log_cmd "pebil --typ sim --inp $app.phase.1o1.`printf %04d $cpu_count`.jbbinst.lbb --lpi --app $app"
run_log_cmd "pebil --typ sim --inp /dev/null --lpi --app $app"

$prep
run_log_cmd "$app_run_prefix $app.siminst $args"

# yay scripts
pebil_full_results.py --application $app --cpu_count $cpu_count --trace_dir . --system_id 77 > $app.fulltrace
cat $app.fulltrace | compute_lookahead.sh | merge_loops.py > $app.mrgloops
nearest_pbench.py --mloop $app.mrgloops --pbench $pbench_pwrs --inrlp $pbench_inrlp > $app.pwrloops

run_log_cmd "pebil --typ thr --app $app --inp $app.pwrloops --lnc libpower.so,libcpufreq.so --trk /dev/null --ext pfreq"
run_log_cmd "pebil --typ thr --app $app --inp /dev/null --lnc libpower.so,libcpufreq.so --trk /dev/null --ext nofreq"

tail -n1 $app.pwrloops

echo ""
echo "********** SUCCESS SUCCESS SUCCESS ***********"
exit 0
