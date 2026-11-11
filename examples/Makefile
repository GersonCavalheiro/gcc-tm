EXAMPLES := 01-deposit 02-transfer 03-concurrent-counter 04-workload 05-nesting 06-functions 07-aggregate 08-explicit-controller 09-compute-transactions
.PHONY: all check clean $(EXAMPLES)
all: $(EXAMPLES)
$(EXAMPLES):
	$(MAKE) -C $@ all
check:
	@set -e; for example in $(EXAMPLES); do $(MAKE) -C $$example check; done
clean:
	@set -e; for example in $(EXAMPLES); do $(MAKE) -C $$example clean; done

.PHONY: benchmark
benchmark:
	python3 benchmarks/run.py
