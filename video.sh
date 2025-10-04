#!/usr/bin/env bash
set -u

# Configuration
COMM="./comm-pi"                     # comm executable (must be executable)
CAPTURE="./capture-pi"               # capture executable
FFMPEG_BIN="${FFMPEG_BIN:-ffmpeg}"
TMP_PREFIX="tmp_"
COMM_WAIT_TIMEOUT=1               # seconds to wait for comm to exit after capture ends


# Start comm in background (if it exists)
COMM_PID=""
if [ -x "$COMM" ]; then
    "$COMM" &
    COMM_PID=$!
    echo "Started comm (pid $COMM_PID)"
else
    echo "Warning: '$COMM' not found or not executable. Not starting comm."
fi


# find next available sequential tmp filename: tmp_1.mp4, tmp_2.mp4, ...
seq=1
while [ -e "${TMP_PREFIX}${seq}.mp4" ]; do
    seq=$((seq + 1))
done
outfile="${TMP_PREFIX}${seq}.mp4"
echo "Recording to ${outfile} ..."


# Run the capture pipeline (blocks until finished)
# "$CAPTURE" | "$FFMPEG_BIN" -f v4l2 -input_format mjpeg -video_size 640x480 -framerate 30 -i - \
#     -c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p "$outfile"
"$CAPTURE" | "$FFMPEG_BIN" -f mjpeg -i - -c copy "$outfile"




# After capture ends, decide whether to rename based on comm's status
if [ -n "${COMM_PID}" ]; then

        # poll for up to COMM_WAIT_TIMEOUT seconds
        end=$((SECONDS + COMM_WAIT_TIMEOUT))
        while kill -0 "$COMM_PID" 2>/dev/null && [ $SECONDS -lt $end ]; do
            sleep 0.1
        done

        if kill -0 "$COMM_PID" 2>/dev/null; then
            echo "comm still running after ${COMM_WAIT_TIMEOUT}s — keeping $outfile"
        else
            # comm exited: reap and get exit code
            wait "$COMM_PID"
            rc=$?
            if [ $rc -eq 0 ]; then
                timestamp=$(date +"%Y-%m-%d_%H-%M-%S")
                finalfile="capture_${timestamp}.mp4"
                candidate="$finalfile"
                i=0
                while [ -e "$candidate" ]; do
                    i=$((i + 1))
                    candidate="${finalfile%.mp4}_$i.mp4"
                done
                mv -- "$outfile" "$candidate"
                echo "Renamed $outfile -> $candidate (comm succeeded)"
            else
                echo "comm exited with non-zero exit code ${rc} — keeping $outfile"
            fi
        fi
    
else
    echo "comm was not started — keeping $outfile"
fi
