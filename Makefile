# Vars to define on command line:
# 	COMPILE_TARGET=...: specify compilation target, REQUIRED
# 	VALIDATE: dumps the result of the multiplication to validate transform

.PHONY: all clean

REMOTE_TARGETS = xeon mic
COMPILE_TARGETS = local $(REMOTE_TARGETS)

CFLAGS = -Wall -Wextra -O3 #-g -O0

# dump results if $(VALIDATE) is yes or 1
ifneq (, $(filter $(VALIDATE), yes 1))
  CFLAGS := $(CFLAGS) -DDEBUG_RESULTS
endif

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
