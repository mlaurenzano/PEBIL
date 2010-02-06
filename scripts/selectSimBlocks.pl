#!/usr/bin/perl

use strict;
use Getopt::Long;

########################## user changable area ##################
# has to be > 6 and it is the index for frequency
my $freq_field_idx = 7;

# index for memory op count 
my $memop_field_idx = 2;
    
# to sort records, what should be use. $memop_field_idx is memop count
sub frequency_function_memop {
    my ($block_seq_to_record,$sequence,$exec_count) = @_;
    return ($$block_seq_to_record[$sequence][$memop_field_idx] * $exec_count);
}
sub frequency_function_visit {
    my ($block_seq_to_record,$sequence,$exec_count) = @_;
    return $exec_count;
}
#########################^^^^^^^^^^^^^^^^^^^^^^##################

sub by_field {
    my @arr1 = @ { $a };
    my @arr2 = @ { $b };
    my $val1 = $arr1[$freq_field_idx];
    my $val2 = $arr2[$freq_field_idx];
    if($val1 < $val2) { 1 } elsif ($val1 > $val2) { -1 } else { 0 }
}

sub by_last_field {
    my @arr1 = @ { $a };
    my @arr2 = @ { $b };
    my $val1 = $arr1[-1];
    my $val2 = $arr2[-1];
    if($val1 < $val2) { 1 } elsif ($val1 > $val2) { -1 } else { 0 }
}


sub cleanCommentWSpace {
    my $line = shift @_;
    $line =~ s/#.*$//g;
    $line =~ s/^\s+//g;
    return $line;
}

sub read_static_info {
    my ($block_info,$block_seq_to_record) = @_;

    my $total_blocks = 0;
    my $line_number = 0;

    open(JBB_STATIC,"<",$block_info) or die "Error: Can not open $block_info for reading\n $!";
    while(defined(my $line = <JBB_STATIC>)){
        chomp($line);
        $line_number++;

        next unless($line = cleanCommentWSpace($line));

        if($line =~ /^\s*\d+/){
            my @tokens = split /\s+/, $line;
    
            die "Error: [$block_info,$line_number] field count is invalid\n"
                unless(@tokens == 7);
            my $block_seq = $tokens[0];
            $tokens[$freq_field_idx] = 0;
            $$block_seq_to_record[$block_seq] = [ @tokens ];
            $total_blocks++;
        } elsif(($line =~ /^\s*\+lpi/) || ($line =~ /^\s*\+cnt/)){
            # These additional lines are not used 
            # current for instrumentation point selection
        }
    }
    close(JBB_STATIC) or die "Error: Can not close $block_info\n";

    die "Error: There is no information in the static file\n"
        unless($total_blocks > 0);

    print "Total numbers of block info read from $block_info is $total_blocks\n";
    return $total_blocks;
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

sub read_frequency_files {
    my ($exec_name,$cpu_count,$source_dir,$source_ext,$block_seq_to_record,
        $per_rank_lbb_on,$visit_cnt_only,
        $per_rank_sorted_records) = @_;


    my @files;
    call_ls($source_dir,"$exec_name\\.meta_.*\\.$source_ext",\@files);
    my $check = scalar @files;
    die "Error: Cput count $cpu_count does not match .$source_ext count $check at $source_dir\n"
        unless($cpu_count == $check);

    for(my $i=0;$i<$cpu_count;$i++){

        my @rank_counts;
        my $rank_idx = 0;

        my $jbb_trace_file = "$source_dir/$exec_name.meta_" . sprintf("%04d",$i) . ".$source_ext";
        print "Processing file $jbb_trace_file";

        open(JBB_TRACE_FILE,"<",$jbb_trace_file) or die "Error: Can not open $jbb_trace_file for reading\n $!";
        my $line_number = 0;

        while(defined(my $line = <JBB_TRACE_FILE>)){
            chomp($line);
            $line_number++;
            next unless($line = cleanCommentWSpace($line));

            my @tokens = split /\s+/, $line;
            next unless(@tokens == 2);

            my ($sequence,$exec_count) = @tokens;
            my $real_freq = frequency_function_memop($block_seq_to_record,$sequence,$exec_count);
            if($visit_cnt_only){
                $real_freq = frequency_function_visit($block_seq_to_record,$sequence,$exec_count);
            }
            $$block_seq_to_record[$sequence][$freq_field_idx] += $real_freq;

            if($per_rank_lbb_on){
                $rank_counts[$rank_idx][0] = $$block_seq_to_record[$sequence][1];
                $rank_counts[$rank_idx++][1] = $real_freq;
            }
        }
        close(JBB_TRACE_FILE) or die "Error: Can not close $jbb_trace_file\n";

        if($per_rank_lbb_on){
            @rank_counts = sort by_last_field @rank_counts;
            $$per_rank_sorted_records[$i] = [ @rank_counts ];
            printf "--- for %d there are %d records",$i,scalar @rank_counts;
        }
        print "\n";
    }
}

sub process_lbbs_per_rank {
    my ($cpu_count,$phase_count,$significance,$per_rank_sorted_records,$included_blocks) = @_;

    for(my $cpu=0;$cpu<$cpu_count;$cpu++){
        my $sum = 0;
        my @rank_counts = @{ $$per_rank_sorted_records[$cpu] };
        my $record_count = scalar @rank_counts;
        for(my $i=0;$i<$record_count;$i++){
            $sum += $rank_counts[$i][-1];
        }
        my $stop_count = $sum * $significance;
        my $stop_unit = $stop_count / (1.0*$phase_count);
        my $phase_sum = 0;
        my $curr_index = 0;
        for(;$curr_index<$record_count;$curr_index++){
            $phase_sum += $rank_counts[$curr_index][-1];
            my $bb_id = $rank_counts[$curr_index][0];
            if(exists $$included_blocks{$bb_id}){
                #$$included_blocks{$bb_id} = $$included_blocks{$bb_id} . ".$cpu";
                $$included_blocks{$bb_id} = $$included_blocks{$bb_id} + 1;
            } else {
                #$$included_blocks{$bb_id} = "$cpu";
                $$included_blocks{$bb_id} = 1;
            }
            if($stop_count <= $phase_sum){
                $curr_index++;
                last;
            }
        }
        print "rank $cpu -- of $record_count record --- " .
              "(sum,$sum),(stop_count,$stop_count),(stop_unit,$stop_unit) -- chose $curr_index\n";

    }
}

sub print_lbbs {

    my ($total_blocks,$block_seq_to_record,$phase_count,
        $header_string,$significance,
        $app_name,$cpu_count,$source_dir,$source_ext,
        $visit_cnt_only,
        $included_blocks_ptr) = @_;

    my %included_blocks = %{ $included_blocks_ptr };

    my $sum = 0;
    for(my $i=0;$i<$total_blocks;$i++){
        die "Error: Why is $i member of block_seq_to_record is not set\n"
            unless(defined($$block_seq_to_record[$i]));
        $sum += $$block_seq_to_record[$i][$freq_field_idx];
    }
    my $stop_count = $sum * $significance;
    my $stop_unit = $stop_count / (1.0*$phase_count);
    print "\n(sum,$sum),(stop_count,$stop_count),(stop_unit,$stop_unit)\n";

    my $phase_sum = 0;
    my $curr_index = 0;
    for(my $i=1;$i<=$phase_count;$i++){
        my $phase_header_string = $header_string;
        $phase_header_string =~ s/PHASE/$i/g;

        my $lbb_file_name = "$source_dir/$app_name.phase.$i" . "o" . "$phase_count." .
                            sprintf("%04d",$cpu_count) . "." . $source_ext . ".lbb";
        my $local_stop_count = $stop_unit * $i;
        if($i == $phase_count){
            $local_stop_count = $stop_count;
        }

        print "Working on phase $i up to count $local_stop_count in $lbb_file_name\n";

        open(LBB_FILE,">",$lbb_file_name) or die "Error: Can not open $lbb_file_name for writing\n $!";

        print LBB_FILE "$phase_header_string\n";

        my $inst_block_count = 0;
        for(;$curr_index<$total_blocks;$curr_index++){
            my @arr = @{ $$block_seq_to_record[$curr_index] };
            $phase_sum += $$block_seq_to_record[$curr_index][$freq_field_idx];
            my $percentage = (100.0 * $phase_sum) / $sum;
            my $count = 0;
            if($arr[$memop_field_idx] == 0){
                print LBB_FILE "## <m0> ";
            }
            if($visit_cnt_only){
                $count = $arr[$freq_field_idx]; 
            } else {
                $count = $arr[$freq_field_idx] / $arr[$memop_field_idx];
            }
            print LBB_FILE "$arr[1]\t# $count";

            for(my $j=$memop_field_idx;$j<$freq_field_idx;$j++){
                print LBB_FILE "\t$arr[$j]";
            }
            my $how_many_ranks = 0;
            if($$included_blocks_ptr{$arr[1]}){
               $how_many_ranks = $$included_blocks_ptr{$arr[1]};
            }
            printf LBB_FILE "\t%9.6f", $percentage;
            print LBB_FILE "\t$how_many_ranks\n";
            $inst_block_count++;
            if($local_stop_count <= $phase_sum){
                $curr_index++;
                last;
            }
        }
        print LBB_FILE "#### inst block count from global sort $inst_block_count\n";
        if($i == $phase_count){
            for(;$curr_index<$total_blocks;$curr_index++){
                my @arr = @{ $$block_seq_to_record[$curr_index] };
                if(exists $$included_blocks_ptr{$arr[1]}){
                    $phase_sum += $$block_seq_to_record[$curr_index][$freq_field_idx];
                    my $percentage = (100.0 * $phase_sum) / $sum;
                    my $count = 0;
                    if($arr[$memop_field_idx] == 0){
                        print LBB_FILE "## <m0> ";
                    }
                    if($visit_cnt_only){
                        $count = $arr[$freq_field_idx]; 
                    } else {
                        $count = $arr[$freq_field_idx] / $arr[$memop_field_idx];
                    }
                    print LBB_FILE "$arr[1]\t# $count";
                    for(my $j=$memop_field_idx;$j<$freq_field_idx;$j++){
                        print LBB_FILE "\t$arr[$j]";
                    }
                    my $how_many_ranks = 0;
                    if($$included_blocks_ptr{$arr[1]}){
                        $how_many_ranks = $$included_blocks_ptr{$arr[1]};
                    }
                    printf LBB_FILE "\t%9.6f", $percentage;
                    print LBB_FILE "\t$how_many_ranks\n";
                    $inst_block_count++;
                }
            }
        }
        print LBB_FILE "#### with addtional inst block count $inst_block_count\n";

        close(LBB_FILE) or die "Error: Can not close $lbb_file_name\n $!";
    }
}

my $help_string = "";

sub printUsage {
    my $script_name = shift @_;
    print "$help_string\n";
    exit;
}

sub SelectSimBlocks_Main {

    my $app_name = undef;
    my $block_info = undef;
    my $exec_name = undef;

    my $source_ext = "jbbinst";
    my $cpu_count = 1;
    my $source_dir = ".";
    my $phase_count = 1;
    my $target_ext = ".lbb";
    my $ishelp = undef;
    my $significance = 0.95;
    my $per_rank_flag = undef;
    my $per_rank_lbb_on = 0;
    my $visit_only_flag = undef;
    my $visit_cnt_only = 0;


    my $result = Getopt::Long::GetOptions (
        'block_info=s'   => \$block_info,
        'application=s'  => \$app_name,
        'exec_name=s'    => \$exec_name,
        'cpu_count=i'    => \$cpu_count,
        'phase_count=i'  => \$phase_count,
        'inst_ext=s'     => \$source_ext,
        'source_dir=s'   => \$source_dir,
        'significance=s' => \$significance,
        'per_rank_on:i'  => \$per_rank_flag,
        'visit_only:i'   => \$visit_only_flag,
        'help:i'         => \$ishelp 
    );

    &printUsage($0) unless $result;
    if(defined($ishelp)){
        print "$help_string\n"; 
        exit;
    }
    if(defined($per_rank_flag)){
        $per_rank_lbb_on = 1;
    }
    if(defined($visit_only_flag)){
        $visit_cnt_only = 1;
    }

    die "Error: Application name is required (--application <name>)\n" 
            unless(defined($app_name));
    die "Error: Block info file is required (--block_info <name>)\n" 
            unless(defined($block_info));
    die "Error: Executable name is required (--exec_name <name>)\n" 
            unless(defined($exec_name));
    die "Error: phase count has to be > 0\n"
            if($phase_count <= 0);
    die "Error: significance has to be in (0,1]\n"
        unless((0.0 < $significance) && ($significance <= 1.0));

    my $command_line = "$0 --block_info $block_info" . 
                       " --application $app_name" .
                       " --exec_name $exec_name" .
                       " --cpu_count $cpu_count" . 
                       " --phase_count $phase_count" .
                       " --inst_ext $source_ext" .
                       " --source_dir $source_dir" .
                       " --significance $significance" .
                       ($per_rank_lbb_on ? " --per_rank_on" : "") .
                       ($visit_cnt_only ? "--visit_only" : "");

    print "\n-------------------------------------------------------------------\n";
    print "$command_line\n";
    print "-------------------------------------------------------------------\n";

    my @block_seq_to_record;
    my @per_rank_sorted_records;

    my $total_blocks = read_static_info($block_info,\@block_seq_to_record);

    read_frequency_files($exec_name,$cpu_count,$source_dir,$source_ext,
                         \@block_seq_to_record,$per_rank_lbb_on,$visit_cnt_only,
                         \@per_rank_sorted_records);

    @block_seq_to_record = sort by_field @block_seq_to_record;

    my $header_string = "\# $app_name $exec_name $cpu_count\n" .
                        "\# $block_info\n" .
                        "\# $source_dir .$source_ext\n" .
                        "\# significance $significance\n" .
                        "\# phase PHASE of $phase_count\n" .
                        "\# block choice " .  ( $per_rank_lbb_on ? "per_rank" : "global" ) . "\n" .
                        "\# frequency choice " . ($visit_cnt_only ? "visit" : "visitXmemop") . "\n" .
                        "\# <block_uid>   \# <freq> <memop> <fpop> <insn> <line> <fname> <perc>";


    my %included_blocks;
    if($per_rank_lbb_on){
        process_lbbs_per_rank($cpu_count,$phase_count,$significance,
        \@per_rank_sorted_records,\%included_blocks);
    }

    print_lbbs($total_blocks,\@block_seq_to_record,$phase_count,
          $header_string,$significance,
          $app_name,$cpu_count,$source_dir,$source_ext,
          $visit_cnt_only,
          \%included_blocks);
}


$help_string = <<HELP_STRING;

usage : $0
    --block_info   <blockfile>    <-- required
    --application  <appname>      <-- required
    --exec_name    <execname>     <-- required
    --cpu_count    <cpucount>     <-- defaults to 1
    --phase_count  <phasecnt>     <-- defaults to 1
    --inst_ext     <sourcext>     <-- defaults to jbbinst
    --source_dir   <srcdir>       <-- defaults to .
    --significance <percentile>   <-- defaults to 0.95
    --per_rank_on
    --visit_only
    --help
HELP_STRING

&SelectSimBlocks_Main;

print "\n\n*** DONE *** SUCCESS *** SUCCESS *** SUCCESS *****************\n";
