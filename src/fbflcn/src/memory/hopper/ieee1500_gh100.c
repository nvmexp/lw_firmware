/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>
#include <lwmisc.h>

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"
#include "dev_pmgr.h"

// project headers
#include "priv.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "fbflcn_mutex.h"
#include "memory.h"

#include "fbflcn_objfbflcn.h"

#include "lwuproc.h"


extern HBM_BCAST_STRUCT hbm_bcast_data[MAX_SITES];
extern HBM_DEVICE_ID_STRUCT hbm_device_id;

#define IEEE1500_MUTEX_REQ_TIMEOUT_NS  1000000

LW_STATUS
memoryReadI1500DeviceId_GH100
(
        void
)
{
    if (localEnabledFBPAMask == 0)
    {
        return FBFLCN_ERROR_CODE_0FB_SETUP_ERROR;
    }
    LwU32 pfb_fbpa_fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);
    LwU32 ddr_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, pfb_fbpa_fbio_broadcast);
    if (ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3)
    {
        // seems to be fine to just return on HBM2 without raising an issue
        return LW_OK;
    }

    // mutex setup
    LwU8 token = 0;
    MUTEX_HANDLER ieee1500Mutex;
    ieee1500Mutex.registerMutex = IEEE1500_MUTEX_REG(IEEE1500_MUTEX_INDEX);
    ieee1500Mutex.registerIdRelease = IEEE1500_MUTEX_ID_RELEASE;
    ieee1500Mutex.registerIdAcquire = IEEE1500_MUTEX_ID_ACQUIRE;
    ieee1500Mutex.valueInitialLock = IEEE1500_MUTEX_REG_INITIAL_LOCK;
    ieee1500Mutex.valueAcquireInit = IEEE1500_MUTEX_ID_ACQUIRE_VALUE_INIT;
    ieee1500Mutex.valueAcquireNotAvailable = IEEE1500_MUTEX_ID_ACQUIRE_VALUE_NOT_AVAIL;
    LW_STATUS status = mutexAcquireByIndex_GV100(&ieee1500Mutex, IEEE1500_MUTEX_REQ_TIMEOUT_NS, &token);

    if (status != LW_OK)
    {
        FW_MBOX_WR32(7, status);
        FW_MBOX_WR32(8, IEEE1500_MUTEX_REQ_TIMEOUT_NS);
        status = mutexReleaseByIndex_GV100(&ieee1500Mutex, token);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_MUTEX_NOT_OBTAINED);
    }

    // WDR Length for DEVICE_ID on HBM3 is 160
    const LwU32 wdrLength = 160;

    hbm_device_id.hynix_es1_workaround = LW_FALSE;
    LwU32 hynix_es1_release = 0x0;
    LwU32 hynix_es2_release = 0x0;

    LwU8 site;
    for (site = 0; site < MAX_SITES; site++)
    {
        if (hbm_bcast_data[site].enabled == LW_FALSE)
        {
            continue;
        }
        LwU8 paIndex;
        for (paIndex=0; paIndex < hbm_bcast_data[site].numValidPa; paIndex++)
        {
            LwU8 partition;
            partition = hbm_bcast_data[site].pa[paIndex];

            // read the device id information for this partition
            // send instruction for DEVICE_ID readout to all channels
            LwU32 instr = ((hbm_bcast_data[site].valid_channel & 0xF) << 8) |  IEEE1500_INSTRUCTION_DEVICE_ID;
            REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_INSTR, partition), instr);
            REG_WR32_STALL(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_MODE, partition), wdrLength);

            // read 1st dwords of data, parse fields
            LwU32 data1 = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA,partition));

            // read 2nd dwords of data, parse fields
            /* LwU32 data2 = */
            REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA,partition));

            // read 3rd dwords of data, parse fields
            /* LwU32 data3 = */
            REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA,partition));

            // read 4th dwords of data, parse fields
            LwU32 data4 = REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA,partition));

            // read 5th dwords of data, parse fields
            /* LwU32 data5 = */
            REG_RD32(BAR0, unicast(LW_PFB_FBPA_FBIO_HBM_TEST_I1500_DATA,partition));

            // populate hbm_device_id
            //hbm_device_id.channelAvailable = (data1 & DRF_SHIFTMASK(DEVICE_ID_HBM3_CHANNEL_AVAILABLE_FIELD))
            //                                             >> DRF_SHIFT(DEVICE_ID_HBM3_CHANNEL_AVAILABLE_FIELD);
            hbm_device_id.partNumber = (data1 & DRF_SHIFTMASK(DEVICE_ID_HBM3_MODEL_PART_NUMBER_FIELD))
                                                                     >> DRF_SHIFT(DEVICE_ID_HBM3_MODEL_PART_NUMBER_FIELD);
            hbm_device_id.manufacturer = (data1 & DRF_SHIFTMASK(DEVICE_ID_HBM3_MANUFACTURER_ID_FIELD))
                                                                       >> DRF_SHIFT(DEVICE_ID_HBM3_MANUFACTURER_ID_FIELD);
            hbm_device_id.density = (data1 & DRF_SHIFTMASK(DEVICE_ID_HBM3_DENSITY_FIELD))
                                                                     >> DRF_SHIFT(DEVICE_ID_HBM3_DENSITY_FIELD);
            //hbm_device_id.serialLow = data2;
            //hbm_device_id.serialHi = data3;

            hbm_device_id.manufacturingWeek = (data4 & DRF_SHIFTMASK(DEVICE_ID_HBM3_MANUFACTURING_WEEK))
                                                                    >> DRF_SHIFT(DEVICE_ID_HBM3_MANUFACTURING_WEEK);
            hbm_device_id.manufacturingYear = (data4 & DRF_SHIFTMASK(DEVICE_ID_HBM3_MANUFACTURING_YEAR))
                                                                    >> DRF_SHIFT(DEVICE_ID_HBM3_MANUFACTURING_YEAR);

            // Bug: 3516083, a bug in hynix hbm3 ES1 sample silicon returns wrong oscillator values, to correct this
            // we detect ES1 through the manufacturing info.  ES1 was produced up to (and including) week 43 of 2021.
            if (hbm_device_id.manufacturer == MEMINFO_E_HBM_VENDID_HYNIX) {
                switch (hbm_device_id.manufacturingYear)
                {
                case MEMINFO_E_HBM_VENDID_MANUFACTURING_YEAR_2021:
                    if (hbm_device_id.manufacturingWeek <= 43)
                    {
                        hynix_es1_release |= (0x1 << partition);
                    }
                    else
                    {
                        hynix_es2_release |= (0x1 << partition);
                    }
                    break;
                default:
                    hynix_es2_release |= (0x1 << partition);
                }
            }
        }

    }

    // release mutex
    status = mutexReleaseByIndex_GV100(&ieee1500Mutex, token);
    if(status != LW_OK) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_I1500_GPIO_MUTEX_NOT_RELEASED);
    }

    hbm_device_id.hynix_es1_workaround = (hynix_es1_release > 0);

    if ((hynix_es1_release > 0) && (hynix_es2_release >0)) {
        FW_MBOX_WR32_STALL(8, hbm_device_id.hynix_es1_workaround );
        FW_MBOX_WR32_STALL(9, hbm_device_id.manufacturer );
        FW_MBOX_WR32_STALL(10, hbm_device_id.manufacturingWeek );
        FW_MBOX_WR32_STALL(11, hbm_device_id.manufacturingYear );
        FW_MBOX_WR32_STALL(12, hynix_es1_release );
        FW_MBOX_WR32_STALL(13, hynix_es2_release );
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HYNIX_MIXED_ES1_AND_ES2);
    }

    return LW_OK;
}


LW_STATUS
memoryDecodeI1500Strap_GH100
(
        void
)
{
    MemInfoHeader* mIHp = gBiosTable.mIHp;
    LwU8 mitHeaderSize = mIHp->HeaderSize;
    LwU8 mitEntrySize = mIHp->EntrySize;
    LwU8 mitEntryCount = mIHp->EntryCount;
    LwU32 memInfoTableAdr = (LwU32)mIHp + mitHeaderSize;
    LwU16 i;
    // detect the first matching entry and pick its strap
    gBiosTable.strap = 0;
    for (i=0; i<mitEntryCount; i++) {
        MemInfoEntry* mIEp = (MemInfoEntry*) memInfoTableAdr;

        FW_MBOX_WR32_STALL(8, i);
        FW_MBOX_WR32_STALL(9, mIEp->mie1500.MIEType);
        FW_MBOX_WR32_STALL(10, mIEp->mie1500.MIEVendorID);
        FW_MBOX_WR32_STALL(11, mIEp->mie3116.MIEDensity);
        FW_MBOX_WR32_STALL(12, TABLE_VAL(mIEp->mie1500));
        FW_MBOX_WR32_STALL(13, TABLE_VAL(mIEp->mie3116));
        if (mIEp->mie1500.MIEType == MEMINFO_E_TYPE_HBM3)
        {
            if (mIEp->mie1500.MIEVendorID == hbm_device_id.manufacturer)
            {
                if (mIEp->mie3116.MIEDensity == hbm_device_id.density)
                {
                    gBiosTable.strap  = mIEp->mie1500.MIEStrap;
                    return LW_OK;
                }
            }
        }
        // jump to start of next entry
        memInfoTableAdr += mitEntrySize;
    }

    // error code from:  /chips_a/sw/dev/gpu_drv/chips_a/apps/lwbucket/include/lwbucket.h
    return LW_ERR_MISSING_TABLE_ENTRY;
}

