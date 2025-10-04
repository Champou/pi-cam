#include <stddef.h>
#include <termios.h>

int uart_open(const char *device, speed_t baudrate);
int uart_read(char *buf, size_t len);
int uart_write(const char *buf, size_t len);
int uart_close();