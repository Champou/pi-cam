# ========= CONFIG =========
TARGET    := capture
SRC       := capture.c

TARGETfilename    := filename
SRCfilename       := filename.c uart/uart.c

# Default: build for host machine
CC        ?= gcc
CFLAGS    ?= -O3 -flto -ffast-math
LDFLAGS   ?=

# ========= HOST (x86) BUILD =========
HOST_CFLAGS = -march=native

# ========= PI ZERO 2 W (ARMv7) BUILD =========
# Cross compiler prefix (installed via `sudo apt install gcc-arm-linux-gnueabihf`)
CROSS_COMPILE ?= arm-linux-gnueabihf-
PI_CC      = $(CROSS_COMPILE)gcc
PI_CFLAGS  = -O3 -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -flto -ffast-math

# ========= RULES =========

.PHONY: all host pi clean

# Default rule → build for host
all: host

# Build for current machine
host:
	$(CC) $(CFLAGS) $(HOST_CFLAGS) $(SRC) -o $(TARGET) &&  \
	$(CC) $(CFLAGS) $(HOST_CFLAGS) $(SRCfilename) -o $(TARGETfilename)

# Cross-compile for Raspberry Pi Zero 2 W
pi:
	$(PI_CC) $(PI_CFLAGS) $(SRC) -o $(TARGET)-pi

# Remove build artifacts
clean:
	rm -f $(TARGET) $(TARGET)-pi $(TARGETfilename)
# ========= END =========