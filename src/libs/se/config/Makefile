# uproc/libs/se/config/Makefile
#
# Misc helper targets for SE-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to se-config.h, se-config.mk
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
secfgOpts := 
secfgOpts += --mode se-config
secfgOpts += --config se-config.cfg
secfgOpts += $(if $(DEBUG),--debug,--verbose)
secfgOpts += --source-root $(LW_ROOT)/uproc/libs/se
secfgOpts += --halinfo none
secfgOpts += --no-rmcontrols
secfgOpts += $(SECFG_OPTIONS)

# for check-se-config target
CHECK_SECFG_OUTDIR = chk-rm-tmp
secfgCheckOpts := $(secfgOpts)
secfgCheckOpts += --output-dir $(CHECK_SECFG_OUTDIR)

default: usage

usage: FORCE
	@echo ""
	@echo "   check-se-config    - a quick test (for use by DVS)"
	@echo ""
	@echo "   test                - All tests"
	@echo ""
	@echo "   halinfo             - dump halinfo in english"
	@echo "   halchk              - run halinfo consistency checks"
	@echo "   logs                - halchk and halinfo .log files"
	@echo ""


all: test

test: clean
	@echo; echo "*** Make sure we can regenerate se-config.h and se-config.mk"; echo
	./se-config
	@echo; echo SE-CONFIG tests succeeded!; echo


# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-se-config:
	mkdir -p $(CHECK_SECFG_OUTDIR)

	@echo; echo "*** Run halchk"; echo
	$(CHIPCONFIG) --dryrun $(secfgCheckOpts) --verbose --halchk --halchk > $(CHECK_SECFG_OUTDIR)/halchk.log

	@echo; echo "*** Run dumphalinfo"; echo
	$(CHIPCONFIG) --dryrun $(secfgCheckOpts) --verbose --dumphalinfo > $(CHECK_SECFG_OUTDIR)/halinfo.log

	@echo; echo "*** Make sure we can regenerate se-config.h and se-config.mk"; echo
	$(CHIPCONFIG) $(secfgCheckOpts) --quiet

	@echo; echo "*** SE-CONFIG 'check-se-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_SECFG_OUTDIR)
	$(RM) se-config.h se-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
	$(CHIPCONFIG) --dryrun $(secfgOpts) --halchk --halchk

halchk.log: FORCE
	$(CHIPCONFIG) --dryrun $(secfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
	$(CHIPCONFIG) --dryrun $(secfgOpts) --dumphalinfo

halinfo.log: FORCE
	$(CHIPCONFIG) --dryrun $(secfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
	etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

