GCC_PATH  := /home/utils/gcc-4.7.2
include ./srcfiles.mk

CXX       := $(GCC_PATH)/bin/g++ -fPIC -std=c++11
CXXWARN   := -W -Wreturn-type -Werror -Wformat

SRC_DIR  := ./src/
INC_DIR  := ./include/
OBJ_DIR  := ./lib/

SOURCES	:= $(FS_LIB_SOURCES)
HEADERS := $(FS_LIB_HEADERS)
OBJECTS := $(FS_LIB_SOURCES:.cpp=.o)

TEST_DIR := ./test/
TESTSRC := $(TEST_DIR)fs_test.cpp
TESTSRC2 := $(TEST_DIR)fs_dump.cpp
FSLIBINC := -lfs
FSLIBPATH := -L$(OBJ_DIR)

CWRAP_SRC := fs_c_wrap.cpp
CWRAP_DIR := ./c_wrap/


.PHONY:all
all: fslib fstest fslibshared fsdump

fslib: $(addprefix $(OBJ_DIR), $(OBJECTS))
	ar -vr $(OBJ_DIR)libfs.a $^

fslibshared: fslib
	$(CXX) -shared $(CWRAP_DIR)$(CWRAP_SRC) $(CXXWARN) -I$(INC_DIR) $(FSLIBPATH) $(FSLIBINC) -Wno-missing-field-initializers -o $(OBJ_DIR)fs_lib.so

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp $(HEADERS)
	echo $@
	echo $<
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXWARN) -I$(INC_DIR) -c $< -o $@

#TODO: Fix -Wno-missing-field-initializers
fstest: $(TESTSRC) fslib 
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(TESTSRC) $(CXXWARN) -I$(INC_DIR) $(FSLIBINC) $(FSLIBPATH) -Wno-missing-field-initializers -o $(OBJ_DIR)fstest

fsdump: $(TESTSRC2) fslib 
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(TESTSRC2) $(CXXWARN) -I$(INC_DIR) $(FSLIBINC) $(FSLIBPATH) -Wno-missing-field-initializers -o $(OBJ_DIR)fsdump

.PHONY: clean
clean:
	rm -r $(OBJ_DIR)
