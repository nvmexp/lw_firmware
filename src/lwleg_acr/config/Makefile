# uproc/tegra_acr/config/Makefile
#
# Misc helper targets for ACR-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to acr-config.h, acr-config.mk
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
acrcfgOpts := 
acrcfgOpts += --mode acr-config
acrcfgOpts += --config acr-config.cfg
acrcfgOpts += $(if $(DEBUG),--debug,--verbose)
acrcfgOpts += --source-root $(LW_ROOT)/uproc/tegra_acr
acrcfgOpts += --halinfo none
acrcfgOpts += --no-rmcontrols
acrcfgOpts += $(ACRCFG_OPTIONS)

# for check-acr-config target
CHECK_ACRCFG_OUTDIR = chk-rm-tmp
acrcfgCheckOpts := $(acrcfgOpts)
acrcfgCheckOpts += --output-dir $(CHECK_ACRCFG_OUTDIR)

default: usage

usage: FORCE
    @echo ""
    @echo "   check-acr-config    - a quick test (for use by DVS)"
    @echo ""
    @echo "   test                - All tests"
    @echo ""
    @echo "   halinfo             - dump halinfo in english"
    @echo "   halchk              - run halinfo consistency checks"
    @echo "   logs                - halchk and halinfo .log files"
    @echo ""

all: test

test: clean
    @echo; echo "*** Make sure we can regenerate acr-config.h and acr-config.mk"; echo
    ./acr-config
    @echo; echo ACR-CONFIG tests succeeded!; echo

# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-acr-config:
	mkdir -p $(CHECK_ACRCFG_OUTDIR)

    @echo; echo "*** Run halchk"; echo
    $(CHIPCONFIG) --dryrun $(acrcfgCheckOpts) --verbose --halchk --halchk > $(CHECK_ACRCFG_OUTDIR)/halchk.log

    @echo; echo "*** Run dumphalinfo"; echo
    $(CHIPCONFIG) --dryrun $(acrcfgCheckOpts) --verbose --dumphalinfo > $(CHECK_ACRCFG_OUTDIR)/halinfo.log

    @echo; echo "*** Make sure we can regenerate acr-config.h and acr-config.mk"; echo
    $(CHIPCONFIG) $(acrcfgCheckOpts) --quiet

    @echo; echo "*** ACR-CONFIG 'check-acr-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_ACRCFG_OUTDIR)
	$(RM) acr-config.h acr-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
    $(CHIPCONFIG) --dryrun $(acrcfgOpts) --halchk --halchk

halchk.log: FORCE
    $(CHIPCONFIG) --dryrun $(acrcfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
    $(CHIPCONFIG) --dryrun $(acrcfgOpts) --dumphalinfo

halinfo.log: FORCE
    $(CHIPCONFIG) --dryrun $(acrcfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
    etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

