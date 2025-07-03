/*
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   se_eccryptogp10x.c
 * @brief  Halified library functions for supporting Elliptic Lwrve Cryptography.
 *         This file contains low-level math functions such as modular addition, modular
 *         multiplication, EC point multiplication and others.  These low-level math
 *         functions are used to implement high level EC algorithms (for example, ECDSA).
 *
 *         SE supplies HW support for Elliptic Lwrve Point Multiplication: 
 *               P = kG based on lwrve C (y^2 = x^3 + ax + b mod p). 
 *
 *         Additionally, SE supplies some more fundamental algorithms like: 
 *              Modular Addition, Modular Multiplication, Elliptic Lwrve Point Verification.
 *
 *         For more information see:
 *              //hw/ar/doc/system/Architecture/Security/Reference/Partners/Elliptic/EDN_0243_PKA_User_Guide_v1p15
 *              //hw/ar/doc/t18x/se/SE_PKA_IAS.doc
 *
 *         Sample HW test code at:
 *              //hw/gpu_ip/unit/falcon/6.0/fmod/se/
 *              //hw/gpu_ip/unit/falcon/6.0/fmod/se_stimgen/
 *              
 */
#include "se_objse.h"
#include "se_private.h"
#include "secbus_ls.h"
#include "config/g_se_hal.h"
#include "dev_se_seb.h"


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */


/*!
 * @brief Elliptic lwrve point multiplication over a prime modulus (Q = kP)
 *
 *        In terms of input parameters this function is:
 *                   outputX/Y =  scalarK*generatorPointX/Y
 *
 *        This HAL is a private function and the public API is seECPointMult.
 *        The public API performs NULL checks on the input data before calling
 *        the private HAL.
 *
 * @param[in]  modulus                     Prime modulus (p in above equation)
 * @param[in]  a                           a from EC equation
 * @param[in]  generatorPointX             Generator base point X  (Px in above equation)
 * @param[in]  generatorPointY             Generator base point Y  (Py in above equation)
 * @param[in]  scalarK                     scalar k  (often times this is the private key)
 * @param[out] outPointX                   Resulting X point of multiplication, Qx
 * @param[out] outPointY                   Resulting Y point of multiplication, Qy
 * @param[in]  pPkaReg                     Struct containing the number of words based
 *                                         on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seECPointMult_GP10X
(
    LwU32       modulus[],
    LwU32       a[],
    LwU32       generatorPointX[],
    LwU32       generatorPointY[],
    LwU32       scalarK[],
    LwU32       outPointX[],
    LwU32       outPointY[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;

    // Store prime modulus in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Calulate r ilwerse. r_ilw is output to C0 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW, pPkaReg->pkaRadixMask));

    // Callwlate modulus'. modulus' is output to D1 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP, pPkaReg->pkaRadixMask));

    // Callwlate (r^2 mod m). Result is output to D3 as a result of HW operation.
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR, pPkaReg->pkaRadixMask));

    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 6, a, pPkaReg->pkaKeySizeDW));

    // Store Gx in A2, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 2, generatorPointX, pPkaReg->pkaKeySizeDW));
    // Store Gy in B2, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_B, 2, generatorPointY, pPkaReg->pkaKeySizeDW));
    // Store k in D7, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 7, scalarK, pPkaReg->pkaKeySizeDW));

    // Perform the point multiplicaton. X point is output to A3, y point is output to B3
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_PMULT, pPkaReg->pkaRadixMask));

    if (outPointX)
    {
        CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_A, 3, outPointX, pPkaReg->pkaKeySizeDW));
    }

    if (outPointY)
    {
        CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_B, 3, outPointY, pPkaReg->pkaKeySizeDW));
    }

ErrorExit:
    return status;
}

/*!
 * @brief Elliptic lwrve point addition over a prime modulus (R = P + Q)
 *
 *        In terms of input parameters this function is:
 *                   outputX/Y =  pointX1/Y1 + pointX2/Y2
 *
 *        This HAL is a private function and there is no corresponding public API
 *        that calls this directly.  All parameter will be checked by a public 
 *        function before reaching private functions.
 *
 *
 * @param[in]  modulus                     Prime modulus
 * @param[in]  pointX1                     X coord of 1st point, Px
 * @param[in]  pointY1                     Y coord of 1st point, Py
 * @param[in]  pointX2                     X coord of 2nd point, Qx
 * @param[in]  pointY2                     Y coord of 2nd point, Qy
 * @param[out] outPointX                   Resulting X point, Rx
 * @param[out] outPointY                   Resulting Y point, Ry
 * @param[in]  pPkaReg                     Struct containing the number of words based
 *                                         on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */ 
SE_STATUS seECPointAdd_GP10X
(
    LwU32       modulus[],
    LwU32       pointX1[],
    LwU32       pointY1[],
    LwU32       pointX2[],
    LwU32       pointY2[],
    LwU32       outPointX[],
    LwU32       outPointY[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;

    // Store prime modulus in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Calulate r ilwerse. r_ilw is output to C0 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW, pPkaReg->pkaRadixMask));

    // Callwlate modulus'. modulus' is output to D1 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP, pPkaReg->pkaRadixMask));

    // Callwlate (r^2 mod m). Result is output to D3 as a result of HW operation.
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR, pPkaReg->pkaRadixMask));

    // Store pointX1 in A2, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 2, pointX1, pPkaReg->pkaKeySizeDW));
    // Store pointY1 in B2, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_B, 2, pointY1, pPkaReg->pkaKeySizeDW));
    // Store pointX2 in A3, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 3, pointX2, pPkaReg->pkaKeySizeDW));
    // Store pointY2 in B3, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_B, 3, pointY2, pPkaReg->pkaKeySizeDW));
    
    // Perform the point addition. X point is output to A3, Y point is output to B3
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_PADD, pPkaReg->pkaRadixMask));

    if (outPointX)
    {
        CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_A, 3, outPointX, pPkaReg->pkaKeySizeDW));
    }

    if (outPointY)
    {
        CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_B, 3, outPointY, pPkaReg->pkaKeySizeDW));
    }

ErrorExit:
    return status;
}

/*!
 * @brief Elliptic lwrve point verification over a prime modulus (y^2 = x3 + ax + b mod p).
 *        Check the point is actually on the lwrve. 
 *
 *        This HAL is a private function and the public API is seECPointVerification.
 *        The public API performms NULL checks on the input data before calling
 *        the private HAL.
 *
 * @param[in]  modulus                     Prime modulus (p in above equation)
 * @param[in]  a                           a from EC equation
 * @param[in]  b                           b from EC equation
 * @param[in]  pointX                      X coord of point
 * @param[in]  pointY                      Y coord of point
 * @param[in]  pPkaReg                     Struct containing the number of words based
 *                                         on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seECPointVerification_GP10X
(
    LwU32       modulus[],
    LwU32       a[],
    LwU32       b[],
    LwU32       pointX[],
    LwU32       pointY[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;
    LwU32     val;

    // Store prime modulus in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Calulate r ilwerse. r_ilw is output to C0 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW, pPkaReg->pkaRadixMask));

    // Callwlate modulus'. modulus' is output to D1 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP, pPkaReg->pkaRadixMask));

    // Callwlate (r^2 mod m). Result is output to D3 as a result of HW operation.
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR, pPkaReg->pkaRadixMask));

    // Store a in A6, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 6, a, pPkaReg->pkaKeySizeDW));
    // Store Gx in A2, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 2, pointX, pPkaReg->pkaKeySizeDW));
    // Store Gy in B2, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_B, 2, pointY, pPkaReg->pkaKeySizeDW));
    // Store b in A7, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 7, b, pPkaReg->pkaKeySizeDW));
    
    // Perform the point verification, ie, check the point is on the lwrve.  The result is output to the PKA_FLAGS register.
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_PVER, pPkaReg->pkaRadixMask));

    CHECK_SE_STATUS(seSelwreBusReadRegisterErrChk(LW_SSE_SE_PKA_FLAGS, &val));
    if (!(val & 0x1))
    {
        return SE_ERR_POINT_NOT_ON_LWRVE;
    }

ErrorExit:
    return status;
}

/*!
 * @brief Modular reduction(x mod m)
 *
 *        This HAL is a private function and there is no corresponding public API
 *        that maps to this directly. It may be called as a helper function by any public
 *        API. All parameters will be checked in a public API before reaching private
 *        functions.
 *
 * @param[in]  x                          X in above equation
 * @param[in]  modulus                    m in above equation
 * @param[out] result                     Result
 * @param[in]  pPkaReg                    Struct containing the number of words based
 *                                        on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seModularReduction_GP10X
(
    LwU32       x[],
    LwU32       modulus[],
    LwU32       result[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;

    // x in C0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_C, 0, x, pPkaReg->pkaKeySizeDW));

    // Store m in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Perform the modular reduction.  The result is output to A0
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_REDUCE, pPkaReg->pkaRadixMask));

    CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_A, 0, result, pPkaReg->pkaKeySizeDW));

ErrorExit:
    return status;
}

/*!
 * @brief Modular ilwersion (1/x mod m)
 *
 *        This HAL is a private function and there is no corresponding public API
 *        that maps to this directly. It may be called as a helper function by any public
 *        API. All parameters will be checked in a public API before reaching private
 *        functions.
 *
 * @param[in]  x                          X in above equation
 * @param[in]  modulus                    m in above equation
 * @param[out] result                     Result
 * @param[in]  pPkaReg                    Struct containing the number of words based
 *                                        on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seModularIlwersion_GP10X
(
    LwU32       x[],
    LwU32       modulus[],
    LwU32       result[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;

    // Store x in C0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 0, x, pPkaReg->pkaKeySizeDW));

    // Store m in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Perform the modular ilwersion.  The result is output to C0
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODILW, pPkaReg->pkaRadixMask));

    CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_C, 0, result, pPkaReg->pkaKeySizeDW));

ErrorExit:
    return status;
}

/*!
 * @brief Modular multiplication (xy mod m)
 *
 *        This HAL is a private function and there is no corresponding public API
 *        that maps to this directly. It may be called as a helper function by any public
 *        API. All parameters will be checked in a public API before reaching private
 *        functions.
 *
 * @param[in]  x                          X in above equation
 * @param[in]  y                          Y in above equation
 * @param[in]  modulus                    m in above equation
 * @param[out] result                     Result
 * @param[in]  pPkaReg                    Struct containing the number of words based
 *                                        on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seModularMult_GP10X
(
    LwU32       x[],
    LwU32       y[],
    LwU32       modulus[],
    LwU32       result[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;

    // Store m in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Calulate r ilwerse. r_ilw is output to C0 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW, pPkaReg->pkaRadixMask));

    // Calulate n'. n' is output to D1 as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP, pPkaReg->pkaRadixMask));

    // Callwlate (r^2 mod m). Result is output to D3 as a result of HW operation.
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR, pPkaReg->pkaRadixMask));

     // Store a in A0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 0, x, pPkaReg->pkaKeySizeDW));
    // Store b in B0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_B, 0, y, pPkaReg->pkaKeySizeDW));

    // Perform the modular multiplication. Result is stored in A0
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODMULT, pPkaReg->pkaRadixMask));
    CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_A, 0, result, pPkaReg->pkaKeySizeDW));

ErrorExit:
    return status;
}

/*!
 * @brief Modular addition (x+y mod m)
 *
 *        This HAL is a private function and there is no corresponding public API
 *        that maps to this directly. It may be called as a helper function by any public
 *        API. All parameters will be checked in a public API before reaching private
 *        functions.
 *
 * @param[in]  x                          X in above equation
 * @param[in]  y                          Y in above equation
 * @param[in]  modulus                    m in above equation
 * @param[out] result                     Result
 * @param[in]  pPkaReg                    Struct containing the number of words based
 *                                        on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seModularAdd_GP10X
(
    LwU32       x[],
    LwU32       y[],
    LwU32       modulus[],
    LwU32       result[],
    PSE_PKA_REG pPkaReg
)
{
    SE_STATUS status = SE_OK;

    // Store x in A0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 0, x, pPkaReg->pkaKeySizeDW));

    // Store y in B0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_B, 0, y, pPkaReg->pkaKeySizeDW));

    // Store m in D0, as expected by the HW for this operation
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, modulus, pPkaReg->pkaKeySizeDW));

    // Perform the modular addition.  The result is output to A0
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODADD, pPkaReg->pkaRadixMask));
    CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_A, 0, result, pPkaReg->pkaKeySizeDW));

ErrorExit:
    return status;
}
