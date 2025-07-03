/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_obji2c.c
 * @brief  Container-object for PMU I2C routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_i2capi.h"
#include "pmu_objhal.h"
#include "pmu_obji2c.h"
#include "task_i2c.h"
#include "flcnifi2c.h"

#include "config/g_i2c_private.h"
#if(I2C_MOCK_FUNCTIONS_GENERATED)
#include "config/g_i2c_mock.c"
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJI2C I2c;

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Timeout in microseconds waiting for the server queue to be ready.
 */
#define PMU_I2C_SEND_TIMEOUT 1000

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the I2C object.  This includes the HAL interface as well as all
 * software objects (ex. semaphores) used by the I2C module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructI2c(void)
{
    return FLCN_OK;
}

/*!
 *  @brief Perform an I2C transaction on behalf of the client.  Supports both
 *         reads and writes, depending on the flags specified.
 *
 *  @param[in]      portId
 *      Identifier of the bus on which to operate.
 *
 *  @param[in]      flags
 *      Bitfield containing index length, speed, address mode, and direction of
 *      the transaction.  See @ref pmu_i2capi.h for the define information and
 *      and explanation of the correct flag usage.
 *
 *  @param[in]      address
 *      Secondary address of this transaction.  Interpretation depends on the
 *      address mode flag, which is describe in detail below.  The address
 *      should be justified such that the least significant bit is always 0.
 *      In other words, address 1101001b would be sent as 0000000011010010b,
 *      and 101100110b would be sent as 00001011001100b.
 *
 *  @param[in]      index
 *      The parameter "index" is also referred to as subaddress or register in
 *      other parts of the codebase and documentation; none are official terms
 *      in the I2C specification.  The index is written to the device prior to
 *      the main data transfer and typically tells the secondary device what to
 *      read or write.  The index is a prelude written to the device prior to
 *      the message either being read or written.  The transaction will be in
 *      combined format if the transaction is a read, as the direction bit
 *      needs to change between the index write and the message read.  If the
 *      command is a write, then operation depends on the WRITE_RESTART flag,
 *      described above. </br>
 *      I2C is a byte format, but for simplicity the index value is 32 bits.
 *      The format of the index is that the first byte to send will be in the
 *      lowest 8 bits of the index.  The second byte is the next 8 bits, and so
 *      on.  For example, a two byte index index param passed in as 0x0000F04D
 *      will be transferred over I2C in this chronological order: 4D F0.
 *      Any values that exceed the index length flag will be ignored.
 *
 *  @param[in]      size
 *      The size of the message in bytes.
 *
 *  @param[in,out]  pMessage
 *      The message buffer.  For reads, this is an output, and for writes this
 *      is an input.
 *
 *  @param[in,out]  hQueue
 *      Queue in which the I2C task will place the resulting message upon
 *      completion.  The client should check the generic errorCode field in
 *      the resulting message to make sure the transaction succeeded.  It is
 *      the responsibility of the client to receive the message.
 *
 *  @return An error code as described by FLCN_ERR_I2C_<xyz>. See @ref flcnretval.h.
 *
 *  This function will request that the PMU I2C task perform an I2C transaction
 *  and then wait for a result.  This function will fail with a timeout if I2C
 *  transactions are not lwrrently being processed, or the transaction exceeds
 *  the IPC time limits.
 *
 *  @section Preconditions
 *
 *  The referenced arguments, pMessage and hQueue, must both refer to valid
 *  memory containing valid structures.  There is no guarantee that this code
 *  or the PMU I2C task can or will test these parameters for validity.
 *  Furthermore, the pMessage must not be changed by the caller via other
 *  threads or interrupt routines during the operation of this function.
 *
 *  The parameter hQueue must have enough space per item to house the
 *  RM_PMU_MESSAGE struct, and should have one empty slot for the I2C task's
 *  message.  All other queue details are left to the client.
 *
 *  @section Postconditions
 *
 *  If the result was FLCN_OK, then the result will be placed in
 *  hQueue upon completion if available space exists.  The parameter pMessage
 *  will at that time either have been written or contain the read message as
 *  appropriate given the error code indicated and the direction flags sent.
 *
 *  @section Use Case
 *
 *  extern const LwU8 hdcPort;
 *  const LwU8 hdcpI2cFlags = I2C_ADDR_7BIT | I2C_SPEED_100KHZ |
 *                            I2C_INDEX_SIZE(1);
 *
 *  LwU8 KSV[5];
 *
 *  (void)i2c(hdcPort, I2C_READ | hdcpI2cFlags, 0x74, 0x00, 5, KSV, hQueue);
 *  // Wait to finish.  Don't ignore message or return value in real code.
 *  (void)xQueueReceive(hQueue, &message, TIMEOUT);
 *
 *  for (i = 0; i < 5; ++i)
 *      printf(KSV[%d]: %02x\n, i, KSV[i]);
 */
FLCN_STATUS
i2c
(
    LwU8                portId,
    LwU32               flags,
    LwU16               address,
    LwU32               index,
    LwU16               size,
    LwU8               *pMessage,
    LwrtosQueueHandle   hQueue
)
{
    I2C_COMMAND         command;    // Command to submit to the server.
    LwU32               i;

    // These variables are used as shorthand.
    PMU_I2C_CMD            *pI2cCmd;
    RM_PMU_I2C_CMD_GENERIC *pGenCmd;

    // Generate shorthand variables.
    pI2cCmd = &command.internal.cmd;
    pGenCmd = &pI2cCmd->generic;

    // Decode arguments into the I2C command structure.
    pGenCmd->i2cAddress = address;

    pGenCmd->flags = FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _START, _SEND, flags);
    pGenCmd->flags =
        FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _STOP, _SEND, pGenCmd->flags);
    pGenCmd->flags =
        FLD_SET_DRF_NUM(_RM_FLCN, _I2C_FLAGS, _PORT, portId, pGenCmd->flags);

    for (i = 0; i < DRF_VAL(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, flags); ++i)
    {
        pGenCmd->index[i] = index >> (i * 8);
    }

    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _DIR, _READ, flags))
    {
        command.hdr.eventType = I2C_EVENT_TYPE_INT_RX;

        pGenCmd->flags =
            FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _INDEX_LENGTH, _ZERO, flags) ?
            FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _RESTART, _NONE, pGenCmd->flags) :
            FLD_SET_DRF(_RM_FLCN, _I2C_FLAGS, _RESTART, _SEND, pGenCmd->flags);

        pI2cCmd->rxQueue.segment        = 0;
        pI2cCmd->rxQueue.genCmd.bufSize = size;
        pI2cCmd->rxQueue.pBuf           = pMessage;
    }
    else
    {
        command.hdr.eventType = I2C_EVENT_TYPE_INT_TX;

        pI2cCmd->txQueue.genCmd.bufSize = size;
        pI2cCmd->txQueue.pBuf           = pMessage;
    }

    command.internal.hQueue = hQueue;

    // Submit the command to the server via IPC.
    return lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, I2C), &command,
                             sizeof(command), PMU_I2C_SEND_TIMEOUT);
}
