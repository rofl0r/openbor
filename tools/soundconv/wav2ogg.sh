
#!/usr/bin/env bash
ffmpeg -i $1 -acodec libvorbis -ac 1 -ab 48k $2
#-ar 22050 $2

