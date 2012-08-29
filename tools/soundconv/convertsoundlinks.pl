#!/usr/bin/env perl
use strict;
use warnings;
use File::Slurp qw(read_file write_file);
use Data::Dump qw(dump);
#use re 'debugcolor';
use IO::Handle;
use File::Basename;
STDOUT->autoflush(1);

sub getext {
	my $a = shift;
	return lc($1) if($a =~ /(\.\w+)$/);
	return "";
}

sub processfile {
	my $f = shift;
	my @fc = read_file $f;
	my $rw = 0;
	my @outfc = "";
	for (@fc) {		
		if (lc($_) =~ /music\s+(\w|~|_|\-|\/|\\)+\.bor/i || lc($_) =~ /bossmusic\s+(\w|~|_|\-|\/|\\)+\.bor/i
			|| lc($_) =~ /playmusic\s*\(\s*"(\w|~|_|\-|\/|\\)+\.bor/i
		) {
			#/^(music\s+)(?:\w+\/)+((\w+)\.bor)(\w+(1|0))/)) {
			#print $_ . "\n";
			#dump @mat;
			$_ = lc($_);
			s/\\/\//g;
			s/\.bor/\.ogg/i;
			$rw = 1;
		} elsif ((/^music/ || /^bossmusic/) && ! /\.ogg/ && ! /^musicoverlap/ ) {
			print "regex failure: $f\n$_\n";
		}
		push(@outfc, $_);
	}
	if ($rw) {write_file $f, @outfc; print "rewrote $f\n";}
#	write_file "/tmp/sgfff.txt",  @outfc if $rw;
}

sub processdir {
	my $ind = shift;
	for my $d(glob($ind . "/*")) {
#		print $d . "\n";
		if ($d ne $ind && $d ne "." && $d ne "..") {
			if (-d $d) { processdir($d); } else {
				my $uf = getext($d);

				if ($uf eq ".txt" || $uf eq ".c") {
					processfile($d);
				} elsif ($uf eq ".txt~") {
					unlink($d);
				}
			}
		}
	}
}

sub syntax {
	print "syntax: $0 path_to_data\n";
}

print "this tool converts all links to music in openbor data directory to point to .ogg instead .bor\n";

my $indir = $ARGV[0] or die ("need indir as p1");
$indir =~ s|//|/|g;
$indir =~ s|$/||;

if (-d $indir) {
	processdir($indir);
} else {
	print "not exist $indir or not dir!!1";
}
