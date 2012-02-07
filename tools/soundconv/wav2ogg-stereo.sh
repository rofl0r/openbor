
#!/usr/bin/env bash
ffmpeg -i $1 -acodec libvorbis $2
#-ar 22050 $2

