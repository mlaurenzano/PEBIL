#!/usr/bin/perl

use strict;
use Getopt::Long;

#################################################################
# from the current .static files, the block id is at index 1 and
# function name is at index 6. Needs to be changed if .static 
# file is changed

my $block_id_idx = 1;
my $func_name_idx = 6;
my $number_of_fields = 7;  ### note that excludes the fields after the comment

# for printing purposes

my $count_per_row = 6;

#################################################################

sub print_usage {
    my $name = shift @_;
    my $msg = shift @_;
    print "usage : $name\n";
    print "      --sim_static <siminst.static file>\n";
    print "      --help\n";
    if(defined($msg)){
        print "error : $msg\n";
    }
    exit;
}

sub Main {

    my $is_help = undef;
    my $static_file = undef;

    my $result = Getopt::Long::GetOptions (
        'sim_static=s' => \$static_file,
        'help:i'   => \$is_help
    );  

    if(!$result){
        print_usage($0,"Error in arguments");
    }
    if(defined($is_help)){
        print_usage($0);
    }
    if(!defined($static_file)){
        print_usage($0,"Siminst static file is missing");
    }

    my %function_map;

    my $max_sequence = -1;
    my $line_no = 0;
    open(STATIC_FD,"<",$static_file) or die "!!!!! Can not open $static_file\n";
    while(my $line = <STATIC_FD>){
        $line_no++;
        chomp($line);

        $line =~ s/#.*$//g;
        $line =~ s/^\s+//g;
        $line =~ s/\s+$//g;
        if($line eq ""){
            next;
        }

        $line =~ s/\s+/ /g;
        my @tokens = split /\s+/, $line;
        die "Error: At line $line_no, the number of fields has to be 7"
            unless (scalar @tokens == $number_of_fields);

        my $function_name = $tokens[$func_name_idx];
        my $block_id = $tokens[$block_id_idx];

        if(!exists $function_map{$function_name}){
            $function_map{$function_name} = [ $block_id ];
        } else {
            push @{ $function_map{$function_name}}, $block_id;
        }
        if($tokens[0] > $max_sequence){
            $max_sequence = $tokens[0];
        }
    }
    close(STATIC_FD) or die "!!!!! Can not close $static_file\n";

    my $total_block_count = 0;
    print "# FORMAT of each line ==>  function=<function_name>:blockid_1:blockid_2:blockid_3:.....";
    foreach my $key ( sort keys %function_map ) {
        my @arr = @{ $function_map{$key} };
        my $i = 0;
        foreach my $mem ( sort @arr ){
            if(!($i++ % $count_per_row)){
                print "\nfunction=$key";
            }
            print ":$mem";
            $total_block_count++;
        }
        die "Error: Number of blocks does match the printed ones\n" 
            unless ($i == @arr);
    }

    die "Error: Number of input blocks and printed ones do not match\n"
        unless ($total_block_count == ($max_sequence+1));

    print "\n###### SUCCESS reading/writing $total_block_count blocks\n";
}

&Main;
