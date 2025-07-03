DEPTH        := $(shell depth)

FAKE_HOST_DIR = $(DEPTH)/fmod/pwr_fake_host

YACC := bison
LEX  := flex

TARGET = falc_debug

.PHONY : $(TARGET) clean

all : $(TARGET)

falc_debug : falc_debug.cpp
	g++ -g -o $@  $<

clean :
	rm $(TARGET)
