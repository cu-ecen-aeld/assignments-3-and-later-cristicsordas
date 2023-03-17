SRC := aesdsocket.c
TARGET = aesdsocket
OBJS := $(SRC:.c=.o)
CC := gcc
CFLAGS:= -DDEBUG -g
LDFLAGS=-pthread

ifeq ("$(CROSS_COMPILE)", "aarch64-none-linux-gnu-")
	CC := aarch64-none-linux-gnu-g++
endif

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map
