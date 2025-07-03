/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <stdbool.h>
#include <test-macros.h>
#include <regmock.h>
#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include <lwmisc.h>

#include <liblwriscv/io_dio.h>
#include LWRISCV64_MANUAL_LOCAL_IO

// in order to save #define wrappers all over the place
#if !LWRISCV_HAS_DIO_SE
#define DIO_TYPE_SE DIO_TYPE_ILWALID
#endif
#if !LWRISCV_HAS_DIO_SNIC
#define DIO_TYPE_SNIC DIO_TYPE_ILWALID
#endif
#if !LWRISCV_HAS_DIO_FBHUB
#define DIO_TYPE_FBHUB DIO_TYPE_ILWALID
#endif

//
// sw defines to unify hw manual defined dio registers fields
// LW_PRGNLCL_FALCON_x and
// LW_PRGNLCL_FALCON_DIO_x(i) with
// LW_PRGNLCL_DIO_x
// Extended on what's included in io_dio.c
//
#define LW_PRGNLCL_DIO_DOC_CTRL_COUNT                                              7:0 /* R-IVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_COUNT_INIT                                  0x00000004 /* R-I-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_RESET                                            16:16 /* RWIVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_RESET_INIT                                  0x00000000 /* RWI-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_STOP                                             17:17 /* RWIVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_STOP_INIT                                   0x00000000 /* RWI-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_EMPTY                                            18:18 /* R-IVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_EMPTY_INIT                                  0x00000001 /* R-I-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_WR_FINISHED                                      19:19 /* R-IVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_WR_FINISHED_INIT                            0x00000001 /* R-I-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_RD_FINISHED                                      20:20 /* R-IVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_RD_FINISHED_INIT                            0x00000001 /* R-I-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_WR_ERROR                                         21:21 /* RWIVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_WR_ERROR_INIT                               0x00000000 /* RWI-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_RD_ERROR                                         22:22 /* RWIVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_RD_ERROR_INIT                               0x00000000 /* RWI-V */
#define LW_PRGNLCL_DIO_DOC_CTRL_PROTOCOL_ERROR                                   23:23 /* RWIVF */
#define LW_PRGNLCL_DIO_DOC_CTRL_PROTOCOL_ERROR_INIT                         0x00000000 /* RWI-V */
#define LW_PRGNLCL_DIO_DOC_D0_DATA                                                31:0 /* RWXVF */
#define LW_PRGNLCL_DIO_DOC_D1_DATA                                                31:0 /* RWXVF */
#define LW_PRGNLCL_DIO_DOC_D2_DATA                                                15:0 /* RWXVF */
#define LW_PRGNLCL_DIO_DIC_CTRL_COUNT                                              7:0 /* R-IVF */
#define LW_PRGNLCL_DIO_DIC_CTRL_COUNT_INIT                                  0x00000000 /* R-I-V */
#define LW_PRGNLCL_DIO_DIC_CTRL_RESET                                            16:16 /* RWIVF */
#define LW_PRGNLCL_DIO_DIC_CTRL_RESET_INIT                                  0x00000000 /* RWI-V */
#define LW_PRGNLCL_DIO_DIC_CTRL_VALID                                            19:19 /* R-IVF */
#define LW_PRGNLCL_DIO_DIC_CTRL_VALID_INIT                                  0x00000000 /* R-I-V */
#define LW_PRGNLCL_DIO_DIC_CTRL_POP                                              20:20 /* -WXVF */
#define LW_PRGNLCL_DIO_DIC_D0_DATA                                                31:0 /* RWXVF */
#define LW_PRGNLCL_DIO_DIC_D1_DATA                                                31:0 /* RWXVF */
#define LW_PRGNLCL_DIO_DIC_D2_DATA                                                15:0 /* RWXVF */
#define LW_PRGNLCL_DIO_DIO_ERR_INFO                                               31:0 /* R--VF */

//
// sw defines for specific DOC Dx interface
//
#define LW_PRGNLCL_DIO_DOC_D0_SEHUB_READ                                         16:16
#define LW_PRGNLCL_DIO_DOC_D0_SEHUB_ADDR                                          15:0
#define LW_PRGNLCL_DIO_DOC_D1_SEHUB_WDATA                                         31:0
#define LW_PRGNLCL_DIO_DIC_D0_SEHUB_RDATA                                         31:0
#define LW_PRGNLCL_DIO_DOC_D0_SNIC_WDATA                                          31:0
#define LW_PRGNLCL_DIO_DOC_D1_SNIC_ADDR                                           31:0
#define LW_PRGNLCL_DIO_DOC_D2_SNIC_READ                                            0:0
#define LW_PRGNLCL_DIO_DIC_D0_SNIC_RDATA                                          31:0
#define LW_PRGNLCL_DIO_DOC_D0_FBHUB_READ                                         16:16
#define LW_PRGNLCL_DIO_DOC_D0_FBHUB_ADDR                                          15:0
#define LW_PRGNLCL_DIO_DOC_D1_FBHUB_WDATA                                         31:0
#define LW_PRGNLCL_DIO_DIC_D0_FBHUB_RDATA                                         31:0

// =============================================================================
// DIO C MODEL
// Function: Provide interface to control and maintain legal DIO states.
// Limitation: 1. Only 1 item in each DIO at a time.
//             2. Does not check dioType
// =============================================================================
typedef enum
{
    // all other states are valid
    UTF_DIO_MODEL_ITEM_STATE_ILWALID,
    // enter when captured in doc fifo and become valid
    // has a configurable countdown in this state
    UTF_DIO_MODEL_ITEM_STATE_IN_DOC,
    // enter when leave doc fifo and sent to device
    // leave to invalid for write when done
    // leave to dic fifo for read when done
    // has a configurable countdown in this state
    UTF_DIO_MODEL_ITEM_STATE_IN_FLIGHT,
    // enter when coming back from device for read
    // has no configurable countdown in this state
    UTF_DIO_MODEL_ITEM_STATE_IN_DIC,
} UTF_DIO_MODEL_ITEM_STATE;

typedef struct
{
    UTF_DIO_MODEL_ITEM_STATE itemState;
    bool bRead;
    uint32_t addr;
    uint32_t data;
    uint32_t resetCountdown;
    uint32_t docCountdown;
    uint32_t inflightCountdown;
} UTF_DIO_MODEL_STATE;

static UTF_DIO_MODEL_STATE _utfDioModelState[] =
{
    {}, // DIO_TYPE_ILWALID
#if LWRISCV_HAS_DIO_SE
    {}, // DIO_TYPE_SE
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
    {}, // DIO_TYPE_SNIC
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
    {}, // DIO_TYPE_FBHUB
#endif // LWRISCV_HAS_DIO_FBHUB
};
#define TARGET_DIO(field) _utfDioModelState[dioType].field

static void resetDioModel(DIO_TYPE dioType);
static void utfDioModelSpin(DIO_TYPE dioType, uint32_t count);
static void utfDioModelPostRequest(DIO_TYPE dioType, uint32_t addr, uint32_t data, uint32_t bRead);
static void utfDioModelClearDic(DIO_TYPE dioType);
static void utfDioModelPop(DIO_TYPE dioType);
static void utfDioModelPostError(DIO_TYPE dioType, bool bProtocalError, bool bReadError, uint32_t errorInfo);

// =============================================================================
// DIO MOCKING INTERFACE
// Function: 1. keep valid register values for read write accesses
//           2. conduct register access to model layer
// =============================================================================
typedef struct
{
    uint32_t docCtrl;
    uint32_t docD0, docD1, docD2;
    uint32_t dicCtrl;
    uint32_t dicD0, dicD1, dicD2;
    uint32_t dioError;
} UTF_DIO_CTRL_REGS;

// control register addresses
static const UTF_DIO_CTRL_REGS _utfDioCtrlRegs[] =
{
    // DIO_TYPE_ILWALID
    {},
#if LWRISCV_HAS_DIO_SE
    // DIO_TYPE_SE
    {
        LW_PRGNLCL_FALCON_DOC_CTRL,
        LW_PRGNLCL_FALCON_DOC_D0,
        LW_PRGNLCL_FALCON_DOC_D1,
        LW_PRGNLCL_FALCON_DOC_D2,
        LW_PRGNLCL_FALCON_DIC_CTRL,
        LW_PRGNLCL_FALCON_DIC_D0,
        LW_PRGNLCL_FALCON_DIC_D1,
        LW_PRGNLCL_FALCON_DIC_D2,
        LW_PRGNLCL_FALCON_DIO_ERR,
    },
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
    // DIO_TYPE_SNIC
    {
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIO_ERR(0),
    },
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
    // DIO_TYPE_FBHUB
    {
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DOC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_CTRL(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D0(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D1(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIC_D2(0),
        (uint32_t)LW_PRGNLCL_FALCON_DIO_DIO_ERR(0),
    },
#endif // LWRISCV_HAS_DIO_FBHUB
};
#define UTF_DIO_CTRL_REG_READ(regName) UTF_IO_CACHE_READ(_utfDioCtrlRegs[dioType].regName, 4)
#define UTF_DIO_CTRL_REG_WRITE(regName, value) UTF_IO_CACHE_WRITE(_utfDioCtrlRegs[dioType].regName, 4, (value))

static void utfDioMockReset(DIO_TYPE dioType);
static void mockDioCtrlRegs(DIO_TYPE dioType);
static LwU32 utfDioMockRead(LwU32 addr, LwU8 size);
static void utfDioMockWrite(LwU32 addr, LwU8 size, LwU64 value);

// =============================================================================
// UTF helper structure and functions
// =============================================================================
typedef struct {
    DIO_PORT dioPort;
    DIO_OPERATION dioOp;
    uint32_t addr;
    // data to write
    uint32_t data;
    LWRV_STATUS status;
    // expected status
    LWRV_STATUS statusExp;
    // expected data
    uint32_t dataExp;
    // original data (not needed for mocking at the moment)
    uint32_t dataOrig;
} DioReadWriteParam_UTF;

static void* ioDioTestSetup(void);
static void ioDioTestTeardown(void *setup_arg);

// =============================================================================
// FUNCTION IMPLEMENTATIONS
// =============================================================================
static void utfDioModelSpin(DIO_TYPE dioType, uint32_t count)
{
    // if in reset state, only reduce resetCountdown, reset if needed and return
    uint32_t docCtrl = UTF_DIO_CTRL_REG_READ(docCtrl);
    if (DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RESET, docCtrl))
    {
        if (TARGET_DIO(resetCountdown) <= count)
        {
            TARGET_DIO(resetCountdown) = 0;
            utfDioMockReset(dioType);
        }
        else
        {
            TARGET_DIO(resetCountdown) -= count;
        }
        return;
    }

    // if valid item present
    if (TARGET_DIO(itemState) != UTF_DIO_MODEL_ITEM_STATE_ILWALID)
    {
        // if not in stop nor error states, reduce docCountdown
        if (!(DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RESET, docCtrl)    ||
              DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _STOP, docCtrl)     ||
              DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RD_ERROR, docCtrl) ||
              DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _WR_ERROR, docCtrl) ||
              DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _PROTOCOL_ERROR, docCtrl)))
        {
            if (TARGET_DIO(itemState) == UTF_DIO_MODEL_ITEM_STATE_IN_DOC)
            {
                if (TARGET_DIO(docCountdown) <= count)
                {
                    count -= TARGET_DIO(docCountdown);
                    TARGET_DIO(docCountdown) = 0;
                    TARGET_DIO(itemState) = UTF_DIO_MODEL_ITEM_STATE_IN_FLIGHT;

                    UTF_DIO_CTRL_REG_WRITE(docCtrl,
                        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _COUNT,          _INIT) |
                        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _EMPTY,          _INIT) |
                        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _WR_FINISHED,    _INIT) |
                        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _RD_FINISHED,    _INIT));
                }
                else
                {
                    TARGET_DIO(docCountdown) -= count;
                    return;
                }
            }
        }

        // reduce inflightCountdown
        // abnormal states does not prevent in flight traffic to finish
        if (TARGET_DIO(itemState) == UTF_DIO_MODEL_ITEM_STATE_IN_FLIGHT)
        {
            if (TARGET_DIO(inflightCountdown) <= count)
            {
                TARGET_DIO(inflightCountdown) = 0;
                // enter dic fifo for read
                if (TARGET_DIO(bRead))
                {
                    TARGET_DIO(itemState) = UTF_DIO_MODEL_ITEM_STATE_IN_DIC;
                    TARGET_DIO(data) = UTF_IO_CACHE_READ(TARGET_DIO(addr), 4);
                    uint32_t dicCtrl = UTF_DIO_CTRL_REG_READ(dicCtrl);
                    UT_ASSERT_EQUAL_UINT(DRF_VAL(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, dicCtrl), 0);
                    UTF_DIO_CTRL_REG_WRITE(dicCtrl, FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, 1, dicCtrl));
                }
                // retire for write
                else
                {
                    TARGET_DIO(itemState) = UTF_DIO_MODEL_ITEM_STATE_ILWALID;
                    UTF_IO_CACHE_WRITE(TARGET_DIO(addr), 4, TARGET_DIO(data));
                }
            }
            else
            {
                TARGET_DIO(inflightCountdown) -= count;
            }
        }
    }
}

static void resetDioModel(DIO_TYPE dioType)
{
    memset(&(_utfDioModelState[dioType]), 0, sizeof(UTF_DIO_MODEL_STATE));
}

// called when d0 is written to
static void utfDioModelPostRequest(DIO_TYPE dioType, uint32_t addr, uint32_t data, uint32_t bRead)
{
    uint32_t docCtrl = UTF_DIO_CTRL_REG_READ(docCtrl);

    // stop if in stop nor error states
    if (DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RESET, docCtrl)    ||
        DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _STOP, docCtrl)     ||
        DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _RD_ERROR, docCtrl) ||
        DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _WR_ERROR, docCtrl) ||
        DRF_VAL(_PRGNLCL, _DIO_DOC_CTRL, _PROTOCOL_ERROR, docCtrl))
    {
        return;
    }

    // update DIO item info
    TARGET_DIO(itemState) = UTF_DIO_MODEL_ITEM_STATE_IN_DOC;
    TARGET_DIO(addr) = addr;
    TARGET_DIO(data) = bRead ? 0 : data;
    TARGET_DIO(bRead) = bRead;

    // update registers
    docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _COUNT, LW_PRGNLCL_DIO_DOC_CTRL_COUNT_INIT - 1, docCtrl);
    docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _EMPTY, 0, docCtrl);
    if (bRead)
    {
        docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _RD_FINISHED, 0, docCtrl);
    }
    else
    {
        docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _WR_FINISHED, 0, docCtrl);
    }
    UTF_DIO_CTRL_REG_WRITE(docCtrl, docCtrl);
}

// called when dic valid is written to
static void utfDioModelClearDic(DIO_TYPE dioType)
{
    // regardless of reset or error states
    uint32_t dicCtrl = UTF_DIO_CTRL_REG_READ(dicCtrl);
    UTF_DIO_CTRL_REG_WRITE(dicCtrl, FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DIC_CTRL, _VALID, 0, dicCtrl));
    UTF_DIO_CTRL_REG_WRITE(dicD0, 0);
    UTF_DIO_CTRL_REG_WRITE(dicD1, 0);
    UTF_DIO_CTRL_REG_WRITE(dicD2, 0);
}

// called when dic pop is written to
static void utfDioModelPop(DIO_TYPE dioType)
{
    uint32_t dicCtrl = UTF_DIO_CTRL_REG_READ(dicCtrl);
    // regardless of reset or error states
    // as long as there is valid result to be poped
    if (DRF_VAL(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, dicCtrl) > 0)
    {
        // todo: remove the assert when multiple dio item in flight supported
        UT_ASSERT_EQUAL_UINT(DRF_VAL(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, dicCtrl), 1);
        dicCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, 0, dicCtrl);
        dicCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DIC_CTRL, _VALID, 1, dicCtrl);
        UTF_DIO_CTRL_REG_WRITE(dicCtrl, dicCtrl);
        UTF_DIO_CTRL_REG_WRITE(dicD0, TARGET_DIO(data));
    }
}

static void utfDioModelPostError(DIO_TYPE dioType, bool bProtocalError, bool bReadError, uint32_t errorInfo)
{
    uint32_t docCtrl = UTF_DIO_CTRL_REG_READ(docCtrl);
    if (bProtocalError)
    {
        docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _PROTOCOL_ERROR, 1, docCtrl);
    }
    else if (bReadError)
    {
        docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _RD_ERROR, 1, docCtrl);
    }
    else
    {
        docCtrl = FLD_SET_DRF_NUM(_PRGNLCL, _DIO_DOC_CTRL, _WR_ERROR, 1, docCtrl);
    }
    UTF_DIO_CTRL_REG_WRITE(docCtrl, docCtrl);
    UTF_DIO_CTRL_REG_WRITE(dioError, errorInfo);
}

static void utfDioMockReset(DIO_TYPE dioType)
{
    UTF_DIO_CTRL_REG_WRITE(docCtrl,
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _COUNT,          _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _RESET,          _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _STOP,           _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _EMPTY,          _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _WR_FINISHED,    _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _RD_FINISHED,    _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _WR_ERROR,       _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _RD_ERROR,       _INIT) |
        DRF_DEF(_PRGNLCL, _DIO_DOC_CTRL, _PROTOCOL_ERROR, _INIT)
    );
    UTF_DIO_CTRL_REG_WRITE(docD0, 0);
    UTF_DIO_CTRL_REG_WRITE(docD1, 0);
    UTF_DIO_CTRL_REG_WRITE(docD2, 0);
    UTF_DIO_CTRL_REG_WRITE(dicCtrl,
        DRF_DEF(_PRGNLCL, _DIO_DIC_CTRL, _COUNT, _INIT)|
        DRF_DEF(_PRGNLCL, _DIO_DIC_CTRL, _RESET, _INIT)|
        DRF_DEF(_PRGNLCL, _DIO_DIC_CTRL, _VALID, _INIT)
    );
    UTF_DIO_CTRL_REG_WRITE(dicD0, 0);
    UTF_DIO_CTRL_REG_WRITE(dicD1, 0);
    UTF_DIO_CTRL_REG_WRITE(dicD2, 0);
    UTF_DIO_CTRL_REG_WRITE(dioError, 0);

    resetDioModel(dioType);
}

static void mockDioCtrlRegs(DIO_TYPE dioType)
{
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].docCtrl,  _utfDioCtrlRegs[dioType].docCtrl,  utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].docD0,    _utfDioCtrlRegs[dioType].docD0,    utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].docD1,    _utfDioCtrlRegs[dioType].docD1,    utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].docD2,    _utfDioCtrlRegs[dioType].docD2,    utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].dicCtrl,  _utfDioCtrlRegs[dioType].dicCtrl,  utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].dicD0,    _utfDioCtrlRegs[dioType].dicD0,    utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].dicD1,    _utfDioCtrlRegs[dioType].dicD1,    utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].dicD2,    _utfDioCtrlRegs[dioType].dicD2,    utfDioMockRead, utfDioMockWrite);
    UTF_IO_MOCK(_utfDioCtrlRegs[dioType].dioError, _utfDioCtrlRegs[dioType].dioError, utfDioMockRead, utfDioMockWrite);

    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].docCtrl,  dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].docD0,    dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].docD1,    dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].docD2,    dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].dicCtrl,  dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].dicD0,    dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].dicD1,    dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].dicD2,    dioType);
    UTF_IO_SET_CONTEXT(_utfDioCtrlRegs[dioType].dioError, dioType);

    utfDioMockReset(dioType);
}

// simply check dioType and return cached value
// spin the model by 1 unit
static LwU32 utfDioMockRead(LwU32 addr, LwU8 size)
{
    DIO_TYPE dioType = UTF_IO_GET_CONTEXT(addr);
    if (dioType == DIO_TYPE_ILWALID)
    {
        UT_FAIL("invalid dioType");
    }
    utfDioModelSpin(dioType, 1);
    return UTF_IO_CACHE_READ(addr, size);
}

// update legal register value and spin the model
static void utfDioMockWrite(LwU32 addr, LwU8 size, LwU64 value)
{
    DIO_TYPE dioType = UTF_IO_GET_CONTEXT(addr);
    if (dioType == DIO_TYPE_ILWALID)
    {
        UT_FAIL("invalid dioType");
    }

    if (addr == _utfDioCtrlRegs[dioType].docCtrl)
    {
        UTF_DIO_CTRL_REG_WRITE(docCtrl,
            // set writeable bits
            (value & (DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_CTRL_RESET)            |
                      DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_CTRL_STOP)             |
                      DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_CTRL_WR_ERROR)         |
                      DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_CTRL_RD_ERROR)         |
                      DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_CTRL_PROTOCOL_ERROR))) |
            // retain reset and read only bits
            (UTF_DIO_CTRL_REG_READ(docCtrl) &
                    (DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DOC_CTRL_COUNT)        |
                     DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_CTRL_RESET)           |
                     DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DOC_CTRL_EMPTY)        |
                     DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DOC_CTRL_WR_FINISHED)  |
                     DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DOC_CTRL_RD_FINISHED))));
    }
    else if (addr == _utfDioCtrlRegs[dioType].docD0)
    {
        if (dioType == DIO_TYPE_SE)
        {
            UTF_DIO_CTRL_REG_WRITE(docD0,
                value & (DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D0_SEHUB_READ) |
                         DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D0_SEHUB_ADDR)));
            utfDioModelPostRequest(dioType,
                DRF_VAL(_PRGNLCL, _DIO_DOC_D0, _SEHUB_ADDR, value),
                DRF_VAL(_PRGNLCL, _DIO_DOC_D1, _SEHUB_WDATA, UTF_DIO_CTRL_REG_READ(docD1)),
                DRF_VAL(_PRGNLCL, _DIO_DOC_D0, _SEHUB_READ, value));
        }
        else if (dioType == DIO_TYPE_SNIC)
        {
            UTF_DIO_CTRL_REG_WRITE(docD0,
                value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D0_SNIC_WDATA));
            utfDioModelPostRequest(dioType,
                DRF_VAL(_PRGNLCL, _DIO_DOC_D1, _SNIC_ADDR, UTF_DIO_CTRL_REG_READ(docD1)),
                DRF_VAL(_PRGNLCL, _DIO_DOC_D0, _SNIC_WDATA, value),
                DRF_VAL(_PRGNLCL, _DIO_DOC_D2, _SNIC_READ, UTF_DIO_CTRL_REG_READ(docD2)));
        }
        else if (dioType == DIO_TYPE_FBHUB)
        {
            UTF_DIO_CTRL_REG_WRITE(docD0,
                value & (DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D0_FBHUB_READ) |
                         DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D0_FBHUB_ADDR)));
            utfDioModelPostRequest(dioType,
                DRF_VAL(_PRGNLCL, _DIO_DOC_D0, _FBHUB_ADDR, value),
                DRF_VAL(_PRGNLCL, _DIO_DOC_D1, _FBHUB_WDATA, UTF_DIO_CTRL_REG_READ(docD1)),
                DRF_VAL(_PRGNLCL, _DIO_DOC_D0, _FBHUB_READ, value));
        }
    }
    else if (addr == _utfDioCtrlRegs[dioType].docD1)
    {
        if (dioType == DIO_TYPE_SE)
        {
            UTF_DIO_CTRL_REG_WRITE(docD1, value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D1_SEHUB_WDATA));
        }
        else if (dioType == DIO_TYPE_SNIC)
        {
            UTF_DIO_CTRL_REG_WRITE(docD1, value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D1_SNIC_ADDR));
        }
        else if (dioType == DIO_TYPE_FBHUB)
        {
            UTF_DIO_CTRL_REG_WRITE(docD1, value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D1_FBHUB_WDATA));
        }
    }
    else if (addr == _utfDioCtrlRegs[dioType].docD2)
    {
        if (dioType == DIO_TYPE_SNIC)
        {
            UTF_DIO_CTRL_REG_WRITE(docD2, value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DOC_D2_SNIC_READ));
        }
    }
    else if (addr == _utfDioCtrlRegs[dioType].dicCtrl)
    {
        // dic reset bit has no effect but can be written
        UTF_DIO_CTRL_REG_WRITE(dicCtrl,
            (value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DIC_CTRL_RESET)) |
            (UTF_DIO_CTRL_REG_READ(dicCtrl) &
                    (DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DIC_CTRL_COUNT) |
                     DRF_SHIFTMASK(LW_PRGNLCL_FALCON_DIC_CTRL_VALID))));
        if (DRF_VAL(_PRGNLCL, _DIO_DIC_CTRL, _VALID, value))
        {
            utfDioModelClearDic(dioType);
        }
        if (DRF_VAL(_PRGNLCL, _DIO_DIC_CTRL, _POP, value))
        {
            utfDioModelPop(dioType);
        }
    }
    else if (addr == _utfDioCtrlRegs[dioType].dicD0)
    {
        UTF_DIO_CTRL_REG_WRITE(dicD0, value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DIC_D0_DATA));
    }
    else if (addr == _utfDioCtrlRegs[dioType].dioError)
    {
        UTF_DIO_CTRL_REG_WRITE(dioError, value & DRF_SHIFTMASK(LW_PRGNLCL_DIO_DIO_ERR_INFO));
    }

    utfDioModelSpin(dioType, 1);
    return;
}

static void* ioDioTestSetup(void)
{
    // mocking is on by default
    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);
#if LWRISCV_HAS_DIO_SE
    mockDioCtrlRegs(DIO_TYPE_SE);
#endif // LWRISCV_HAS_DIO_SE
#if LWRISCV_HAS_DIO_SNIC
    mockDioCtrlRegs(DIO_TYPE_SNIC);
#endif // LWRISCV_HAS_DIO_SNIC
#if LWRISCV_HAS_DIO_FBHUB
    mockDioCtrlRegs(DIO_TYPE_FBHUB);
#endif // LWRISCV_HAS_DIO_FBHUB

    // set wait time default
    gDioMaxWait = DIO_MAX_WAIT_DEFAULT;
    return NULL;
}

static void ioDioTestTeardown(void *setup_arg)
{
    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);
    UTF_IO_CLEAR_MOCKS();
    UTF_IO_RESET_REGCACHE();
}

/*
x possitive test with real hw
    goal: to check sw regression against legal hw use cases
- negative test with register mocking and function mocking
    goal: get code coverage
    try not to cause errors in real hw since modules that are beyond control of the subject
    dio type is the only thing that the subject can guard against
    x _dioWaitForDocEmpty: return dioErrorCode or LWRV_ERR_DIO_DOC_CTRL_ERROR, or LWRV_ERR_TIMEOUT
    - _dioSendRequest: LWRV_ERR_NOT_SUPPORTED (change dioType after check in dioReadWrite pass)
    - _dioWaitForOperationComplete: return dioErrorCode or LWRV_ERR_DIO_DOC_CTRL_ERROR, or LWRV_ERR_TIMEOUT (on request sent or on data return)
    - _dioReset return LWRV_ERR_TIMEOUT
*/

UT_SUITE_DEFINE(IO_DIO,
                UT_SUITE_SET_COMPONENT("IO_DIO")
                UT_SUITE_SET_DESCRIPTION("io_dio tests")
                UT_SUITE_SET_OWNER("yitianc")
                UT_SUITE_SETUP_HOOK(ioDioTestSetup)
                UT_SUITE_TEARDOWN_HOOK(ioDioTestTeardown))

UT_CASE_DEFINE(IO_DIO, ioDioPositiveTest,
    UT_CASE_SET_DESCRIPTION("normal read write tests")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(IO_DIO, ioDioPositiveTest)
{
    DioReadWriteParam_UTF dioReadWriteParam_UTF[] =
    {
        // portType,        portIdx, dioOp,            addr,       data,       status,  statusExp, dataExp,    dataOrig
        {  {DIO_TYPE_SE,    0},      DIO_OPERATION_WR, 0xbeef,     0xfafafafa, LWRV_OK, LWRV_OK,   0x0,        0x0      },
        {  {DIO_TYPE_SE,    0},      DIO_OPERATION_RD, 0xbeef,     0x0,        LWRV_OK, LWRV_OK,   0xfafafafa, 0x0      },
        {  {DIO_TYPE_SNIC,  0},      DIO_OPERATION_WR, 0xbadbeef0, 0xfbfbfbfb, LWRV_OK, LWRV_OK,   0x0,        0x0      },
        {  {DIO_TYPE_SNIC,  0},      DIO_OPERATION_RD, 0xbadbeef0, 0x0,        LWRV_OK, LWRV_OK,   0xfbfbfbfb, 0x0      },
        {  {DIO_TYPE_FBHUB, 0},      DIO_OPERATION_WR, 0xfeed,     0xfcfcfcfc, LWRV_OK, LWRV_OK,   0x0,        0x0      },
        {  {DIO_TYPE_FBHUB, 0},      DIO_OPERATION_RD, 0xfeed,     0x0,        LWRV_OK, LWRV_OK,   0xfcfcfcfc, 0x0      },
        {  {DIO_TYPE_END,   0},      DIO_OPERATION_RD, 0x0,        0x0,        LWRV_OK, LWRV_OK,   0x0,        0x0      },
    };

    int i;
    while (dioReadWriteParam_UTF[i].dioPort.dioType != DIO_TYPE_END)
    {
        DioReadWriteParam_UTF *pParam = dioReadWriteParam_UTF + i;
        if (pParam->dioPort.dioType != DIO_TYPE_ILWALID)
        {
            // mock target register
            if (pParam->dioOp == DIO_OPERATION_WR)
            {
                UT_EXPECT_ASSERT(UTF_IO_CACHE_READ(pParam->addr, 4));
                UTF_IO_CACHE_WRITE(pParam->addr, 4, 0);
            }
            // test dio read write
            pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
            UT_ASSERT_EQUAL_UINT(pParam->status, pParam->statusExp);
            UT_ASSERT_EQUAL_UINT(pParam->data, pParam->dioOp == DIO_OPERATION_WR ? UTF_IO_CACHE_READ(pParam->addr, 4) : pParam->dataExp);
            utf_printf("[type: 0x%x] reg 0x%x %s: 0x%x\n", pParam->dioPort.dioType, pParam->addr, pParam->dioOp == DIO_OPERATION_RD ? "rd" : "wr", pParam->data);
        }
        i++;
    }
}

UT_CASE_DEFINE(IO_DIO, ioDioIlwalidType,
    UT_CASE_SET_DESCRIPTION("read write requests with invalid dioType")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(IO_DIO, ioDioIlwalidType)
{
    // mocking not needed for this case
    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    DioReadWriteParam_UTF dioReadWriteParam_UTF[] =
    {
        // portType,          portIdx, dioOp,            addr, data, status,  statusExp,              dataExp, dataOrig
        {  {DIO_TYPE_ILWALID, 0},      DIO_OPERATION_RD, 0x0,  0x0,  LWRV_OK, LWRV_ERR_NOT_SUPPORTED, 0x0,     0x0      },
        {  {DIO_TYPE_END,     0},      DIO_OPERATION_RD, 0x0,  0x0,  LWRV_OK, LWRV_ERR_NOT_SUPPORTED, 0x0,     0x0      },
        {  {(DIO_TYPE)(-1),   0},      DIO_OPERATION_RD, 0x0,  0x0,  LWRV_OK, LWRV_ERR_NOT_SUPPORTED, 0x0,     0x0      },
        {  {DIO_TYPE_ILWALID, 0},      DIO_OPERATION_WR, 0x0,  0x0,  LWRV_OK, LWRV_ERR_NOT_SUPPORTED, 0x0,     0x0      },
        {  {DIO_TYPE_END,     0},      DIO_OPERATION_WR, 0x0,  0x0,  LWRV_OK, LWRV_ERR_NOT_SUPPORTED, 0x0,     0x0      },
        {  {(DIO_TYPE)(-1),   0},      DIO_OPERATION_WR, 0x0,  0x0,  LWRV_OK, LWRV_ERR_NOT_SUPPORTED, 0x0,     0x0      },
    };

    int i;
    while (dioReadWriteParam_UTF[i].dioPort.dioType != DIO_TYPE_END)
    {
        DioReadWriteParam_UTF *pParam = dioReadWriteParam_UTF + i;
        pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
        UT_ASSERT_EQUAL_UINT(pParam->status, pParam->statusExp);
        UT_ASSERT_EQUAL_UINT(pParam->data, pParam->dataExp);
        i++;
    }
}

UT_CASE_DEFINE(IO_DIO, ioDioDocEmptyError,
    UT_CASE_SET_DESCRIPTION("todo")
    UT_CASE_SET_TYPE(FAULT_INJECTION)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(IO_DIO, ioDioDocEmptyError)
{
    uint32_t addr = 0xbadadd; // this is unmocked, should not even be touched in the whole process

    DioReadWriteParam_UTF dioReadWriteParam_UTF[] =
    {
        // portType,        portIdx, dioOp,            addr,  data, status,  statusExp,                         dataExp, dataOrig
        {  {DIO_TYPE_SE,    0},      DIO_OPERATION_WR, addr,  0x0,  LWRV_OK, LWRV_ERR_GENERIC/*details_below*/, 0x0,     0x0      },
        {  {DIO_TYPE_SE,    0},      DIO_OPERATION_RD, addr,  0x0,  LWRV_OK, LWRV_ERR_GENERIC/*details_below*/, 0x0,     0x0      },
        {  {DIO_TYPE_SNIC,  0},      DIO_OPERATION_WR, addr,  0x0,  LWRV_OK, LWRV_ERR_GENERIC/*details_below*/, 0x0,     0x0      },
        {  {DIO_TYPE_SNIC,  0},      DIO_OPERATION_RD, addr,  0x0,  LWRV_OK, LWRV_ERR_GENERIC/*details_below*/, 0x0,     0x0      },
        {  {DIO_TYPE_FBHUB, 0},      DIO_OPERATION_WR, addr,  0x0,  LWRV_OK, LWRV_ERR_GENERIC/*details_below*/, 0x0,     0x0      },
        {  {DIO_TYPE_FBHUB, 0},      DIO_OPERATION_RD, addr,  0x0,  LWRV_OK, LWRV_ERR_GENERIC/*details_below*/, 0x0,     0x0      },
    };

    int i;
    for (i = 0; i < 6; i++)
    {
        DioReadWriteParam_UTF *pParam = dioReadWriteParam_UTF + i;
        if (pParam->dioPort.dioType != DIO_TYPE_ILWALID)
        {
            // post errors and check correct feedback
            {
                utfDioModelPostError(pParam->dioPort.dioType, true,  false, 0x0);
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, LWRV_ERR_DIO_DOC_CTRL_ERROR);

                utfDioModelPostError(pParam->dioPort.dioType, true,  false, 0xf0f0f0f0);
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, 0xf0f0f0f0);

                utfDioModelPostError(pParam->dioPort.dioType, false, false, 0x0);
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, LWRV_ERR_DIO_DOC_CTRL_ERROR);

                utfDioModelPostError(pParam->dioPort.dioType, false, false, 0xf1f1f1f1);
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, 0xf1f1f1f1);

                utfDioModelPostError(pParam->dioPort.dioType, false, true,  0x0);
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, LWRV_ERR_DIO_DOC_CTRL_ERROR);

                utfDioModelPostError(pParam->dioPort.dioType, false, true,  0xf2f2f2f2);
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, 0xf2f2f2f2);
            }
            // cause timeout
            {
                gDioMaxWait = 0;
                pParam->status = dioReadWrite(pParam->dioPort, pParam->dioOp, pParam->addr, &(pParam->data));
                UT_ASSERT_EQUAL_UINT(pParam->status, LWRV_ERR_TIMEOUT);
            }
        }
    }
}
