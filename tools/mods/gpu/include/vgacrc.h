/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef VGACRC_H
#define VGACRC_H

#include <string>
#include <list>
#include "core/include/lwrm.h"

namespace DispTest
{
    /*
     * CRC event data structures
     */

    /* CRC parameters for each head notifier */
    struct crc_head_info {
        UINT32 head;                        // head number (0 or 1 or 2 or 3)
        string logical_head;                // head number (A/B/C/D)
        LwRm::Handle channel_handle;        // parent channel handle for notifier
        LwRm::Handle ctx_dma_handle;        // notifier handle
        UINT32 offset;                      // offset within context dma of start of notifier memory
        UINT32 status_pos;                  // position of status word relative to start of notifier memory
                                            //      or Address of the primary CRC Status Register in VGA Mode
        UINT32 bitmask;                     // bitmask to apply to status
                                            //      or Bitmask for the VALID bit in the primary CRC Status Register in VGA Mode
        UINT32 poll_value;                  // status value to poll
                                            //      or PollVAlue for the VALID field in the primary CRC Status Register in VGA Mode
        UINT32 tolerance;                   // check tolerance
        bool is_active;                     // record events from this head notifier?
                                            //      or Active indication for the primary OR in VGA Mode
        bool VgaMode;                       // HEAD is in VGA mode
        bool FcodeMode;                     // HEAD is in Fcode mode

        // Fields for VGA operation
        bool   crc_overflow;                // Overflow in primary CRC register
        string *pOr_type;                   // Type of Primary OR (to extract the fields from the CRC registers)
        UINT32 or_number;                   // Index of Primary OR (to extract the fields from the CRC registers)
        bool   sec_is_active;               // Active indication for the secondary OR in VGA Mode
        bool   sec_crc_overflow;            // Overflow in secondary CRC register
        string *pSec_or_type;               // Type of Secondary OR (to extract the fields from the CRC registers)
        UINT32 sec_or_number;               // Index of Secondary OR (to extract the fields from the CRC registers)

        // Fields for Fcode Operation
        PHYSADDR FcodeBaseAdd;              // Base Address for the CRC Notifier (Only valid in Fcode mode)
        string *pFcodeTarget;               // CRC Notifier Memory Target
        bool   ModeEnabled;                 // Flag to specify that Crc Consistency checking is enabled
        list <string *> *pHeadModeList;     // List holding the raster modes that the test enqueues for each update
        bool IsFirstSnooze;                 // Flag to track whether the current frame is the first Snooze frame in a Mode Switch

    };

    /* head event */
    struct crc_head_event {
        struct crc_head_info *p_info;       // head associated with event
        UINT32 tag;                         // tag
        UINT32 time;                        // time of event
        bool   timestamp_mode;              // notifier TimestampMode field
        bool   hw_fliplock_mode;            // notifier hardware fliplock mode field
        bool   present_interval_met;        // notifier present interval met field
        UINT32 compositor_crc;              // notifier CompositorCrc field
        UINT32 primary_output_crc;          // notifier PrimaryOutputCrc field
        UINT32 secondary_output_crc;        // notifier SecondaryOutputCrc field
        string *EventMode;                  // Content Mode for the current frame
    };
}

#endif /* VGACRC_H */
