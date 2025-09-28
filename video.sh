

./capture | ffmpeg -f mjpeg -framerate 30 -i - \
    -c:v libx264 -preset veryfast -crf 23 \
    -pix_fmt yuv420p output.mp4
