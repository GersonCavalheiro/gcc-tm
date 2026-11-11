# Included by all example Makefiles; paths are independent of the caller directory.
COMMON := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(COMMON)/../..)
GCC_BUILD ?= $(ROOT)/gcc-build
GCC_BUILD := $(abspath $(GCC_BUILD))
TRANSACTION_CC ?= $(GCC_BUILD)/gcc/xgcc
TARGET := $(shell $(TRANSACTION_CC) -dumpmachine)
GOMP_BUILD ?= $(GCC_BUILD)/$(TARGET)/libgomp
ITM_BUILD ?= $(GCC_BUILD)/$(TARGET)/libitm
GOMP_SOURCE := $(ROOT)/gcc-src-tm/libgomp
VARIANTS := sequential internal directive gnu_tm
BINARIES := $(addprefix .build/,$(VARIANTS))
CFLAGS ?= -O2 -g -Wall -Wextra -Werror
COMPONENTS := -B$(GCC_BUILD)/gcc/ -B$(GOMP_BUILD)/
GOMP_LINK := -L$(GOMP_BUILD)/.libs -Wl,-rpath,$(GOMP_BUILD)/.libs
ITM_LINK := -L$(ITM_BUILD)/.libs -Wl,-rpath,$(ITM_BUILD)/.libs
.PHONY: all check clean $(VARIANTS)
all: $(BINARIES)
$(VARIANTS): %: .build/%
.build:
	mkdir -p $@
.build/sequential: sequential.c $(COMMON)/input.h $(COMMON)/timing.h Makefile $(COMMON)/rules.mk | .build
	$(TRANSACTION_CC) $(COMPONENTS) $(CPPFLAGS) $(CFLAGS) -I$(COMMON) $< $(LDFLAGS) $(LDLIBS) -o $@
.build/internal.o: internal.c $(COMMON)/internal.h $(COMMON)/input.h $(COMMON)/timing.h Makefile $(COMMON)/rules.mk | .build
	$(TRANSACTION_CC) $(COMPONENTS) $(CPPFLAGS) $(CFLAGS) -I$(COMMON) -I$(GOMP_SOURCE) -I$(ROOT)/gcc-src-tm/include -I$(GOMP_BUILD) -fopenmp -c $< -o $@
# Link without -fopenmp: private and OpenMP symbols come from the same archive.
.build/internal: .build/internal.o $(GOMP_BUILD)/.libs/libgomp.a
	$(TRANSACTION_CC) $(COMPONENTS) $(CFLAGS) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@
.build/directive: directive.c $(COMMON)/input.h $(COMMON)/timing.h Makefile $(COMMON)/rules.mk $(GOMP_BUILD)/.libs/libgomp.so | .build
	$(TRANSACTION_CC) $(COMPONENTS) $(CPPFLAGS) $(CFLAGS) -I$(COMMON) -I$(GOMP_BUILD) -fopenmp -fopenmp-transaction-stm $< $(LDFLAGS) $(GOMP_LINK) $(LDLIBS) -o $@
.build/gnu_tm: gnu_tm.c $(COMMON)/input.h $(COMMON)/timing.h Makefile $(COMMON)/rules.mk $(ITM_BUILD)/.libs/libitm.so | .build
	$(TRANSACTION_CC) $(COMPONENTS) $(CPPFLAGS) $(CFLAGS) -B$(ITM_BUILD)/ -I$(COMMON) -I$(GOMP_BUILD) -fopenmp -fgnu-tm $< $(LDFLAGS) $(GOMP_LINK) $(ITM_LINK) $(LDLIBS) -o $@
check: all
	python3 $(COMMON)/verify.py .
clean:
	rm -rf .build
