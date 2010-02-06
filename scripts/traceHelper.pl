#!/bin/perl

use strict;
use Cwd;
use Getopt::Long;
use File::Path;

my %action_hash = ( "jbbinst" => 1,"siminst" => 1,"jbbcoll" => 1,"simcoll" => 1);
my %size_hash   = ( "standard" => 1,"medium" => 1,"large" => 1 );

my $help_string = undef;
my $readme_string = undef;
my $top_pmacinst_dir = "pmacTRACE";

sub printUsage {
    my ($script,$msg) = @_;
    print "$help_string\n";
    print "$msg\n\n";
    exit;
}

sub checkForSuccess {
    my $array_ptr = shift @_;
    my @result_lines = @{ $array_ptr };
    my $success = 0;
    foreach my $line ( @result_lines ){
        chomp($line);
        if($line =~ /SUCCESS.*SUCCESS.*SUCCESS/){
            $line =~ s/.*://g;
            $line =~ s/^/\t\t/g;
            $success = 1;
            print "$line\n\n";
            last;
        }
    }
    return $success;
}

sub call_ls {
    my ($dir,$pattern,$buffer) = @_;
    my @all_lines = `ls -1 $dir 2> /dev/null`;
    my $index = 0;
    foreach my $file ( @all_lines ){
        chomp($file);
        if($file =~ /$pattern/){
            $$buffer[$index++] = $file;
        }
    }
}
sub satisfy_jbbinst {
    my ($exec_file,$target_for_files,$run_dir,$pmacinst_exec,$inst_lib_top_dir,$no_detail_print) = @_;

    die "Error : *** Executable $exec_file does not exist\n" unless(-e $exec_file);

    my $run_command = "$pmacinst_exec --typ jbb --app $exec_file" .
                      (!defined($no_detail_print) ? " --dtl" : "") .
                      (defined($inst_lib_top_dir) ? " --lib $inst_lib_top_dir" : "");
    my $insted_exec = "$exec_file.jbbinst";
    my $static_file = "$insted_exec.static";

    print "\nInform : Running \"$run_command\"\n";
    my @result_lines = `$run_command`;

    die "Error : *** Run \"$run_command\" manually and see why it did not succeed\n" 
            unless &checkForSuccess(\@result_lines);

    if((-e $insted_exec) && (-e $static_file)){
        `cp $insted_exec $target_for_files`;
        `cp $static_file $target_for_files`;
        if(defined($run_dir)){
            `cp $insted_exec $run_dir`;
            print "Inform : Instrumented execuatble is also copied under $run_dir for running\n";
        }

    } else {
        die "Error : *** Expected files are not generated.\n" .
            "Run \"$run_command\" manually snd wee why.\n" .
            "Expecting\n$insted_exec\nor\n$static_file\n";
    } 

    print "Inform : Copied under $target_for_files\n";
    print "Inform : \t$insted_exec\n";
    print "Inform : \t$static_file\n";
}

sub include_dfp_blocks {
    my ($lbb_file,$dfp_file) = @_;

    open(LBB_FILE,">>",$lbb_file) or die "Error : $lbb_file can not be opened for append\n";
    open(DFP_FILE,"<",$dfp_file) or die "Error : $dfp_file can not be opened for reading\n";
    my $line_number = 0;
    while(my $line = <DFP_FILE>){
        $line_number++;
        chomp($line);
        $line =~ s/#.*$//g;
        $line =~ s/^\s+//g;
        if($line eq ""){
            next;
        }
        $line =~ s/^\s+//g;
        $line =~ s/\s+$//g;
        $line =~ s/\s+/ /g;
        my @tokens = split /\s+/, $line;
        die "Error : line is invalid at line $line_number of $dfp_file\n"
            unless ($tokens[0] =~ /\d+/);
        print LBB_FILE $tokens[0] . " # DFPattern $line\n";
    }
    close(DFP_FILE) or die "Error : $dfp_file can not be closed\n";
    close(LBB_FILE) or die "Error : $lbb_file can not be closed\n";
}

sub satisfy_siminst {
    my ($app_name,$cpu_count,$exec_file,$jbb_trace_dir,
        $phase_count,$jbb_static,$target_for_files,$run_dir,
        $pmacinst_exec,$simulation_type,$dump_type,
        $bb_significance,$select_visit_only,$select_per_rank,$disable_loop_level,
        $inst_lib_top_dir,$no_detail_print,$dfp_file) = @_;

    die "Error : *** Executable $exec_file does not exist\n" 
            unless(-e $exec_file);
    die "Error : *** Jbb trace directory $jbb_trace_dir does not exist\n" 
            unless(-e $jbb_trace_dir);
    die "Error : *** Static file $jbb_static does not exist\n" 
            unless(-e $jbb_static);
    die "Error : *** DFPattern file does not exist\n" 
            if(defined($dfp_file) && !-e $dfp_file);
    die "Error : DFPattern can be used when only 1 phase is used\n"
        if(defined($dfp_file) && ($phase_count > 1));

    my $exec_str = $exec_file;
    $exec_str =~ s/.*\///g;

    my $jbb_file_name_templ = "$exec_str.*\\.meta_.*\\.jbbinst";

    print "Inform : Listing files that follow $jbb_file_name_templ at $jbb_trace_dir\n";

    my @result_lines;
    &call_ls($jbb_trace_dir,$jbb_file_name_templ,\@result_lines);
    my $result_line_count = scalar @result_lines;

    die "Error : *** Jbb trace file count ($result_line_count) != CPU count ($cpu_count)\n"
            unless (@result_lines  == $cpu_count);

    my $addition_args = "";
    if(defined($bb_significance)){
        $addition_args = $addition_args . " --significance $bb_significance";
    }
    if(defined($select_visit_only)){
        $addition_args = $addition_args . " --visit_only";
    }
    if(defined($select_per_rank)){
        $addition_args = $addition_args . " --per_rank_on";
    }

    my $run_command = "selectSimBlocks.pl --block_info $jbb_static --application $app_name " .
                      "--cpu_count $cpu_count --source_dir $jbb_trace_dir " .
                      "--phase_count $phase_count --exec_name $exec_str $addition_args";

    print "\nInform : Running \"$run_command\"\n";
    my @result_lines = `$run_command`;

    die "Error : *** Run \"$run_command\" manually and see why it did not succeed\n"
        unless &checkForSuccess(\@result_lines);

    for(my $i=1;$i<=$phase_count;$i++){

        my $phs_dir = "$target_for_files/" . sprintf("p%02d",$i);
        mkdir "$phs_dir" or 
                warn "Warnin : $phs_dir already exists, files will be overwritten\n";

        my $dfp_dir = "$target_for_files/" . sprintf("p%02d",$i) . "/dfp";
        if(defined($dfp_file)){
            mkdir "$dfp_dir" or 
                warn "Warnin : $dfp_dir already exists, files will be overwritten\n";
        }

        my $lbb_file = "$jbb_trace_dir/$app_name.phase.$i" . "o" . "$phase_count." . 
                       sprintf("%04d",$cpu_count) . ".jbbinst.lbb";

        die "Error : Lbb file $lbb_file was not generated\n" unless (-e $lbb_file);

        if(defined($dfp_file)){
            &include_dfp_blocks($lbb_file,$dfp_file);
            `cp $lbb_file $dfp_dir`;
            `cp $dfp_file $dfp_dir/$exec_str.$cpu_count.inp`;
        }

        `cp $lbb_file $phs_dir`;

        $run_command = "$pmacinst_exec --typ $simulation_type --app $exec_file --inp $lbb_file " .
                       "--dmp $dump_type" . 
                       (defined($disable_loop_level) ? "" : " --lpi") .
                       (!defined($no_detail_print) ? " --dtl" : "") .
                       (defined($dfp_file) ? " --dfp $dfp_file " : "") .
                       (defined($inst_lib_top_dir) ? " --lib $inst_lib_top_dir" : "") .
                       " --phs $i --ext " . sprintf("%04d",$cpu_count) . ".siminst";

        $lbb_file =~ s/.*\//$phs_dir\//g;
        die "Error : Copying to $lbb_file failed\n" unless (-e $lbb_file);

        print "\nInform : Running \"$run_command\"\n";

        @result_lines = `$run_command`;

        die "Error : *** Run \"$run_command\" manually and see why it did not succeed\n"
            unless &checkForSuccess(\@result_lines);

        my $insted_exec = "$exec_file.phase.$i." . sprintf("%04d",$cpu_count) . ".siminst";
        my $static_file = "$insted_exec.static";
        my $static_dfp_file = "$insted_exec.dfp";

        if((-e $insted_exec) && (-e $static_file)){
            `cp $insted_exec $phs_dir`;
            `cp $static_file $phs_dir`;
            if(defined($dfp_file)){
                `cp $static_file $dfp_dir`;
                `cp $static_dfp_file $dfp_dir`;
            }
            if(defined($run_dir)){
                `cp $insted_exec $run_dir`;
                print "Inform : Instrumented execuatble is also copied under $run_dir for running\n";
            }
        } else {
            die "Error : *** Expected files are not generated.\n" .
                "Run \"$run_command\" manually snd wee why.\n" .
                "Expecting\n$insted_exec\nor\n$static_file\n";
        }

        print "Inform : Copied under $phs_dir\n";
        print "Inform : \t$insted_exec\n";
        print "Inform : \t$static_file\n";
        if(defined($dfp_file)){
            print "Inform : Copied under $dfp_dir\n";
            print "Inform : \t$static_file\n";
            print "Inform : \t$static_dfp_file\n";
        }
    }
}

sub satisfy_jbbcoll {
    my ($app_name,$cpu_count,$exec_name,$jbb_trace_dir,$target_for_files) = @_;

    die "Error : *** $jbb_trace_dir does not exist\n" unless (-e $jbb_trace_dir);

    my $jbb_file_name_templ = "$exec_name.*\\.meta_.*\\.jbbinst";

    print "Inform : Listing files that follow $jbb_file_name_templ at $jbb_trace_dir\n";

    my @result_lines;
    &call_ls($jbb_trace_dir,$jbb_file_name_templ,\@result_lines);
    my $result_line_count = scalar @result_lines;

    die "Error : *** Jbb trace file count ($result_line_count) != CPU count ($cpu_count)\n"
            unless (@result_lines  == $cpu_count);

    foreach my $line ( @result_lines ){
        $line =~ /.*meta_(\d+)\./;
        my $task_id = $1;
        # TODO : here we can check which file in particular is missing 
    }

    foreach my $file ( @result_lines ){
        `cp $jbb_trace_dir/$file $target_for_files`;
    }

    @result_lines = ();
    &call_ls($target_for_files,$jbb_file_name_templ,\@result_lines);
    $result_line_count = scalar @result_lines;

    die "Error : *** Copying jbb trace files failed ($result_line_count of $cpu_count) at $target_for_files\n"
            unless (@result_lines  == $cpu_count);
}

sub satisfy_simcoll {
    my ($app_name,$cpu_count,$exec_name,$phase_number,$sim_trace_dir,$target_for_files,$dfp_file) = @_;

    die "Error : *** $sim_trace_dir does not exist\n" unless (-e $sim_trace_dir);

    satisfy_simcoll_generic($app_name,$cpu_count,$exec_name,$phase_number,$sim_trace_dir,$target_for_files,
                            sprintf("%04d",$cpu_count)."\\.siminst");
    if(defined($dfp_file)){
        my $dfp_dir = $target_for_files . "/dfp";
        mkdir "$dfp_dir" or 
            warn "Warnin : $dfp_dir already exists, files will be overwritten\n";
        satisfy_simcoll_generic($app_name,$cpu_count,$exec_name,$phase_number,$sim_trace_dir,$dfp_dir,
                                "dfp");
    }
}

sub satisfy_simcoll_generic {
    my ($app_name,$cpu_count,$exec_name,$phase_number,$sim_trace_dir,$target_for_files,$trc_extension) = @_;

    my $sim_file_name_templ = "$exec_name.*\\.phase\\.$phase_number\\.meta_.*\\." . $trc_extension;

    print "Inform : Listing files that follow $sim_file_name_templ at $sim_trace_dir\n";

    my @result_lines;
    &call_ls($sim_trace_dir,$sim_file_name_templ,\@result_lines);
    my $result_line_count = scalar @result_lines;

    die "Error : *** Sim trace file count ($result_line_count) is different from CPU count ($cpu_count)\n"
            unless (@result_lines  == $cpu_count);

    foreach my $line ( @result_lines ){
        $line =~ /.*meta_(\d+)\./;
        my $task_id = $1;
        # TODO : here we can check which file in particular is missing 
    }

    foreach my $file ( @result_lines ){
        `cp $sim_trace_dir/$file $target_for_files`;
    }

    @result_lines = ();
    &call_ls($target_for_files,$sim_file_name_templ,\@result_lines);
    $result_line_count = scalar @result_lines;

    die "Error : *** Copying simtrace files failed ($result_line_count of $cpu_count) at $target_for_files\n"
            unless (@result_lines  == $cpu_count);
}

sub TraceHelper_Main {

    my $app_name      = undef;
    my $cpu_count     = undef;
    my $data_size     = undef;
    my $action        = undef;
    my $exec_file     = undef;
    my $phase_count   = undef;
    my $phase_number  = undef;
    my $jbb_trace_dir = undef;
    my $sim_trace_dir = undef;
    my $store_dir     = getcwd;
    my $is_help       = undef;
    my $is_readme     = undef;
    my $jbb_static    = undef;
    my $exec_name     = undef;
    my $run_dir       = undef;
    my $pmacinst_exec   = "pmacinst";
    my $simulation_type = "sim";
    my $dump_type       = "off";
    my $bb_significance   = 0.95;
    my $select_visit_only = undef;
    my $select_per_rank   = undef;
    my $disable_loop_level = undef;
    my $inst_lib_top_dir  = undef;
    my $no_detail_print   = undef;

    my $curr_dir = getcwd;

    my $dfp_file = undef;

    my $result = Getopt::Long::GetOptions (
        'action=s'           => \$action,
        'application=s'      => \$app_name,
        'dataset=s'          => \$data_size,
        'cpu_count=i'        => \$cpu_count,
        'pmacinst_dir=s'     => \$store_dir,
        'exec_file=s'        => \$exec_file,
        'phase_count=i'      => \$phase_count,
        'phase_no=i'         => \$phase_number,
        'jbb_trc_dir=s'      => \$jbb_trace_dir,
        'sim_trc_dir=s'      => \$sim_trace_dir,
        'jbb_static=s'       => \$jbb_static,
        'exec_name=s'        => \$exec_name,
        'run_dir=s'          => \$run_dir,
        'instor=s'           => \$pmacinst_exec,
        'simtyp=s'           => \$simulation_type,
        'dmptyp=s'           => \$dump_type,
        'inst_lib=s'         => \$inst_lib_top_dir,
        'readme:i'           => \$is_readme,
        'no_dtl_print:i'     => \$no_detail_print,
        'select_sig=s'       => \$bb_significance,
        'select_vis:i'       => \$select_visit_only,
        'select_rnk:i'       => \$select_per_rank,
        'select_nlp:i'       => \$disable_loop_level,
        'dfp:s'              => \$dfp_file,
        'help:i'             => \$is_help
    );

    if(!$result){
        &printUsage($0,"Error : *** Unknown option");
    }

    if(defined($is_readme)){
        print "$readme_string\n";
        exit;
    }
    if(defined($is_help)){
        print "$help_string\n";
        exit;
    }

    &printUsage($0,"Error : *** Action is required")           
            unless defined($action);
    &printUsage($0,"Error : *** Action $action is not valid")  
            unless exists $action_hash{$action};
    &printUsage($0,"Error : *** Application name is required") 
            unless defined($app_name);
    &printUsage($0,"Error : *** Application inst_lib is required. dir to instrumentor top directory")
            unless defined($inst_lib_top_dir);

    if(defined($inst_lib_top_dir)){
        my $tmpdir = "$inst_lib_top_dir/lib";
        die "Error: The inst_lib directory $tmpdir does not exist\n" unless (-e $tmpdir);
    } 

    if($action ne "jbbinst"){
        &printUsage($0,"Error : *** Cpu count is required")        
                unless defined($cpu_count);
        &printUsage($0,"Error : *** Data size is required")        
                unless defined($data_size);
        ##-LAURA   &printUsage($0,"Error : *** Size $data_size is not valid") 
        ##-LAURA        unless exists $size_hash{$data_size};
    }
    
    &printUsage($0,"Error : *** Executable file is required") 
            if(!defined($exec_file) && (($action eq "jbbinst") || ($action eq "siminst")));
    &printUsage($0,"Error : *** Phase count file is required") 
            if(!defined($phase_count) && ($action eq "siminst"));
    &printUsage($0,"Error : *** Phase number is required") 
            if(!defined($phase_number) && ($action eq "simcoll"));
    &printUsage($0,"Error : *** Path to jbb trace files is required") 
            if(!defined($jbb_trace_dir) && (($action eq "siminst") || ($action eq "jbbcoll")));
    &printUsage($0,"Error : *** Path to sim trace files is required") 
            if(!defined($sim_trace_dir) && ($action eq "simcoll"));
    &printUsage($0,"Error : *** Static file from jbb instrumentation is required") 
            if(!defined($jbb_static) && ($action eq "siminst"));
    &printUsage($0,"Error : *** Name of the executable is required") 
            if(!defined($exec_name) && (($action eq "jbbcoll") || ($action eq "simcoll")));

    print "\nInform : Running $0 from $curr_dir\n";
    print "Inform : Newly generated and already existing files will be collected under\n";
    print "         $store_dir/$top_pmacinst_dir\n";
    print "Inform : Action is \"$action\"\n";

    my $top_level_dir = "$store_dir/$top_pmacinst_dir";
    my $target_for_action = "$top_level_dir/$action";

    mkdir $store_dir or 
            warn "Warnin :  $store_dir already exists, files will be overwritten\n"; 
    mkdir $top_level_dir or 
            warn "Warnin :  $top_level_dir already exists, files will be overwritten\n"; 
    mkdir $target_for_action or 
            warn "Warnin :  $target_for_action already exists, files will be overwritten\n"; 

    my $target_for_files = "";

    if($action eq "jbbinst"){
        $target_for_files = "$target_for_action/$app_name";
        if(defined($data_size)){
            $target_for_files = $target_for_files . "_" . $data_size;
            if(defined($cpu_count)){
                $target_for_files = $target_for_files . "_" . sprintf("%04d",$cpu_count);
            }
        }
    } elsif($action eq "simcoll") {
        my $tmp_dir = "$target_for_action/$app_name" . 
                      "_" . $data_size . "_" . sprintf("%04d",$cpu_count);

        mkdir "$tmp_dir" or 
                warn "Warnin :  $tmp_dir already exists, files will be overwritten\n"; 
        $target_for_files = $tmp_dir . "/" . sprintf("p%02d",$phase_number);
    } else {
        $target_for_files = "$target_for_action/$app_name" . "_" . 
                            $data_size . "_" . sprintf("%04d",$cpu_count);
    } 

    mkdir $target_for_files or 
            warn "Warnin :  $target_for_files already exists, files will be overwritten\n"; 

    print "Inform : Files for $action will be generated/collected under $target_for_files\n";

    if($action eq "jbbinst"){
        &satisfy_jbbinst($exec_file,$target_for_files,$run_dir,$pmacinst_exec,$inst_lib_top_dir,$no_detail_print);
    } elsif($action eq "siminst"){
        &printUsage($0,"Error : *** Type of the simulation is incorrect")
            unless (($simulation_type eq "sim") || ($simulation_type eq "csc"));
        &printUsage($0,"Error : *** Type of dump is incorrect")
            unless (($dump_type eq "off") || ($dump_type eq "on") || ($dump_type eq "nosim"));
        die "Error : DFPattern file \"$dfp_file\" does not exist\n" if(defined($dfp_file) && !-e $dfp_file);
        &satisfy_siminst($app_name,$cpu_count,$exec_file,$jbb_trace_dir,
                         $phase_count,$jbb_static,$target_for_files,$run_dir,
                         $pmacinst_exec,$simulation_type,$dump_type,
                         $bb_significance,$select_visit_only,$select_per_rank,$disable_loop_level,
                         $inst_lib_top_dir,$no_detail_print,$dfp_file);
    } elsif($action eq "jbbcoll"){
        &satisfy_jbbcoll($app_name,$cpu_count,$exec_name,$jbb_trace_dir,$target_for_files);
    } elsif($action eq "simcoll"){ 
        &satisfy_simcoll($app_name,$cpu_count,$exec_name,$phase_number,$sim_trace_dir,$target_for_files,$dfp_file);
    } 

    print "\n\n*** DONE *** SUCCESS *** SUCCESS *** SUCCESS *****************\n";
}


$help_string =<<HELP_STRING;

usage: $0
  --action       (jbbinst|siminst|jbbcoll|simcoll)
  --application  <application_name>              
  --dataset      (standard|medium|large) 
  --cpu_count    <cpu_count>             
  --exec_file    <executable_path>       
  --exec_name    <executable_name>       
  --phase_count  <phase_count>           
  --phase_no     <phase_number>          
  --jbb_trc_dir  <dir_path_to_jbb_traces>    
  --sim_trc_dir  <dir_path_to_sim_traces>    
  --jbb_static   <jbbinst_static_file>   
  --run_dir      <run_dir_for_jbb_execution> 
  --pmacinst_dir <collection_dir_for_traces_and_execs>
  --inst_lib     <top_directory_of_inst_libs>
  --dfp          ?<filename>
  --readme
  --help

(for testing and alterations. only by developers and variations)
  --instor       <name_of_the_instrumentor>
  --simtyp       (sim|csc)
  --dmptyp       (off|on|nosim)
  --select_sig   <bb_selection_percentile>
  --no_dtl_print ? disable the detailed print of file and line info
  --select_vis   ? select blocks wrt to the visit counts only
  --select_rnk   ? select blocks from each rank seperately
  --select_nlp   ? disable loop level block inclusion

HELP_STRING

$readme_string = <<DESCRIPTION_STR;

Summary:
=======================================================================================

traceHelper.pl is designed to automate many of the steps in gathering a 
PMaCinst trace for an application. There are 5 steps to gathering a complete 
trace:

  1.run the original executable 
    a. obtain pmac getpid package (babbage: /site/pmac_tools/PMaCgetpid.tar.gz) 
    b. run the original executable
    c. retrieve the runtime from job output 
    d. submit the original executable and the execution time

  2.instrument given executable for basic block execution counts (jbb) 
    a. run pmacinst to get instrumented executable 
       \$ traceHelper.pl \
          --action jbbinst \
          --application hycom --dataset standard \
          --exec_file /scr/mtikir/hycom/bin/hycom.standard \
          --pmacinst_dir /home/mtikir 
    b. run the new instrumented executable to produce jbb traces
    c. submit the jbb instrumented executable 

  3.copy the jbb execution traces under collection directory
    a. run pmacinst to copy jbb trace files
       \$ traceHelper.pl \
          --action jbbcoll \
          --application hycom --dataset standard --cpu_count 59 \
          --jbb_trc_dir /scr/mtikir/hycom_jbb_trace \
          --exec_name hycom.standard \
          --pmacinst_dir /home/mtikir
    b. submit jbb trace files with runtime with jbb instrumentation

  4.instrument the executable for cache simulations in one or more phases(sim)
    a. run pmacinst to get instrumented executable 
       \$ traceHelper.pl \
          --action siminst \
          --application hycom --dataset standard --cpu_count 59 \
          --exec_file /scr/mtikir/hycom/bin/hycom.standard \
          --phase_count 1 \
          --jbb_trc_dir /scr/mtikir/hycom_jbb_trace
          --jbb_static /home/mtikir/hycom_standard/hycom.standard.jbbinst.static
          --pmacinst_dir /home/mtikir 
          --dfp /scr/mtikir/hycom_dfpattern_file
    b. run the new instrumented executable to gather traces for cache simulation
    c. submit the sim instrumented executable

  5.copy the sim execution traces for a given phase under collection directory
    a. run pmacinst to copy sim trace files
       \$ traceHelper.pl \
          --action simcoll \
          --application hycom --dataset standard --cpu_count 59 \
          --phase_no 1 --sim_trc_dir /scr/mtikir/hycom_sim_trace \
          --exec_name hycom.standard \
          --pmacinst_dir /home/mtikir 
          --dfp
    b. submit sim trace files with runtime with sim instrumentation 

  A sample data submission command is as follows. Note that this command 
  includes options to submit all the necessary data at once. However, users 
  are encouraged to submit files after each step above. More information on 
  data submission can be found by typing pmacSubmit --help.

    \$ pmacSubmit \
       --project ti07 --round 1 \
       --application hycom --dataset standard --cpu_count 59 \
       --pmacinst_dir /home/mtikir/pmacTRACE \
       --exec /scr/mtikir/hycom/bin/hycom.standard,1234 \
       --jbbinst \
       --jbbcoll 2543 \
       --siminst p01 --num_phases 1 \
       --simcoll p01=6574 \
       --mpidtrace /scr/mtikir/hycom/bin/hycom_trf.trf.gz,1300 \
       --notify mtikir\@sdsc.edu

Below is more detail on each step and the tool.
================================================================================
$help_string

Before Invoking traceHelper.pl:
============================
  Please make sure you include the /site/pmac_tools/bin directory in your path, 
  and set the PMACINST_LIB_HOME variable to /site/pmac_tools. Users are
  encouraged to include the following lines in their .cshrc or .profile files.

  tcsh:
  setenv PMACINST_LIB_HOME /site/pmac_tools
  setenv PATH \$PMACINST_LIB_HOME/bin:\$PATH

  bash:
  export PMACINST_LIB_HOME=/site/pmac_tools
  PATH = ( \$PMACINST_LIB_HOME/bin \$PATH )


Descriptions of Arguments:
==========================
 action      : Trace action. 
                 jbbinst : instruments for basic block execution count (jbb) 
                 siminst : instruments for cache simulation (sim) tracing
                 jbbcoll : collects jbb traces generated
                 simcoll : collects sim traces generated
 application : Name of the application. Note that application name
               is not necessarily same as the name of the executable. 
               For example, for hycom, there are two executables, 
               hycom.standard and hycom.large, but in both case application 
               name should be hycom. (this should be all lower case)
 dataset     : Input data size. Can be one of standard, medium, and large.
 cpu_count   : Number of MPI tasks.
 pmacinst_dir: Collection directory where all files generated by instrumentation
               or tracing will be copied under. It is recommended one directory
               is used for all tracing activities for an application as this 
               target directory is given to the submission script pmacSubmit. 
               The same collection directory can also be used for different 
               applications.
 exec_file   : Path to the executable file.
 phase_count : Number of phases in which cache simulation will be completed.
 phase_no    : Phase number of the trace files collected.
 jbb_trc_dir : Directory the trace files generated by jbb tracing are located.
 sim_trc_dir : Directory the trace files generated by sim tracing are located.
 jbb_static  : Static file generated by jbb instrumentation.
 exec_name   : Name of the executable. Note that this is name of the executable
               not a full path.
 run_dir     : The run directory for jbb tracing. 
 help        : Displays the help information.

(for testing and alterations. only by developers and variations)
 instor      : Name of the instrumentor. 
 simtyp      : Type of the instrumentor. It can be sim or csc. Default is sim
               and it simulates caches only. csc simulates caches as well as 
               stores basic block execution freuencies. This is mainly designed 
               for codes that handles load balancing dynamically. If the basic
               blocks executed during jbb runs may be different than the runs for
               cache simulation, this type needs to be used.
 dmptyp      : Invoke this flag to change the options used to dump the address stream
               to disk. `off' is the default, which does not dump the stream to disk.
               `on' dumps the stream and performs simulation on it. `nosim' dumps the 
               stream and does not perform simulation.
 select_sig  : Basic block selection percentile for siminst instrumentation. 
               The default value is 95.0.
 select_vis  : Enabling basic block selection for siminst using visit count only.
               Default behavior is to choose according to the number of memory
               instructions executed.
 select_rnk  : Enabling basic block selection using per rank rather than global 
               information. Default is that the counts are totaled for all ranks
               and basic blocks are chosen. This flag enables sleecting blocks per
               rank and unioning all for siminst instrumentation.
 select_nlp  : Disable loop basic block inclusion for siminst. Default is to include 
               all blocks in a loop if any of the loop blocks is included in lbb. This
               option disables it.
 inst_lib    : The top directory of the instrumentation libraries that will be used.
               Note that this is not the dircetory where the shared libs reside, but
               rather a directory above. 
 no_dtl_print: disables the printing line number and loop information in the static files too
 dfp         : passes the input file for DFPattern analysis for siminst action
               and informs to collect .dfp trace files for simcoll action. The DFPattern
               file lis a list of basic block ids and their DFPattern type.

Description:
============

  This script enables users to instrument and collect files generated 
  during tracing for Technology Insertion rounds for a given application. 

  The main goal is to store all files related to tracing under one top directory
  (given with --pmacinst_dir option) that later can be used to submit data. 
  (This directory should be placed in a \$HOME directory rather than work or 
  scratch directory). It is recommended that the same --pmacinst_dir directory 
  is used as an argument for an application. This script handles directory 
  making and copying files, hence, users of this script should keep in mind that
  if the script is invoked more than once with the same arguments, some files 
  will be overwritten.

  All files are stored/copied under a user provided directory given with the 
  option --pmacinst_dir. The directory hierarchy is predefined based on the 
  functionality of this script. There is a pmacTRACE directory below the 
  directory given with --pmacinst_dir option which includes tracing related 
  files. Note that when you see a directory names pmacTRACE anywhere in your 
  work area, it is likely that this directory is the outcome of a call to this 
  script.

  Examples below show sample usages of this script on hycom assuming the name 
  of the executable is hycom.standard and is located at /scr/mtikir/hycom/bin/. 
  It also assumes files for pmacSubmit will be collected under /home/mtikir 
  (use directory under \$HOME to prevent any purging).  For these examples, it 
  is also assumed simulations will be completed in only 1 phase. 
  
  After hycom is traced as shown ine examples, the collection directory 
  structure will look like as follows: 

  /home/mtikir/pmacTRACE/jbbinst
  /home/mtikir/pmacTRACE/jbbinst/hycom_standard
  /home/mtikir/pmacTRACE/jbbcoll
  /home/mtikir/pmacTRACE/jbbcoll/hycom_standard_0059
  /home/mtikir/pmacTRACE/siminst
  /home/mtikir/pmacTRACE/siminst/hycom_standard_0059
  /home/mtikir/pmacTRACE/siminst/hycom_standard_0059/p01
  /home/mtikir/pmacTRACE/simcoll
  /home/mtikir/pmacTRACE/simcoll/hycom_standard_0059
  /home/mtikir/pmacTRACE/simcoll/hycom_standard_0059/p01

  This script provides four different functionalities. 
    
1. "jbbinst" takes an executable and instruments for JBB tracing.

    The instrumented executable and static files generated during 
    instrumentation are stored under jbbinst directory under the target 
    directory defined with the --pmacinst_dir flag. Since an application may 
    have more than one executable depending on input data size (hycom.standard,
    hycom.large), this script takes both the application name(hycom) and path to
    the executable itself and stores instrumentation files under different 
    directories using the actual name of application. Moreover, if the data size
    and/or cpu count is provided (which are optional to this action) the output 
    directory will include these arguments to store the files generated. If you
    have different executables for standard or large case, or for a cpu count, 
    it is advised that the user includes this option parameters.
    
    Naturally, this step is the first step to trace an application.  After this 
    step, the user can copy the instrumented executable to any location and run 
    it the same as the original executable.

    Required arguments for jbbinst are:
      --action --application --pmacinst_dir --exec_file

    Optional arguments for jbbinst are:
      --dataset --cpu_count

    For example:

    \$ traceHelper.pl \
          --action jbbinst \
          --application hycom --dataset standard \
          --exec_file /scr/mtikir/hycom/bin/hycom.standard \
          --pmacinst_dir /home/mtikir 

    makes pmacTRACE/jbbinst/hycom_standard directory under pmacinst_dir 
    collection directory and stores instrumented executable and static file(s) 
    about the instrumentation. The instrumented executable has a suffix .jbbinst
    following the original executable name.

    User needs to copy the instrumented executable, hycom.standard.jbbinst, to 
    the directory where it will be run (i.e./scr/mtikir/hycom_jbb_trace) 
    (alternatively, --run_dir option can also be used) and use this executable 
    instead of the original to submit the batch job. 
    
    User needs to submit files under pmacTRACE/jbbinst directory using -jbbinst 
    option of pmacSubmit. 

2. "jbbcoll" takes a directory where the trace files for jbb instrumentation are
    located and copies them under jbbcoll directory under the target directory 
    (e.g. --pmacinst_dir).

    When an jbb-instrumented executable runs, it generates trace files with an
    extension of .jbbinst. This functionality checks whether the number of these
    trace files are the same as expected and if so, copies them under the target
    directory. 

    Required arguments for jbbcoll are:
      --action --application --dataset --cpu_count --pmacinst_dir 
      --jbb_trc_dir --exec_name

    For example:

    \$ traceHelper.pl \
          --action jbbcoll \
          --application hycom --dataset standard --cpu_count 59 \
          --jbb_trc_dir /scr/mtikir/hycom_jbb_trace \
          --exec_name hycom.standard \
          --pmacinst_dir /home/mtikir

    makes pmacTRACE/jbbcoll/hycom_standard_0059 directory under pmacinst_dir
    collection directory and copies the jbb execution traces from the run 
    directory. User needs to run jbbcoll action after jbb instrumented 
    executable completes successfully. 

    User needs to submit files under pmacTRACE/jbbcoll directory using -jbbcoll 
    option of pmacSubmit.

3. "siminst" takes an executable, a static file generated during jbb
    instrumentation and location of jbb trace files, and instruments the
    executable for cache simulation. 
    
    User is also expected to provide the number of phases needed for cache 
    simulation tracing so that this script can instrument the executable for 
    each phase separately.  

    This script automatically handles generation of "list-of-bb" files for each 
    phase as well as instrumentation. The instrumented files are stored under 
    siminst directory under the target directory. Additionally, files related 
    to each phase are stored separately under pNN directories.

    The number of phases is determined by the amount of time your tracing will 
    take to complete. The slowdown for siminst tracing is ~6x. If the runtime of
    your application multiplied by 6 is larger than your batch queue limit, you 
    need more than one phase. Please contact Laura lcarring\@sdsc.edu for more 
    information on determining the number of phases.

    Naturally, this step should be executed after jbb tracing is completed. The
    user can copy the instrumented executables to any location and run them the 
    same as the original executable itself.

    Required arguments for siminst are:
      --action --application --dataset --cpu_count --pmacinst_dir
      --exec_file --phase_count --jbb_trc_dir --jbb_static 

    For example:

    \$ traceHelper.pl \
          --action siminst \
          --application hycom --dataset standard --cpu_count 59 \
          --exec_file /scr/mtikir/hycom/bin/hycom.standard \
          --phase_count 1 \
          --jbb_trc_dir /scr/mtikir/hycom_jbb_trace
          --jbb_static /home/mtikir/hycom_standard/hycom.standard.jbbinst.static
          --pmacinst_dir /home/mtikir 

    makes pmacTRACE/siminst/hycom_standard_0059/p01 directory under pmacinst_dir
    collection directory and stores instrumented executable and static file(s) 
    about the cache simulation instrumentation. The instrumented executable has 
    a suffix .siminst following the original executable name.

    User needs to copy the instrumented executable, 
    hycom.standard.phase.1.0059.siminst, to the directory where it will be run 
    (i.e. /scr/mtikir/hycom_sim_trace) and use this executable instead of the 
    original to submit the batch job.

    User needs to submit files under pmacTRACE/siminst directory using -siminst 
    option of pmacSubmit.

 4. "simcoll" takes a directory where the trace files for cache simulation
    instrumentation are located and copies them under simcoll directory under 
    the target directory.

    When an executable instrumented for cache simulation is run, it generates
    trace files with an extension of .siminst . This functionality checks 
    whether number of these trace files are same as expected and if so, copies 
    them under the target directory. 

    Required arguments for simcoll are:
      --action --application --dataset --cpu_count --pmacinst_dir
      --phase_no --sim_trc_dir --exec_name

    For example:

    \$ traceHelper.pl \
          --action simcoll \
          --application hycom --dataset standard --cpu_count 59 \
          --phase_no 1 --sim_trc_dir /scr/mtikir/hycom_sim_trace \
          --exec_name hycom.standard \
          --pmacinst_dir /home/mtikir 

    makes pmacTRACE/simcoll/hycom_standard_0059/p01 directory under pmacinst_dir
    collection directory and copies the execution traces for cache simulation 
    from the run directory. User needs to run simcoll action after instrumented 
    executable completes successfully.

    User needs to submit files under pmacTRACE/jbbcoll directory using -simcoll 
    option of pmacSubmit.

 Please note that each functionality requires a different combination of 
 arguments in traceHelper.pl. This script fails if a required argument is 
 missing. It also ignores if an argument is provided for a functionality that 
 is not needed. Please refer to the examples provided above to see combinations
 of arguments needed for each functionality. 

================================================================================
DESCRIPTION_STR

&TraceHelper_Main;
