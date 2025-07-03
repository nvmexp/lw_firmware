# uproc/sec2/config/Makefile
#
# Misc helper targets for SEC2-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to sec2-config.h, sec2-config.mk
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
sec2cfgOpts := 
sec2cfgOpts += --mode sec2-config
sec2cfgOpts += --config sec2-config.cfg
sec2cfgOpts += $(if $(DEBUG),--debug,--verbose)
sec2cfgOpts += --source-root $(LW_ROOT)/uproc/sec2
sec2cfgOpts += --halinfo none
sec2cfgOpts += --no-rmcontrols
sec2cfgOpts += $(SEC2CFG_OPTIONS)

# for check-sec2-config target
CHECK_SEC2CFG_OUTDIR = chk-rm-tmp
sec2cfgCheckOpts := $(sec2cfgOpts)
sec2cfgCheckOpts += --output-dir $(CHECK_SEC2CFG_OUTDIR)

default: usage

usage: FORCE
	@echo ""
	@echo "   check-sec2-config    - a quick test (for use by DVS)"
	@echo ""
	@echo "   test                - All tests"
	@echo ""
	@echo "   halinfo             - dump halinfo in english"
	@echo "   halchk              - run halinfo consistency checks"
	@echo "   logs                - halchk and halinfo .log files"
	@echo ""


all: test

test: clean
	@echo; echo "*** Make sure we can regenerate sec2-config.h and sec2-config.mk"; echo
	./sec2-config
	@echo; echo SEC2-CONFIG tests succeeded!; echo


# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-sec2-config:
	mkdir -p $(CHECK_SEC2CFG_OUTDIR)

	@echo; echo "*** Run halchk"; echo
	$(CHIPCONFIG) --dryrun $(sec2cfgCheckOpts) --verbose --halchk --halchk > $(CHECK_SEC2CFG_OUTDIR)/halchk.log

	@echo; echo "*** Run dumphalinfo"; echo
	$(CHIPCONFIG) --dryrun $(sec2cfgCheckOpts) --verbose --dumphalinfo > $(CHECK_SEC2CFG_OUTDIR)/halinfo.log

	@echo; echo "*** Make sure we can regenerate sec2-config.h and sec2-config.mk"; echo
	$(CHIPCONFIG) $(sec2cfgCheckOpts) --quiet

	@echo; echo "*** SEC2-CONFIG 'check-sec2-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_SEC2CFG_OUTDIR)
	$(RM) sec2-config.h sec2-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
	$(CHIPCONFIG) --dryrun $(sec2cfgOpts) --halchk --halchk

halchk.log: FORCE
	$(CHIPCONFIG) --dryrun $(sec2cfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
	$(CHIPCONFIG) --dryrun $(sec2cfgOpts) --dumphalinfo

halinfo.log: FORCE
	$(CHIPCONFIG) --dryrun $(sec2cfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
	etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

