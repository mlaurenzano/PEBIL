#!/usr/bin/perl
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;
use File::Path;
use Getopt::Long;
use Cwd;

######### Here are the variables and method that can be changed ##########

# fortran version
my $mmaps_source_file_name     = "MultiMAPS.f90";
# c version
#my $mmaps_source_file_name     = "fd1200_v4.c";

# fortran version
my $mmaps_exec_file_name       = "multi_maps.exe";
# c version
#my $mmaps_exec_file_name       = "c_fd1200_v4";

my $inst_line_file_name        = "lines.txt";
my $mmaps_submission_file_name = "multimaps.bat";
my $block_info_file_name       = "blocks.txt";
my $frequency_info_file_name   = "frequency.txt";
my $hitratio_info_file_name    = "hitratio.txt";
my $test_select_file_name      = "test_selection.inp";
my $maximum_system_id          = 100;
my $maximum_cache_level        = 3;
my $elimination_variance       = 0.05;

my $multimaps_source_lang      = "fortran";


my %extension_to_file_number = (
    "1_02" => 1247,
    "2_01" => 1254,
    "2_02" => 1255,
    "2_04" => 1256,
    "2_08" => 1257
);

sub isHelperBlock {
    my ($line_tokens,$stat_tokens) = @_;
    return 0;
    if($$line_tokens[1] == 1){
            return 0;
    } elsif($$line_tokens[1] == 2){
        if(($$stat_tokens[2] == 14) &&
           ($$stat_tokens[3] == 9) &&
           ($$stat_tokens[4] == 26)){

            return 1;
        }
    }
    return 0;
}

sub isExecutionBlock {
    my ($line_tokens,$stat_tokens) = @_;
    if($$line_tokens[1] == 1){
        if(($$stat_tokens[2] == 16) &&
           ($$stat_tokens[3] == 12) &&
           ($$stat_tokens[4] == 25)){

            return 1;
        }
    } elsif($$line_tokens[1] == 2){
        if(($$stat_tokens[2] == 16) &&
           ($$stat_tokens[3] == 24) &&
           ($$stat_tokens[4] == 28)){
            return 1;
        } elsif(($$stat_tokens[2] == 1) &&
                ($$stat_tokens[3] == 1) &&
                ($$stat_tokens[4] == 4)){
            return 1;
        }
    }
    return 0;
}

sub generate_submission_script {
    my($exec_name,$time_limit,$cpu_count,$tgt_path,$job_name,$batch_type) = @_;;

    my $submit_string = "No script is available";

    if($batch_type eq "ll"){

$submit_string = <<LL_STRING;
#!/bin/sh -vx
#@ output = $job_name.o\$(jobid)
#@ error = $job_name.o\$(jobid)
#@ notification = always
#@ notify_user = michaell\@sdsc.edu
#@ environment = COPY_ALL
#@ wall_clock_limit = $time_limit
#@ class = normal
#@ node = 1
#@ total_tasks = $cpu_count
#@ initialdir = $tgt_path
#@ job_type = parallel
#@ job_name = $job_name
#@ shell = /bin/sh
#@ node_usage = not_shared
#@ network.MPI = sn_all,shared,US
#@ checkpoint = no
#@ requirements = (Feature == "MEM32")
#@ account_no = CSD102
#@ queue

export MP_CSS_INTERRUPT=no
export MP_STDINMODE=0
export MP_EAGER_LIMIT=65536
export MP_INFOLEVEL=0
export MP_INTRDELAY=1
export MP_LABELIO=no
export MP_SHARED_MEMORY=yes
export MP_SINGLE_THREAD=yes
export MP_STDOUTMODE=unordered
export MP_WAIT_MODE=poll
export OMP_NUM_THREADS=1
export SPINLOOPTIME=5000
export XLSMPOPTS="parthds=1"

TBEGIN=`echo "print time();" | perl`
poe ./$exec_name
TEND=`echo "print time();" | perl`
echo "$job_name walltime: `expr \$TEND - \$TBEGIN` seconds"

LL_STRING

    } elsif($batch_type eq "lsf"){

$submit_string = <<LSF_STRING;
#!/bin/bash -vx
#BSUB -J $job_name
#BSUB -o $job_name.o\%J
#BSUB -e $job_name.e\%J
#BSUB -a poe
#BSUB -P HPCMO990
#BSUB -W $time_limit
#BSUB -q special
#BSUB -n $cpu_count
#BSUB -R "span[ptile=16]"
#BSUB -B
#BSUB -N

export MP_EAGER_LIMIT=65536
export MP_INFOLEVEL=0
export MP_INTRDELAY=1
export MP_LABELIO=no
export MP_SHARED_MEMORY=yes
export MP_SINGLE_THREAD=yes
export MP_STDOUTMODE=unordered
export MP_WAIT_MODE=poll
export OMP_NUM_THREADS=1
export SPINLOOPTIME=5000
export XLSMPOPTS="parthds=1"

TBEGIN=`echo "print time();" | perl`
mpirun.lsf ./$exec_name
TEND=`echo "print time();" | perl`
echo "$job_name walltime: `expr \$TEND - \$TBEGIN` seconds"

LSF_STRING
    }

    my $submission_file = "$tgt_path/$mmaps_submission_file_name";
    open(SUB_FILE,">",$submission_file) or die "Error: can not open $submission_file\n";
    print SUB_FILE "$submit_string\n";
    close(SUB_FILE) or die "Error: can not close $submission_file\n";

}
####################^^^^^^^^^^^^^^^^^^^^^^^^^^######################


my $help_string = "";
my $readme_string = "";

sub cleanCommentWSpace {
    my $line = shift @_;
    $line =~ s/#.*$//g;
    $line =~ s/^\s+//g;
    return $line;
}

sub prepare_org {
    my ($multimap_src_dir,$multimap_bin_dir,$multimap_etc_dir,
        $pln_run_dir,$ext_info_dir,$batch_type) = @_;

    my $test_case_str = "";
    my $test_type = -1;
    my $line_size = 0;
    my $data_size = 0;

    my $lineNumber = 0;

    my $src_file_path = "$multimap_src_dir/$mmaps_source_file_name";
    my $bin_file_path = "$multimap_bin_dir/$mmaps_exec_file_name";
    my $line_no_file  = "$ext_info_dir/$inst_line_file_name";    

    my $subroutine_match_str = "";
    my $line_number_qp_str = "";

    if ($multimaps_source_lang =~ /fortran/){
        $subroutine_match_str = "subroutine (multimap_test_case_(\\d+)_(\\d+)_(\\d+))";
        $line_number_qp_str = "LINE_NUMBER_QUERY_POINT.*(multimap_test_case_.*)";
    } else {
        $subroutine_match_str = "int32_t (c_multimap_test_case_(\\d+)_(\\d+)_(\\d+))";
        $line_number_qp_str = "LINE_NUMBER_QUERY_POINT.*(c_multimap_test_case_(\\d+)_(\\d+)_(\\d+)).*";
    }

    open(LINE_NUMBERS,">",$line_no_file) or die "Error: can not open $line_no_file\n";
    open(SOURCE_FILE,"<",$src_file_path) or die "Error: can not open $src_file_path\n";
    while(my $line = <SOURCE_FILE>){
        $lineNumber++;
        chomp($line);

        if($line =~ /$subroutine_match_str/){
            $test_case_str = $1;
            $test_type = $2;
            $line_size = $3;
            $data_size = $4;

        } elsif($line =~ /$line_number_qp_str/){

            die "Error: line does not belong to $test_case_str, as it says $line\n$test_case_str\n$1\n"
                unless($test_case_str eq $1);

            ### HERE IT WILL BE CHANGED IF MORE TEST TYPES OR LINE SIZES ARE ADDED IN THE FUTURE
            die "Error: invalid test type $test_type\n" unless((0 <= $test_type) && ($test_type <= 2));
            die "Error: invalid line size $line_size\n" unless((1 <= $line_size) && ($line_size <= 8));
            die "Error: invalid data size $data_size\n" unless((1024 <= $data_size) && ($data_size <= 32768000));

#            printf STDOUT "line numbers info: %30s\t%1d\t%2d\t%12d\t%12d\n",
#                $test_case_str,$test_type,$line_size,$data_size,$lineNumber;
            printf LINE_NUMBERS "%30s\t%1d\t%2d\t%12d\t%12d\n",
                $test_case_str,$test_type,$line_size,$data_size,$lineNumber;
        }
    }
    close(SOURCE_FILE)  or die "Error: can not close $src_file_path\n";
    close(LINE_NUMBERS) or die "Error: can not close $line_no_file\n";

    `cp $src_file_path          $ext_info_dir/$mmaps_source_file_name.src`;
    `cp $bin_file_path          $pln_run_dir`;
    `cp $multimap_etc_dir/*.inp $pln_run_dir`;

    &generate_submission_script($mmaps_exec_file_name,
                                ($batch_type eq "ll" ? "3:00:00" : "3:00"),
                                ($batch_type eq "ll" ? 8 : 16),$pln_run_dir,"mmaps_org_run",$batch_type);
}

sub check_success {
    my $success = 0;
    my @lines = @{ shift @_ };
    foreach my $line ( @lines ){
        if($line =~ /SUCCESS.*SUCCESS.*SUCCESS/){
            #print "$line\n";
            $success = 1;
            last;
        }
    }
    return $success;
}

sub prepare_jbb {
    my ($multimap_src_dir,$multimap_bin_dir,$multimap_etc_dir,
        $jbb_run_dir,$ext_info_dir,$pebil_exec,$batch_type,$inst_lib_top_dir) = @_;

    my $bin_file_path = "$multimap_bin_dir/$mmaps_exec_file_name";
    my $line_no_file  = "$ext_info_dir/$inst_line_file_name";    
    my $block_id_file = "$ext_info_dir/$block_info_file_name";    
    my $jbb_exec      = "$bin_file_path.jbbinst";
    my $jbb_stat      = "$bin_file_path.jbbinst.static";

    my $command = "$pebil_exec --app $bin_file_path --typ jbb --dtl" .
                  (defined($inst_lib_top_dir) ? " --lib $inst_lib_top_dir" : "");
    print "\nrunning ===> $command\n";
    my @lines = `$command`;

    die "Error: $pebil_exec failed running $command\n" unless &check_success(\@lines);

    die "Error: $jbb_exec does not exist\n" unless(-e $jbb_exec);
    die "Error: $jbb_stat does not exist\n" unless(-e $jbb_stat);

    `cp $jbb_exec               $jbb_run_dir`;
    `cp $jbb_stat               $ext_info_dir`;

    # copying test selection under etc of multimaps directory
    # note that make dist puts the right one
    `cp $multimap_etc_dir/*.inp $jbb_run_dir`;

    # but if you would like to hard code the following will work
    # my $select_file = "$jbb_run_dir/$test_select_file_name";
    # open(SELECT_FILE,">",$select_file) or die "Error: $select_file can not be opened\n";
    # print SELECT_FILE "5\n1 2\n2 1\n2 2\n2 4\n2 8\n";
    # close(SELECT_FILE) or die "Error: $select_file can not be closed\n";

    my %line_no_to_test_case;

    my $test_count = 0;
    open(LINE_NUMBERS,"<",$line_no_file) or die "Error: can not open $line_no_file\n";
    print "line numbers read from $line_no_file\n";
    while(my $line = <LINE_NUMBERS>){
        chomp($line);
        my @tokens = split /\s+/, $line;
        die "Error: line $tokens[-1] is defined multiple times\n"
            if(exists $line_no_to_test_case{$tokens[-1]});
        $line_no_to_test_case{$tokens[-1]} = [ @tokens ];
        $test_count++;
    }
    close(LINE_NUMBERS) or die "Error: can not close $line_no_file\n";


    my $block_count = 0;
    open(BLOCK_FILE,">",$block_id_file) or die "Error: can not open $block_id_file\n";

    my %block_id_to_stat_fields;
    genBlockIdToStatFields(\%block_id_to_stat_fields,$jbb_stat);

    my %blocks_hash;
    foreach my $key ( sort keys %block_id_to_stat_fields ){
        my @tokens = @{ $block_id_to_stat_fields{$key} };
        my $line_str = $tokens[5];
        my ($src_file,$src_lineno) = split /:/, $line_str;
        if($src_file eq $mmaps_source_file_name){
#            print "looking for line $src_lineno in file $src_file\n";
            if(exists $line_no_to_test_case{$src_lineno}){
                my @line_tokens = @{ $line_no_to_test_case{$src_lineno} };
                if(&isHelperBlock(\@line_tokens,\@tokens)){
#                    print "line $src_lineno is in execution block: @line_tokens, @tokens\n";
                    if(!defined($blocks_hash{$src_lineno}[1])){
                        $blocks_hash{$src_lineno}[1] = "";
                    }
                    $blocks_hash{$src_lineno}[1] .= "\t$tokens[1]";
                    next;
                }
                if(&isExecutionBlock(\@line_tokens,\@tokens)){
#                    print "line $src_lineno is in execution block: @line_tokens, @tokens\n";
                    my $print_str = "";
                    for(my $i=0;$i<@line_tokens;$i++){
                        $print_str .= "$line_tokens[$i]\t";
                    }
                    $print_str .= $tokens[1];
                    $blocks_hash{$src_lineno}[0] = $print_str;
                     $block_count++;
                    next;
                } 
#                print "line $src_lineno not in execution block: @line_tokens, @tokens\n";
            }
        }
    }
    my @key_arry = keys %blocks_hash;
    @key_arry = sort {$a <=> $b} @key_arry;
    foreach my $l (@key_arry){
         if(defined($blocks_hash{$l}[0])){
            print BLOCK_FILE $blocks_hash{$l}[0];
            if(defined($blocks_hash{$l}[1])){
                print BLOCK_FILE $blocks_hash{$l}[1];
            }
            print BLOCK_FILE "\n";
        }
    }
    close(BLOCK_FILE) or die "Error: can not close $block_id_file\n";

    die "FATAL: $test_count tests and $block_count blocks associated do not match. Compile with -g and try\n"
        unless ($test_count == $block_count);

    &generate_submission_script("$mmaps_exec_file_name.jbbinst",
                                ($batch_type eq "ll" ? "6:00:00" : "6:00" ),
                                1,$jbb_run_dir,"mmaps_jbb_run",$batch_type);
}

sub setSimTestCases {
    my ($ext_info_dir) = @_;

    my @test_case_array;
    my $block_id_file = "$ext_info_dir/$block_info_file_name";
    open(BLOCK_FILE,"<",$block_id_file) or die "Error: can not open $block_id_file\n";
    while(my $line = <BLOCK_FILE>){
        chomp($line);
        my @tokens = split /\s+/, $line;
        die "Error: In $block_id_file each line should have 6 tokens\n"
            unless(@tokens >= 6);
        my $test_type = $tokens[1];
        my $line_size = $tokens[2];
        $test_case_array[$test_type][$line_size] = 1;
    }
    close(BLOCK_FILE) or die "Error: can not close $block_id_file\n";

    my @ret_arr;

    ### HERE IT WILL BE CHANGED IF MORE TEST TYPES OR LINE SIZES ARE ADDED IN THE FUTURE
    for(my $i=0;$i<=2;$i++){
        for(my $j=1;$j<=8;$j=$j*2){
            if(defined($test_case_array[$i][$j])){
                push @ret_arr, "$i,$j";
            }
        }
    }

    return @ret_arr;
}

sub prepare_sim {
    my ($multimap_src_dir,$multimap_bin_dir,$multimap_etc_dir,
        $sim_top_dir,$ext_info_dir,
        $pebil_exec,$simulation_type,$batch_type,$inst_lib_top_dir) = @_;

    my $bin_file_path = "$multimap_bin_dir/$mmaps_exec_file_name";
    my $line_no_file  = "$ext_info_dir/$inst_line_file_name";    
    my $block_id_file = "$ext_info_dir/$block_info_file_name";    
    my $jbb_stat      = "$bin_file_path.jbbinst.static";

    my @test_list = setSimTestCases($ext_info_dir);

    for(my $i=0;$i<@test_list;$i++){
        my ($test_type,$line_size) = split /,/, $test_list[$i];
        print("\nGenerating simulation directory for test $test_type line $line_size\n");
        my $extension_str = sprintf("%d_%02d",$test_type,$line_size);

        my $sim_run_dir = "$sim_top_dir/$extension_str";
        mkdir "$sim_run_dir",0755  or warn "Warning: $sim_run_dir exists, files will be overwritten\n";

        my $lbb_file = "$ext_info_dir/$extension_str.lbb";
        open(LBB_FILE,">",$lbb_file) or die "Error: Can not open $lbb_file\n";

        my @candidates;
        if ($multimaps_source_lang =~ /fortran/){
            @candidates = `grep "multimap_test_case_$extension_str.*" $jbb_stat`;
        } else {
            @candidates = `grep "c_multimap_test_case_$extension_str.*" $jbb_stat`;
        }
        foreach my $candidate ( @candidates ){
            chomp($candidate);
            my @tokens = split /\s+/,$candidate;
            shift @tokens;
            $tokens[0] = $tokens[0] . "\t#\t";
            print LBB_FILE "@tokens\n";
        }
        close(LBB_FILE) or die "Error: Can not close $lbb_file\n";

        my $sim_exec = "$multimap_bin_dir/$mmaps_exec_file_name.$extension_str.siminst";
        my $sim_stat = "$sim_exec.static";

        my $command = "$pebil_exec --app $bin_file_path --typ $simulation_type --inp $lbb_file" .
                      " --ext $extension_str.siminst --dtl" .
                      (defined($inst_lib_top_dir) ? " --lib $inst_lib_top_dir" : "");
        print "running ===> $command\n";
        my @lines = `$command`;
        die "Error: $pebil_exec failed running $command\n" unless &check_success(\@lines);

        `cp $multimap_etc_dir/*.inp $sim_run_dir`;
        `cp $sim_exec $sim_run_dir`;
        `cp $sim_stat $ext_info_dir`;

        my $select_file = "$sim_run_dir/$test_select_file_name";

        open(SELECT_FILE,">",$select_file) or die "Error: $select_file can not be opened\n";
        print SELECT_FILE "1\n$test_type $line_size\n";
        close(SELECT_FILE) or die "Error: $select_file can not be closed\n";

        &generate_submission_script("$mmaps_exec_file_name.$extension_str.siminst",
                                    ($batch_type eq "ll" ? "10:00:00" : "10:00" ),
                                    1,$sim_run_dir,"mmaps_sim_$extension_str",$batch_type);
    }
}

sub genBlockIdToStatFields {
    my ($block_id_to_stat_fields,$stat_file) = @_;
    print "Reading static file $stat_file\n";

    open(JBB_STATIC,"<",$stat_file) or die "Error: can not open $stat_file\n";
    while(my $line = <JBB_STATIC>){
        chomp($line);
        next unless($line = cleanCommentWSpace($line));
        my @tokens = split /\s+/, $line;
        $$block_id_to_stat_fields{$tokens[1]} = [ @tokens ];
    }
    close(JBB_STATIC) or die "Error: can not close $stat_file\n";
}



sub extract_frequency {
    my ($ext_info_dir,$jbb_run_dir) = @_;

    my $block_id_file  = "$ext_info_dir/$block_info_file_name";
    my $jbb_stat       = "$ext_info_dir/$mmaps_exec_file_name.jbbinst.static";
    my $freq_info_file = "$ext_info_dir/$frequency_info_file_name";

    print "-------------------------------------------------------------\n";

    die "Error: $block_id_file does not exist\n" unless (-e $block_id_file);
    die "Error: $jbb_stat does not exist\n" unless (-e $jbb_stat);

    my %block_id_to_stat_fields;
    genBlockIdToStatFields(\%block_id_to_stat_fields,$jbb_stat);

    print "Checking trace file for jbb run\n";
    my @lines = `ls $jbb_run_dir/$mmaps_exec_file_name.meta_*.jbbinst`;
    die "Error: The trace file for jbb run at $jbb_run_dir is not found or are multiples\n"
        unless(@lines == 1);

    my $trace_file = shift @lines;
    chomp($trace_file);
    print "Trace file for jbb run is $trace_file\n";
    
    my @block_freq_array;

    print "Reading the trace file\n";
    open(JBB_TRACE_FILE,"<",$trace_file) or die "Error: can not open $trace_file\n";
    while(my $line = <JBB_TRACE_FILE>){
        chomp($line);
        next unless($line = cleanCommentWSpace($line));
        my @tokens = split /\s+/, $line;
        $block_freq_array[$tokens[0]] = $tokens[1];
    }
    close(JBB_TRACE_FILE) or die "Error: can not close $trace_file\n";

    print "Reading the block file and generating frequency file\n";
    open(FREQ_FILE,">",$freq_info_file) or die "Error: can not open $freq_info_file\n";
    open(BLOCK_FILE,"<",$block_id_file) or die "Error: can not open $block_id_file\n";
    while(my $line = <BLOCK_FILE>){
        chomp($line);
        my @tokens = split /\s+/, $line;
        die "Error: In $block_id_file each line should have 6 tokens\n"
            unless(@tokens >= 6);
        my $print_str = "";
        for(my $i=5;$i<@tokens;$i++){
            my $freq = 0;
            die "Error: how come this block does not have a record in stat file\n"
                unless(exists($block_id_to_stat_fields{$tokens[$i]}));
            my $block_seq = $block_id_to_stat_fields{$tokens[$i]}[0];
            if(defined($block_freq_array[$block_seq])){
                $freq = $block_freq_array[$block_seq];
            } 
            $print_str .= "\t$tokens[$i]\t$freq";
        }
        for(my $i=0;$i<5;$i++){
            print FREQ_FILE ( $i ? "\t" : "") . $tokens[$i];
        }
        print FREQ_FILE "$print_str\n";
    }
    close(BLOCK_FILE) or die "Error: can not close $block_id_file\n";
    close(FREQ_FILE) or die "Error: can not close $freq_info_file\n";
}

sub merge_hit_ratios {

    my ($rep_sequence,$repsequence_hit_data,
        $total_sample_count,$cache_hit_counts,$sysid_index,$size_to_bw_fields,
         $freq_fields_ref) = @_;

    die "Error: Invalid sysid $sysid_index\n" 
        if((1 > $sysid_index) || ($sysid_index > $maximum_system_id));
    die "Error: Invalid sysid $sysid_index as no hit caount\n" 
        unless(defined($$cache_hit_counts[$sysid_index]) && 
               defined($$total_sample_count[$sysid_index]));

    if(!defined($$repsequence_hit_data{$rep_sequence})){
        $$repsequence_hit_data{$rep_sequence} = ();
        $$repsequence_hit_data{$rep_sequence}[0] = "N/A";
        $$repsequence_hit_data{$rep_sequence}[1] = 0;

        for(my $j=1;$j<=$maximum_cache_level;$j++){
            if(defined($$cache_hit_counts[$sysid_index][$j])){
                $$repsequence_hit_data{$rep_sequence}[$j+1] = 0;
            }
        }
    }
    for(my $j=1;$j<=$maximum_cache_level;$j++){
        if(defined($$cache_hit_counts[$sysid_index][$j])){
            $$repsequence_hit_data{$rep_sequence}[$j+1] += $$cache_hit_counts[$sysid_index][$j];
        }
    }
    $$repsequence_hit_data{$rep_sequence}[1] += $$total_sample_count[$sysid_index];

    my $bw = "N/A";
    if(exists $$size_to_bw_fields{$$freq_fields_ref[3]}){
        $bw = 1.0*$$size_to_bw_fields{$$freq_fields_ref[3]}[2];
    }
    $$repsequence_hit_data{$rep_sequence}[0] = $bw;
}

sub read_bandwidth_values {
    my ($size_to_bw_fields,$bws_dirs,$file_number) = @_;

    $bws_dirs =~ s/\s+//g;
    my @bw_dir_list = split /:/, $bws_dirs;

    my @valid_bw_dirs;

    for(my $i=0;$i<@bw_dir_list;$i++){
        my $bws_dir = $bw_dir_list[$i];
        my @files = `ls $bws_dir/MultiMAPS_*_$file_number.out`;
        if(@files != 1){
            warn "Warning: There is no unique BW files in $bws_dir for $file_number\n";
        } else {
            my $bw_file = $files[0];
            chomp($bw_file);
            push @valid_bw_dirs, $bw_file;
        }
    }
    my $valid_bw_dir_count = scalar @valid_bw_dirs;

    if($valid_bw_dir_count == 0){
        warn "Warning: There is no valid BW file in $bws_dirs for $file_number\n";
        return;
    } 

    my %variance_hash;

    for(my $i=0;$i<$valid_bw_dir_count;$i++){

        my $bw_file = $valid_bw_dirs[$i];

        print "Reading $bw_file for BW variance values\n";
        open(BWS_FILE,"<",$bw_file) or die "Error: Can not open $bw_file file\n";
        while(my $line = <BWS_FILE>){
            chomp($line);
            $line =~ s/\s+//g;
            if($line =~ /^(\d+),(.*),(.*),(.*)/){
                if($3 ne "N/A"){
                    $variance_hash{$1/8}[$i] = $3;
                    if($valid_bw_dir_count == 1){
                        $variance_hash{$1/8}[$i+1] = $2;
                    }
                }
                if(!$i){
                    $$size_to_bw_fields{$1/8} = [ split /,/, $line ];
                }
            }
        }
        close(BWS_FILE) or die "Error: Can not close $bw_file file\n";
    }

    if($valid_bw_dir_count == 1){
        print("Using MIN and MAX of the same multimaps output for elimination\n");
        $valid_bw_dir_count++;
    }

    foreach my $key ( keys %variance_hash ){
        my $min = 1.0E12;
        my $max = -1.0E12;
        for(my $i=0;$i<$valid_bw_dir_count;$i++){
            if(defined($variance_hash{$key}[$i])){
                if($variance_hash{$key}[$i] < $min){
                    $min = $variance_hash{$key}[$i];
                }
                if($variance_hash{$key}[$i] > $max){
                    $max = $variance_hash{$key}[$i];
                }
            }
            $$size_to_bw_fields{$key}[2] = $max;
            my $var = 1.0*($max-$min)/$min;
            if($var >= $elimination_variance){
                print "===> Eliminating $key ---- $min ---- $max -- $var\n";
                delete $$size_to_bw_fields{$key};
            }
        }
    }
}


sub extract_hitratios {
    my ($ext_info_dir,$sim_top_dir,$sysid_index,$bws_dirs,$simulation_type) = @_;

    die "Error: sysid $sysid_index is not a valid index\n"
        unless((1<=$sysid_index) && ($sysid_index<=$maximum_system_id));

    my @test_list = setSimTestCases($ext_info_dir);
    for(my $i=0;$i<@test_list;$i++){
        print "-------------------------------------------------------------\n";
        my ($test_type,$line_size) = split /,/, $test_list[$i];

        print("Extracting hit ratios for test $test_type line $line_size\n");
        my $extension_str = sprintf("%d_%02d",$test_type,$line_size);

        my $sim_run_dir = "$sim_top_dir/$extension_str";
        my $sim_stat    = "$ext_info_dir/$mmaps_exec_file_name.$extension_str.siminst.static";

        if(!-e $sim_stat){
            warn "Warning: $sim_stat does not exist so skipping hit ratios for $extension_str\n";
            next;
        }

        my %block_id_to_stat_fields;
        genBlockIdToStatFields(\%block_id_to_stat_fields,$sim_stat);

        my $freq_file = "$ext_info_dir/$frequency_info_file_name";    

        my @sequence_order_for_test;
        my %block_seq_to_repsequence;
        my %repsequence_to_freq_fields;
        open(FREQ_FILE,"<",$freq_file) or die "Error: can not open $freq_file\n";
        while(my $line = <FREQ_FILE>){
            chomp($line);
            my @tokens = split /\s+/, $line;
            die "Error: In $freq_file each line should have 7 tokens\n"
                unless(@tokens >= 7);
            if(($tokens[1] == $test_type) && ($tokens[2] == $line_size)){
                my $repseq = 0;
                for(my $i=5;$i<@tokens;$i+=2){
                    die "Error: There is a problem in blocks file\n"
                        unless(exists $block_id_to_stat_fields{$tokens[$i]});
                    if($i == 5){
                        $repseq = $block_id_to_stat_fields{$tokens[$i]}[0];
                        $repsequence_to_freq_fields{$repseq} = [ @tokens ];
                        push @sequence_order_for_test, $repseq;
                    }
                    $block_seq_to_repsequence{$block_id_to_stat_fields{$tokens[$i]}[0]} = $repseq;
                }
            }
        }
        close(FREQ_FILE) or die "Error: can not close $freq_file\n";

        print "Reading the bws_dirs file\n";
        if(defined($bws_dirs)){
            if(!exists $extension_to_file_number{$extension_str}){
                warn "Warning: test number for bws_dirs file is unknown for $extension_str so not extracting bws_dirs\n";
                $bws_dirs = undef;
            }
        } else {
            warn "Warning: BWs will not be extracted since --bws_dirs option is not given\n";
        }

        my %size_to_bw_fields;
        if(defined($bws_dirs)){
            read_bandwidth_values(\%size_to_bw_fields,$bws_dirs,$extension_to_file_number{$extension_str});
        }

        print "Checking trace file for $simulation_type run $extension_str\n";
        my @lines = `ls $sim_run_dir/$mmaps_exec_file_name.meta_*.$extension_str.siminst`;
        die "Error: The trace file for $simulation_type run at $sim_run_dir is multiples\n"
            if(@lines > 1);

        my $trace_file = $lines[0];
        chomp($trace_file);
        if((@lines == 0) || !(-e $trace_file)){
            warn "Warning: The trace file does not exist so skipping hit ratios for $extension_str\n";
            next;
        }

        print "Trace file for $simulation_type $extension_str run is $trace_file\n";
        
        print "Reading the trace file\n";
        my @total_sample_count;
        my @cache_hit_counts;

        my $start_sys_lines = 0;
         my $block_seq = 0;
        my %repsequence_hit_data;

        open(SIM_TRACE_FILE,"<",$trace_file) or die "Error: can not open $trace_file\n";
        while(my $line = <SIM_TRACE_FILE>){
            chomp($line);
            next unless($line = cleanCommentWSpace($line));
            if($line =~ /^block\s+(\d+)\s+(\d+)\s+(\d+)/){
                if($start_sys_lines){
                    merge_hit_ratios($block_seq_to_repsequence{$block_seq},\%repsequence_hit_data,
                                     \@total_sample_count,\@cache_hit_counts,$sysid_index,\%size_to_bw_fields,
                                     $repsequence_to_freq_fields{$block_seq_to_repsequence{$block_seq}});
                }
                $start_sys_lines = 0;
                $block_seq = $1;
                if(exists $block_seq_to_repsequence{$block_seq}){
                    $start_sys_lines = 1;
                    @total_sample_count = ();
                    @cache_hit_counts = ();
                }
            } elsif($line =~ /sys\s+(\d+)\s+lvl\s+(\d+)\s+(\d+)\s+(\d+)/){
                my $system_idx = $1;
                my $cache_idx  = $2;
                my $hit_count  = $3;
                my $miss_count = $4;
                if($start_sys_lines){
                    if($cache_idx == 1){
                        $total_sample_count[$system_idx] = $hit_count + $miss_count;
                    }
                    $cache_hit_counts[$system_idx][$cache_idx] = $hit_count;
                }
            }
        }
        if($start_sys_lines){
            merge_hit_ratios($block_seq_to_repsequence{$block_seq},\%repsequence_hit_data,
                             \@total_sample_count,\@cache_hit_counts,$sysid_index,\%size_to_bw_fields,
                             $repsequence_to_freq_fields{$block_seq_to_repsequence{$block_seq}});
        }
        close(SIM_TRACE_FILE) or die "Error: can not close $trace_file\n";

        my $hit_ratios_file = "$ext_info_dir/$extension_str." . sprintf("sysid%d",$sysid_index) . 
                              ".$hitratio_info_file_name";
        print "Writing hit ratios for system $sysid_index and $extension_str in $hit_ratios_file\n";
        open(HIT_FILE,">",$hit_ratios_file) or die "Error: $hit_ratios_file can not be opened\n";
        for(my $j=0;$j<@sequence_order_for_test;$j++){
            my @stokens = @{ $repsequence_to_freq_fields{$sequence_order_for_test[$j]} };
            print HIT_FILE "@stokens";
            if(exists $repsequence_hit_data{$sequence_order_for_test[$j]}){
                my @htokens = @{ $repsequence_hit_data{$sequence_order_for_test[$j]} };
                my $curr_hit = 0;
                for(my $c=1;$c<=$maximum_cache_level;$c++){
                    if(defined($htokens[$c+1])){
                        $curr_hit += $htokens[$c+1];
                        print HIT_FILE sprintf(" %8.5f",(100.0*$curr_hit)/$htokens[1]);
                    }
                }
                print HIT_FILE " $htokens[0]";
            }
            print HIT_FILE "\n";
        }
        close(HIT_FILE) or die "Error: $hit_ratios_file can not be closed\n";
    }
}


sub printUsage {
    my $str = shift @_;
    my $extra = shift @_;
    print "$help_string\n";
    print "$extra\n";
    exit;
}

sub doIt_Main {

    my $action = undef;
    my $src_top_dir = undef;
    my $tgt_top_dir = undef;
    my $system_idx = undef;
    my $is_help = undef;
    my $is_readme = undef;
    my $sysid_index = 1;
    my $bws_dirs = undef;
    my $pebil_exec = "pebil";
    my $simulation_type = "sim";
    my $batch_type = "lsf";
    my $inst_lib_top_dir  = undef;

    my $result = Getopt::Long::GetOptions (
        'action=s'      => \$action,
        'mmaps_dir=s'   => \$src_top_dir,
        'trace_dir=s'   => \$tgt_top_dir,
        'system_id=i'   => \$system_idx,
        'system_id=i'   => \$sysid_index,
        'bws_dirs=s'    => \$bws_dirs,
        'instor=s'      => \$pebil_exec,
        'simtyp=s'      => \$simulation_type,
        'batch=s'       => \$batch_type,
        'inst_lib=s'    => \$inst_lib_top_dir,
        'readme:i'      => \$is_readme,
        'elim:s'        => \$elimination_variance,
        'help:i'        => \$is_help,
        'source_lang=s' => \$multimaps_source_lang
    );

    print "Info: using language $multimaps_source_lang to build multimaps\n";

    if(!$result){
        &printUsage($0,"Error: Error in options\n");
    }
    if(defined($is_readme)){
        print "$help_string\n";
        print "$readme_string\n";
        exit;
    }
    if(defined($is_help)){
        print "$help_string\n";
        exit;
    }

    if(!defined($action)){
        &printUsage($0,"Error: Action is not defined\n");
    }

    if(!defined($tgt_top_dir)){
        warn "Warning: --trace_dir option is not used, using the . directory instead\n";
        $tgt_top_dir = getcwd;
    }

    &printUsage($0,"Error : Type of the simulation $simulation_type is incorrect")
                unless (($simulation_type eq "sim") || ($simulation_type eq "csc"));
    &printUsage($0,"Error : Type of the batch $batch_type is incorrect")
                unless (($batch_type eq "lsf") || ($batch_type eq "ll"));

    if(defined($inst_lib_top_dir)){
        my $tmpdir = "$inst_lib_top_dir/lib";
        die "Error: The inst_lib directory $tmpdir does not exist\n" unless (-e $tmpdir);
    }

    my $multimap_inst = "$tgt_top_dir/mmapTRACE";
    my $pln_run_dir   = "$multimap_inst/org";
    my $jbb_run_dir   = "$multimap_inst/jbb";
    my $sim_top_dir   = "$multimap_inst/sim";
    my $ext_info_dir  = "$multimap_inst/etc";

    if($action eq "inst"){
        if(!defined($src_top_dir)){ 
            warn "Warning: --mmaps_dir option is not used, using the . directory instead\n";
            $src_top_dir = getcwd;
        }
        my $multimap_etc_dir = "$src_top_dir/etc";
        my $multimap_src_dir = "$src_top_dir/src";
        print "The source code for MMAPS is $multimap_src_dir/$mmaps_source_file_name\n";

        die "Error: The source directory for MMAPS $multimap_src_dir does not exist\n"
            unless (-e $multimap_src_dir);
        die "Error: The source file $mmaps_source_file_name at $multimap_src_dir does not exist\n"
            unless (-e "$multimap_src_dir/$mmaps_source_file_name");

        my $multimap_bin_dir = "$src_top_dir/bin";
        print "The bin executable for MMAPS is $multimap_src_dir/$mmaps_exec_file_name\n";

        die "Error: The bin directory for MMAPS $multimap_bin_dir does not exist\n"
            unless (-e $multimap_bin_dir);
        die "Error: The executable $multimap_bin_dir/$mmaps_exec_file_name does not exist\n"
            unless (-e "$multimap_bin_dir/$mmaps_exec_file_name");

        mkdir "$tgt_top_dir",0755   or warn "Warning: $tgt_top_dir exists, files will be overwritten\n";
        mkdir "$multimap_inst",0755 or warn "Warning: $multimap_inst exists, files will be overwritten\n";
        mkdir "$pln_run_dir",0755 or warn "Warning: $pln_run_dir exists, files will be overwritten\n";
        mkdir "$jbb_run_dir",0755  or warn "Warning: $jbb_run_dir exists, files will be overwritten\n";
        mkdir "$sim_top_dir",0755  or warn "Warning: $sim_top_dir exists, files will be overwritten\n";
        mkdir "$ext_info_dir",0755  or warn "Warning: $ext_info_dir exists, files will be overwritten\n";

        prepare_org($multimap_src_dir,$multimap_bin_dir,$multimap_etc_dir,$pln_run_dir,$ext_info_dir,
                    $batch_type);
        prepare_jbb($multimap_src_dir,$multimap_bin_dir,$multimap_etc_dir,$jbb_run_dir,$ext_info_dir,
                    $pebil_exec,$batch_type,$inst_lib_top_dir);
        prepare_sim($multimap_src_dir,$multimap_bin_dir,$multimap_etc_dir,$sim_top_dir,$ext_info_dir,
                    $pebil_exec,$simulation_type,$batch_type,$inst_lib_top_dir);

    } elsif($action eq "bbct"){
        extract_frequency($ext_info_dir,$jbb_run_dir);
    } elsif($action eq "hits"){
        extract_hitratios($ext_info_dir,$sim_top_dir,$sysid_index,$bws_dirs,$simulation_type);
    } else {
        &printUsage($0);
    }
}

$help_string = <<HELP_STRING;

usage : $0
   --action inst [--trace_dir <run_top_dir>] [--mmaps_dir <mmaps_src_top_dir>] [--source_lang (fortran|C)] [--batch (lsf|ll)]
   --action bbct [--trace_dir <run_top_dir>]                                   
   --action hits [--trace_dir <run_top_dir>] [--system_id <sysid>] [--bws_dirs <bws_dirs_:_seperated>] 
   --help
   --readme

(for testing and alterations. only used by developers)
   --instor       <name_of_the_instrumentor>
   --simtyp       (sim|csc)
   --inst_lib     <top_dir_of_inst_libraries>

HELP_STRING

$readme_string = <<README_STRING;

Usage Examples (Arguments):
===========================
    Assuming MultiMAPS distribution is at /home/mtikir/MultiMAPS.

    1. For action inst
        --action inst --mmaps_dir /home/mtikir/MultiMAPS --trace_dir /gpfs/mtikir/

    2. For action bbct
        --action bbct --trace_dir /gpfs/mtikir/

    3. For action hits
        --action hits --trace_dir /gpfs/mtikir/ --system_id 3 \
        --bws_dirs /home/mtikir/mmap_results/run1:/home/mtikir/mmap_results/run2


Descriptions of Arguments:
==========================
    action    :  MultiMAPS tracing action
         inst :  prepares directory structure and instrumented executables
                 to trace MultiMAPS for jbb and cache simulation as well
                 as other necessary files.
         bbct :  extracts the basic block execution counts after jbb 
                 instrumented run.
         hits :  extracts the cache hit rates for a given sysid after 
                 cache simulation runs. It also extracts the bandwidths
                 for the given sysid from a given directory.
    mmaps_dir :  the top directory where the MultiMAPS distribution is installed.
                 If omitted, the current directory is assumed as the mmaps_dir directory.
    trace_dir :  the target directory where directory structure for MultiMAPS will
                 be generated and tracing related files will be stored. If omitted, 
                 the current directory is assumed as the trace_dir directory.
    system_id :  sysid of the machine of which cache hit ratios will be extracted.
                 If omitted, sysid 1 is assumed.
    bws_dirs  :  the top directories where actual measured bandwidths are stored.
                 If omitted, no bandwidth is extracted. To give multiple directories, directories 
                 needs to be seperated using :. If bws differ by more than 5% for a size, it is not used.
                 In addtion, the bw files under the first directory is always the one used for bw extraction.
  source_lang :  The source language used to create the Multimaps benchmark. Default value is fortran.


Description:
============
    This script enables users to instrument and collect information from
    MultiMAPS. The main goal is to  store all files related to tracing of 
    MultiMAPS under one top directory (given with --trace_dir option) and eliminate
    as much manual work as possible. This script handles directory making and 
    copying files, hence, user of this script should keep in mind that if 
    the script is invoked more than one with the same --trace_dir option,
    some files will be overwritten. 

    All files are stored/copied under a user provided directory given with 
    the option --trace_dir. The directory hierarchy is predefined based on the 
    functionality of this script. However, common to all actions mmapTRACE 
    directory is the directory below the directory given with --trace_dir option 
    and includes tracing related files. Note that when you see a directory 
    names mmapTRACE anywhere in your work area, it is likely that this 
    directory is outcome of call to this script.

    This script provides three different functionalities.

    1. Action "inst" takes the directory where MultiMAPS is installed and
       a target directory to store tracing related files. It prepares the
       directory structure and the files for tracing of MultiMAPS. It 
       makes a directory mmapTRACE under the given --trace_dir directory
       (default is the current directory for target) and makes four 
       directories under the mmapTRACE directory, namely etc, org, jbb, sim.
       The etc directory includes files that are needed to automate
       this script as well as the extracted frequency counts and hit ratios.
       The org directory includes the original executable, input files and
       submission script to run the original (un-instrumented) MultiMAPS.
       The jbb directory includes the executable for jbb instrumentation 
       as well as the input and submission files. The sim directory 
       contains sub directories for each test combination of test_type and
       line_size (currently 1_02,2_01,2_02,2_04,2_08) that are included 
       in the source code of MultiMAPS. Under each such test case directory, 
       the instrumented executables for cache simulation as well as input 
       files and submission scripts are included.

       To eliminate user intervention or manual work as much as possible,
       this action first goes over the MultiMAPS source file and extracts
       which test cases are included. Using their line number and list of
       basic blocks in the executable, it also extracts the list of 
       basic blocks (their ids) for which hit ratios will be extracted and
       the test cases each basic block represents. Thus, besides the executables
       and static information about executables, this action generates
       etc/lines.txt file for line number information and etc/blocks.txt for
       basic block information. 

       After this action, executables are ready to be submitted to the batch.
       User needs to go to jbb directory, and subdirectories under the sim directory
       under the given target directory and submit the jobs manually.

    2. Action "bbct" extracts the basic block execution counts after the jbb run.
       Thus user needs to run this action after jbb runs complete. It will extract
       basic block execution count for each test case and generate etc/frequency.txt
       file. This file includes information in the etc/blocks.txt file and the
       execution for each block. If a test case is not run, 0 will be substituted
       for the execution count of the basic block.

    3. Action "hits" extracts the cache hit ratio for a given sysid (the 
       default is sysid 1). It also extract the measured actual bandwidths 
       from a given directory. User should run this action when one or more 
       simulation runs are over. This action parses the trace file generated for 
       cache simulation and extracts the hit ratios for the given system for
       the basic blocks of test cases that ran. If we assume the test case 2_02
       has finished, this action will generate a file etc/2_02.sysidNN.hitratio.txt
       (for --system_id N) that includes block, frequency, and hit ratio information for data
       sizes in this test. If --bws_dirs is provides, it will also extract the bandwidths
       from the MultiMAPS results files in the directory.

       Note that, if all simulation runs are over, for a given sysid, if hits
       action is ran, it will generate a results file for each test case combination
       test_type and line_size that includes a line for each data size.

README_STRING

&doIt_Main;
print "-------------------------------------------------------------\n";
print "\n\n*** DONE *** SUCCESS *** SUCCESS *** SUCCESS *****************\n";

