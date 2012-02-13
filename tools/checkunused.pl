#!/usr/bin/env perl
use strict;
use warnings;
use List::Util qw (first);

use File::Basename; 
use File::Slurp;
use Cwd 'abs_path';
use Data::Dump qw(dump);
use IO::Handle;
STDOUT->autoflush(1);

my $path = dirname(abs_path($0));
my $syntax = "\nunnecessary file remover 1.0 by anallyst\n\n"
. "syntax: $0 [-d]\n\n"
. "your working directory needs to contain a data folder containing the pak files\n"
. "the files will be checked if they are required by the game or not.\nif -d is used, the unnecesarry files will be deleted.\n"
. "if -d is ommited, only a dry-run is executed and a usage log of each file is printed.\n";

if(! -d "data/") {
	print "couldn't find data/ dir !\n\n$syntax";
	exit 1;
}

my $delete = defined($ARGV[0]) && $ARGV[0] eq "-d";

my @a = `find data/ -type f`;
my @rootfiles = qw(data/scenes/howto.txt data/scenes/logo.txt data/scenes/intro.txt data/scenes/gameover.txt data/script.txt data/models.txt data/levels.txt data/lifebar.txt data/menu.txt);
my @cfiles = qw(data/scripts/update.c data/scripts/updated.c data/scripts/level.c data/scripts/endlevel.c data/scripts/keyall.c data/scripts/timetick.c data/scripts/score1.c data/scripts/score2.c data/scripts/score3.c data/scripts/score4.c data/scripts/key1.c data/scripts/key2.c data/scripts/key3.c data/scripts/key4.c data/scripts/join1.c data/scripts/join2.c data/scripts/join3.c data/scripts/join4.c data/scripts/respawn1.c data/scripts/respawn2.c data/scripts/respawn3.c data/scripts/respawn4.c data/scripts/die1.c data/scripts/die2.c data/scripts/die3.c data/scripts/die4.c);
my @neededfiles = qw(data/pal.act);
my @other_gif = qw(data/bgs/logo.gif data/bgs/title.gif data/bgs/titleb.gif data/bgs/loading.gif data/bgs/loading2.gif data/bgs/hiscore.gif data/bgs/complete.gif data/bgs/unlockbg.gif data/bgs/select.gif);
my @other_png = qw(data/bgs/logo.png data/bgs/title.png data/bgs/titleb.png data/bgs/loading.png data/bgs/loading2.png data/bgs/hiscore.png data/bgs/complete.png data/bgs/unlockbg.png data/bgs/select.png);
my @other_bmp = qw(data/bgs/logo.bmp data/bgs/title.bmp data/bgs/titleb.bmp data/bgs/loading.bmp data/bgs/loading2.bmp data/bgs/hiscore.bmp data/bgs/complete.bmp data/bgs/unlockbg.bmp data/bgs/select.bmp);
my @other_pcx = qw(data/bgs/logo.pcx data/bgs/title.pcx data/bgs/titleb.pcx data/bgs/loading.pcx data/bgs/loading2.pcx data/bgs/hiscore.pcx data/bgs/complete.pcx data/bgs/unlockbg.pcx data/bgs/select.pcx);
my @other = (@other_gif, @other_png, @other_bmp, @other_pcx);
my @sprites = qw(data/sprites/shadow1.gif data/sprites/shadow2.gif data/sprites/shadow3.gif data/sprites/shadow4.gif data/sprites/shadow5.gif data/sprites/shadow6.gif data/sprites/arrow.gif data/sprites/arrowl.gif data/sprites/hole.gif data/sprites/hole.png data/sprites/hole.pcx data/sprites/hole.png); 
my @fonts = qw(data/sprites/font.gif data/sprites/font2.gif data/sprites/font3.gif data/sprites/font4.gif data/sprites/font5.gif data/sprites/font6.gif data/sprites/font7.gif data/sprites/font8.gif);
my @sounds = qw(data/sounds/go.wav data/sounds/beat1.wav data/sounds/block.wav data/sounds/fall.wav data/sounds/get.wav data/sounds/money.wav data/sounds/jump.wav data/sounds/indirect.wav data/sounds/punch.wav data/sounds/1up.wav data/sounds/timeover.wav data/sounds/beep.wav data/sounds/beep2.wav data/sounds/bike.wav);
my @video = qw(data/video.txt data/videopc.txt data/videowii.txt data/video169.txt data/video43.txt data/videopsp.txt data/video169.txt data/videodc.txt data/video43.txt data/videowiz.txt data/video169.txt data/videogp2x.txt data/video43.txt data/videodingoo.txt data/video43.txt data/videosymbian.txt);
my @music_bor = qw(data/music/complete.bor data/music/menu.bor data/music/remix.bor data/music/gameover.bor);
my @music_ogg = qw(data/music/complete.ogg data/music/menu.ogg data/music/remix.ogg data/music/gameover.ogg);
my @music_oga = qw(data/music/complete.oga data/music/menu.oga data/music/remix.oga data/music/gameover.oga);
my @music_wav = qw(data/music/complete.wav data/music/menu.wav data/music/remix.wav data/music/gameover.wav);
my @music = (@music_bor, @music_ogg, @music_oga, @music_wav);
my @combinedroot = (@rootfiles, @video, @cfiles);
my @needed = (@neededfiles, @other, @sprites, @fonts, @sounds, @music, @combinedroot);
#dump(@chkfiles);

my %hneeded;

sub scandeps {
	my $fn = shift;
	$hneeded{lc($fn)} = 1;
	if(lc($fn) =~ /\.txt$/ || lc($fn) =~ /\.c$/) {
		my @fc = read_file $fn;
		my $ccomment = lc(substr($fn, -4)) ne ".txt";
		for my $line(@fc) {
			$line = lc($line);
	                if( ! (
        	                ($ccomment && $line =~ /^(\s*?)\/\//) ||
                	        (!$ccomment && $line =~ /^(\s*?)#/)
	                )) {
 				#print $line;
				if($line =~ /\./) {
					while(
						(
							$line =~ /(data\/[\w_\-\/]+\.(pal|txt|c|bor|ogg|oga|wav|gif|png|bmp|pcx))/g
						)
					) {
						my $file = $1;
						#print "XXXX $file\n";
						scandeps($file) if(-e $file) && !defined($hneeded{$file});
					}
				}
			}
		}
	}
}


for my $n(@needed) {
	scandeps($n) if (-e $n);
}

for my $b(@a) {
	chomp $b;
	if(!defined($hneeded{lc($b)})) {
		if($delete) {
			unlink $b;
			print "$b deleted\n";
		} else {
			print "$b is not needed\n";
		}
	}
}

