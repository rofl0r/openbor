#!/usr/bin/env perl
use strict;
use warnings;
use File::Basename;
use Cwd 'abs_path';
my $path = dirname(abs_path($0));

my $infile = $ARGV[0] or die ("need infile as p1");
my $outfile = substr($infile, 0, length($infile) - 3) . "wav";
`$path/../bor2wav/bor2wav $infile $outfile`;
die "couldnt convert to wave!" if (! -e $outfile);
$infile = $outfile;
$outfile = substr($infile, 0, length($infile) - 3) . "ogg";
`$path/wav2ogg.sh $infile $outfile`;
die "couldnt convert to ogg!" if (! -e $outfile);
