# uproc/fbflcn/config/Makefile
#
# Misc helper targets for FBFALCON-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to fbfalcon-config.h, fbfalcon-config.mk
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
fbfalconcfgOpts := 
fbfalconcfgOpts += --mode fbfalcon-config
fbfalconcfgOpts += --config fbfalcon-config.cfg
fbfalconcfgOpts += $(if $(DEBUG),--debug,--verbose)
fbfalconcfgOpts += --source-root $(LW_ROOT)/uproc/fbflcn
fbfalconcfgOpts += --halinfo none
fbfalconcfgOpts += --no-rmcontrols
fbfalconcfgOpts += $(FBFALCONCFG_OPTIONS)

# for check-fbfalcon-config target
CHECK_FBFALCONCFG_OUTDIR = chk-rm-tmp
fbfalconcfgCheckOpts := $(fbfalconcfgOpts)
fbfalconcfgCheckOpts += --output-dir $(CHECK_FBFALCONCFG_OUTDIR)

default: usage

usage: FORCE
	@echo ""
	@echo "   check-fbfalcon-config    - a quick test (for use by DVS)"
	@echo ""
	@echo "   test                - All tests"
	@echo ""
	@echo "   halinfo             - dump halinfo in english"
	@echo "   halchk              - run halinfo consistency checks"
	@echo "   logs                - halchk and halinfo .log files"
	@echo ""


all: test

test: clean
	@echo; echo "*** Make sure we can regenerate fbfalcon-config.h and fbfalcon-config.mk"; echo
	./fbfalcon-config
	@echo; echo FBFALCON-CONFIG tests succeeded!; echo


# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-fbfalcon-config:
	mkdir -p $(CHECK_FBFALCONCFG_OUTDIR)

	@echo; echo "*** Run halchk"; echo
	$(CHIPCONFIG) --dryrun $(fbfalconcfgCheckOpts) --verbose --halchk --halchk > $(CHECK_FBFALCONCFG_OUTDIR)/halchk.log

	@echo; echo "*** Run dumphalinfo"; echo
	$(CHIPCONFIG) --dryrun $(fbfalconcfgCheckOpts) --verbose --dumphalinfo > $(CHECK_FBFALCONCFG_OUTDIR)/halinfo.log

	@echo; echo "*** Make sure we can regenerate fbfalcon-config.h and fbfalcon-config.mk"; echo
	$(CHIPCONFIG) $(fbfalconcfgCheckOpts) --quiet

	@echo; echo "*** FBFALCON-CONFIG 'check-fbfalcon-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_FBFALCONCFG_OUTDIR)
	$(RM) fbfalcon-config.h fbfalcon-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
	$(CHIPCONFIG) --dryrun $(fbfalconcfgOpts) --halchk --halchk

halchk.log: FORCE
	$(CHIPCONFIG) --dryrun $(fbfalconcfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
	$(CHIPCONFIG) --dryrun $(fbfalconcfgOpts) --dumphalinfo

halinfo.log: FORCE
	$(CHIPCONFIG) --dryrun $(fbfalconcfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
	etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

