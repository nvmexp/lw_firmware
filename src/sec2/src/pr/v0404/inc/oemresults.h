/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMRESULTS_H__
#define __OEMRESULTS_H__

/*
** This file is provided for you to add your own error codes which you
** can then return from your OEM functions.
**
** This file is included in drmresults.h where common error codes
** for the PlayReady PK are defined.
**
** The error code range 0x8004DC80 to 0x8004DDFF is reserved for
** your OEM-defined error codes.  Make sure to define your error
** codes within this range to ensure that they are handled properly
** both by the PlayReady PK and by higher-level components in the stack.
**
** This range is large enough to provide you with more than 380 unique
** error codes.  Microsoft recommends that you use a unique error code
** for every location where you use your own error code (except where
** you are intentionally hiding information from hostile actors).
**
** Doing so will lower your troubleshooting and customer support costs
** by enabling you to precisely pinpoint where an error is coming from.
**
** Here are examples of defining your own codes.
** Again, note that they are in the range 0x8004DC80 to 0x8004DDFF.
**
** #define OEM_E_MY_FIRST_ERROR   ((DRM_RESULT)0x8004DC80L)
** #define OEM_E_MY_SECOND_ERROR  ((DRM_RESULT)0x8004DC81L)
** ...
** #define OEM_E_MY_LAST_ERROR    ((DRM_RESULT)0x8004DDFFL)
*/

/*
** Reminder: do not define error codes outside the range
** OEM_E_MINIMUM_ERROR_CODE to OEM_E_MAXIMUM_ERROR_CODE
*/
#define OEM_E_MINIMUM_ERROR_CODE  ((DRM_RESULT)0x8004DC80L)
#define OEM_E_MAXIMUM_ERROR_CODE  ((DRM_RESULT)0x8004DDFFL)


/*
 * MessageId: DRM_E_MUTEX_ACQUIRE_FAILED
 *
 * MessageText:
 *
 * The mutex acquire for critical section failed.
 *
 */
#define DRM_E_MUTEX_ACQUIRE_FAILED                    ((DRM_RESULT)0x8004DD00L)

/*
 * MessageId: DRM_E_MUTEX_LEAVE_FAILED
 *
 * MessageText:
 *
 * The mutex leave for critical section failed.
 *
 */
#define DRM_E_MUTEX_LEAVE_FAILED                      ((DRM_RESULT)0x8004DD01L)

/*
 * MessageId: DRM_E_OEM_DPU_IS_BUSY
 *
 * MessageText:
 *
 * The DPU is still busy handling the previous request.
 *
 */
#define DRM_E_OEM_DPU_IS_BUSY                         ((DRM_RESULT)0x8004DD02L)

/*
 * MessageId: DRM_E_OEM_NOTIFY_DPU_TIMEOUT_FOR_HDCP_TYPE1_LOCK_REQUEST
 *
 * MessageText:
 *
 * The DPU can not finish the command handling from SEC2 before timeout.
 *
 */
#define DRM_E_OEM_DPU_TIMEOUT_FOR_HDCP_TYPE1_LOCK_REQUEST ((DRM_RESULT)0x8004DD03L)

/*
 * MessageId: DRM_E_OEM_HDCP_TYPE1_LOCK_FAILED
 *
 * MessageText:
 *
 * The DPU failed to lock HDCP2.2 type as requested by SEC2.
 *
 */
#define DRM_E_OEM_HDCP_TYPE1_LOCK_FAILED              ((DRM_RESULT)0x8004DD04L)

/*
 * MessageId: DRM_E_OEM_HDCP_TYPE1_LOCK_UNKNOWN
 *
 * MessageText:
 *
 * The HDCP type1 lock RESPONSE value set by DPU is unknown.
 *
 */
#define DRM_E_OEM_HDCP_TYPE1_LOCK_UNKNOWN             ((DRM_RESULT)0x8004DD05L)

/*
 * MessageId: DRM_E_OEM_BAR0_PRIV_WRITE_ERROR
 *
 * MessageText:
 *
 * The register could not be written due to PRI failure.
 *
 */
#define DRM_E_OEM_BAR0_PRIV_WRITE_ERROR               ((DRM_RESULT)0x8004DD06L)

/*
 * MessageId: DRM_E_OEM_BAR0_PRIV_READ_ERROR
 *
 * MessageText:
 *
 * The register could not be read due to PRI failure.
 *
 */
#define DRM_E_OEM_BAR0_PRIV_READ_ERROR                ((DRM_RESULT)0x8004DD07L)

/*
 * MessageId: DRM_E_OEM_DMA_FAILURE
 *
 * MessageText:
 *
 * The DMA transaction failure.
 *
 */
#define DRM_E_OEM_DMA_FAILURE                         ((DRM_RESULT)0x8004DD08L)

/*
 * MessageId: DRM_E_OEM_UNSUPPORTED_HS_ACTION
 *
 * MessageText:
 *
 * The requested HS action is not supported.
 *
 */
#define DRM_E_OEM_UNSUPPORTED_HS_ACTION               ((DRM_RESULT)0x8004DD09L)

/*
 * MessageId: DRM_E_OEM_UNSUPPORTED_KDF
 *
 * MessageText:
 *
 * The KDF is not supported in OEM's encryption/decryption implementation.
 *
 */
#define DRM_E_OEM_UNSUPPORTED_KDF                     ((DRM_RESULT)0x8004DD0AL)

/*
 * MessageId: DRM_E_OEM_ILWALID_AES_CRYPTO_MODE
 *
 * MessageText:
 *
 * The requested AES crypto mode is invalid.
 *
 */
#define DRM_E_OEM_ILWALID_AES_CRYPTO_MODE             ((DRM_RESULT)0x8004DD0BL)

/*
 * MessageId: DRM_E_OEM_HS_CHK_ILWALID_INPUT
 *
 * MessageText:
 *
 * One or more of the input arguments to HS mode are invalid.
 *
 */
#define DRM_E_OEM_HS_CHK_ILWALID_INPUT                ((DRM_RESULT)0x8004DD0CL)

/*
 * MessageId: DRM_E_OEM_HS_CHK_CHIP_NOT_SUPPORTED
 *
 * MessageText:
 *
 * The GPU does not support playready.
 *
 */
#define DRM_E_OEM_HS_CHK_CHIP_NOT_SUPPORTED           ((DRM_RESULT)0x8004DD0DL)

/*
 * MessageId: DRM_E_OEM_HS_CHK_UCODE_REVOKED
 *
 * MessageText:
 *
 * Current ucode was revoked.
 *
 */
#define DRM_E_OEM_HS_CHK_UCODE_REVOKED                ((DRM_RESULT)0x8004DD0EL)

/*
 * MessageId: DRM_E_OEM_HS_CHK_NOT_IN_LSMODE
 *
 * MessageText:
 *
 * Relevant falcon (depending upon the context from which this error code is being returned) is not in LS mode.
 *
 */
#define DRM_E_OEM_HS_CHK_NOT_IN_LSMODE                ((DRM_RESULT)0x8004DD0FL)

/*
 * MessageId: DRM_E_OEM_HS_CHK_ILWALID_LS_PRIV_LEVEL
 *
 * MessageText:
 *
 * Relevant falcon (depending upon the context from which this error code is being returned) is not at proper LS priv level.
 *
 */
#define DRM_E_OEM_HS_CHK_ILWALID_LS_PRIV_LEVEL        ((DRM_RESULT)0x8004DD10L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_ILWALID_REGIONCFG
 *
 * MessageText:
 *
 * The REGIONCFG is not set to correct WPR region.
 *
 */
#define DRM_E_OEM_HS_CHK_ILWALID_REGIONCFG            ((DRM_RESULT)0x8004DD11L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_PRIV_SEC_DISABLED_ON_PROD
 *
 * MessageText:
 *
 * The priv sec is unexpectedly disabled on production board.
 *
 */
#define DRM_E_OEM_HS_CHK_PRIV_SEC_DISABLED_ON_PROD    ((DRM_RESULT)0x8004DD12L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_SW_FUSING_ALLOWED_ON_PROD
 *
 * MessageText:
 *
 * The SW fusing is unexpectedly allowed on production board.
 *
 */
#define DRM_E_OEM_HS_CHK_SW_FUSING_ALLOWED_ON_PROD    ((DRM_RESULT)0x8004DD13L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_INTERNAL_SKU_ON_PROD
 *
 * MessageText:
 *
 * The SKU is internal on production board.
 *
 */
#define DRM_E_OEM_HS_CHK_INTERNAL_SKU_ON_PROD         ((DRM_RESULT)0x8004DD14L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_DEVID_OVERRIDE_ENABLED_ON_PROD
 *
 * MessageText:
 *
 * The devid override is enabled on production board.
 *
 */
#define DRM_E_OEM_HS_CHK_DEVID_OVERRIDE_ENABLED_ON_PROD ((DRM_RESULT)0x8004DD15L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_INCONSISTENT_PROD_MODES
 *
 * MessageText:
 *
 * The falcons in GPU are not in consistent debug/production mode.
 *  
 */
#define DRM_E_OEM_HS_CHK_INCONSISTENT_PROD_MODES      ((DRM_RESULT)0x8004DD16L)

/*
 * MessageId: DRM_E_OEM_HS_CHK_HUB_ENCRPTION_DISABLED
 *
 * MessageText:
 *
 * The hub encryption is not enabled on FB.
 *
 */
#define DRM_E_OEM_HS_CHK_HUB_ENCRPTION_DISABLED       ((DRM_RESULT)0x8004DD17L)

/*
 * MessageId: DRM_E_OEM_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_ENTRY
 *
 * MessageText:
 *
 * LASSAHS is not in a correct state before doing HS entry
 *
 */
#define DRM_E_OEM_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_ENTRY ((DRM_RESULT)0x8004DD18L)

/*
 * MessageId: DRM_E_OEM_HS_PR_ILLEGAL_LASSAHS_STATE_AT_MPK_DECRYPT
 *
 * MessageText:
 *
 * LASSAHS is not in a correct state before doing MPK decryption
 *
 */
#define DRM_E_OEM_HS_PR_ILLEGAL_LASSAHS_STATE_AT_MPK_DECRYPT ((DRM_RESULT)0x8004DD19L)

/*
 * MessageId: DRM_E_OEM_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_EXIT
 *
 * MessageText:
 *
 * LASSAHS is not in a correct state before doing HS exit
 *
 */
#define DRM_E_OEM_HS_PR_ILLEGAL_LASSAHS_STATE_AT_HS_EXIT ((DRM_RESULT)0x8004DD1AL)

/*
 * MessageId: DRM_E_OEM_PREENTRY_GDK_OUT_OF_MEMORY
 *
 * MessageText:
 *
 * Preallocation for GDK failed
 *
 */
#define DRM_E_OEM_PREENTRY_GDK_OUT_OF_MEMORY          ((DRM_RESULT)0x8004DD1BL)

/*
 * MessageId: DRM_E_OEM_GDK_IMEM_BLOCK_REVALIDATION_FAILED
 *
 * MessageText:
 *
 * Failed to revalidate imem blocks ilwalidated during LASSAHS
 *
 */
#define DRM_E_OEM_GDK_IMEM_BLOCK_REVALIDATION_FAILED  ((DRM_RESULT)0x8004DD1CL)

/*
 * MessageId: DRM_E_OEM_ILWALID_PDI
 *
 * MessageText:
 *
 * The read PDI value is invalid
 *
 */
#define DRM_E_OEM_ILWALID_PDI                         ((DRM_RESULT)0x8004DD1DL)

/*
 * MessageId: DRM_E_OEM_ILWALID_SIZE_OF_CDKBDATA
 *
 * MessageText:
 *
 * The size of LW_KB_CDKBData is not multiple of 16
 *
 */
#define DRM_E_OEM_ILWALID_SIZE_OF_CDKBDATA            ((DRM_RESULT)0x8004DD1EL)

/*
 * MessageId: DRM_E_SE_CRYPTO_MUTEX_ACQUIRE_FAILED
 *
 * MessageText:
 *
 * Failed to acquire the Security Engine mutex for cypto
 * operations
 *
 */
#define DRM_E_SE_CRYPTO_MUTEX_ACQUIRE_FAILED          ((DRM_RESULT)0x8004DD1FL)

/*
 * MessageId: DRM_E_SE_CRYPTO_MUTEX_RELEASE_FAILED
 *
 * MessageText:
 *
 * Failed to release the Security Engine mutex for cypto
 * operations
 *
 */
#define DRM_E_SE_CRYPTO_MUTEX_RELEASE_FAILED          ((DRM_RESULT)0x8004DD20L)

/*
 * MessageId: DRM_E_SE_CRYPTO_POINT_NOT_ON_LWRVE
 *
 * MessageText:
 *
 * Point is not on the elliptic lwrve
 *
 */
#define DRM_E_SE_CRYPTO_POINT_NOT_ON_LWRVE            ((DRM_RESULT)0x8004DD21L)

/*
 * MessageId: DRM_E_OEM_WAIT_FOR_BAR0_IDLE_FAILED
 *
 * MessageText:
 *
 * WFI on a PRI read/write failed
 *
 */
#define DRM_E_OEM_WAIT_FOR_BAR0_IDLE_FAILED           ((DRM_RESULT)0x8004DD22L)

/*
 * MessageId: DRM_E_OEM_ERROR_PRI_UNEXPECTED_FAILURE
 *
 * MessageText:
 *
 * Unexpected PRI error -  This likely means we have added errors and not updated
 * DRM errors.
 *
 */
#define DRM_E_OEM_ERROR_PRI_UNEXPECTED_FAILURE        ((DRM_RESULT)0x8004DD23L)

/*
 * MessageId: DRM_E_OEM_CSB_PRIV_WRITE_ERROR
 *
 * MessageText:
 *
 * The register could not be written due to CSB PRI failure.
 *
 */
#define DRM_E_OEM_CSB_PRIV_WRITE_ERROR                ((DRM_RESULT)0x8004DD24L)

/*
 * MessageId: DRM_E_OEM_CSB_PRIV_READ_ERROR
 *
 * MessageText:
 *
 * The register could not be read due to CSB PRI failure.
 *
 */
#define DRM_E_OEM_CSB_PRIV_READ_ERROR                 ((DRM_RESULT)0x8004DD25L)

/*
 * MessageId: DRM_E_OEM_HS_MUTEX_ACQUIRE_FAILED
 *
 * MessageText:
 *
 * The mutex acquire for critical section at HS mode failed.
 *
 */
#define DRM_E_OEM_HS_MUTEX_ACQUIRE_FAILED             ((DRM_RESULT)0x8004DD26L)

/*
 * MessageId: DRM_E_OEM_HS_MUTEX_RELEASE_FAILED
 *
 * MessageText:
 *
 * The mutex leave for critical section at HS mode failed.
 *
 */
#define DRM_E_OEM_HS_MUTEX_RELEASE_FAILED             ((DRM_RESULT)0x8004DD27L)

/*
 * MessageId: DRM_E_OEM_FLCN_STATUS_TO_DRM_STATUS_TRANSLATION_FAILED
 *
 * MessageText:
 *
 * Failed to translate the FLCN_STATUS code to a proper DRM_STATUS code
 *
 */
#define DRM_E_OEM_FLCN_STATUS_TO_DRM_STATUS_TRANSLATION_FAILED ((DRM_RESULT)0x8004DC80)

#endif   /* __OEMRESULTS_H__ */

