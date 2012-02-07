#!/bin/sh
wavfile=$1
mp3file=$2
#-a : mono
lame -h -b 96 -a $wavfile $mp3file

