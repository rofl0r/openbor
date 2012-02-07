#!/usr/bin/env perl
use strict;
use warnings;
use List::Util qw (first);

use File::Basename; 
use File::Slurp;
use Cwd 'abs_path';
use Data::Dump qw(dump);

my $path = dirname(abs_path($0));
my $syntax = "\nunnecessary file remover 1.0 by anallyst\n\n"
. "syntax: $0 list.txt [-d]\n\nwhere list.txt is the name of a file containing all the filenames which\n"
. "shall be checked if they are required by the game or not.\nif -d is used, the unnecesarry files will be deleted.\n"
. "if -d is ommited, only a dry-run is executed and a usage log of each file is printed.\n"
. "the list is typically created with find (shell command) or extracted from OpenBOR.log after playing a game while using the\n"
. "showfilesused=1 command line argument to OpenBORs binary\n\n";

my $in = $ARGV[0] or die ($syntax);
my $delete = defined($ARGV[1]) ? 1 : 0;

my @a = read_file($in);
my @textfiles = `find data/ -iname '*.txt'`;
my @scriptfiles = `find data/ -iname '*.c'`;
my @chkfiles = (@textfiles, @scriptfiles);
my @rootfiles = qw(data/scenes/howto.txt data/scenes/logo.txt data/scenes/intro.txt data/scenes/gameover.txt data/script.txt data/models.txt data/levels.txt data/lifebar.txt data/menu.txt);
my @cfiles = qw(data/scripts/update.c data/scripts/updated.c data/scripts/level.c data/scripts/endlevel.c data/scripts/keyall.c data/scripts/timetick.c data/scripts/score1.c data/scripts/score2.c data/scripts/score3.c data/scripts/score4.c data/scripts/key1.c data/scripts/key2.c data/scripts/key3.c data/scripts/key4.c data/scripts/join1.c data/scripts/join2.c data/scripts/join3.c data/scripts/join4.c data/scripts/respawn1.c data/scripts/respawn2.c data/scripts/respawn3.c data/scripts/respawn4.c data/scripts/die1.c data/scripts/die2.c data/scripts/die3.c data/scripts/die4.c);
my @neededfiles = qw(data/pal.act);
my @other = qw(data/bgs/logo data/bgs/title data/bgs/titleb data/bgs/loading data/bgs/loading2 data/bgs/hiscore data/bgs/complete data/bgs/unlockbg data/bgs/select);
my @sprites = qw(data/sprites/shadow1.gif data/sprites/shadow2.gif data/sprites/shadow3.gif data/sprites/shadow4.gif data/sprites/shadow5.gif data/sprites/shadow6.gif data/sprites/arrow.gif data/sprites/arrowl.gif data/sprites/hole); 
my @fonts = qw(data/sprites/font data/sprites/font2 data/sprites/font3 data/sprites/font4 data/sprites/font5.gif data/sprites/font6.gif data/sprites/font7.gif data/sprites/font8.gif);
my @sounds = qw(data/sounds/go.wav data/sounds/beat1.wav data/sounds/block.wav data/sounds/fall.wav data/sounds/get.wav data/sounds/money.wav data/sounds/jump.wav data/sounds/indirect.wav data/sounds/punch.wav data/sounds/1up.wav data/sounds/timeover.wav data/sounds/beep.wav data/sounds/beep2.wav data/sounds/bike.wav);
my @video = qw(data/video.txt data/videopc.txt data/videowii.txt data/video169.txt data/video43.txt data/videopsp.txt data/video169.txt data/videodc.txt data/video43.txt data/videowiz.txt data/video169.txt data/videogp2x.txt data/video43.txt data/videodingoo.txt data/video43.txt data/videosymbian.txt);
my @music = qw(data/music/complete data/music/menu data/music/remix);
my @combinedroot = (@rootfiles, @video, @cfiles);
my @needed = (@neededfiles, @other, @sprites, @fonts, @sounds, @music, @combinedroot);
#dump(@chkfiles);

sub isInUncommentedLine {
	my ($search, $file) = @_;
	return 0 if(! -e $file);
	my @fc = read_file $file;
	my $ccomment = lc(substr($file, -4)) ne ".txt";
	for my $l(@fc) {
		if( ! (
			($ccomment && $l =~ /^(\s*?)\/\//) ||
			(!$ccomment && $l =~ /^(\s*?)#/) 
		)) {
			return 1 if $l =~ /\Q$search\E/i;
		}
	}
	return 0;
}

sub checkfile {
	my ($search) = @_;
	chomp($search);
	for my $f(@chkfiles) {
		chomp($f);
		if (isInUncommentedLine($search, $f)) {
			print "$search is referenced from $f\n" unless $delete;
			if(first {/^\Q$f\E/i} @combinedroot) {
				return $f;
			} else {
				my $ret = (checkfile($f));
				return $ret if $ret ne "";
			}
		}
	}
	return "";
}

for my $b (@a) {
	my $c = $b;
	chomp($c);
	my $req = first {(lc($c) eq $_) || ($_ !~ /\./ && $c =~ /\Q$_\E\./i)} @needed;
	my $r = $req ? "default" : checkfile($b);
	if($r ne "") {
		print $c . " is required by $r!\n" unless $delete;
	} else {
		print $c . " is not needed\n" unless $delete;
		if ($delete) {
			unlink $c;
			print "deleted $c\n";
		}
	}
}
