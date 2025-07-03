PREFIX=$(shell basename `pwd`)

all: diff

test.s: riscv.py
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " PYTHON " $<
	@python3 riscv.py > test.s

test.o: test.s
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " AS " $<
	@riscv64-unknown-elf-as --mabi=lp64 --march=rv64imc test.s -o test.o

test.bin: test.o
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " OBJCOPY " $<
	@riscv64-unknown-elf-objcopy -O binary test.o test.bin

test: test-driver.c ../../lib/libriscv.c ../../include/libriscv.h
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " GCC " $<
	@gcc -DLIBOS_CONFIG_RISCV_COMPRESSED -I$(P4ROOT)/sw/lwriscv/main/inc/lwpu-sdk ../../lib/libriscv.c test-driver.c -I../../include -o test

test.s.out: test.bin test
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " EXEC " $<
	@./test > test.s.out

# Canonicalize li/addi
# Also canonicalize add rd, r0, rd -> add rd, rd, r0
%.canon: %
	@cat $^ | sed 's/addi \([^,]*\), x0/li \1/' |  \
	sed 's/add \([^,]*\), \([^,]*\), \1/add \1, \1, \2/' |  \
	sed 's/xor \([^,]*\), \([^,]*\), \1/xor \1, \1, \2/' | \
	sed 's/or \([^,]*\), \([^,]*\), \1/or \1, \1, \2/' | \
	sed 's/addi \([^,]*\), x0, \([^,]*\)/li \1, \2/' | \
	sed 's/and \([^,]*\), \([^,]*\), \1/and \1, \1, \2/' | \
	sed 's/addi \([^,]*\), \([^,]*\), 0/add \1, \2/, x0' | \
	sed 's/addw \([^,]*\), \([^,]*\), \1/addw \1, \1, \2/' > $@

diff: test.s.canon test.s.out.canon
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " DIFF " $<
	@diff -a -E -Z -w  test.s.canon test.s.out.canon

clean: 
	@rm -f test.*
	@rm -f test

