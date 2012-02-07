#!/usr/bin/env perl
use strict;
use warnings;
use IO::Handle;

use File::Slurp;

use File::Basename; 
use Cwd 'abs_path';
my $path = dirname(abs_path($0));

STDOUT->autoflush(1);


sub getext {
	my $a = shift;
	return substr($a, length($a) - 3, 3);
}

sub convdir {
	my $ind = shift;
	my $delete = shift;
	for my $d(glob($ind . "/*")) {
#		print $d . "\n";
#		sleep(1);
		if ($d ne $ind && $d ne "." && $d ne "..") {
			if (-d $d) { convdir($d, $delete); } else {
				my $uf = uc(getext($d));
#				print $uf . "\n";
				if (!$delete && $uf eq "BOR") {
					`$path/bor2ogg.pl $d`;
				} elsif ($delete) {
					unlink $d if($uf eq "WAV" || $uf eq "BOR");
				}				
			}
		}
	}
}

sub syntax {
	print "syntax: $0 path_to_data\nif a 2nd param is passed, BOR and WAV files will be deleted\n";
}

print "BOR to OGG converter v 1.0 by anallyst\n";

my $indir = $ARGV[0] or die ("need indir as p1");
my $delete = defined($ARGV[1]) ? 1 : 0;
$indir =~ s|//|/|g;
$indir =~ s|$/||;
if (-d "$indir/music") {
	convdir("$indir/music", $delete);
} else {
	print "not exist $indir/music  or not dir!!1";
}
