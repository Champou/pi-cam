#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "uart.h"


#define READ_BUFFER_SIZE 512
#define WRITE_BUFFER_SIZE 128
#define MAX_MESSAGE_SIZE 256
#define TEMP_BUFFER_SIZE 64

static const char START_DELIM[] = "START";
static const char END_DELIM[]   = "END";

// Parse date string "YYYY-MM-DD HH:MM:SS" into struct tm
int parse_datetime(const char *str, struct tm *tm_out) {
    if (strptime(str, "%Y-%m-%d %H:%M:%S", tm_out) == NULL)
        return -1;
    return 0;
}

// Set system time
int set_system_time(struct tm *tm_val) {
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

static void _atexit() {
    uart_close();
} 

int main() {
    const char *uart_device = "/dev/ttyS0";  // change as needed
    int baudrate = B115200;

    //setup cleanup handler
    atexit(_atexit);

    // Open UART
    if (uart_open(uart_device, baudrate) != 0) 
        return 1;

    char buffer[512];   // accumulation buffer
    int buf_len = 0;

    while (1) {
        char temp[64];
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
            if (start_ptr && end_ptr && start_ptr < end_ptr) {
                int msg_len = end_ptr - (start_ptr + strlen(START_DELIM));
                char msg[MAX_MESSAGE_SIZE];
                if (msg_len >= (int)sizeof(msg)) msg_len = sizeof(msg) - 1;
                strncpy(msg, start_ptr + strlen(START_DELIM), msg_len);
                msg[msg_len] = '\0';

                struct tm tm_val = {0};
                if (parse_datetime(msg, &tm_val) == 0) 
                {
                    if (set_system_time(&tm_val) == 0) 
                    {
                        printf("System time updated: %s\n", msg);
                        return 0; // exit after successful update
                    }
                }
                // remove processed message from buffer
                memmove(buffer, end_ptr + strlen(END_DELIM), buf_len - (end_ptr + strlen(END_DELIM) - buffer));
                buf_len -= (end_ptr + strlen(END_DELIM) - buffer);
            }
        }
        usleep(100000); // 100ms
    }

    return 0;
}
