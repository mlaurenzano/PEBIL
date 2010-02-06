#!/usr/local/bin/perl

use strict;

sub cleanCommentWSpace {
	my $line = shift @_;
	$line =~ s/#.*$//g;
	$line =~ s/^\s+//g;
	return $line;
}

my $lastjbbseq = 0;
my @jbbmapping;
my $jbbstatic = "/dsgpfs/mtikir/pmacTRACE/jbbinst/hycom_standard/hycom.standard.jbbinst.static";
open(JBB_STATIC,"<",$jbbstatic) or die "Error:$jbbstatic\n";
while(my $line = <JBB_STATIC>){
	chomp($line);
	if(!($line = cleanCommentWSpace($line))){
		next;
	}
	my @tokens = split /\s+/,$line;
	$jbbmapping[$tokens[0]] = $tokens[1];
	$lastjbbseq = $tokens[0];
}
close(JBB_STATIC);

my $lastsimseq = 0;
my @simmapping;
my $simstatic = "/dsgpfs/mtikir/pmacTRACE/siminst/hycom_standard_0059/p01/hycom.standard.phase.1.0059.siminst.static";
open(SIM_STATIC,"<",$simstatic) or die "Error:$simstatic\n";
while(my $line = <SIM_STATIC>){
	chomp($line);
	if(!($line = cleanCommentWSpace($line))){
		next;
	}
	my @tokens = split /\s+/,$line;
	$simmapping[$tokens[0]] = $tokens[1];
	$lastsimseq = $tokens[0];
}
close(SIM_STATIC);

for(my $i=0;$i<59;$i++){
	my %id_to_count;
	for(my $j=0;$j<=$lastjbbseq;$j++){
		$id_to_count{$jbbmapping[$j]} = 0;
	}
	my $jbbfile = "/dsgpfs/mtikir/pmacTRACE/jbbcoll/hycom_standard_0059/hycom.standard.meta_".
					sprintf("%04d",$i) . ".jbbinst";
	my $simfile = "/dsgpfs/mtikir/pmacTRACE/simcoll/hycom_standard_0059/p01/hycom.standard.phase.1.meta_" .
					sprintf("%04d",$i) . ".0059.siminst";
	my $outfile = "hycom.standard.phase.1.meta_" .  sprintf("%04d",$i) . ".0059.siminst";
	print "Processing $jbbfile and $simfile\n";
	open(JBB_FILE,"<",$jbbfile) or die "Error:$jbbfile\n";
	while(my $line = <JBB_FILE>){
		chomp($line);
		if(!($line = cleanCommentWSpace($line))){
			next;
		}
		my @tokens = split /\s+/,$line;
		my $bbid = $jbbmapping[$tokens[0]];
		die "Error: unknown id $bbid at $jbbfile\n" unless (exists $id_to_count{$bbid});
		$id_to_count{$bbid} = $tokens[1];
	}
	close(JBB_FILE);

	open(OUT_FILE,">",$outfile) or die "Error:$outfile\n";
	open(SIM_FILE,"<",$simfile) or die "Error:$simfile\n";
	while(my $line = <SIM_FILE>){
		chomp($line);
		if(!cleanCommentWSpace($line)){
			print OUT_FILE "$line\n";
		} elsif($line =~ /^block/){
			my @tokens = split /\s+/,$line;
			my $bbid = $simmapping[$tokens[1]];
			die "Error: unknown id $bbid at $simfile\n" unless (exists $id_to_count{$bbid});
			print OUT_FILE "$tokens[0]\t$tokens[1]\t$id_to_count{$bbid}\t$tokens[2]\t$tokens[3]\n";
		} else {
			print OUT_FILE "$line\n";
		}
	}
	close(SIM_FILE);
	close(OUT_FILE);
}
