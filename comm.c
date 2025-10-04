/// Module to read from UART, parse messages, and manage system settings
/// Messages are delimited by "START" and "END" and formatted in JSON
/// Example message: START{"time":"2023-10-05 14:30:00"}END
/// Uses uart/uart.h for UART operations and json_wrapper.h for JSON parsing


#define _XOPEN_SOURCE // for strptime
#define _POSIX_C_SOURCE 200809L // for clock_settime

#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "uart/uart.h"
#include "util/json_wrapper.h"

#define BAUD B115200

#define READ_BUFFER_SIZE 512        // accumulation of message buffer size
#define WRITE_BUFFER_SIZE 128       // not used yet
#define UART_READ_BUFSIZE 64        // read chunk size from UART
#define MAX_MESSAGE_SIZE 256        // max size of extracted message between delimiters

// Delimiters for messages, Adjust as needed
static const char START_DELIM[] = "START";
static const char END_DELIM[]   = "END";


/// @brief Parse date string "YYYY-MM-DD HH:MM:SS" into struct tm
static int parse_datetime(const char *str, struct tm *tm_out) {
    if (strptime(str, "%Y-%m-%d %H:%M:%S", tm_out) == NULL)
        return -1;
    return 0;
}


/// @brief Set system time from struct tm
static int set_system_time(struct tm *tm_val) {
    struct timespec ts;
    ts.tv_sec = mktime(tm_val);
    ts.tv_nsec = 0;

    if (ts.tv_sec == -1) {
        fprintf(stderr, "Invalid time\n");
        return -1;
    }

    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        perror("clock_settime");
        return -1;
    }
    return 0;
}



//TODO : Insert other needed Settings functions here and use them in process_message
///  @note example message  that can be Decoded:  {"time":"2023-10-05 14:30:00"}
static int process_message(const char *msg) 
{
    //time 
    //fetch "time" key from JSON and set system time
    char *timeString = json_search(msg, "time");
    struct tm tm_val = {0};
    if (parse_datetime(timeString, &tm_val) == 0) {
        if (set_system_time(&tm_val) == 0) {
            printf("System time updated: %s\n", msg);
            return EXIT_SUCCESS;
        }
    }

    return EXIT_FAILURE;
}


/// @brief cleanup handler to close UART on exit
static void _atexit() {
    uart_close();
} 

/// @brief Fetches UART messages, validates JSON, and processes them
/// @details Continuously reads from UART, accumulates data, extracts messages
/// between START and END delimiters, validates JSON format, and processes valid messages.
/// @note Adjust START_DELIM and END_DELIM as needed for your message format.
/// @note Uses uart/uart.h for UART operations and util/json_wrapper.h for JSON parsing
/// @return EXIT_SUCCESS on successful processing
int main() {
    const char *uart_device = "/dev/ttyS0";  // change as needed

    //setup cleanup handler
    atexit(_atexit);

    // Open UART
    if (uart_open(uart_device, BAUD) != 0) 
        return EXIT_FAILURE;

    char buffer[READ_BUFFER_SIZE];   // accumulation buffer
    int buf_len = 0;

    struct timespec ts = {0, 100000000}; // 0s + 100ms, time for nanosleep

    while (1) 
    {
        char temp[UART_READ_BUFSIZE];
        int n = uart_read(temp, sizeof(temp));
        if (n > 0) {
            // append read bytes to buffer
            if (buf_len + n >= (int)sizeof(buffer)) {
                // avoid overflow
                memmove(buffer, buffer + n, buf_len - n);
                buf_len -= n;
            }
            memcpy(buffer + buf_len, temp, n);
            buf_len += n;
            buffer[buf_len] = '\0';

            // search for complete message
            char *start_ptr = strstr(buffer, START_DELIM);
            char *end_ptr   = strstr(buffer, END_DELIM);

            //complete message found
            if (start_ptr && end_ptr && start_ptr < end_ptr)
            {
                // extract message between delimiters
                char msg[MAX_MESSAGE_SIZE];

                int msg_len = end_ptr - (start_ptr + strlen(START_DELIM));
                if (msg_len >= (int)sizeof(msg)) msg_len = sizeof(msg) - 1;
                strncpy(msg, start_ptr + strlen(START_DELIM), msg_len);
                msg[msg_len] = '\0';

                //check json validity
                if (json_validate(msg)) 
                {
                    if (process_message(msg) == EXIT_SUCCESS) 
                    {
                        return EXIT_SUCCESS;
                    } 
                }

                // remove processed message from buffer
                memmove(buffer, end_ptr + strlen(END_DELIM), buf_len - (end_ptr + strlen(END_DELIM) - buffer));
                buf_len -= (end_ptr + strlen(END_DELIM) - buffer);
            }
        }

        nanosleep(&ts, NULL);
    }

    return EXIT_FAILURE;
}
