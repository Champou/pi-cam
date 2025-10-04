#!/usr/bin/env bash
set -euo pipefail

UART_DEVICE="/dev/ttyUSB0"
BAUDRATE=115200
DELIM_START="START"
DELIM_END="END"

# configure UART
stty -F "$UART_DEVICE" "$BAUDRATE" raw -echo

# prepare JSON with system time
current_time=$(date +"%Y-%m-%d %H:%M:%S")
json_payload="{\"time\":\"${current_time}\"}"

# send
{
    printf "%s" "$DELIM_START"
    printf "%s" "$json_payload"
    printf "%s" "$DELIM_END"
} > "$UART_DEVICE"

echo "Sent: $DELIM_START$json_payload$DELIM_END"
