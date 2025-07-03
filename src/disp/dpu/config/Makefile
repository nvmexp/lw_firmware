# uproc/disp/dpu/config/Makefile
#
# Misc helper targets for DPU-CONFIG
#
# Don't run targets in this Makefile in parallel since chip-config
# always writes to dpu-config.h, dpu-config.mk
.NOTPARALLEL:;

# RESMAN_ROOT must be set correctly as a normal path (no "dots").
# Allow for some typical defaults and give a useful error if its bad.
RESMAN_ROOT ?= $(if $(LW_SOURCE),  \
                    $(LW_SOURCE)/drivers/resman, \
                    $(if $(LW_ROOT), \
                        $(LW_ROOT)/drivers/resman, \
                        $(shell cd ../../../../drivers/resman 2> /dev/null; pwd)))
ifeq (,$(wildcard $(RESMAN_ROOT)/kernel))
  $(error "RESMAN_ROOT not correctly set! => $(RESMAN_ROOT) ")
endif
# if still using $(shell) resolve it now
export RESMAN_ROOT := $(RESMAN_ROOT)

P4UPDATE   = $(RESMAN_ROOT)/config/p4-update-if-changed.pl

CHIPCONFIG = $(RESMAN_ROOT)/../common/chip-config/chip-config.pl

# default rmconfig opts for these tests
dpucfgOpts := 
dpucfgOpts += --mode dpu-config
dpucfgOpts += --config dpu-config.cfg
dpucfgOpts += $(if $(DEBUG),--debug,--verbose)
dpucfgOpts += --source-root $(LW_ROOT)/uproc/disp/dpu
dpucfgOpts += --halinfo none
dpucfgOpts += --no-rmcontrols
dpucfgOpts += $(DPUCFG_OPTIONS)

# for check-dpu-config target
CHECK_DPUCFG_OUTDIR = chk-rm-tmp
dpucfgCheckOpts := $(dpucfgOpts)
dpucfgCheckOpts += --output-dir $(CHECK_DPUCFG_OUTDIR)

default: usage

usage: FORCE
	@echo ""
	@echo "   check-dpu-config    - a quick test (for use by DVS)"
	@echo ""
	@echo "   test                - All tests"
	@echo ""
	@echo "   halinfo             - dump halinfo in english"
	@echo "   halchk              - run halinfo consistency checks"
	@echo "   logs                - halchk and halinfo .log files"
	@echo ""


all: test

test: clean
	@echo; echo "*** Make sure we can regenerate dpu-config.h and dpu-config.mk"; echo
	./dpu-config
	@echo; echo DPU-CONFIG tests succeeded!; echo


# This target is run by DVS on every resman checkin.
# DVS runs target 'check_rm' in resman/arch/lwalloc/mods/Makefile
# which (might someday) run this.
# Don't use -j2 here since it will scramble DVS logs..

check-dpu-config:
	mkdir -p $(CHECK_DPUCFG_OUTDIR)

	@echo; echo "*** Run halchk"; echo
	$(CHIPCONFIG) --dryrun $(dpucfgCheckOpts) --verbose --halchk --halchk > $(CHECK_DPUCFG_OUTDIR)/halchk.log

	@echo; echo "*** Run dumphalinfo"; echo
	$(CHIPCONFIG) --dryrun $(dpucfgCheckOpts) --verbose --dumphalinfo > $(CHECK_DPUCFG_OUTDIR)/halinfo.log

	@echo; echo "*** Make sure we can regenerate dpu-config.h and dpu-config.mk"; echo
	$(CHIPCONFIG) $(dpucfgCheckOpts) --quiet

	@echo; echo "*** DPU-CONFIG 'check-dpu-config' completed successfully!"; echo

clean:
	$(RM) -rf $(CHECK_DPUCFG_OUTDIR)
	$(RM) dpu-config.h dpu-config.mk
	$(RM) halchk.log halinfo.log
	$(RM) g_*.[ch]
	$(RM) *.o
	$(RM) TAGS tags
	$(RM) prof.out

# look for problems in halinfo
halchk: FORCE
	$(CHIPCONFIG) --dryrun $(dpucfgOpts) --halchk --halchk

halchk.log: FORCE
	$(CHIPCONFIG) --dryrun $(dpucfgOpts) --halchk --halchk >$@ 2>&1

# do full dump of hal info
halinfo: FORCE
	$(CHIPCONFIG) --dryrun $(dpucfgOpts) --dumphalinfo

halinfo.log: FORCE
	$(CHIPCONFIG) --dryrun $(dpucfgOpts) --dumphalinfo >$@ 2>&1


logs: halinfo.log halchk.log

tags: FORCE
	etags --lang perl *.pl *.pm haldefs/*.def

FORCE:;

