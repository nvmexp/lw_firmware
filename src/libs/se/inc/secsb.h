
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SECSB_H
#define SECSB_H

/*!
 * @file  secsb.h 
 * $brief Maps the falcon registers needed for secure bus read and write 
 *        based on which falcon is compiling the SE library with it. The flags
 *        are set in the makefile.
 *  
 */
 
#include "lwuproc.h"
#include "setypes.h"
#include "dev_falcon_v4.h"

/* ------------------------ External Definitions --------------------------- */
extern unsigned int csbBaseAddress;

/* ------------------------- Function Prototypes ---------------------------- */
static inline SE_STATUS _seCsbErrorChkRegRd32_GP10X(LwUPtr addr, LwU32 *pData)
    GCC_ATTRIB_ALWAYSINLINE();
static inline SE_STATUS _seCsbErrorChkRegWr32NonPosted_GP10X(LwUPtr addr, LwU32 data)
    GCC_ATTRIB_ALWAYSINLINE();

/* 
 * Based on the variables set in the makefile, include the relevant
 * falcon based header file. 
 * Also define CSB_REG for each of the falcons. 
*/
#ifdef SEC2_RTOS
    #include "dev_sec_csb.h"

    // OpenRtosFalcon.h has same definition.
    #ifndef CSB_REG
    #define CSB_REG(name)   (unsigned int*)LW_CSEC_FALCON##name
    #endif
    #ifndef CSB_FIELD
    #define CSB_FIELD(name) LW_CSEC_FALCON##name
    #endif
    #ifndef CSB_FLD_TEST_DRF_NUM
    #define CSB_FLD_TEST_DRF_NUM(r,f,c,v) FLD_TEST_DRF_NUM(_CSEC,r,f,c,v)
    #endif
    #ifndef CSB_DRF_VAL
    #define CSB_DRF_VAL(r,f,v) DRF_VAL(_CSEC,r,f,v)
    #endif
    #ifndef CSB_DRF_DEF
    #define CSB_DRF_DEF(r,f,c) DRF_DEF(_CSEC,r,f,c)
    #endif
#endif

#ifdef PMU_RTOS
    #include "dev_pwr_falcon_csb.h"
    #ifndef CSB_REG
    #define CSB_REG(name)   (unsigned int*)LW_CMSDEC_FALCON##name
    #define CSB_FIELD(name) LW_CMSDEC_FALCON##name
    #endif
    #ifndef CSB_FLD_TEST_DRF_NUM
    #define CSB_FLD_TEST_DRF_NUM(r,f,c,v) FLD_TEST_DRF_NUM(_CMSDEC,r,f,c,v)
    #endif
    #ifndef CSB_DRF_VAL
    #define CSB_DRF_VAL(r,f,v) DRF_VAL(_CMSDEC,r,f,v)
    #endif
    #ifndef CSB_DRF_DEF
    #define CSB_DRF_DEF(r,f,c) DRF_DEF(_CMSDEC,r,f,c)
    #endif
#endif

#ifdef GSPLITE_RTOS
    #include "dev_gsp_csb.h"
    #ifndef CSB_REG
    #define CSB_REG(name)   (unsigned int*)LW_CGSP_FALCON##name
    #define CSB_FIELD(name) LW_CGSP_FALCON##name
    #endif
    #ifndef CSB_FLD_TEST_DRF_NUM
    #define CSB_FLD_TEST_DRF_NUM(r,f,c,v) FLD_TEST_DRF_NUM(_CGSP,r,f,c,v)
    #endif
    #ifndef CSB_DRF_VAL
    #define CSB_DRF_VAL(r,f,v) DRF_VAL(_CGSP,r,f,v)
    #endif
    #ifndef CSB_DRF_DEF
    #define CSB_DRF_DEF(r,f,c) DRF_DEF(_CGSP,r,f,c)
    #endif
#endif

/*!
 * Base address for the CSB access is only required in certain falcon
 * (e.g. DPU). BASEADDR_NEEDED will be set to 0 or 1 based on the makefile
 * setting in each falcon. When it's 0, csbBaseAddress will not be considered,
 * and it will not be compiled into the binary thus saving some resident code
 * space.
 */
#ifdef DPU_RTOS
    #include "dev_disp_falcon.h"
    #ifndef CSB_REG
    #define CSB_REG(name)   (unsigned int*)LW_PDISP_FALCON##name
    #define CSB_FIELD(name) LW_PDISP_FALCON##name
    #endif
    #ifndef CSB_FLD_TEST_DRF_NUM
    #define CSB_FLD_TEST_DRF_NUM(r,f,c,v) FLD_TEST_DRF_NUM(_PDISP,r,f,c,v)
    #endif
    #ifndef CSB_DRF_VAL
    #define CSB_DRF_VAL(r,f,v) DRF_VAL(_PDISP,r,f,v)
    #endif
    #ifndef CSB_DRF_DEF
    #define CSB_DRF_DEF(r,f,c) DRF_DEF(_PDISP,r,f,c)
    #endif
#endif

#ifdef LWDEC_RTOS
    #include "dev_lwdec_csb.h"
    #ifndef CSB_REG
    #define CSB_REG(name)   (unsigned int*)LW_CLWDEC_FALCON##name
    #define CSB_FIELD(name) LW_CLWDEC_FALCON##name
    #endif
    #ifndef CSB_FLD_TEST_DRF_NUM
    #define CSB_FLD_TEST_DRF_NUM(r,f,c,v) FLD_TEST_DRF_NUM(_CLWDEC,r,f,c,v)
    #endif
    #ifndef CSB_DRF_VAL
    #define CSB_DRF_VAL(r,f,v) DRF_VAL(_CLWDEC,r,f,v)
    #endif
    #ifndef CSB_DRF_DEF
    #define CSB_DRF_DEF(r,f,c) DRF_DEF(_CLWDEC,r,f,c)
    #endif

#endif


#define CSB_REG_RD32_ERRCHK(addr, pData)    _seCsbErrorChkRegRd32_GP10X(addr, pData)
#define CSB_REG_WR32_ERRCHK(addr, data)     _seCsbErrorChkRegWr32NonPosted_GP10X(addr, data)

/*!
 * Reads the given CSB address. The read transaction is nonposted.
 * 
 * Checks the return read value for priv errors and returns a status.
 * It is up the the calling apps to decide how to handle a failing status.
 *
 * @param[in]  addr   The CSB address to read
 * @param[out] *pData The 32-bit value of the requested address.
 *                    This function allows this value to be NULL
 *                    as well.
 *
 * @return The status of the read operation.
 */
static inline SE_STATUS
_seCsbErrorChkRegRd32_GP10X
(
    LwUPtr  addr,
    LwU32  *pData
)
{
    LwU32                 val;
    LwU32                 csbErrStatVal = 0;
    SE_STATUS             seStatus      = SE_ERR_CSB_PRIV_READ_ERROR;

    // Read and save the requested register and error register
    val = lwuc_ldxb_i ((LwU32*)(addr), 0);
    csbErrStatVal = lwuc_ldxb_i(CSB_REG(_CSBERRSTAT), 0);

    // If FALCON_CSBERRSTAT_VALID == _FALSE, CSB read succeeded
    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _FALSE, csbErrStatVal))
    {
        //
        // It is possible that sometimes we get bad data on CSB despite hw
        // thinking that read was a success. Apply a heuristic to trap such
        // cases.
        //
        if ((val & SE_CSB_PRIV_PRI_ERROR_MASK) == SE_CSB_PRIV_PRI_ERROR_CODE)
        {
            seStatus = SE_ERR_CSB_PRIV_READ_0xBADF_VALUE_RECEIVED;

            if (addr == (LwUPtr)CSB_REG(_PTIMER1))
            {
                //
                // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
                // just make an exception for it.
                //
                seStatus = SE_OK;
            }
            else if (addr == (LwUPtr)CSB_REG(_PTIMER0))
            {
                //
                // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
                // should keep changing its value every nanosecond. We just check
                // to make sure we can detect its value changing by reading it a
                // few times.
                //
                LwU32   val1 = 0;
                LwU32   i;

                for (i = 0; i < 3; i++)
                {
                    val1 = lwuc_ldx_i((LwU32 *)(addr), 0);
                    if (val1 != val)
                    {
                        val = val1;
                        seStatus = SE_OK;
                        break;
                    }
                }
            }
        }
        else
        {
            seStatus = SE_OK;
        }
    }

    //
    // Always update the output read data value, even in case of error.
    // The data is always invalid in case of error,  so it doesn't matter
    // what the value is.  The caller should detect the error case and
    // act appropriately.
    //
    if (pData != NULL)
    {
        *pData = val;
    }
    return seStatus;
}

/*!
 * Writes a 32-bit value to the given CSB address. 
 *
 * After writing the registers, check for issues that may have oclwrred with
 * the write and return status.  It is up the the calling apps to decide how
 * to handle a failing status.
 *
 * According to LW Bug 292204 -  falcon function "stxb" is the non-posted
 * version for CSB writes.
 *
 * @param[in]  addr  The CSB address to write
 * @param[in]  data  The data to write to the address
 *
 * @return The status of the write operation.
 */
static inline SE_STATUS
_seCsbErrorChkRegWr32NonPosted_GP10X
(
    LwUPtr addr,
    LwU32  data
)
{
    LwU32                 csbErrStatVal = 0;
    SE_STATUS             seStatus      = SE_OK;

    // Write requested register value and read error register
    lwuc_stxb_i((LwU32*)(addr), 0, data);
    csbErrStatVal = lwuc_ldxb_i(CSB_REG(_CSBERRSTAT), 0);

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        seStatus = SE_ERR_CSB_PRIV_WRITE_ERROR;
    }

    return seStatus;
}

#endif // SECSB_H
