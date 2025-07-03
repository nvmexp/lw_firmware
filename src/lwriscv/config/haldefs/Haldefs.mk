#
# A makefile fragment 'include'd by uproc/lwriscv/config/makefile.mk
# in order to pick
# up names of haldef files. Each haldef file that is part of the build should
# appear here. Failure to do so will NOT result in a build failure, but will
# prevent 'make' from treating the file as a build prerequisite for
# chip-config. This essentially means that 'make' will NOT re-run chip-config
# when the file changes (leading to any number of undesirable consequences).
#
# Please maintain the following list in alphabetical order:
#

RISCVLIBCFG_HALDEFS :=
RISCVLIBCFG_HALDEFS += drivers
RISCVLIBCFG_HALDEFS += sepkern
RISCVLIBCFG_HALDEFS += syslib
RISCVLIBCFG_HALDEFS += shlib
RISCVLIBCFG_HALDEFS += Gpuhal
