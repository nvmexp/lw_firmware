# uproc/fub/config/Makefile
#
# Misc helper targets for FUB-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to fub-config.h, fub-config.mk
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
fubcfgOpts := 
fubcfgOpts += --mode fub-config
fubcfgOpts += --config fub-config.cfg
fubcfgOpts += $(if $(DEBUG),--debug,--verbose)
fubcfgOpts += --source-root $(LW_ROOT)/uproc/fub
fubcfgOpts += --halinfo none
fubcfgOpts += --no-rmcontrols
fubcfgOpts += $(FUBCFG_OPTIONS)

# for check-fub-config target
CHECK_FUBCFG_OUTDIR = chk-rm-tmp
fubcfgCheckOpts := $(fubcfgOpts)
fubcfgCheckOpts += --output-dir $(CHECK_FUBCFG_OUTDIR)

default: usage

usage: FORCE
    @echo ""
    @echo "   check-fub-config    - a quick test (for use by DVS)"
    @echo ""
    @echo "   test                - All tests"
    @echo ""
    @echo "   halinfo             - dump halinfo in english"
    @echo "   halchk              - run halinfo consistency checks"
    @echo "   logs                - halchk and halinfo .log files"
    @echo ""

all: test

test: clean
    @echo; echo "*** Make sure we can regenerate fub-config.h and fub-config.mk"; echo
    ./fub-config
    @echo; echo FUB-CONFIG tests succeeded!; echo

# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-fub-config:
	mkdir -p $(CHECK_FUBCFG_OUTDIR)

    @echo; echo "*** Run halchk"; echo
    $(CHIPCONFIG) --dryrun $(fubcfgCheckOpts) --verbose --halchk --halchk > $(CHECK_FUBCFG_OUTDIR)/halchk.log

    @echo; echo "*** Run dumphalinfo"; echo
    $(CHIPCONFIG) --dryrun $(fubcfgCheckOpts) --verbose --dumphalinfo > $(CHECK_FUBCFG_OUTDIR)/halinfo.log

    @echo; echo "*** Make sure we can regenerate fub-config.h and fub-config.mk"; echo
    $(CHIPCONFIG) $(fubcfgCheckOpts) --quiet

    @echo; echo "*** FUB-CONFIG 'check-fub-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_FUBCFG_OUTDIR)
	$(RM) fub-config.h fub-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
    $(CHIPCONFIG) --dryrun $(fubcfgOpts) --halchk --halchk

halchk.log: FORCE
    $(CHIPCONFIG) --dryrun $(fubcfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
    $(CHIPCONFIG) --dryrun $(fubcfgOpts) --dumphalinfo

halinfo.log: FORCE
    $(CHIPCONFIG) --dryrun $(fubcfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
    etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;
