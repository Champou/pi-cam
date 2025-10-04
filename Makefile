# ========= CONFIG =========
TARGET    := capture
SRC       := capture.c

TARGET_comm    := comm
SRC_comm       := comm.c uart/uart.c util/cJSON/cJSON.c util/json_wrapper.c

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
PI_CFLAGS  = -O3 -march=armv7-a -mfpu=neon -mfloat-abi=hard -flto -ffast-math
PI_CFLAGS_DEBUG = -O0 -g -march=armv7-a -mfpu=neon -mfloat-abi=hard

# For original Pi Zero (ARMv6)
# Cross-compile for Raspberry Pi Zero (ARMv6)
PI_ZERO_CC       = arm-linux-gnueabi-gcc
PI_ZERO_CFLAGS   = -O3 -mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=soft -static 
PI_ZERO_CFLAGS_DEBUG = -O0 -g -mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=soft -static 

# ========= RULES =========

.PHONY: all host pi clean debug

# Default rule → build for pi
all: pi

# Build for current machine (only for testing some high level things)
host:
	$(CC) $(CFLAGS) $(HOST_CFLAGS) $(SRC) -o $(TARGET) &&  \
	$(CC) $(CFLAGS) $(HOST_CFLAGS) $(SRC_comm) -o $(TARGET_comm)

# Cross-compile for Raspberry Pi Zero 2 W
pi:
	$(PI_CC) $(PI_CFLAGS) $(SRC) -o $(TARGET)-pi
	$(PI_CC) $(PI_CFLAGS) $(SRC_comm) -o $(TARGET_comm)-pi

# Cross-compile for Raspberry Pi Zero 2 W with debug flags (0 optimisation and -g flage for debug info)
debug:
	$(PI_CC) $(PI_CFLAGS_DEBUG) $(SRC) -o $(TARGET)-pi
	$(PI_CC) $(PI_CFLAGS_DEBUG) $(SRC_comm) -o $(TARGET_comm)-pi

# Cross-compile for Raspberry Pi Zero 2 W
piz:
	$(PI_ZERO_CC) $(PI_ZERO_CFLAGS) $(SRC) -o $(TARGET)-pi
	$(PI_ZERO_CC) $(PI_ZERO_CFLAGS) $(SRC_comm) -o $(TARGET_comm)-pi

# Cross-compile for Raspberry Pi Zero 2 W with debug flags (0 optimisation and -g flage for debug info)
debugz:
	$(PI_ZERO_CC) $(PI_ZERO_CFLAGS_DEBUG) $(SRC) -o $(TARGET)-pi
	$(PI_ZERO_CC) $(PI_ZERO_CFLAGS_DEBUG) $(SRC_comm) -o $(TARGET_comm)-pi

# Remove build artifacts
clean:
	rm -f $(TARGET) $(TARGET)-pi $(TARGET_comm) $(TARGET_comm)-pi
# ========= END =========