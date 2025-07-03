#
# A makefile fragment 'include'd by uproc/acr/config/makefile.mk in order to pick
# up names of haldef files. Each haldef file that is part of the build should
# appear here. Failure to do so will NOT result in a build failure, but will
# prevent 'make' from treating the file as a build prerequisite for
# 'acr-config'. This essentially means that 'make' will NOT re-run 'acr-config'
# when the file changes (leading to any number of undesirable consequences).
#
# Please maintain the following list in alphabetical order:
#

ACRCFG_HALDEFS :=
ACRCFG_HALDEFS += acr
ACRCFG_HALDEFS += acrlib
ACRCFG_HALDEFS += sha
ACRCFG_HALDEFS += se

