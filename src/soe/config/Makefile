# uproc/soe/config/Makefile
#
# Misc helper targets for SOE-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to soe-config.h, soe-config.mk
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
soecfgOpts := 
soecfgOpts += --mode soe-config
soecfgOpts += --config soe-config.cfg
soecfgOpts += $(if $(DEBUG),--debug,--verbose)
soecfgOpts += --source-root $(LW_ROOT)/uproc/soe
soecfgOpts += --halinfo none
soecfgOpts += --no-rmcontrols
soecfgOpts += $(SOECFG_OPTIONS)

# for check-soe-config target
CHECK_SOECFG_OUTDIR = chk-rm-tmp
soecfgCheckOpts := $(soecfgOpts)
soecfgCheckOpts += --output-dir $(CHECK_SOECFG_OUTDIR)

default: usage

usage: FORCE
	@echo ""
	@echo "   check-soe-config    - a quick test (for use by DVS)"
	@echo ""
	@echo "   test                - All tests"
	@echo ""
	@echo "   halinfo             - dump halinfo in english"
	@echo "   halchk              - run halinfo consistency checks"
	@echo "   logs                - halchk and halinfo .log files"
	@echo ""


all: test

test: clean
	@echo; echo "*** Make sure we can regenerate soe-config.h and soe-config.mk"; echo
	./soe-config
	@echo; echo SOE-CONFIG tests succeeded!; echo


# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-soe-config:
	mkdir -p $(CHECK_SOECFG_OUTDIR)

	@echo; echo "*** Run halchk"; echo
	$(CHIPCONFIG) --dryrun $(soecfgCheckOpts) --verbose --halchk --halchk > $(CHECK_SOECFG_OUTDIR)/halchk.log

	@echo; echo "*** Run dumphalinfo"; echo
	$(CHIPCONFIG) --dryrun $(soecfgCheckOpts) --verbose --dumphalinfo > $(CHECK_SOECFG_OUTDIR)/halinfo.log

	@echo; echo "*** Make sure we can regenerate soe-config.h and soe-config.mk"; echo
	$(CHIPCONFIG) $(soecfgCheckOpts) --quiet

	@echo; echo "*** SOE-CONFIG 'check-soe-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_SOECFG_OUTDIR)
	$(RM) soe-config.h soe-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
	$(CHIPCONFIG) --dryrun $(soecfgOpts) --halchk --halchk

halchk.log: FORCE
	$(CHIPCONFIG) --dryrun $(soecfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
	$(CHIPCONFIG) --dryrun $(soecfgOpts) --dumphalinfo

halinfo.log: FORCE
	$(CHIPCONFIG) --dryrun $(soecfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
	etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

