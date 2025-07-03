# uproc/booter/config/Makefile
#
# Misc helper targets for BOOTER-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to booter-config.h, booter-config.mk
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
booterCfgOpts := 
booterCfgOpts += --mode booter-config
booterCfgOpts += --config booter-config.cfg
booterCfgOpts += $(if $(DEBUG),--debug,--verbose)
booterCfgOpts += --source-root $(LW_ROOT)/uproc/booter
booterCfgOpts += --halinfo none
booterCfgOpts += --no-rmcontrols
booterCfgOpts += $(BOOTERCFG_OPTIONS)

# for check-booter-config target
CHECK_BOOTERCFG_OUTDIR = chk-rm-tmp
bootercfgCheckOpts := $(booterCfgOpts)
bootercfgCheckOpts += --output-dir $(CHECK_BOOTERCFG_OUTDIR)

default: usage

usage: FORCE
    @echo ""
    @echo "   check-booter-config    - a quick test (for use by DVS)"
    @echo ""
    @echo "   test                - All tests"
    @echo ""
    @echo "   halinfo             - dump halinfo in english"
    @echo "   halchk              - run halinfo consistency checks"
    @echo "   logs                - halchk and halinfo .log files"
    @echo ""

all: test

test: clean
    @echo; echo "*** Make sure we can regenerate booter-config.h and booter-config.mk"; echo
    ./booter-config
    @echo; echo BOOTER-CONFIG tests succeeded!; echo

# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-booter-config:
	mkdir -p $(CHECK_BOOTERCFG_OUTDIR)

    @echo; echo "*** Run halchk"; echo
    $(CHIPCONFIG) --dryrun $(bootercfgCheckOpts) --verbose --halchk --halchk > $(CHECK_BOOTERCFG_OUTDIR)/halchk.log

    @echo; echo "*** Run dumphalinfo"; echo
    $(CHIPCONFIG) --dryrun $(bootercfgCheckOpts) --verbose --dumphalinfo > $(CHECK_BOOTERCFG_OUTDIR)/halinfo.log

    @echo; echo "*** Make sure we can regenerate booter-config.h and booter-config.mk"; echo
    $(CHIPCONFIG) $(bootercfgCheckOpts) --quiet

    @echo; echo "*** BOOTER-CONFIG 'check-booter-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_BOOTERCFG_OUTDIR)
	$(RM) booter-config.h booter-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
    $(CHIPCONFIG) --dryrun $(booterCfgOpts) --halchk --halchk

halchk.log: FORCE
    $(CHIPCONFIG) --dryrun $(booterCfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
    $(CHIPCONFIG) --dryrun $(booterCfgOpts) --dumphalinfo

halinfo.log: FORCE
    $(CHIPCONFIG) --dryrun $(booterCfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
    etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

