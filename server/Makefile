SRC ?= aesdsocket.c
TARGET ?= aesdsocket
OBJS := $(SRC:.c=.o)
CROSS_COMPILE:=
CC ?= $(CROSS_COMPILE)gcc
CFLAGS?= -g -Wall -DUSE_AESD_CHAR_DEVICE
LDFLAGS?=-pthread -lrt

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
