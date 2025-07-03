/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_HOSTIF_H
#define SOE_HOSTIF_H

/*!
 * Array to store semaphore method data
 */
typedef struct
{
    struct
    {
        /*!
         * Semaphore payload
         */
        LwU32            payload;

        /*!
         * Reserved dword (used for long report type). This matches the struct
         * used by clients, so we can DMA this structure directly.
         */
        LwU32            reserved;

        /*!
         * Ptimer timestamp (used for long report type). 
         */
        FLCN_TIMESTAMP   time;
    } longPayload;

    /*!
     * Memory descriptor for semaphore
     */
    RM_FLCN_MEM_DESC semDesc;

    /*!
     * Bitmask to hold the valid semaphore input methods received. The valid 
     * bits to set here are described below the structure definition.
     */
    LwU8             inputBitMask;

    /*!
     * Notify at the end of exelwting app methods
     */
    LwBool           bNotifyOnEnd;

    /*!
     * Send an intr when releasing semaphore (when writing payload)
     */
    LwBool           bAwaken;
} SOE_SEMAPHORE_MTHD_ARRAY;

/*!
 * Defines the valid semaphore input events for inputBitMask
 */
#define SOE_SEMAPHORE_PAYLOAD_RECEIVED 0x0
#define SOE_SEMAPHORE_A_RECEIVED       0x1
#define SOE_SEMAPHORE_B_RECEIVED       0x2

/*!
 * Bitmask that indicates semaphore input is valid and payload can be written
 */
#define SOE_SEMAPHORE_INPUT_VALID (BIT(SOE_SEMAPHORE_PAYLOAD_RECEIVED) | \
                                    BIT(SOE_SEMAPHORE_A_RECEIVED)       | \
                                    BIT(SOE_SEMAPHORE_B_RECEIVED))

/*!
 * Colwience macro to get method ID stored in method FIFO from the method tag
 * in the class header files
 */
#define SOE_METHOD_TO_METHOD_ID(x) (x >> 2)

/*!
 * Colwience macro to get method tag (matching class file) from method ID
 */
#define SOE_METHOD_ID_TO_METHOD(x) (x << 2)

#endif // SOE_HOSTIF_H
