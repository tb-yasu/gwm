# Convenience wrapper; the real build lives in src/Makefile.
all clean debug:
	$(MAKE) -C src $@

test: all
	test/regress.sh

bench: all
	test/bench.sh

.PHONY: all clean debug test bench
