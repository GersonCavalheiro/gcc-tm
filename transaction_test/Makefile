# Compilador e runtime construidos a partir desta arvore, sem instalacao.
GCC_BUILD ?= ../gcc-build
GCC_BUILD := $(abspath $(GCC_BUILD))

TRANSACTION_CC ?= $(GCC_BUILD)/gcc/xgcc
GOMP_BUILD := $(GCC_BUILD)/x86_64-pc-linux-gnu/libgomp

CPPFLAGS += -I$(GOMP_BUILD) -ftransaction-debug
CFLAGS ?= -O2 -Wall -Wextra

TRANSACTION_FLAGS = -B$(GCC_BUILD)/gcc/ -B$(GOMP_BUILD)/ -fopenmp
TRANSACTION_LIBS = -L$(GOMP_BUILD)/.libs -Wl,-rpath,$(GOMP_BUILD)/.libs

PROGRAMS = .build/transaction_exemplo .build/transaction_aninhamento

.PHONY: all run check check-lifecycle

all: $(PROGRAMS)

.build:
	mkdir -p $@

.build/%: %.c Makefile | .build
	$(TRANSACTION_CC) $(CPPFLAGS) $(CFLAGS) $(TRANSACTION_FLAGS) $< $(LDFLAGS) $(TRANSACTION_LIBS) $(LDLIBS) -o $@

# Executa todos os programas compilados.
run: all
	@set -e; \
	for program in $(PROGRAMS); do \
		echo "========================================"; \
		echo "Executando: $$program"; \
		echo "========================================"; \
		./$$program; \
	done

check: all
	python3 verificar.py

# Hidden internal interfaces are tested through the archive, never exported.
.build/lifecycle_stubs: lifecycle_stubs.c ../libgomp/newtm-transaction.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) -I../libgomp $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-lifecycle: .build/lifecycle_stubs
	python3 verificar_lifecycle.py $(GOMP_BUILD)/.libs
