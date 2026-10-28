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
CARGA_PROGRAM = .build/transaction_carga
CARGA_THREADS ?= 4
CARGA_REPETICOES ?= 1000

.PHONY: all run check check-carga check-lifecycle check-hash-create-destroy check-hash-acquire check-hash-entry check-hash-count check-hash-diagnostics check-hash-owner

all: $(PROGRAMS) $(CARGA_PROGRAM)

.build:
	mkdir -p $@

.build/%: %.c Makefile | .build
	$(TRANSACTION_CC) $(CPPFLAGS) $(CFLAGS) $(TRANSACTION_FLAGS) $< $(LDFLAGS) $(TRANSACTION_LIBS) $(LDLIBS) -o $@

# Executa os exemplos sem argumentos; a carga parametrizada usa check-carga.
run: all
	@set -e; \
	for program in $(PROGRAMS); do \
		echo "========================================"; \
		echo "Executando: $$program"; \
		echo "========================================"; \
		./$$program; \
	done

check: all check-carga
	python3 verificar.py

check-carga: $(CARGA_PROGRAM)
	python3 verificar_carga.py "$(CARGA_THREADS)" "$(CARGA_REPETICOES)"

# Hidden internal interfaces are tested through the archive, never exported.
.build/lifecycle_stubs: lifecycle_stubs.c ../gcc-src-tm/libgomp/newtm-transaction.h ../gcc-src-tm/libgomp/newtm-context.h ../gcc-src-tm/libgomp/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) -I../gcc-src-tm/libgomp $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-lifecycle: .build/lifecycle_stubs
	python3 verificar_lifecycle.py $(GOMP_BUILD)/.libs

# White-box test: use the same private headers/configuration as this Linux build.
HASH_SOURCE := ../gcc-src-tm/libgomp
HASH_TEST_INCLUDES = -DHAVE_CONFIG_H -I$(GOMP_BUILD) \
	-I$(HASH_SOURCE)/config/linux/x86 -I$(HASH_SOURCE)/config/linux \
	-I$(HASH_SOURCE)/config/posix -I$(HASH_SOURCE) -I../gcc-src-tm/include

.build/hash_create_destroy: hash_create_destroy.c $(HASH_SOURCE)/newtm-hash.c $(HASH_SOURCE)/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) $(HASH_TEST_INCLUDES) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-hash-create-destroy: .build/hash_create_destroy
	./.build/hash_create_destroy > .build/hash_create_destroy.log
	@cat .build/hash_create_destroy.log

.build/hash_acquire: hash_acquire.c $(HASH_SOURCE)/newtm-hash.c $(HASH_SOURCE)/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) $(HASH_TEST_INCLUDES) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-hash-acquire: .build/hash_acquire
	python3 -c 'import subprocess; from pathlib import Path; r = subprocess.run(["./.build/hash_acquire"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60); Path(".build/hash_acquire.log").write_text(r.stdout); print(r.stdout, end=""); raise SystemExit(r.returncode)'

.build/hash_entry: hash_entry.c $(HASH_SOURCE)/newtm-hash.c $(HASH_SOURCE)/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) $(HASH_TEST_INCLUDES) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-hash-entry: .build/hash_entry
	python3 -c 'import subprocess; from pathlib import Path; r = subprocess.run(["./.build/hash_entry"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60); Path(".build/hash_entry.log").write_text(r.stdout); print(r.stdout, end=""); raise SystemExit(r.returncode)'

.build/hash_count: hash_count.c $(HASH_SOURCE)/newtm-hash.c $(HASH_SOURCE)/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) $(HASH_TEST_INCLUDES) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-hash-count: .build/hash_count
	python3 -c 'import subprocess; from pathlib import Path; r = subprocess.run(["./.build/hash_count"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=60); Path(".build/hash_count.log").write_text(r.stdout); print(r.stdout, end=""); raise SystemExit(r.returncode)'

.build/hash_diagnostics: hash_diagnostics.c $(HASH_SOURCE)/newtm-hash.c $(HASH_SOURCE)/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) $(HASH_TEST_INCLUDES) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-hash-diagnostics: .build/hash_diagnostics
	python3 verificar_hash_diagnostics.py

.build/hash_owner: hash_owner.c $(HASH_SOURCE)/newtm-hash.c $(HASH_SOURCE)/newtm-hash.h $(GOMP_BUILD)/.libs/libgomp.a | .build
	$(TRANSACTION_CC) -B$(GCC_BUILD)/gcc/ $(CFLAGS) $(HASH_TEST_INCLUDES) $< $(LDFLAGS) $(GOMP_BUILD)/.libs/libgomp.a -pthread $(LDLIBS) -o $@

check-hash-owner: .build/hash_owner
	python3 verificar_hash_owner.py

# Checkpoint da base de hash/owner/contexto; cabecalho do script delimita cobertura.
.PHONY: check-checkpoint-contexto
check-checkpoint-contexto:
	+python3 checkpoint_contexto_pronto_para_registro.py --make "$(MAKE)" \
	  --var 'GCC_BUILD=$(GCC_BUILD)' --var 'TRANSACTION_CC=$(TRANSACTION_CC)' \
	  --var 'CFLAGS=$(CFLAGS)' --var 'CPPFLAGS=$(CPPFLAGS)' \
	  --var 'LDFLAGS=$(LDFLAGS)' --var 'LDLIBS=$(LDLIBS)' \
	  --var 'CARGA_THREADS=$(CARGA_THREADS)' --var 'CARGA_REPETICOES=$(CARGA_REPETICOES)'
