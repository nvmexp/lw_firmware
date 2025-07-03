# pmu_sw/config/Makefile
#
# Misc helper targets for PMU-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to pmu-config.h, pmu-config.mk
.NOTPARALLEL:;

# RESMAN_ROOT must be set correctly as a normal path (no "dots").
# Allow for some typical defaults and give a useful error if its bad.
RESMAN_ROOT ?= $(if $(LW_SOURCE),  \
                    $(LW_SOURCE)/drivers/resman, \
                    $(if $(LW_ROOT), \
                        $(LW_ROOT)/drivers/resman, \
                        $(shell cd ../../drivers/resman 2> /dev/null; pwd)))
ifeq (,$(wildcard $(RESMAN_ROOT)/kernel))
  $(error "RESMAN_ROOT not correctly set! => $(RESMAN_ROOT) ")
endif
# if still using $(shell) resolve it now
export RESMAN_ROOT := $(RESMAN_ROOT)

P4UPDATE   = $(RESMAN_ROOT)/config/p4-update-if-changed.pl

CHIPCONFIG = $(RESMAN_ROOT)/../common/chip-config/chip-config.pl

# default rmconfig opts for these tests
pmucfgOpts := 
pmucfgOpts += --mode pmu-config
pmucfgOpts += --config pmu-config.cfg
pmucfgOpts += $(if $(DEBUG),--debug,--verbose)
pmucfgOpts += --source-root $(LW_ROOT)/pmu_sw
pmucfgOpts += --halinfo none
pmucfgOpts += --no-rmcontrols
pmucfgOpts += $(PMUCFG_OPTIONS)

# for check-pmu-config target
CHECK_PMUCFG_OUTDIR = chk-rm-tmp
pmucfgCheckOpts := $(pmucfgOpts)
pmucfgCheckOpts += --output-dir $(CHECK_PMUCFG_OUTDIR)

default: usage

usage: FORCE
	@echo ""
	@echo "   check-pmu-config    - a quick test (for use by DVS)"
	@echo ""
	@echo "   test                - All tests"
	@echo ""
	@echo "   halinfo             - dump halinfo in english"
	@echo "   halchk              - run halinfo consistency checks"
	@echo "   logs                - halchk and halinfo .log files"
	@echo ""


all: test

test: clean
	@echo; echo "*** Make sure we can regenerate pmu-config.h and pmu-config.mk"; echo
	./pmu-config
	@echo; echo PMU-CONFIG tests succeeded!; echo


# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-pmu-config:
	mkdir -p $(CHECK_PMUCFG_OUTDIR)

	@echo; echo "*** Run halchk"; echo
	$(CHIPCONFIG) --dryrun $(pmucfgCheckOpts) --verbose --halchk --halchk > $(CHECK_PMUCFG_OUTDIR)/halchk.log

	@echo; echo "*** Run dumphalinfo"; echo
	$(CHIPCONFIG) --dryrun $(pmucfgCheckOpts) --verbose --dumphalinfo > $(CHECK_PMUCFG_OUTDIR)/halinfo.log

	@echo; echo "*** Make sure we can regenerate pmu-config.h and pmu-config.mk"; echo
	$(CHIPCONFIG) $(pmucfgCheckOpts) --quiet

	@echo; echo "*** PMU-CONFIG 'check-pmu-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_PMUCFG_OUTDIR)
	$(RM) pmu-config.h pmu-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
	$(CHIPCONFIG) --dryrun $(pmucfgOpts) --halchk --halchk

halchk.log: FORCE
	$(CHIPCONFIG) --dryrun $(pmucfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
	$(CHIPCONFIG) --dryrun $(pmucfgOpts) --dumphalinfo

halinfo.log: FORCE
	$(CHIPCONFIG) --dryrun $(pmucfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
	etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

