#!/usr/bin/env perl

# purpose : replace all links to a specific filename [arg 1]
# in a specific file [arg 0] with another filename [arg 2]

use strict;
use warnings;

use File::Basename; 
use File::Slurp;
use Cwd 'abs_path';

my $path = dirname(abs_path($0));

my $infile = $ARGV[0] or die "need filename to change as argv0\n";
my $what = $ARGV[1] or die "need filename which should be replaced as argv1\n";
my $with = $ARGV[2] or die "need filename which should be used as a replacement as argv2\n";

my @fc = read_file($infile) or die "error reading $infile: $!\n";
my @out;

for (@fc) {
	if(! /^#/) {
		s/\Q$what\E/$with/ig;
	}
	push(@out, $_);
}
write_file $infile, @out;




