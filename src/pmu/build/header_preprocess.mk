#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

##############################################################################
# PMU Preprocessed headers
#
# Please pre-define the following:
#   PREPROC_OUTDIR (output directory for generated headers)
#   PREPROC_INCDIR (directory to relwrsively search for pre-processable headers in)
#
# This makefile will generate a list of preprocessed headers,
# GEN_PREPROC_HEADERS, which should then be make prerequisites of the final
# ucode build.
##############################################################################
ifndef PREPROC_INCDIR
$(error PREPROC_INCDIR is not set)
endif

ifndef LW_SOURCE
$(error LW_SOURCE is not set)
endif

##############################################################################
# Default configuration if those are not specified yet
##############################################################################
PREPROC_OUTDIR ?= $(OUTPUTDIR)
PREPROC_PERL   := $(if $(findstring undefined,$(origin PERL)),  perl,  $(PERL))
PREPROC_TOUCH  := $(if $(findstring undefined,$(origin TOUCH)), touch, $(TOUCH))
PREPROC_ECHO   := $(if $(findstring undefined,$(origin ECHO)),  echo,  $(ECHO))
PREPROC_MKDIR  := $(if $(findstring undefined,$(origin MKDIR)), mkdir, $(MKDIR))

##############################################################################
# All generation script dependancies
##############################################################################
GEN_PREPROC_DEPS =
GEN_PREPROC_DEPS += $(LW_SOURCE)/pmu_sw/build/header_preprocess.pl
GEN_PREPROC_DEPS += $(LW_SOURCE)/pmu_sw/build/header_preprocess_find.pl

##############################################################################
# All headers that need preprocessing
##############################################################################
PREPROC_HEADERS = $(shell $(PREPROC_PERL) $(LW_SOURCE)/pmu_sw/build/header_preprocess_find.pl --search_path $(PREPROC_INCDIR))

##############################################################################
# Targets of generation script
##############################################################################
GEN_PREPROC_HEADERS := $(patsubst %.h, $(PREPROC_OUTDIR)/g_%.h, $(notdir $(PREPROC_HEADERS)))

##############################################################################
# Target static pattern rule
#
# NOTE: vpath allows %.h target dependencies to be found outside of
# $(PREPROC_OUTDIR) for the below static pattern rule
##############################################################################
vpath %.h $(sort $(dir $(PREPROC_HEADERS)))

$(GEN_PREPROC_HEADERS): $(PREPROC_OUTDIR)/g_%.h: %.h $(GEN_PREPROC_DEPS)
	$(PREPROC_ECHO) [preproc] generating pre processed header: $(notdir $@)
	$(PREPROC_MKDIR) $(PREPROC_OUTDIR)
	$(PREPROC_PERL) $(LW_SOURCE)/pmu_sw/build/header_preprocess.pl --input_file $< --output_file $@
