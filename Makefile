##
# Vars:
# - COMPILE_TARGET	(req)		specify target arch
# - DEBUGDUMP		(def 0)		check results
# - DEBUG		(def 0)		compile for debug/amplxe
# - OMP			(def 1)		compile w/ OpenMP
# - LLGNUMP		(def 0)		use low-level GNU MP routines
# - IR			(def 0)		use IR code as baseline
# - DEBUGIR		(def 0)		debug IR code, must have IR=1
# - RESTRICT		(def 0)		use restrict keyword, must be remote compilation, with IR=1
# - STRRED		(def 0)		use strength reduction, IR=1
##

.PHONY: all clean

IR_OBJS = integer-reg.o
OBJS = globals.o client.o server.o
TARGET = ./ko

REMOTE_TARGETS = xeon mic
COMPILE_TARGETS = local $(REMOTE_TARGETS)

CFLAGS += -Wall -Wextra

# optimize only if DEBUG is either yes or 1
ifneq (, $(filter $(DEBUG), yes 1))
  CFLAGS := $(CFLAGS) -g -O3
else
  CFLAGS := $(CFLAGS) -O3
endif

# dump results from server (for checking) if DEBUGDUMP is either yes or 1
ifneq (, $(filter $(DEBUGDUMP), yes 1))
  CFLAGS := $(CFLAGS) -DDEBUG_RESULTS=1
endif

# low-level GNU MP only if LLGNUMP is either yes or 1
ifneq (, $(filter $(LLGNUMP), yes 1))
  $(warning This code is not perfect, fails to compute proper result)
  CFLAGS := $(CFLAGS) -DLLIMPL
endif

# use IR-based hand-written code only if IR is either yes or 1
ifneq (, $(filter $(IR), yes 1))
  CFLAGS := $(CFLAGS) -DIR_CODE
  OBJS := $(OBJS) $(IR_OBJS)
endif

# debug IR-based hand-written code only if DEBUGIR is either yes or 1
ifneq (, $(filter $(DEBUGIR), yes 1))
  CFLAGS := $(CFLAGS) -DDEBUG_IREG
endif

# strength reduction in IR code if STRRED is yes or 1
ifneq (, $(filter $(STRRED), yes 1))
  CFLAGS := $(CFLAGS) -DSTRRED
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

    # skip OpenMP if OMP is no or 0
    ifneq (, $(filter $(OMP), no 0))
    else
      CFLAGS := $(CFLAGS) -DHAVEOMP -fopenmp -qopt-report=4 -qopt-report-phase ipo
    endif

    # enable restrict keyword if RESTRICT is 1 or yes
    ifneq (, $(filter $(RESTRICT), yes 1))
      CFLAGS := $(CFLAGS) -DRESTRICT -restrict
    endif

    CFLAGS := $(CFLAGS) -I /usr/manual_install/gmp-6.0.0/build/$(COMPILE_TARGET)/include
    LDLIBS = /usr/manual_install/gmp-6.0.0/build/$(COMPILE_TARGET)/lib/libgmp.a
    ifeq ($(COMPILE_TARGET), mic)
      CFLAGS := $(CFLAGS) -mmic
    endif
  endif
endif

all: $(TARGET)

$(TARGET): $(OBJS)

clean:
	$(RM) $(TARGET) $(OBJS) $(IR_OBJS)
