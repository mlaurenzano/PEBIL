#!/bin/perl

use strict;
use Getopt::Long;

my $inform = "INFOR: ";
my $help_string = "";
my $PACK_CODE_32BIT_BIGENDIAN = 'N';
my $PACK_CODE_32BIT_LITTLEENDIAN = 'V';

sub printUsage {
    my $script_name = shift @_;
    print "$help_string\n";
    exit;
}

sub dec2bin {
    return unpack('B32', pack('N', shift));
}

sub AddrStreamRead_Main {

    my $addr_info = undef;
    my $skip_addrs = undef;
    my $count_addrs = undef;
    my $ishelp = undef;
    my $header_lines = 0;
    my $total_addrs = 0;
    my $is32bit = undef;
    my $byte_order = undef;
    my $bytes_per_addr = undef;
    my $bytes;
    my $flags;
    my $unpack_code;
    my $addr_count = 0;
    my $continue_read = 1;
    my $verbose = undef;

    my $result = Getopt::Long::GetOptions (
        'addr_file=s'   => \$addr_info,
        'skip=i'        => \$skip_addrs,
        'count=i'       => \$count_addrs,
        'help:i'        => \$ishelp,
        'verbose:i'     => \$verbose                                           
    );

    &printUsage($0) unless $addr_info;
    if(defined($ishelp)){
        print "$help_string\n"; 
        exit;
    }

    if (defined($verbose)){
        print $inform, "argument listing:\n";
        print $inform, "\t\taddr_file       $addr_info\n";
        print $inform, "\t\tskip            $skip_addrs\n";
        print $inform, "\t\tcount           $count_addrs\n";
        print $inform, "\t\thelp            $ishelp\n";
        print $inform, "\t\tverbose         $verbose\n";
    }

    open(ADDR_FILE, "<", $addr_info) or die "Error: Can not open $addr_info for reading\n $!";

    # the first value is not an address, it is a 32-bit value telling us whether the file
    # contains 32 or 64-bit addresses
    if (read(ADDR_FILE, $bytes, 4)){
        if (defined($verbose)){
            print $inform, "Reading 4-byte magic number in file header -- $bytes\n";
        }
        die "Error: input file magic number bytes 1-3 should be `pad'\n"
            unless substr($bytes, 0, 3) eq 'pad';
        $flags = dec2bin(unpack('N', $bytes));

        # least sig bit: is 32 bit?
        if ($flags & 1)        { $is32bit = 32; }
        else                   { $is32bit = 64; }

        # 2nd least sig bit: is big endian?
        if (($flags >> 1) & 1) { $byte_order = 'big'; }
        else                   { $byte_order = 'little'; }

        die "Error: input file magic number `$is32bit' should be either `32' or `64'"
            unless ($is32bit == 32 or $is32bit == 64);
        if (defined($verbose)){
            print $inform, "Input file contains $is32bit", "-bit addresses and is $byte_order endian.\n";
        }
    } else {
        die "Error: problem reading input file magic number\n";
    }
    $bytes_per_addr = $is32bit / 8;

    binmode(ADDR_FILE);

    # seek to the proper place in the file if --skip was used
    if (defined($skip_addrs)){
        die "Error: argument $skip_addrs should be a non-negative int\n"
            unless $skip_addrs > 0;
        my $skip_amt = $skip_addrs * $bytes_per_addr;
        # seek to the computed offset +4bytes for the magic number in the file header
        die "Error: cannot skipping $skip_addrs addresses, the input file probably doesn't contain that many addresses\n"
            unless seek(ADDR_FILE, $skip_amt + 4, 0);
        if (defined($verbose)){
            print $inform, "skipping $skip_addrs addresses ($skip_amt bytes)\n";
        }
    }

    # verify the legitimacy of the argument to --count
    if (defined($count_addrs)){
        die "Error: argument $count_addrs should be a non-negative int\n"
            unless $count_addrs > 0;
        if (defined($verbose)){
            print $inform, "printing only $count_addrs addresses\n";
        }        
        if ($is32bit == 64){
            $count_addrs *= 2;
        }
    }

    if ($byte_order eq 'big'){
        $unpack_code = $PACK_CODE_32BIT_BIGENDIAN;
    } elsif ($byte_order eq 'little'){
        $unpack_code = $PACK_CODE_32BIT_LITTLEENDIAN;
    } else {
        die "Error: unknown byte ordering found: $byte_order\n";
    }

    # do the printing of addresses
    while (read(ADDR_FILE, $bytes, 4) and $continue_read){
        $addr_count++;
        print sprintf("%x", unpack($unpack_code, $bytes));
        if (($is32bit == 32) or ($addr_count %2 == 0)){
            print "\n";
        }
        if (defined($count_addrs)){
            if ($count_addrs - $addr_count <= 0){
                $continue_read = 0;
            }
        }
    }
    if ($is32bit == 64){
        $addr_count /= 2;
    }

    if (defined($verbose)){
        print $inform, "Printed $addr_count addresses from input file $addr_info\n";
    }
}

$help_string = <<HELP_STRING;

usage : $0
    --addr_file   <addrfile>    <-- required
        The input file is expected to be a binary file. The first 4 bytes
        are interpreted as a magic number. The first 3 bytes of this number
        are currently fixed as the ascii values for 'p' (pmac), 'a'
        (address) and 'd' (dump) respectively, and the 4th contains flags
        bits. The first (most significant) 6 bits are unused, the 7th bit is
        set if the rest of the file's contents are to be interpreted as big 
        endian (rather than little endian), and the 8th bit is set if the
        rest of the file's contents are to be interpreted as 32-bit/4-byte
        values (rather than 64-bit/8-byte values).
    --skip        <num_skip>    <-- optional
    --count       <num_count>   <-- optional
    --verbose                   <-- optional
    --help
HELP_STRING

&AddrStreamRead_Main;

