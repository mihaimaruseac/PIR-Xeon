.PHONY: all clean

CC = gcc
CFLAGS = -Wall -Wextra -g -O0
LDFLAGS = -lgmp
OBJS = globals.o client.o
TARGET = ./ko

all: $(TARGET)

$(TARGET): $(OBJS)

clean:
	$(RM) $(TARGET) $(OBJS)
