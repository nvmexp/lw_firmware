# uproc/selwrescrub/config/Makefile
#
# Misc helper targets for SELWRESCRUB-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to selwrescrub-config.h, selwrescrub-config.mk
.NOTPARALLEL:;

# RESMAN_ROOT must be set correctly as a normal path (no "dots").
# Allow for some typical defaults and give a useful error if its bad.
RESMAN_ROOT ?= $(if $(LW_SOURCE),  \
                    $(LW_SOURCE)/drivers/resman, \
                    $(if $(LW_ROOT), \
                        $(LW_ROOT)/drivers/resman, \
                        $(shell cd ../../../drivers/resman 2> /dev/null; pwd)))
ifeq (,$(wildcard $(RESMAN_ROOT)/kernel))
  $(error "RESMAN_ROOT not correctly set! => $(RESMAN_ROOT) ")
endif
# if still using $(shell) resolve it now
export RESMAN_ROOT := $(RESMAN_ROOT)

P4UPDATE   = $(RESMAN_ROOT)/config/p4-update-if-changed.pl

CHIPCONFIG = $(RESMAN_ROOT)/../common/chip-config/chip-config.pl

# default rmconfig opts for these tests
selwrescrubcfgOpts := 
selwrescrubcfgOpts += --mode selwrescrub-config
selwrescrubcfgOpts += --config selwrescrub-config.cfg
selwrescrubcfgOpts += $(if $(DEBUG),--debug,--verbose)
selwrescrubcfgOpts += --source-root $(LW_ROOT)/uproc/selwrescrub
selwrescrubcfgOpts += --halinfo none
selwrescrubcfgOpts += --no-rmcontrols
selwrescrubcfgOpts += $(SELWRESCRUBCFG_OPTIONS)

# for check-selwrescrub-config target
CHECK_SELWRESCRUBCFG_OUTDIR = chk-rm-tmp
selwrescrubcfgCheckOpts := $(selwrescrubcfgOpts)
selwrescrubcfgCheckOpts += --output-dir $(CHECK_SELWRESCRUBCFG_OUTDIR)

default: usage

usage: FORCE
    @echo ""
    @echo "   check-selwrescrub-config    - a quick test (for use by DVS)"
    @echo ""
    @echo "   test                - All tests"
    @echo ""
    @echo "   halinfo             - dump halinfo in english"
    @echo "   halchk              - run halinfo consistency checks"
    @echo "   logs                - halchk and halinfo .log files"
    @echo ""

all: test

test: clean
    @echo; echo "*** Make sure we can regenerate selwrescrub-config.h and selwrescrub-config.mk"; echo
    ./selwrescrub-config
    @echo; echo SELWRESCRUB-CONFIG tests succeeded!; echo

# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-selwrescrub-config:
	mkdir -p $(CHECK_SELWRESCRUBCFG_OUTDIR)

    @echo; echo "*** Run halchk"; echo
    $(CHIPCONFIG) --dryrun $(selwrescrubcfgCheckOpts) --verbose --halchk --halchk > $(CHECK_SELWRESCRUBCFG_OUTDIR)/halchk.log

    @echo; echo "*** Run dumphalinfo"; echo
    $(CHIPCONFIG) --dryrun $(selwrescrubcfgCheckOpts) --verbose --dumphalinfo > $(CHECK_SELWRESCRUBCFG_OUTDIR)/halinfo.log

    @echo; echo "*** Make sure we can regenerate selwrescrub-config.h and selwrescrub-config.mk"; echo
    $(CHIPCONFIG) $(selwrescrubcfgCheckOpts) --quiet

    @echo; echo "*** SELWRESCRUB-CONFIG 'check-selwrescrub-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_SELWRESCRUBCFG_OUTDIR)
	$(RM) selwrescrub-config.h selwrescrub-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
    $(CHIPCONFIG) --dryrun $(selwrescrubcfgOpts) --halchk --halchk

halchk.log: FORCE
    $(CHIPCONFIG) --dryrun $(selwrescrubcfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
    $(CHIPCONFIG) --dryrun $(selwrescrubcfgOpts) --dumphalinfo

halinfo.log: FORCE
    $(CHIPCONFIG) --dryrun $(selwrescrubcfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
    etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

