#!/usr/bin/env bash

#usage="usage: $0 cpu_count program arg1 arg2 ..."

cpu_count="$1"
shift;
app="$1"
shift;

# for MPI runs
#app_run_prefix="time mpirun -np $cpu_count"
app_run_prefix="time"
run_msg="RUNNING ==> "

# collect all command line options into args
args=""
while [ $# -ne 0 ] ; do
    args="$args $1"
    shift
done

log_file="$app.tracelog"
echo "Writing tracing activity log to $app.tracelog"
echo "" > "$log_file"

run_log_cmd() { echo "$run_msg $1" | tee -a "$log_file" 2>&1; $1 >> "$log_file" 2>&1; }

run_log_cmd "$app_run_prefix $app $args"
run_log_cmd "pebil --typ jbb --dtl --app $app"
run_log_cmd "$app_run_prefix $app.jbbinst $args"
run_log_cmd "selectSimBlocks.pl --block_info $app.jbbinst.static --exec_name `basename $app` --application $app --cpu_count $cpu_count"
run_log_cmd "pebil --typ sim --inp $app.phase.1o1.`printf %04d $cpu_count`.jbbinst.lbb --dtl --lpi --app $app"
run_log_cmd "$app_run_prefix $app.siminst $args"

echo ""
echo "see $app.tracelog for details"
echo "********** SUCCESS SUCCESS SUCCESS ***********"
exit 0
