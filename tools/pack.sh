#!/usr/bin/env bash
DIR=$(dirname "$(readlink -f "$0")")
echo "this tools makes a PAK archive from a data folder. the data folder should be in the directory as this script,"
echo "otherwise borpak will prefix crap to the pakfiles file names"
echo "syntax $0 path_to_data myfilename.pak"
path_to_data=$1
outfile=$2
if [[ -e $path_to_data ]] ; then
#	path_to_data=$(readlink -f $path_to_data)
#	echo $path_to_data
	#delete MS shit
	find $path_to_data -iname 'thumbs.db' -exec rm "{}" \;
	#delete leftovers of text editor...
	find $path_to_data -name '*.*~' -exec rm "{}" \;
	#and pack it
	$DIR/borpak/borpak -b -d $path_to_data $outfile
fi
