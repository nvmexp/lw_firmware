TEST_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/unit-tests/wpr/wpr-test.c
UNIT_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/src/wpr/t21x/acr_wprt210.c
UNIT_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/src/wpr/t23x/acr_wprt234.c
UNIT_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/src/falcon/maxwell/acr_falct210.c
UNIT_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/src/dma/maxwell/acr_dmagm200.c
UNIT_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/src/acr_utils/acrutils.c
UNIT_SOURCES += $(LW_SOURCE)/uproc/tegra_acr/src/acrlib/maxwell/acruclibgm200.c
ACRCFG_PROFILE := acr_pmu-t234_load
include $(LW_SOURCE)/uproc/tegra_acr/unit-tests/cheetah-acr-unit-test-cflags.mk
