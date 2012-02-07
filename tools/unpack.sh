#!/usr/bin/env bash
DIR=$(dirname "$(readlink -f "$0")")
path_to_data=$1
if [[ -e "$path_to_data" ]] ; then
#	path_to_data=$(readlink -f $path_to_data)
#	echo $path_to_data
	$DIR/borpak/borpak "$path_to_data"
fi
