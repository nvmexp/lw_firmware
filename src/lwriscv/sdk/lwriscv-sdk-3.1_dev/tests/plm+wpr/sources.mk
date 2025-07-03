#################################################################################
# Additional object files to include in the compilation (main.o is implicit).   #
# New source files added to src/ should have their corresponding object files   #
# appended to the list below.                                                   #
#                                                                               #
# For example, if adding the file "src/example.c", the line "OBJS += example.o" #
# should be appended below.                                                     #
#                                                                               #
# Note that the manifest (src/manifest.c) is intentionally omitted from this    #
# list as it requires sepcial handling.                                         #
#################################################################################

OBJS += config.o
OBJS += fetch.o
OBJS += hal.o
OBJS += harness.o
OBJS += io.o
OBJS += start.o
OBJS += suites.o
OBJS += targets.o
OBJS += tests.o
OBJS += trap.o
