/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <list>
#include <map>
#include <set>
#include <stdio.h>

#include "core/include/types.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tee.h"

#include "vgacore.h"
#include "core/include/disp_test.h"
#include "crccheck.h"

#include "lw50/dev_disp.h"

#include "lwmisc.h"

#define line_size 256
#define dir_separator '/'

string LegacyVGA::CRCProfile::delims = "_";

LegacyVGA::CRCProfile::CRCProfile(UINT32 chipid, bool active_raster, bool full_frame, UINT32 frame) :
            m_chipid(chipid), m_active_raster(active_raster),
            m_full_frame(full_frame), m_frame(frame)
{
}

LegacyVGA::CRCProfile::CRCProfile(bool active_raster, bool full_frame, UINT32 frame) :
            m_chipid(0), m_active_raster(active_raster),
            m_full_frame(full_frame), m_frame(frame)
{
    m_chipid = DispTest::GetBoundGpuSubdevice()->DeviceId();
}
//colwersion constructor
LegacyVGA::CRCProfile::CRCProfile(const CRCProfile &orig, UINT32 new_chipid) :
    m_chipid(new_chipid),
    m_active_raster(orig.m_active_raster),
    m_full_frame(orig.m_full_frame),
    m_frame(orig.m_frame)
{
}

// LegacyVGA::CRCProfile::CRCProfile(const string& str)
LegacyVGA::CRCProfile* LegacyVGA::CRCProfile::CreateCRCProfile(const string& str)
{
    // example of profile string: lw50_ActiveRaster_SmallFrame_03
    // example of profile string: g82_ActiveRaster_SmallFrame_03

    string::size_type beg, end;

    // looking for chip ID
    beg = str.find_first_not_of(delims);
    if (beg == string::npos)
        return 0;

    end = str.find_first_of(delims, beg);
    if (end == string::npos)
        return 0;

    UINT32 chipid;
    int rc=sscanf(str.substr(beg, end-beg).c_str(), "lw%x", &chipid);
    if(!rc)
    {
        Printf(Tee::PriLow, "Cannot find lw will seach for gf \n");
        rc=sscanf(str.substr(beg, end-beg).c_str(), "gf%x", &chipid);
        if(!rc)
        {
            Printf(Tee::PriLow, "Cannot find gf will seach for g \n");
            rc=sscanf(str.substr(beg, end-beg).c_str(), "g%x", &chipid);
            if(!rc)
            {
                Printf(Tee::PriLow, "Cannot find g will seach for gt \n");
                sscanf(str.substr(beg, end-beg).c_str(), "gt%x", &chipid);
            }
        }
        else
        {
            // the chip ids for gf11x do not simply match the gf%x number
            switch(chipid)
            {
                case 0x110d:
                    chipid = 0x50D; // Gpu::GF110D;
                    break;
                case 0x110f:
                    chipid = 0x50F; // Gpu::GF110F;
                    break;
                case 0x110f2:
                    chipid = 0x51F; //Gpu::GF110F2;
                    break;
                case 0x110f3:
                    chipid = 0x52F; //Gpu::GF110F3;
                    break;
                default:
                    chipid += 0x300;
            }
        }
    }

    // raster type
    beg = str.find_first_not_of(delims, end);
    if (beg == string::npos)
        return 0;

    end = str.find_first_of(delims, beg);
    if (end == string::npos)
        return 0;

    bool active_raster = false;
    if (str.substr(beg, end-beg) == "ActiveRaster")
        active_raster = true;
    else if (str.substr(beg, end-beg) == "FullRaster")
        active_raster = false;
    else
    {
        Printf(Tee::PriHigh, "*** ERROR: Wrong raster type\n");
        return 0;
    }

    // frame size
    beg = str.find_first_not_of(delims, end);
    if (beg == string::npos)
        return 0;

    end = str.find_first_of(delims, beg);
    if (end == string::npos)
        return 0;

    bool full_frame = true;
    if (str.substr(beg, end-beg) == "FullFrame")
        full_frame = true;
    else if (str.substr(beg, end-beg) == "SmallFrame")
        full_frame = false;
    else
    {
        Printf(Tee::PriHigh, "*** ERROR: Wrong frame size\n");
        return 0;
    }

    // frame number
    beg = str.find_first_not_of(delims, end);
    if (beg == string::npos)
        return 0;

    end = str.find_first_of(delims, beg);
    if (end == string::npos)
        end = str.length();

    UINT32 frame;
    sscanf(str.substr(beg, end-beg).c_str(), "%x", &frame);

    Printf(Tee::PriLow, "New Profile: cid=0x%08x ar=%d ff=%d fr=%d\n", chipid, active_raster, full_frame, frame);

    return new LegacyVGA::CRCProfile(chipid, active_raster, full_frame, frame);
}

string LegacyVGA::CRCProfile::Profile() const
{
    string chip;
    switch (m_chipid)
    {
        case 0x50D: //Gpu::GF110D:
        case 0x50F: //Gpu::GF110F:
        case 0x51F: //Gpu::GF110F2:
        case 0x52F: //Gpu::GF110F3:
            chip = "gf110d";
            break;
        case 0x419: //Gpu::GF119:
            chip = "gf119";
            break;
        default:
            chip = Utility::StrPrintf("0x%x", m_chipid);
            break;
    }

    return Utility::StrPrintf("%s_%s_%s_%02x",
                  chip.c_str(),
                  m_active_raster ? "ActiveRaster" : "FullRaster",
                  m_full_frame    ? "FullFrame"    : "SmallFrame",
                  m_frame);
}

bool LegacyVGA::CRCProfile::compare(const CRCProfile& that) const
{
    return m_chipid == that.m_chipid &&
           m_active_raster == that.m_active_raster &&
           m_full_frame == that.m_full_frame &&
           m_frame == that.m_frame;
}

LegacyVGA::CrcMap::iterator  LegacyVGA::SearchCRCChain(const CRCProfile &lwrr, CrcMap &crc_map, const CrcChain &crc_chain)
{
    LegacyVGA::CrcMap::iterator pos=crc_map.find(lwrr);
    if(pos != crc_map.end())
    {
        s_last_match.insert(0);          // saving index pointing to chipid in chain
        if(s_last_match.size()>1)        // should be just one index
            Printf(Tee::PriHigh, "Error, partial chaining detected\n");
        return pos;
    }
    else
    {
        //Go along the crc chain starting with
        //next chipid and check if there is a matching profile
        unsigned chain_head=0;
        while(pos==crc_map.end() && chain_head < crc_chain.size())
        {
            CRCProfile *tmp=new CRCProfile(lwrr,crc_chain[chain_head]);
            Printf(Tee::PriLow,"Looking for chipid=%0x in crc_chain\n",crc_chain[chain_head]);
            pos=crc_map.find(*tmp);
            delete tmp;
            ++chain_head;
        }

        //Check for partial chaining
        s_last_match.insert(chain_head); //saving index pointing to chipid in chain
        if(s_last_match.size()>1)        // should be just one index
            Printf(Tee::PriHigh, "Error, partial chaining detected, current profile match chipid=%0x\n",crc_chain[chain_head]);
    }
    return pos;
}

bool LegacyVGA::CRCProfile::operator<(const CRCProfile& that) const
{
    if (m_chipid == that.m_chipid)
        if (m_active_raster == that.m_active_raster)
            if (m_full_frame == that.m_full_frame)
                return m_frame < that.m_frame;
            else
                return !m_full_frame;
        else
            return !m_active_raster;
    else
        return m_chipid < that.m_chipid;
}

VGA_CRC* LegacyVGA::CRCManager()
{
    static VGA_CRC crcmanager;
    return &crcmanager;
}

LegacyVGA::CrcMap* LegacyVGA::ReadCRCs(const string& filename)
{
    FILE *   fp = 0;
    unique_ptr<CrcMap> crcmap;

    Printf(Tee::PriHigh, "Info : Reading %s ...\n", filename.c_str());
    if (OK == Utility::OpenFile(filename, &fp, "r"))
    {
        crcmap = make_unique<CrcMap>();
        char buff[line_size];
        string delims = " \t\n=#";

        while(fgets(buff, line_size, fp))
        {
            string line(buff);
            string::size_type beg, end;

            // first, looking for profile string
            beg = line.find_first_not_of(delims);
            if (beg == string::npos)
                continue;

            end = line.find_first_of(delims, beg);
            if (end == string::npos)
                continue;

            unique_ptr<LegacyVGA::CRCProfile> found_profile(
                    CRCProfile::CreateCRCProfile(line.substr(beg, end-beg)));
            if (!found_profile)
            {
                Printf(Tee::PriHigh, "*** ERROR: %s seems to be corrupted crc file\n",
                        filename.c_str());
                fclose(fp);
                return nullptr;
            }

            // second, looking for CRC
            beg = line.find_first_not_of(delims, end);
            if (beg == string::npos)
                continue;

            end = line.find_first_of(delims, beg);
            if (end == string::npos)
                end = line.length();

            UINT32 crc;
            sscanf(line.substr(beg, end-beg).c_str(), "0x%x", &crc);

            crcmap->insert(make_pair(*found_profile, crc));
        }
        fclose(fp);
    } else
        Printf(Tee::PriHigh, "*** ERROR: Failed to read %s CRC file\n", filename.c_str());

    return crcmap.release();
}

unsigned LegacyVGA::SaveCRCs(const CrcMap& crcmap, const string& filename, bool approvinfo)
{
    // reading existing CRCs...
    unique_ptr<CrcMap> crcs(ReadCRCs(filename));

    // ... and merging them with callwlated crcs
    if (crcs)
        for (CrcMap::const_iterator i = crcmap.begin(); i != crcmap.end(); ++i)
            (*crcs)[i->first] = i->second;

    FILE *   fp = 0;

    if (OK == Utility::OpenFile(filename, &fp, "w"))
    {
        char buff[line_size];
        CrcMap::const_iterator i, e;
        if (crcs)
        {
            i = crcs->begin();
            e = crcs->end();
        }
        else
        {
            i = crcmap.begin();
            e = crcmap.end();
        }

        for (; i != e; ++i)
        {
            sprintf(buff, "%s = 0x%08x\n", i->first.Profile().c_str(), i->second);
            fputs(buff, fp);
        }
        fclose(fp);
    }
    else
    {
        Printf(Tee::PriHigh, "*** ERROR: Failed to update %s CRC file\n", filename.c_str());
        return ERROR_FILE;
    }

    return ERROR_NONE;
}

unsigned LegacyVGA::CountCRCs(const CrcMap* crcs, bool active_raster, bool full_frame, UINT32 chip_id)
{
    if (!crcs)
        return 0;

    unsigned count = 0;
    for (CrcMap::const_iterator iter = crcs->begin(); iter != crcs->end(); ++iter)
    {
        Printf(Tee::PriLow, "CountCRCs: cid in : 0x%08x cid map : 0x%08x\n", chip_id, iter->first.ChipID());

        if ((iter->first.ActiveRaster() == active_raster) &&
            (iter->first.FullFrame() == full_frame) &&
            (iter->first.ChipID() == chip_id))
        {
            ++count;
        }
    }

    return count;
}

/****************************************************************************************
 *
 *  Functional Description:  Initialize CRC capture
 *
 ***************************************************************************************/

VGA_CRC::VGA_CRC(UINT32 head, UINT32 dac, UINT32 int_mask):
    m_frame_state(0),
    m_frame_state_set(false),
    m_clock_changed(false),
    m_head_index(head),
    m_or_index(dac),
    m_int_mask(int_mask),
    m_or_type("dac")
{
}

VGA_CRC::VGA_CRC(UINT32 head, UINT32 dac, UINT32 int_mask, const string& or_type):
    m_frame_state(0),
    m_frame_state_set(false),
    m_clock_changed(false),
    m_head_index(head),
    m_or_index(dac),
    m_int_mask(int_mask),
    m_or_type(or_type)
{
}

VGA_CRC::VGA_CRC() : m_frame_state(0), m_frame_state_set(false), m_clock_changed(false),
                   m_head_index(0), m_or_index(1),
                   m_int_mask(REF_NUM(LW_PDISP_VGA_CR_REG4_VGA_OUT_INT_DISB, 0))
{
    m_or_type = "dac";
}

VGA_CRC::~VGA_CRC()
{
}

/****************************************************************************************
 * VGA_CRC::SetFrameState
 *  description: set the blink state for the next frame
 *
 *  parameters:
 *               cursor - 1 - cursor on
 *                        0 - cursor off
 *               attr   - 1 - cursor on
 *                        0 - cursor off
 ***************************************************************************************/
RC VGA_CRC::SetFrameState(bool cursor, bool attr)
{
  m_frame_state = 0x0;
  if(cursor)
    m_frame_state |= 0x1;

  if(!attr)
    m_frame_state |= 0x2;

  m_frame_state_set = true;

  return OK;
}

/****************************************************************************************
 * VGA_CRC::ClearFrameState
 *  description: clear the waiting for blink state on each frame
 *
 *  parameters:
 *               None
 ***************************************************************************************/
RC VGA_CRC::ClearFrameState()
{
  m_frame_state_set = false;

  return OK;
}

/****************************************************************************************
 * VGA_CRC::SetHeadDac
 *  description: set a class variable to know which head to send CRC enable method
 *  on and which DAC to grab CRC from
 *
 *  parameters: unsigned int m_head_index and unsigned int m_or_index
 ***************************************************************************************/
RC VGA_CRC::SetHeadDac(UINT32 head, UINT32 dac)
{
    m_head_index = head;
    m_or_index = dac;
    m_or_type = "dac";

    return OK;
}

/****************************************************************************************
 * VGA_CRC::SetHeadSor
 *  description: set a class variable to know which head to send CRC enable method
 *  on and which SOR to grab CRC from
 *
 *  parameters: unsigned int m_head_index and unsigned int m_or_index
 ***************************************************************************************/
RC VGA_CRC::SetHeadSor(UINT32 head, UINT32 sor)
{
    m_head_index = head;
    m_or_index = sor;
    m_or_type = "sor";

    return OK;
}

/****************************************************************************************
 * VGA_CRC::GetHeadDac
 *  description: set a class variable to know which head to send CRC enable method
 *  on and which DAC to grab CRC from
 *
 *  parameters: unsigned int m_head_index and unsigned int m_or_index
 ***************************************************************************************/
RC VGA_CRC::GetHeadDac(UINT32 *phead, UINT32 *pdac)
{
    *phead = m_head_index;
    *pdac = m_or_index;

    return OK;
}

RC VGA_CRC::SetInt(UINT32 int_mask)
{
    m_int_mask = REF_NUM(LW_PDISP_VGA_CR_REG4_VGA_OUT_INT_DISB, int_mask ? 1 : 0);
    return OK;
}

/****************************************************************************************
 * VGA_CRC::SetClockChanged
 *  description: set that a mode set has oclwred and the rg will sleep
 *
 *  parameters:
 ***************************************************************************************/
RC VGA_CRC::SetClockChanged()
{
  m_clock_changed = true;

  return OK;
}

/****************************************************************************************
 * VGA_CRC::ClearClockChanged
 *  description: clear the flag that signals that a mode set has oclwred and the rg will sleep
 *
 *  parameters:
 ***************************************************************************************/
RC VGA_CRC::ClearClockChanged (void)
{
   m_clock_changed = false;

   return OK;
}

/****************************************************************************************
 * VGA_CRC::WaitCapture
 *
 * description: wait until capture is done
 *
 *  parameters:
 *            last -   0 - not last frame to capture
 *                     1 - last frame to capture
 *            update - 0 - don't call UpdateNow
 *                     1 - call UpdateNow
 ***************************************************************************************/
RC VGA_CRC::WaitCapture(UINT32 last, UINT32 update, FLOAT64 timeoutMs)
{
    RC rc = OK;

    if (!last)
    {
        DispTest::SetLegacyCrc(1, m_head_index); // (enable, head)
        // Send any queued up methods
        if(update)
            DispTest::UpdateNow(timeoutMs);
    }

    Printf(Tee::PriDebug, "Info : Waiting for CRC valid bit 2\n");

    // wait for CRC valid bit
    if(m_or_type == "dac")
    {
        rc = DispTest::PollRegValue(LW_PDISP_DAC_CRCA(m_or_index), 0x1, 0x1, timeoutMs);
    }
    else if (m_or_type == "sor")
    {
        rc = DispTest::PollRegValue(LW_PDISP_SOR_CRCA(m_or_index), 0x1, 0x1, timeoutMs);
    }
    else
    {
        Printf(Tee::PriNormal, "ERROR: WaitCapture : m_or_type was not dac or sor  \n");
        MASSERT(!"Unknown or_type");
    }

    UINT32 value = 0;
    if(rc != OK) {
        Printf(Tee::PriNormal, "Info : Error (possibly timeout) waiting for real CRC\n");
    } else {
        // reset CRCA_VALID
        if(m_or_type == "dac")
        {
            DispTest::GetBoundGpuSubdevice()->RegWr32(LW_PDISP_DAC_CRCA(m_or_index),
                                                      LW_PDISP_DAC_CRCA_VALID_RESET);

            Printf(Tee::PriDebug, "Info : Found real CRC\n");

            // read CRC and print it to the log
            value = DispTest::GetBoundGpuSubdevice()->RegRd32(LW_PDISP_DAC_CRCB(m_or_index));
            Printf(Tee::PriNormal, "Info : CRC = 0x%08x\n", value);
        }
        else if(m_or_type == "sor")
        {
            DispTest::GetBoundGpuSubdevice()->RegWr32(LW_PDISP_SOR_CRCA(m_or_index),
                                                      LW_PDISP_SOR_CRCA_VALID_RESET);

            Printf(Tee::PriDebug, "Info : Found real CRC\n");

            // read CRC and print it to the log
            value = DispTest::GetBoundGpuSubdevice()->RegRd32(LW_PDISP_SOR_CRCB(m_or_index));
            Printf(Tee::PriNormal, "Info : CRC = 0x%08x\n", value);
        }

        m_crc.push_back(value);
    }

    return OK;
}

/****************************************************************************************
 * VGA_CRC::CaptureFrame
 * description: Capture both CRC and image of next frame
 *
 *  parameters:
 *               wait   - 0 - return control immediately
 *                        1 - wait for frame to finish capture
 *               update - 0 - don't send UpdateNow method
 *                        1 - send UpdateNow method
 *               sync   - 0 - synchronize CaptureFrame to vsync
 *                        1 - don't wait for vsync
 *               timeoutMs - time out value
 *
 *
 *  Note: In selecting blink state of frames this code relys on the fact
 *        that fastblink is enabled. This allows the code to determine
 *        the correct previous frame state for any given frame. The code
 *        then waits for the correct previous frame state and enables
 *        capture for the next frame. Thus the next frame will have the
 *        correct blink state.
 *
 ***************************************************************************************/
RC VGA_CRC::CaptureFrame(UINT32 wait, UINT32 update, UINT32 sync, FLOAT64 timeoutMs)
{
    RC rc = OK;
    UINT32 value;
    UINT32 i;
    const UINT32 int_mask = ~(REF_NUM(LW_PDISP_VGA_CR_REG4_VSYNC_INT_ENAB, 1) |
                              REF_NUM(LW_PDISP_VGA_CR_REG4_RSVD, 1));

    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

    Printf(Tee::PriDebug, "CaptureFrame\n");

    Printf(Tee::PriNormal, "[crccheck.cpp] VGACaptureFrame wait=%d update=%d sync=%d m_clock_changed=%d \n",
        wait, update, sync, m_clock_changed);

    // Wait for notifier after mode switch
    if(m_clock_changed) {
        //
        // RMG FIXME - change to handle both color and mono
        //
        if (update) {
            // Send any queued up methods and set legacy crc
            rc = DispTest::UpdateNow(timeoutMs);
            CHECK_RC(rc);
        }

        // We must wait a frame after a clock change
        // In order to ensure one frame I am waiting for two vsyncs
        // enclosing that frame
        for(i=0;i<3;i++) {
             Printf(Tee::PriNormal, "Info : Mode Set Skipping Frame\n");

             //
             // Note: Using Vertical Interrupt is unreliable
             //       during a clock switch so wait a frame
             //       the old fasioned way

             // Note: Using Vertical Sync is not reliable either
             //       due to short duration so we are going back
             //       to using Vertical Interrupt

             // Wait for vsync == 0
#if 0
             Printf(Tee::PriNormal, "Info : Waiting for VSYNC equal to 0\n");
             timeout_reached = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP1,
                       0x00, 0x08, timeoutMs);
             if(timeout_reached) {
                  Printf(Tee::PriNormal, "Info : Timeout waiting for VSYNC = 0\n");
             }

             // Wait for vsync == 1
             Printf(Tee::PriNormal, "Info : Waiting for VSYNC equal to 1\n");
             timeout_reached = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP1,
                       0x08, 0x08, timeoutMs);
             if(timeout_reached) {
                  Printf(Tee::PriNormal, "Info : Timeout waiting for VSYNC = 1\n");
             }
#endif
             Printf(Tee::PriNormal, "Info : Enable Interrupt\n");

             // Reset and enable Vertical Interrupt
             value = pSubdev->RegRd32(LW_PDISP_VGA_CR_REG4);
             value = (value & int_mask) | m_int_mask;
             pSubdev->RegWr32(LW_PDISP_VGA_CR_REG4, value);
             value = (value & int_mask) | m_int_mask |
                REF_NUM(LW_PDISP_VGA_CR_REG4_VSYNC_INT_ENAB, 1);
             pSubdev->RegWr32(LW_PDISP_VGA_CR_REG4, value);

             // Wait for interrupt
             Printf(Tee::PriNormal, "Info : Wait for Interrupt\n");
             rc = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP0,
                    0x80, 0x80, timeoutMs);
             if(rc != OK)
                Printf(Tee::PriNormal, "Info : Timeout waiting for VINT = 1\n");
             CHECK_RC(rc);

        }
        m_clock_changed = false;
    }

    if(sync) {
#if 0
      // Wait for vsync == 1
      Printf(Tee::PriNormal, "Info : Waiting for VSYNC equal to 1\n");
      timeout_reached = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP1,
                                               0x08, 0x08, timeoutMs);
      if(timeout_reached) {
        Printf(Tee::PriNormal, "Info : Timeout waiting for VSYNC = 1\n");
      }
#endif
      Printf(Tee::PriNormal, "Info : Enable Interrupt\n");

      // Reset and enable Vertical Interrupt
      value = pSubdev->RegRd32(LW_PDISP_VGA_CR_REG4);
      value = (value & int_mask) | m_int_mask;
      pSubdev->RegWr32(LW_PDISP_VGA_CR_REG4, value);
      value = (value & int_mask) | m_int_mask |
         REF_NUM(LW_PDISP_VGA_CR_REG4_VSYNC_INT_ENAB, 1);
      pSubdev->RegWr32(LW_PDISP_VGA_CR_REG4, value);

      // Wait for interrupt
      Printf(Tee::PriNormal, "Info : Wait for Interrupt\n");
      rc = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP0,
             0x80, 0x80, timeoutMs);
      if(rc != OK)
         Printf(Tee::PriNormal, "Info : Timeout waiting for VINT = 1\n");
      CHECK_RC(rc);
    }

    // Wait for correct frame state
    if(m_frame_state_set) {
        // Wait for VSYNC high before sampling phases since phases are updated on VSYNC
        Printf(Tee::PriNormal, "Info : Wait for VSYNC = 1\n");
        rc = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP1,
                0x08, 0x08, timeoutMs);
        if(rc != OK)
            Printf(Tee::PriNormal, "Info : Timeout waiting for VSYNC = 1\n");
        CHECK_RC(rc);

        // Wait for a blink match
        value = pSubdev->RegRd32(LW_PDISP_VGA_CR_REG8)&0x3;
        while(value != m_frame_state) {
            // Wait for next frame
            Printf(Tee::PriNormal, "Info : Frame wait current %d wanted %d\n", value, m_frame_state);

#if 1    // LGC
         WaitFrames (1, timeoutMs);
#else
            Printf(Tee::PriNormal, "Info : Skipping Frame\n");

            Printf(Tee::PriNormal, "Info : Enable Interrupt\n");

            // Reset and enable Vertical Interrupt
            value = pSubdev->RegRd32(LW_PDISP_VGA_CR_REG4);
            value = (value & int_mask) | m_int_mask;
            pSubdev->RegWr32(LW_PDISP_VGA_CR_REG4, value);
            value = (value & int_mask) | m_int_mask |
                REF_NUM(LW_PDISP_VGA_CR_REG4_VSYNC_INT_ENAB, 1);
            pSubdev->RegWr32(LW_PDISP_VGA_CR_REG4, value);

            // Wait for interrupt
            Printf(Tee::PriNormal, "Info : Wait for Interrupt\n");
            rc = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP0,
                    0x80, 0x80, timeoutMs);
            if(rc != OK)
                Printf(Tee::PriNormal, "Info : Timeout waiting for VINT = 1\n");
            CHECK_RC(rc);

            // Wait for VSYNC high
            Printf(Tee::PriNormal, "Info : Wait for VSYNC = 1\n");
            rc = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP1,
                    0x08, 0x08, timeoutMs);
            if(rc != OK)
                Printf(Tee::PriNormal, "Info : Timeout waiting for VSYNC = 1\n");
            CHECK_RC(rc);
#endif

            // Check this frame
            value = pSubdev->RegRd32(LW_PDISP_VGA_CR_REG8)&0x3;
        }

        Printf(Tee::PriNormal, "Info : Frame state synchronized to %d\n", value);
    }

    // CRC valid must not be cleared or a CRC may be lost.
    // A CRC may be valid when we are sending set legacy crc in continuous capture,
    // but it will depend on how many register/memory writes you do before calling
    // CaptureFrame.
    //
    //if (wait) {
    //     // reset CRCA_VALID
    //     // This does not cause any problems in continous capture because
    //     // crcs are not supposed to be ready when we are sending set legacy crc
    //     // for the next frame
    //     pSubdev->RegWr32(LW_PDISP_DAC_CRCA(m_dac_index), LW_PDISP_DAC_CRCA_VALID_RESET);
    //}

    // Wait for first display_enable so SetLegacyCrc will always occur after loadv
    Printf(Tee::PriNormal, "Info : Wait for Display Enable\n");
    rc = DispTest::PollRegValue(LW_PDISP_VGA_GEN_REGS_INP1, 0x00, 0x01, timeoutMs);
    if(rc != OK)
       Printf(Tee::PriNormal, "Info : Timeout waiting for Display Enable\n");
    CHECK_RC(rc);
    // Set to capture next frame
    Printf(Tee::PriNormal, "Info : Send capture and update \n");
    DispTest::SetLegacyCrc(1, m_head_index); // (enable, head)

    if (update)
        // Send any queued up methods and set legacy crc
        DispTest::UpdateNow(timeoutMs);

    // nowait case - return at start of frame
    if(!wait) return OK;

    Printf(Tee::PriNormal, "Info : Waiting for CRC valid bit\n");

    // wait for CRC valid bit
    if(m_or_type == "dac")
    {
        rc = DispTest::PollRegValue(LW_PDISP_DAC_CRCA(m_or_index), 0x1, 0x1,
                timeoutMs);
    }
    else if (m_or_type == "sor")
    {
        rc = DispTest::PollRegValue(LW_PDISP_SOR_CRCA(m_or_index), 0x1, 0x1,
                timeoutMs);
    }
    else
    {
        Printf(Tee::PriNormal, "Error : Invalid OR type\n");
        MASSERT(!"Unknown or_type");
    }
    if(rc != OK) {
        Printf(Tee::PriNormal, "Info : Timeout waiting for real CRC\n");
    } else {
        // reset CRCA_VALID
        if(m_or_type == "dac")
        {
            pSubdev->RegWr32(LW_PDISP_DAC_CRCA(m_or_index), LW_PDISP_DAC_CRCA_VALID_RESET);

            Printf(Tee::PriNormal, "Info : Found real CRC\n");

            // read CRC and print it to the log
            value = pSubdev->RegRd32(LW_PDISP_DAC_CRCB(m_or_index));
            Printf(Tee::PriNormal, "Info : CRC = 0x%08x\n", value);
        }
        else if (m_or_type == "sor")
        {
            pSubdev->RegWr32(LW_PDISP_SOR_CRCA(m_or_index), LW_PDISP_SOR_CRCA_VALID_RESET);

            Printf(Tee::PriNormal, "Info : Found real CRC\n");

            // read CRC and print it to the log
            value = pSubdev->RegRd32(LW_PDISP_SOR_CRCB(m_or_index));
            Printf(Tee::PriNormal, "Info : CRC = 0x%08x\n", value);
        }
        else
        {
            Printf(Tee::PriNormal, "Error : Invalid OR type\n");
        }

        m_crc.push_back(value);
    }
    CHECK_RC(rc);
    return OK;
}

/****************************************************************************************
 * VGA_CRC::WaitBlinkState
 * description: Wait until the blinke state is AT THE START of the given state
 *
 *  parameters:
 *               blink_request - 0..3 - Blink state
 *               timeoutMs     - Time out value
 *
 *
 *  Note: This method is independent of whether fastblink is enabled or not;
 *        however, without fastblink enabled, there could be up to a 15 frame
 *        delay ((blink frames * 2) - 1). Without fastblink, the cursor blinks
 *        at a rate of eight frames on followed by eight frames off.
 *
 ***************************************************************************************/
RC VGA_CRC::WaitBlinkState (UINT32 blink_request, FLOAT64 timeoutMs)
{
    GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();
#if 1
   UINT32   blink_state;

   blink_request &= 0x03;
   blink_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x3;
   Printf (Tee::PriNormal, "Info : Wait for blink state %d (current state = %d).\n", blink_request, blink_state);

   // Wait for the state to complete
   while (blink_request == blink_state)
   {
      // Wait for next frame
      Printf (Tee::PriNormal, "Info : Waiting for blink state %d to complete.\n", blink_request);
      WaitFrames (1, timeoutMs);

      // Get state of this frame
      blink_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x3;
   }

   // Wait for the blink state to start
   while (blink_request != blink_state)
   {
      // Wait for next frame
      Printf (Tee::PriNormal, "Info : Waiting for blink state %d (current state = %d).\n", blink_request, blink_state);
      WaitFrames (1, timeoutMs);

      // Get state of this frame
      blink_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x3;
   }
   Printf (Tee::PriNormal, "Info : Blink state %d started.\n", blink_request);

   // Finally, at the start of the matching state
   return (OK);

#else
   UINT32   lwrsor_state;

   // Get the current state
   lwrsor_on &= 0x01;
   lwrsor_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x1;
   Printf (Tee::PriNormal, "Info : Wait for cursor to start blink state %d (current state = %d).\n", lwrsor_on, lwrsor_state);

   // Wait for matching state to be over
   while (lwrsor_state == lwrsor_on)
   {
      // Wait for next frame
      Printf (Tee::PriNormal, "Info : Waiting for cursor blink state %d to complete.\n", lwrsor_on);
      WaitFrames (1, timeoutMs);

      // Get state of this frame
      lwrsor_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x1;
   }

   // Wait all the way thru the non-matching state
#if 1
   Printf (Tee::PriNormal, "Info : Waiting 8 frames for cursor blink state %d to occur.\n", lwrsor_on);
   WaitFrames (8, timeoutMs);
#else
   while (lwrsor_state != lwrsor_on)
   {
      // Wait for next frame
      Printf (Tee::PriNormal, "Info : Waiting for cursor blink state %d to occur.\n", lwrsor_on);
      WaitFrames (1, timeoutMs);

      // Get state of this [new] frame
      lwrsor_state = pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8) & 0x1;
   }
#endif
   Printf (Tee::PriNormal, "Info : Cursor blink state %d started.\n", lwrsor_on);

   // Finally, at the start of the matching state
   return (OK);
#endif
}

#if 0
/****************************************************************************************
 * VGA_CRC::WaitAttributeStartBlink
 * description: Wait until the cursor is AT THE START of the given state
 *
 *  parameters:
 *               attr_on   - 0 - Attribute blink is off
 *                           1 - Attribute blink is on
 *               timeoutMs - Time out value
 *
 *
 *  Note: This method is independent of whether fastblink is enabled or not;
 *        however, without fastblink enabled, there could be up to a 31 frame
 *        delay ((blink frames * 2) - 1). Without fastblink, the attribute blinks
 *        at a rate of 16 frames on followed by 16 frames off.
 *
 ***************************************************************************************/
RC VGA_CRC::WaitAttributeStartBlink (UINT32 attr_on, FLOAT64 timeoutMs)
{
   UINT32   attr_state;
   GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

   // Get the current state
   attr_on = (attr_on & 0x01) << 1;
   attr_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x2;
   Printf (Tee::PriNormal, "Info : Wait for attribute to start blink state %d (current state = %d).\n", attr_on, attr_state);

   // Wait for matching state to be over
   while (attr_state == attr_on)
   {
      // Wait for next frame
      Printf (Tee::PriNormal, "Info : Waiting for attribute blink state %d to complete.\n", attr_on);
      WaitFrames (1, timeoutMs);

      // Get state of this frame
      attr_state = (pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8)) & 0x2;
   }

   // Wait all the way thru the non-matching state
#if 1
   Printf (Tee::PriNormal, "Info : Waiting 16 frames for attribute blink state %d to occur.\n", attr_on);
   WaitFrames (15, timeoutMs);
#else
   while (attr_state != attr_on)
   {
      // Wait for next frame
      Printf (Tee::PriNormal, "Info : Waiting for attribute blink state %d to occur.\n", attr_on);
      WaitFrames (1, timeoutMs);

      // Get state of this [new] frame
      attr_state = pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG8) & 0x2;
   }
#endif
   Printf (Tee::PriNormal, "Info : Attribute blink state %d started.\n", attr_on);

   return (OK);
}
#endif

/****************************************************************************************
 * VGA_CRC::ResetCRCValid
 * description: Resets the CRC valid. This should be done as part of the initialization
 *
 *  parameters: none
 *
 ***************************************************************************************/
RC VGA_CRC::ResetCRCValid()
{
   // reset CRCA_VALID
    if(m_or_type == "dac")
    {
        DispTest::GetBoundGpuSubdevice()->RegWr32(LW_PDISP_DAC_CRCA(m_or_index), LW_PDISP_DAC_CRCA_VALID_RESET);
    }
    else if(m_or_type == "sor")
    {
        DispTest::GetBoundGpuSubdevice()->RegWr32(LW_PDISP_SOR_CRCA(m_or_index), LW_PDISP_SOR_CRCA_VALID_RESET);
    }
    else
    {
        Printf(Tee::PriNormal, "ERROR: ResetCRCValid : m_or_type was not dac or sor  \n");
        MASSERT(!"Unknown or_type");
    }
  return OK;
}

unsigned VGA_CRC::Check(const string& test_dir, bool active_raster, bool full_frame, bool dump,
                        const string& pixel_trace)
{
    // this routine can be called on pre-lw5x chips
    UINT32 chipid=DispTest::GetBoundGpuSubdevice()->DeviceId();

    //construct check_crc_chain and dump_crc_chain
    LegacyVGA::CrcChain check_crc_chain, dump_crc_chain;
    switch (chipid)
    {
        case Gpu::GM200:
            check_crc_chain.push_back(Gpu::GM200);
            dump_crc_chain.push_back(Gpu::GM200);
        case Gpu::GM204:
            check_crc_chain.push_back(Gpu::GM204);
            dump_crc_chain.push_back(Gpu::GM204);
        case Gpu::GM206:
            check_crc_chain.push_back(Gpu::GM206);
            dump_crc_chain.push_back(Gpu::GM206);
        case Gpu::GP100:
            check_crc_chain.push_back(Gpu::GP100);
            dump_crc_chain.push_back(Gpu::GP100);
        case Gpu::GP102:
            check_crc_chain.push_back(Gpu::GP102);
            dump_crc_chain.push_back(Gpu::GP102);
        case Gpu::GP104:
            check_crc_chain.push_back(Gpu::GP104);
            dump_crc_chain.push_back(Gpu::GP104);
        case Gpu::GP106:
            check_crc_chain.push_back(Gpu::GP106);
            dump_crc_chain.push_back(Gpu::GP106);
        case Gpu::GP107:
            check_crc_chain.push_back(Gpu::GP107);
            dump_crc_chain.push_back(Gpu::GP107);
        case Gpu::GP108:
            check_crc_chain.push_back(Gpu::GP108);
            dump_crc_chain.push_back(Gpu::GP108);
        case Gpu::GM107:
            check_crc_chain.push_back(Gpu::GM107);
            dump_crc_chain.push_back(Gpu::GM107);
            // do not add break
        case 0x419: //Gpu::GF119:
            check_crc_chain.push_back(0x419);
            dump_crc_chain.push_back(0x419);
            // do not add break
        case 0x417: //Gpu::GF117:
            check_crc_chain.push_back(0x417);
            dump_crc_chain.push_back(0x417);
            // do not add break
        case 0x50D: //Gpu::GF110D:
        case 0x50F: //Gpu::GF110F:
        case 0x51F: //Gpu::GF110F2:
        case 0x52F: //Gpu::GF110F3:
            check_crc_chain.push_back(0x50D);
            dump_crc_chain.push_back(0x50D);
            check_crc_chain.push_back(0x50F);
            dump_crc_chain.push_back(0x50F);
            check_crc_chain.push_back(0x51F);
            dump_crc_chain.push_back(0x51F);
            check_crc_chain.push_back(0x52F);
            dump_crc_chain.push_back(0x52F);
            // do not add break
        case 0x408: //Gpu::GF108:
            check_crc_chain.push_back(0x408);
            dump_crc_chain.push_back(0x408);
            // do not add break
        case 0x406: //Gpu::GF106:
            check_crc_chain.push_back(0x406);
            dump_crc_chain.push_back(0x406);
            // do not add break
        case 0x404: // Gpu::GF104:
            check_crc_chain.push_back(0x404);
            dump_crc_chain.push_back(0x404);
            // do not add break
        case 0x400: // Gpu::GF100:
            check_crc_chain.push_back(0x400);
            dump_crc_chain.push_back(0x400);
            // Default "lw50" profile
            check_crc_chain.push_back(0x50);
            dump_crc_chain.push_back(0x50);
            break;
        case Gpu::GV100:
#if LWCFG(GLOBAL_CHIP_T194)
        case Gpu::GV11B:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
        case Gpu::TU102:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU104)
        case Gpu::TU104:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU106)
        case Gpu::TU106:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU116)
        case Gpu::TU116:
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU117)
        case Gpu::TU117:
#endif
            check_crc_chain.push_back(0x900); // Gpu::GV100
#if LWCFG(GLOBAL_GPU_IMPL_GA102F)
        case Gpu::GA102F:
            check_crc_chain.push_back(0xC0F); // Gpu::GA102F
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD102F)
        case Gpu::AD102F:
            check_crc_chain.push_back(0xD0F); // Gpu::AD102F
#endif
        default:
            MASSERT(!"Unknown DeviceId");
    }

    if(!check_crc_chain.size())
    {
        Printf(Tee::PriHigh, "Error, check_crc_chain is missing!\n");
        return ERROR_CHECKSUM;
    }
    if(dump && !dump_crc_chain.size())
    {
        Printf(Tee::PriHigh, "Error, crc dump requested but dump_crc_chain is missing!\n");
        return ERROR_CHECKSUM;
    }

    Printf(Tee::PriHigh, "Info: Checking gathered CRCs...\n");

    string lead_file = test_dir + dir_separator + "test.crc";
    string gold_file = test_dir + dir_separator + "golden.crc";

    unique_ptr<LegacyVGA::CrcMap> lead_crcs(LegacyVGA::ReadCRCs(lead_file));
    unique_ptr<LegacyVGA::CrcMap> gold_crcs(LegacyVGA::ReadCRCs(gold_file));

    bool lead_crcs_defined = false;
    if (lead_crcs)
        lead_crcs_defined = true;

    bool all_gold_crcs_found = true;
    unsigned failed_crcs = 0;
    unsigned mismatch_crcs = 0;
    bool found_lead_match;
    UINT32 dump_chipid=dump_crc_chain[0];

    UINT32 leadChipID = 0;
    UINT32 goldChipID = 0;

    //check computed profiles and dump them if necessary
    for (unsigned i = 0; i < m_crc.size(); ++i)
    {
        found_lead_match=false;

        //for each computed profile we define dump_profile which is the same,
        //except it has chipid from the head of dump chain (logic from GpuVerif).
        //Why we need the whole chain for dump ?
        //Also data from current chip can override crc's from any chip, if it is in a
        //head of dump chain.
        LegacyVGA::CRCProfile lwrr_profile(active_raster, full_frame, i);
        LegacyVGA::CRCProfile dump_profile(dump_chipid,active_raster,full_frame,i);

        Printf(Tee::PriLow, "****Checking profile #%d %s\n",i, lwrr_profile.Profile().c_str());
        Printf(Tee::PriLow, "****Dump profile #%d %s\n",i, dump_profile.Profile().c_str());
        LegacyVGA::CrcMap::const_iterator lpos = lead_crcs->end();
        if (lead_crcs_defined)
        {
            lpos=SearchCRCChain(lwrr_profile,*lead_crcs,check_crc_chain);
            if (lpos == lead_crcs->end())
            {
                Printf(Tee::PriHigh,"*** Error: Cannot find lead match for profile %s in %s\n", lwrr_profile.Profile().c_str(), lead_file.c_str());
            }
            else
            {
                 Printf(Tee::PriHigh, "*** Found matching lead profile %s\n", (lpos->first).Profile().c_str());
                 found_lead_match=true;
                 if(leadChipID == 0)
                     leadChipID = lpos->first.ChipID();
            }
        }

        LegacyVGA::CrcMap::const_iterator gpos = gold_crcs->end();
        if (gold_crcs)
        {
            gpos=SearchCRCChain(lwrr_profile,*gold_crcs,check_crc_chain);
            if (gpos == gold_crcs->end())
            {
                Printf(Tee::PriHigh,"*** Error: Cannot find gold match for profile %s in %s\n", lwrr_profile.Profile().c_str(), gold_file.c_str());
                all_gold_crcs_found = false;
            }
            else
            {
                 Printf(Tee::PriHigh, "*** Found matching gold profile %s\n", (gpos->first).Profile().c_str());
                 if(goldChipID == 0)
                     goldChipID = gpos->first.ChipID();
            }
        }

        if (lead_crcs_defined && gold_crcs &&
            lpos != lead_crcs->end() && gpos != gold_crcs->end() && lpos->second != gpos->second)
        {
            Printf(Tee::PriHigh, "*** ERROR: LEAD CRC(0x%08x) != GOLD CRC(0x%08x)\n",
                    lpos->second, gpos->second);
            Printf(Tee::PriHigh, "           LEAD and GOLD CRCs are expected to be the same\n");
            ++mismatch_crcs;
        }

        if (lead_crcs_defined && lpos != lead_crcs->end() && lpos->second == m_crc[i])
        {
            Printf(Tee::PriHigh, "Frame: 0x%04x, CRC: 0x%08x, exptected CRC: 0x%08x        PASS\n",
                    i, m_crc[i], lpos->second);
        }
        else if (!lead_crcs_defined || lpos == lead_crcs->end() )
        {
            Printf(Tee::PriHigh, "Frame: 0x%04x, CRC: 0x%08x, no expected value            FAIL\n",
                    i, m_crc[i]);
            ++failed_crcs;
        }
        else
        {
            Printf(Tee::PriHigh, "Frame: 0x%04x, CRC: 0x%08x, exptected CRC: 0x%08x        FAIL\n",
                    i, m_crc[i], lpos->second);
            ++failed_crcs;
        }

        if (dump)
        {
            Printf(Tee::PriHigh,"Updating profile, %s\n",lwrr_profile.Profile().c_str());
            if (!lead_crcs)
                lead_crcs = make_unique<LegacyVGA::CrcMap>();
            if(found_lead_match)
            {
                if(lpos->second != m_crc[i])
                {
                    Printf(Tee::PriHigh,"Dump new profile, %s\n",dump_profile.Profile().c_str());
                    (*lead_crcs)[dump_profile] = m_crc[i];
                }
                else  {} //match profile and crc, do nothing
            }
            else //no matching profile or no test.crc file
            {
                Printf(Tee::PriHigh,"Dump new profile, %s\n",dump_profile.Profile().c_str());
                (*lead_crcs)[dump_profile] = m_crc[i];
            }
        }
    }

    if (dump)
    {
        // saving CRCs
        // LEAD CRC is always updated
        bool p4_edited = TryP4Edit(lead_file);
        if (LegacyVGA::SaveCRCs(*lead_crcs, lead_file, false))
            Printf(Tee::PriHigh, "*** ERROR while saving CRC file\n");
        else
        {
            if (!p4_edited)
            {
                if(DispTest::P4Action("add", lead_file.c_str()))
                    Printf(Tee::PriHigh, "***ERROR: p4 add %s failed\n", lead_file.c_str());
                else
                    Printf(Tee::PriHigh, "p4 add %s ... done\n", lead_file.c_str());
            }
        }

        // GOLD CRC is updated only if does not exist so far
        if (!gold_crcs)
        {
            if (LegacyVGA::SaveCRCs(*lead_crcs, gold_file, true))
                Printf(Tee::PriHigh, "*** ERROR while saving gold CRC file\n");
            else
            {
                if(DispTest::P4Action("add", gold_file.c_str()))
                    Printf(Tee::PriHigh, "***ERROR: p4 add %s failed\n", gold_file.c_str());
                else
                    Printf(Tee::PriHigh, "p4 add %s ... done\n", gold_file.c_str());
            }
        }

        // saving images
        string::size_type end = test_dir.rfind(dir_separator);
        string image_loc;
        if (end != string::npos)
            image_loc = test_dir.substr(0, end) + dir_separator + "IMG_lw50r_Z2B10G10R10_" +
                        (active_raster ? "ActiveRaster_" : "FullRaster_") +
                        (full_frame ? "FullFrame" : "SmallFrame") +
                        test_dir.substr(end, test_dir.length() - end);
        else
            image_loc = test_dir;

        DispTest::IsoDumpImages(pixel_trace, image_loc, true);

    }

    unsigned ret = 0;

    unsigned lead_crcs_n = LegacyVGA::CountCRCs(lead_crcs.get(), active_raster,
                                                full_frame, leadChipID);
    unsigned gold_crcs_n = LegacyVGA::CountCRCs(gold_crcs.get(), active_raster,
                                                full_frame, goldChipID);

    if (!lead_crcs)
    {
        Printf(Tee::PriHigh, "*** ERROR: could not read LEAD CRC file\n");
        ret = ERROR_CHECKSUM;
    }
    else if (gold_crcs && lead_crcs_n != gold_crcs_n)
    {
        Printf(Tee::PriHigh, "*** ERROR: number of LEAD/GOLD CRCs is different: %d/%d\n",
                lead_crcs_n, gold_crcs_n);
        ret = ERROR_CHECKSUM;
    }
    else if (lead_crcs_n != m_crc.size())
    {
        Printf(Tee::PriHigh, "*** ERROR: %d CRCs are collected; %d CRCs are expected\n",
               (int)m_crc.size(), lead_crcs_n);
        ret = ERROR_CHECKSUM;
    }
    else if (!failed_crcs && !mismatch_crcs)
    {
        if (gold_crcs && all_gold_crcs_found)
        {
            Printf(Tee::PriHigh, "Test passed all GOLD CRC(s)!\n");
            ret = ERROR_NONE;
        }
        else
        {
            Printf(Tee::PriHigh, "Test passed all LEAD CRC(s)!\n");
            ret = ERROR_CRCLEAD;
        }
    }
    else
    {
        if (failed_crcs)
            Printf(Tee::PriHigh, "*** ERROR: Test failed %d CRC%c\n",
                    failed_crcs, failed_crcs != 1 ? 's' : ' ');
        if (mismatch_crcs)
            Printf(Tee::PriHigh, "*** ERROR: %d LEAD/GOLD mismatch%s\n",
                    mismatch_crcs, mismatch_crcs != 1 ? "es" : "");
        ret = ERROR_CHECKSUM;
    }

    // if there are any problems with CRCs then we need to dump images to the lwr dir
    if (ret != ERROR_NONE && ret != ERROR_CRCLEAD)
    {
        string::size_type end = test_dir.rfind(dir_separator);
        string image_loc;
        if (end != string::npos)
            image_loc = test_dir.substr(end+1, test_dir.length() - end);
        else
            image_loc = test_dir;

        DispTest::IsoDumpImages(pixel_trace, image_loc, false);
    }

    // writing status in trep.txt
    FILE * trep = 0;
    if (OK == Utility::OpenFile("trep.txt", &trep, "a"))
    {
        switch(ret)
        {
        case ERROR_NONE:
            fprintf(trep, "test #0: TEST_SUCCEEDED (gold)\n");
            fprintf(trep, "summary = all tests passed\n");
            break;
        case ERROR_CRCLEAD:
            fprintf(trep, "test #0: TEST_SUCCEEDED\n");
            fprintf(trep, "summary = all tests passed\n");
            break;
        case ERROR_CRCMISSING:
            fprintf(trep, "test #0: TEST_CRC_NON_EXISTANT\n");
            fprintf(trep, "summary = tests failed\n");
            break;
        case ERROR_CHECKSUM:
            fprintf(trep, "test #0: TEST_FAILED_CRC\n");
            fprintf(trep, "summary = tests failed\n");
            break;
        default:
            fprintf(trep, "test #0: TEST_FAILED\n");
            fprintf(trep, "summary = tests failed\n");
            break;
        }
        fprintf(trep, "expected results = 1\n");
        fclose(trep);
    }
    else
    {
        Printf(Tee::PriHigh, "*** ERROR: can not open trep.txt file for writing\n");
    }

    return ret;
}

bool VGA_CRC::TryP4Edit(const string& file_name)
{
    FILE* fp;
    bool ret = false;
    if (OK == Utility::OpenFile(file_name, &fp, "r"))
    {
        if (DispTest::P4Action("edit", file_name.c_str()))
            Printf(Tee::PriHigh, "***ERROR: p4 edit %s failed\n", file_name.c_str());
        else
        {
            Printf(Tee::PriHigh, "p4 edit %s ... done\n", file_name.c_str());
            ret = true;
        }
        fclose(fp);
    }
    return ret;
}

/****************************************************************************************
 * VGA_CRC::WaitFrames
 * description: Wait a given number of frames
 *
 *  parameters:
 *               count     - Number of frames to wait
 *               timeoutMs - Time out value
 *
 *
 ***************************************************************************************/
RC VGA_CRC::WaitFrames (UINT32 count, FLOAT64 timeoutMs)
{
   RC          rc = OK;
   UINT32         value;
   const UINT32   int_mask = ~(REF_NUM(LW_PDISP_VGA_CR_REG4_VSYNC_INT_ENAB, 1) |
                         REF_NUM(LW_PDISP_VGA_CR_REG4_RSVD, 1));
   GpuSubdevice *pSubdev = DispTest::GetBoundGpuSubdevice();

   while (count--)
   {
      // Reset and enable Vertical Interrupt
      Printf (Tee::PriNormal, "Info : Skipping Frame\n");
      Printf (Tee::PriNormal, "Info : Enable Interrupt\n");
      value = pSubdev->RegRd32 (LW_PDISP_VGA_CR_REG4);
      value = (value & int_mask) | m_int_mask;
      pSubdev->RegWr32 (LW_PDISP_VGA_CR_REG4, value);
      value = (value & int_mask) | m_int_mask | REF_NUM(LW_PDISP_VGA_CR_REG4_VSYNC_INT_ENAB, 1);
      pSubdev->RegWr32 (LW_PDISP_VGA_CR_REG4, value);

      // Wait for interrupt
      Printf (Tee::PriNormal, "Info : Wait for Interrupt\n");
      rc = DispTest::PollRegValue (LW_PDISP_VGA_GEN_REGS_INP0, 0x80, 0x80, timeoutMs);
      if (rc != OK)
         Printf (Tee::PriNormal, "Info : Timeout waiting for VINT = 1\n");
      CHECK_RC (rc);

      // Wait for VSYNC high
      Printf(Tee::PriNormal, "Info : Wait for VSYNC = 1\n");
      rc = DispTest::PollRegValue (LW_PDISP_VGA_GEN_REGS_INP1, 0x08, 0x08, timeoutMs);
      if (rc != OK)
         Printf(Tee::PriNormal, "Info : Timeout waiting for VSYNC = 1\n");
      CHECK_RC (rc);
   }

   return (OK);
}

const char* LegacyVGA::ExceptionFile(const string& efile)
{
    static string exception_file;

    if (efile == "")
        return exception_file.c_str();
    else
        exception_file = efile;
    return 0;
}

const char* LegacyVGA::SubTarget(const string& target)
{
    static string sub_target;

    if (target == "")
        return sub_target.c_str();
    else
        sub_target = target;
    return 0;
}

const FLOAT64 LegacyVGA::vga_timeout_ms(const FLOAT64 timeoutMs)
{
    static FLOAT64 vgaTimeoutMs;

    if (timeoutMs == 0)
        return vgaTimeoutMs;
    else
        vgaTimeoutMs = timeoutMs;
    return 0;
}
