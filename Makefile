.PHONY: all clean

REMOTE_TARGETS = xeon mic
COMPILE_TARGETS = local $(REMOTE_TARGETS)

CFLAGS += -Wall -Wextra -O3 #-g -O0

ifneq ($(MAKECMDGOALS), clean)
  # if the value of $(COMPILE_TARGET) is not in $(COMPILE_TARGETS)
  ifeq ($(filter $(COMPILE_TARGET), $(COMPILE_TARGETS)),)
        $(error Please set COMPILE_TARGET to one of {$(COMPILE_TARGETS)})
  endif

  ifeq ($(filter $(COMPILE_TARGET), $(REMOTE_TARGETS)),)
    CC = gcc
    LDFLAGS = -lgmp
  else
    CC = icc
    LD = icc
    CFLAGS := $(CFLAGS) -DHAVEOMP -fopenmp -qopt-report=4 -qopt-report-phase ipo
    CFLAGS := $(CFLAGS) -I /usr/manual_install/gmp-6.0.0/build/$(COMPILE_TARGET)/include
    LDLIBS = /usr/manual_install/gmp-6.0.0/build/$(COMPILE_TARGET)/lib/libgmp.a
    ifeq ($(COMPILE_TARGET), mic)
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
