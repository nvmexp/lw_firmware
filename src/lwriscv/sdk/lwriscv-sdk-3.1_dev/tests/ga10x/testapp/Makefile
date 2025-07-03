TESTS += sample-test
TESTS += vguberinic-ga10x-sanity

TESTS_B:=$(addsuffix .build, $(TESTS))
TESTS_C:= $(addsuffix .clean, $(TESTS))
TESTS_I:= $(addsuffix .install, $(TESTS))

all: install
build: $(TESTS_B)
clean: $(TESTS_C)
install: $(TESTS_I)

.PHONY: all build install clean

%.build:
	$(MAKE) -C $* build

%.clean:
	$(MAKE) -C $* clean

%.install: %.build
	$(MAKE) -C $* install

