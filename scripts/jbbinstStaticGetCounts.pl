#!/usr/bin/perl

use strict;
use Getopt::Long;

sub print_usage {
    my $name = shift @_;
    my $msg = shift @_;
    print "usage : $name\n";
    print "      --jbb_static <jbbinst.static file>\n";
    print "      --jbb_trace  <tracefile.jbbinst>\n";
    print "      --help\n";
    if(defined($msg)){
        print "error : $msg\n";
    }
    exit;
}

sub Main {

    my $is_help = undef;
    my $static_file = undef;
    my $trace_file = undef;

    my $args_res = Getopt::Long::GetOptions (
					   'jbb_static=s' => \$static_file,
					   'jbb_trace=s' =>\$trace_file,
					   'help:i'   => \$is_help
					   );  

    if(!$args_res){
        print_usage($0,"Error in arguments");
    }
    if(defined($is_help)){
        print_usage($0);
    }
    if(!defined($static_file)){
        print_usage($0,"Jbbinst static file is missing");
    }
    if (!defined($trace_file)){
	print_usage($0,"Jbbinst trace file is missing");
    }

    my %id_map;
    my @block_ids = ();
    my @memops = ();
    my @fpops = ();
    my @insns = ();
    my @loads = ();
    my @stores = ();

    my $line_no = 0;
    my $block_idx = 0;

    open(STATIC_FD,"<",$static_file) or die "!!!!! Cannot open $static_file\n";
    while(my $line = <STATIC_FD>){
	$line_no++;
        chomp($line);

	if ($line =~ m/^#/g){
	    #print "$static_file: line $line_no is a comment\n";
#	} elsif ($line =~ m/^(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*#\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*$/){
	} elsif ($line =~ m/^(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*#\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*(\S*)\s*$/){

            print "readline: $1\t$2\t$3\t$4\t$5\t$6\t$7\t$8\t$9\t$10\t$11\t$12\t$13\t$14\t$15\t$16\n";

	    push(@block_ids,$2);
	    push(@memops,$3);
	    push(@fpops,$4);
	    push(@insns,$5);
            push(@loads,$13);
            push(@stores,$14);

            die "# of loads+stores should be the same as the # of memops: $10+$11!=$3"
                 unless $13+$14==$3;

            $id_map{$1} = $block_idx;
            $block_idx++;
	} else {
	    die "line $line_no of $static_file cannot be identified as a comment or as BB data\n";
	}

    }
    close(STATIC_FD) or die "!!!!! Can not close $static_file\n";

    $line_no = 0;
    my $block_count = 0;
    print "#seq_id\tblock_id\tcount\tmemop\tfpop\tinsn\tmem_and_fp_perc\tload_count\tstore_count\n";

    my $total_loads = 0;
    my $total_stores = 0;

    open(TRACE_FD,"<",$trace_file) or die "!!!!! Can not open $trace_file\n";
    while(my $line = <TRACE_FD>){
	$line_no++;
        chomp($line);

	if ($line =~ m/^#/g){
	    print "$trace_file: line $line_no is a comment\n";
	} elsif ($line =~ m/^(\S*)\s*(\S*)\s*#.*$/){
	    $block_count++;

	    my $dyn_count = $2;	    
	    
	    if (!defined($id_map{$1})){
		die "Error: cannot find block id $1 in static file: $line\n";
	    }

	    $block_idx = $id_map{$1};

            print "rawdata: $dyn_count\t$loads[$block_idx]\t $stores[$block_idx]\n";

	    my $block_id = $block_ids[$block_idx];
	    my $memop_count = $memops[$block_idx] * $dyn_count;
	    my $fpop_count = $fpops[$block_idx] * $dyn_count;
	    my $insn_count = $insns[$block_idx] * $dyn_count;
	    my $mem_and_fp_perc = ($memops[$block_idx] + $fpops[$block_idx]) / $insns[$block_idx];
            my $load_count = $loads[$block_idx] * $dyn_count;
            my $store_count = $stores[$block_idx] * $dyn_count;

            $total_loads += $load_count;
            $total_stores += $store_count;

            die "# of loads+stores should be the same as the # of memops"
                unless $load_count+$store_count==$memop_count;

	    print "BLOCK:\t$1\t$block_id\t$dyn_count\t$memop_count\t$fpop_count\t$insn_count\t$mem_and_fp_perc\t$load_count\t$store_count\n";
	} else{
            die "line $line_no in $trace_file unrecognized format";
        }
    } 
    close(TRACE_FD) or die "!!!!! Can not close $trace_file\n";


    my $total_memops = $total_loads + $total_stores;
    print "\nSUMMARY INFORMATION: loads $total_loads, stores $total_stores, memops $total_memops\n";
    my $load_perc = $total_loads / $total_memops;
    my $store_perc = $total_stores / $total_memops;

    print "trace_file\ttotal_loads\tload_perc\ttotal_stores\tstore_perc\ttotal_memops\n";
    print "SUMM:\t$trace_file\t$total_loads\t$load_perc\t$total_stores\t$store_perc\t$total_memops\n";
    print "\n";

    print "\n###### SUCCESS reading $block_count blocks\n";
}

&Main;
