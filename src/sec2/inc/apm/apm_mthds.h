/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef APM_MTHDS_H
#define APM_MTHDS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief The SEC2 APM short-hand notation for the LW95A1 APM class method.
 *
 * @param[in]   mthd    SEC2 APM short-hand notation.
 *
 * @return  LW95A1 APM class method.
 */
#define APM_METHOD_ID(mthd) LW95A1_APM_##mthd

/*!
 * @brief Colwerts an index in appMthdArray into the LW95A1 APM method value.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  LW95A1 APM class method.
 */
#define APM_GET_METHOD_ID(idx) \
    (APM_METHOD_ID(GET_ACK) + ((idx) * 4))

/*!
 * @brief Obtains a pointer to the method's parameter which is stored within the
 *        frame buffer.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define APM_GET_METHOD_PARAM_OFFSET(idx) appMthdArray[(idx)]

/*!
 * @brief Colwerts method ID to index used for appMthdArray
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */

#define APM_GET_SEC2_METHOD_ID(mthdId)                                        \
         (((APM_METHOD_ID(mthdId)) - APM_METHOD_ID(GET_ACK)) / 4)

/*!
 * @brief Obtains Method parameter using METHOD ID
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define APM_GET_METHOD_PARAM_OFFSET_FROM_ID(mthdId)                           \
         appMthdArray[APM_GET_SEC2_METHOD_ID(mthdId)]

 /*!
 * @brief Colwerts an LW95A1 APM method value into an index into appMthdArray.
 *
 * @param[in]  mthd     LW95A1 APM method value.
 *
 * @return  Index within appMthdArray.
 */
#define APM_GET_METHOD_INDEX(mthd)                                            \
        (((mthd) - APM_METHOD_ID(GET_ACK)) >> 2)

 /*!
 * @brief Specifies if the specified LW95A1 APM method is set in
 *        appMthdValidMask.
 *
 * @param[in]  mthd     LW95A1 APM method valie.
 *
 * @return Non-zero value if the method is valid; 0 if the method is not valid.
 */
#define APM_IS_METHOD_VALID(mthd)                                             \
    (appMthdValidMask.mthdGrp[APM_GET_METHOD_INDEX(mthd) / 32] &              \
    BIT(APM_GET_METHOD_INDEX(mthd) % 32))


typedef struct
{
    LwU32  apmCopyType;
    LwU32  apmCopySrcLo;
    LwU32  apmCopySrcHi;
    LwU32  apmCopyDstLo;
    LwU32  apmCopyDstHi;
    LwU32  apmCopySizeBytes;
    LwU32  apmDigestAddrLo;
    LwU32  apmDigestAddrHi;
    LwU32  apmEncryptIVAddrLo;
    LwU32  apmEncryptIVAddrHi;
    LwBool bApmEncryptIVSent;
} APM_COPY_DATA;

/* ------------------------- Prototypes ------------------------------------- */
LwU32 apmInitiateCopy(void);


#endif // APM_MTHDS_H
