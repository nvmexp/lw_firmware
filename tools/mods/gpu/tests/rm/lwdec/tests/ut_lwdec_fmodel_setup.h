#ifdef UTF_BUILD_GH100_LWDEC_FMODEL
/* Version checking. If this file is updated,
 * it MUST be copied over to the MODS RMTEST
 * area for LWDEC testing */
#define __UCODE_FMODEL_SETUP_VERSION       1       /* Update this when making changes to this file */

#if __UCODE_FMODEL_SETUP_VERSION != __UCODE_FMODEL_SETUP_BUILD_VERSION
#error "ut_lwdec_fmodel_setup.h version mismatch!! Copy ut_lwdec_fmodel_setup.h to MODS RMTEST area, \
and update the makefile"
#endif
#endif // UTF_BUILD_GH100_LWDEC_FMODEL

typedef enum {
    LWDEC_UT_FMODEL_ACR_ENABLED     = 0x1111,
    LWDEC_UT_FMODEL_DEBUG_ENABLED   = 0x2222,
    LWDEC_UT_FMODEL_FB_SCRATCH_ADDR = 0x3333,
    LWDEC_UT_FMODEL_FB_DATA_ADDR    = 0x4444,
    LWDEC_UT_FMODEL_COMPLETE_HANDSHAKE = 0x5555,
    LWDEC_UT_FMODEL_SETUP_COMPLETE  = 0x6666,
    LWDEC_UT_FMODEL_TESTS_COMPLETE  = 0x7777,
} LwdelwtFmodelSetupStates_t;

#define LWDEC_UT_FMODEL_EXCEPTION_HIT               (0x80000000)
#define LWDEC_UT_FMODEL_EXCEPTION_HIT_MASK          (0x80000000)
#define LWDEC_UT_FMODEL_EXCEPTION_TEST_NO_MASK      (0x07FF0000)

#define LWDEC_UT_FMODEL_IS_EXCEPTION_HIT(state) \
    ((state & LWDEC_UT_FMODEL_EXCEPTION_HIT_MASK) == LWDEC_UT_FMODEL_EXCEPTION_HIT)

#define LWDEC_UT_FMODEL_SET_EXCEPTION_TEST_NO(state, test_no) \
    (((((LwU32)test_no) << 16) & LWDEC_UT_FMODEL_EXCEPTION_TEST_NO_MASK) | state)

#define LWDEC_UT_FMODEL_GET_EXCEPTION_TEST_NO(state) \
    ((state & LWDEC_UT_FMODEL_EXCEPTION_TEST_NO_MASK) >> 16)
