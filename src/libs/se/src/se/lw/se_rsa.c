/*
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   se_rsa.c
 * @brief  Non halified library functions for supporting RSA Cryptography.
 *         This file contains low-level math functions such as modular exponentiation, montgomery
 *         pre-computation as well as support for some higher level RSA algorithms
 *         that makes use of the low-level math functions.
 *
 *         For more information see:
 *              //hw/ar/doc/system/Architecture/Security/Reference/Partners/Elliptic/EDN_0243_PKA_User_Guide_v1p15
 *              //hw/gpu_ip/doc/falcon/arch/TU10x_SE_PKA_IAS.doc
 *              //sw/docs/resman/chips/Turing/SW_Design/se_design_doc.docx
 *
 *         Sample HW test code at:
 *              //hw/gpu_ip/unit/falcon/6.0/fmod/se/
 *              //hw/gpu_ip/unit/falcon/6.0/fmod/se_stimgen/
 *
 *
 */

#include "se_objse.h"
#include "seapi.h"
#include "se_private.h"
#include "dev_se_seb.h"
#include "flcnifcmn.h"

/* ------------------------- Type Definitions ------------------------------- */
#define RSA_COMPUTE_BUFFER_SIZE   512
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief To implement "Modular Exponentiation" operation for RSA decrypt or encrypt request.
 *
 * c = (b ^ e) mod m. The input parameters are b,e and m. The function output is c.
 *
 * @param[in]   keySize                    Key Size for RSA operation
 * @param[in]   modulus                    The prime modulus for modular exponentiation
 * @param[in]   exponent                   The exponent for modular exponentiation
 * @param[in]   base                       The base for modular exponentiation
 * @param[out]  output                     The ouput buffer to save c
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS
seRsaModularExp
(
    SE_KEY_SIZE keySize, 
    LwU32 modulus[],
    LwU32 exponent[],
    LwU32 base[],    
    LwU32 output[]
)
{
    SE_STATUS   status      = SE_OK;
    SE_STATUS   mutexStatus = SE_ERROR;
    LwU32       inputSize   = 0;
    SE_PKA_REG  rsaPkaReg   = { 0 };
    LwU8        buffer[RSA_COMPUTE_BUFFER_SIZE]   GCC_ATTRIB_ALIGN(0x4);

    // Check input data
    if ((modulus == NULL)  ||
        (exponent == NULL) ||
        (base == NULL)     ||
        (output == NULL))
    {
        CHECK_SE_STATUS(SE_ERR_BAD_INPUT_DATA);
    }

    //
    // Callwlate input buffer size
    // Presently supporting for RSA1K and RSA3K only as that has been validated. 
    // For others please validate and add support for it as required.    
    // 
    switch(keySize)
    {
        case SE_KEY_SIZE_1024: 
            inputSize = SE_KEY_SIZE_1024/8;
            break;

        case SE_KEY_SIZE_3072: 
            inputSize = SE_KEY_SIZE_3072/8;
            break;

        default: 
            status = SE_ERR_NOT_SUPPORTED;
            break; 
    }

    if (status != SE_OK)
    {
        goto ErrorExit;
    }

    // 1. Acquire mutex
    mutexStatus = seMutexAcquireSEMutex();
    status = mutexStatus;
    CHECK_SE_STATUS(mutexStatus);

    // 2. Initialize context
    CHECK_SE_STATUS(seInit(keySize, &rsaPkaReg));

    // 3.  Set Modulus. Store modulus in D(0) as expected by HW. 
    seMemSet(buffer, 0, RSA_COMPUTE_BUFFER_SIZE);
    seMemCpy(buffer, modulus, inputSize);
    seSwapEndianness(buffer, buffer, inputSize);
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 0, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    //
    // 4. Compute Montgomery products: r_ilw, mp, r_sqr
    // SE engine needs other 2 pre-computed mp, r_sqr values to accomplish "Modular Exponentiation" operation.
    //
    // Calulate r ilwerse. r_ilw is output to C(0) as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW, rsaPkaReg.pkaRadixMask));
    // Callwlate modulus'. modulus' is output to D(1) as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP, rsaPkaReg.pkaRadixMask));
    // Callwlate (r^2 mod m). Result is output to D(3) as a result of HW operation.
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR, rsaPkaReg.pkaRadixMask));

    // 5. Set Exponent. Store exponent in D(2) as expected by HW.
    seMemSet(buffer, 0, RSA_COMPUTE_BUFFER_SIZE);
    seMemCpy(buffer, exponent, sizeof(LwU32));
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_D, 2, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    // 6. Set Base. Store base in A(0) as expected by HW.
    seMemSet(buffer, 0, RSA_COMPUTE_BUFFER_SIZE);
    seMemCpy(buffer, base, inputSize);
    seSwapEndianness(buffer, buffer, inputSize);
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddress(SE_PKA_BANK_A, 0, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    // 7. Callwlate Modular Exponential and read result from from A(0)
    CHECK_SE_STATUS(seStartPkaOperationAndPoll_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODEXP, rsaPkaReg.pkaRadixMask));
    CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK_A, 0, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    // 8. Copy to output buffer. 
    seMemCpy(output, buffer, inputSize);

 ErrorExit:
    // 9. Release Mutex
    CHECK_MUTEX_STATUS_AND_RELEASE(mutexStatus);
    return status;
}

/*!
 * @brief To implement "Modular Exponentiation" operation for RSA decrypt or encrypt request in HS Mode.
 *
 * c = (b ^ e) mod m. The input parameters are b,e and m. The function output is c.
 *
 * @param[in]   keySize                    Key Size for RSA operation
 * @param[in]   modulus                    The prime modulus for modular exponentiation
 * @param[in]   exponent                   The exponent for modular exponentiation
 * @param[in]   base                       The base for modular exponentiation
 * @param[out]  output                     The ouput buffer to save c
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS
seRsaModularExpHs
(
    SE_KEY_SIZE keySize, 
    LwU32 modulus[],
    LwU32 exponent[],
    LwU32 base[],    
    LwU32 output[]
)
{
    SE_STATUS   status      = SE_OK;
    SE_STATUS   mutexStatus = SE_ERROR;
    LwU32       inputSize   = 0;
    SE_PKA_REG  rsaPkaReg;
    LwU8        buffer[RSA_COMPUTE_BUFFER_SIZE]   GCC_ATTRIB_ALIGN(0x4);

    seMemSetHs(&rsaPkaReg, 0, sizeof(SE_PKA_REG));

    // Check input data
    if ((modulus == NULL)  ||
        (exponent == NULL) ||
        (base == NULL)     ||
        (output == NULL))
    {
        CHECK_SE_STATUS(SE_ERR_BAD_INPUT_DATA);
    }

    //
    // Callwlate input buffer size
    // Presently supporting for RSA1K and RSA3K only as that has been validated. 
    // For others please validate and add support for it as required.    
    // 
    switch(keySize)
    {
        case SE_KEY_SIZE_1024: 
            inputSize = SE_KEY_SIZE_1024/8;
            break;

        case SE_KEY_SIZE_3072: 
            inputSize = SE_KEY_SIZE_3072/8;
            break;

        default: 
            status = SE_ERR_NOT_SUPPORTED;
            break; 
    }

    if (status != SE_OK)
    {
        goto ErrorExit;
    }

    // 1. Acquire mutex
    mutexStatus = seMutexAcquireSEMutexHs();
    status = mutexStatus;
    CHECK_SE_STATUS(mutexStatus);

    // 2. Initialize context
    CHECK_SE_STATUS(seInitHs(keySize, &rsaPkaReg));

    // 3.  Set Modulus. Store modulus in D(0) as expected by HW. 
    seMemSetHs(buffer, 0, RSA_COMPUTE_BUFFER_SIZE);
    seMemCpyHs(buffer, modulus, inputSize);
    seSwapEndiannessHs(buffer, buffer, inputSize);
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddressHs(SE_PKA_BANK_D, 0, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    //
    // 4. Compute Montgomery products: r_ilw, mp, r_sqr
    // SE engine needs other 2 pre-computed mp, r_sqr values to accomplish "Modular Exponentiation" operation.
    //
    // Calulate r ilwerse. r_ilw is output to C(0) as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPollHs_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_ILW, rsaPkaReg.pkaRadixMask));
    // Callwlate modulus'. modulus' is output to D(1) as a result of HW operation
    CHECK_SE_STATUS(seStartPkaOperationAndPollHs_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_MP, rsaPkaReg.pkaRadixMask));
    // Callwlate (r^2 mod m). Result is output to D(3) as a result of HW operation.
    CHECK_SE_STATUS(seStartPkaOperationAndPollHs_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_CALC_R_SQR, rsaPkaReg.pkaRadixMask));

    // 5. Set Exponent. Store exponent in D(2) as expected by HW.
    seMemSetHs(buffer, 0, RSA_COMPUTE_BUFFER_SIZE);
    seMemCpyHs(buffer, exponent, sizeof(LwU32));
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddressHs(SE_PKA_BANK_D, 2, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    // 6. Set Base. Store base in A(0) as expected by HW.
    seMemSetHs(buffer, 0, RSA_COMPUTE_BUFFER_SIZE);
    seMemCpyHs(buffer, base, inputSize);
    seSwapEndiannessHs(buffer, buffer, inputSize);
    CHECK_SE_STATUS(sePkaWritePkaOperandToBankAddressHs(SE_PKA_BANK_A, 0, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    // 7. Callwlate Modular Exponential and read result from from A(0)
    CHECK_SE_STATUS(seStartPkaOperationAndPollHs_HAL(&Se, LW_SSE_SE_PKA_PROGRAM_ENTRY_ADDRESS_ALIAS_PROG_ADDR_MODEXP, rsaPkaReg.pkaRadixMask));
    CHECK_SE_STATUS(sePkaReadPkaOperandFromBankAddressHs(SE_PKA_BANK_A, 0, (LwU32 *)buffer, rsaPkaReg.pkaKeySizeDW));

    // 8. Copy to output buffer. 
    seMemCpyHs(output, buffer, inputSize);

ErrorExit:
    // 9. Release Mutex.
    CHECK_MUTEX_STATUS_AND_RELEASE_HS(mutexStatus);
    return status;
}

