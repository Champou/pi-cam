===========================
PROJECT SETUP
===========================
This project uses Makefile for compilation.
All compilation options for cross-compile are there :
- make pi (build for pi zero 2 w)
- make piz (build for pi zero)
- make clean (clean project outputs)
- make host (build for host machine nb:used only for testing, as the code will run on rpi)

===========================
PROJECT Architechture
===========================
- capture : interface with camera and fetch frames, push camra data out.
-   -v option for verbosity (if used indepedently, will print debug messages)

- comm : UART interface to fetch info, includes START and END delimiters to message.
        note : message must be json format
        you may modify message decoding to support more configurations
- video.sh : shellscript that handles both executables, launches comm and capture and once capture is done, modifies video file name to match date&time

Captured frames are mjpeg format
Captured frames are piped into ffmpeg to handle mp4 file creation
No codecs are used (h.264 is too cpu-heavy for raspberry pi zero), hence the -c copy option

