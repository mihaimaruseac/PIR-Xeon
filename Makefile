.PHONY: all clean

REMOTE_TYPES = xeon mic
COMPILE_TYPES = local $(REMOTE_TYPES)

CFLAGS = -Wall -Wextra -g -O0 -fopenmp

ifneq ($(MAKECMDGOALS), clean)
  # if the value of $(COMPILE_TYPE) is not in $(COMPILE_TYPES)
  ifeq ($(filter $(COMPILE_TYPE), $(COMPILE_TYPES)),)
        $(error Please set COMPILE_TYPE to one of {$(COMPILE_TYPES)})
  endif

  ifeq ($(filter $(COMPILE_TYPE), $(REMOTE_TYPES)),)
    CC = gcc
    LDFLAGS = -lgmp
  else
    CC = icc
    LD = icc
    CFLAGS := $(CFLAGS) -I /usr/manual_install/gmp-6.0.0/build/$(COMPILE_TYPE)/include
    LDLIBS = /usr/manual_install/gmp-6.0.0/build/$(COMPILE_TYPE)/lib/libgmp.a
    ifeq ($(COMPILE_TYPE), mic)
      CFLAGS := $(CFLAGS) -mmic
    endif
  endif
endif

OBJS = globals.o client.o server.o
TARGET = ./ko

all: $(TARGET)

$(TARGET): $(OBJS)

clean:
	$(RM) $(TARGET) $(OBJS)
