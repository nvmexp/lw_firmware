#
# A makefile fragment 'include'd by uproc/booter/config/makefile.mk in order to pick
# up names of haldef files. Each haldef file that is part of the build should
# appear here. Failure to do so will NOT result in a build failure, but will
# prevent 'make' from treating the file as a build prerequisite for
# 'booter-config'. This essentially means that 'make' will NOT re-run 'booter-config'
# when the file changes (leading to any number of undesirable consequences).
#
# Please maintain the following list in alphabetical order:
#

BOOTERCFG_HALDEFS :=
BOOTERCFG_HALDEFS += booter
BOOTERCFG_HALDEFS += sha
BOOTERCFG_HALDEFS += se

