/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2008, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/tests/stdtest.h"
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include "trace_3d.h"
#include "massage.h"
#include "gpu/include/gpudev.h"

#include "fermi/gf100/dev_ram.h"
#include "class/cl9097.h"
#include "teegroups.h"

#define MSGID() T3D_MSGID(SLI)

RC Trace3DTest::SetupScissor()
{
    // Configure SLI scissor

    UINT32 numSubdevs = 0;
    // Ideally we'd like to group option -sli_scissor and -sli_surfaceclip so only
    // the last option can take effect. Since we're not sure if it is doable for
    // option with more than one parameters, for now let's just make option
    // -sli_surfaceclip always take precedence.
    if (params->ParamPresent("-sli_surfaceclip"))
    {
        numSubdevs = params->ParamNArgs("-sli_surfaceclip");
        for (UINT32 i=0; i < numSubdevs; i++)
        {
            m_SLIScissorSpec.push_back(params->ParamNUnsigned("-sli_surfaceclip", i));
        }
        m_EnableSLISurfClip = true;

    }
    else if (params->ParamPresent("-sli_scissor"))
    {
        numSubdevs = params->ParamNArgs("-sli_scissor");
        for (UINT32 i=0; i < numSubdevs; i++)
        {
            m_SLIScissorSpec.push_back(params->ParamNUnsigned("-sli_scissor", i));
        }
        m_EnableSLIScissor = true;

    }
    else
    {
        return OK;
    }

    if (params->ParamPresent("-sli_sfr_subdev") > 0)
    {
        // Sanity check, this option is legal only for single GPU mode
        UINT32 fake_subdev = params->ParamUnsigned("-sli_sfr_subdev");
        if (fake_subdev + 1 > numSubdevs)
        {
            ErrPrintf("Invalid number of GPUs passed (%u) to -sli_sfr_subdev(max:%u)\n",
                fake_subdev, numSubdevs - 1);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        {
            ErrPrintf("Option -sli_sfr_subdev can only be used on single GPU mode\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        else
        {
            InfoPrintf("Option -sli_sfr_subdev is used, mods applies the clip/scissor of GPU%u"
                "in single mode\n", fake_subdev);
        }
    }
    else if (numSubdevs != GetBoundGpuDevice()->GetNumSubdevices())
    {
        ErrPrintf("Invalid number of GPUs passed (%u) to -sli_scissor parameter (%u GPUs found)\n",
            numSubdevs, GetBoundGpuDevice()->GetNumSubdevices());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    const IGpuSurfaceMgr::SLIScissorSpec scanlineCounts =
        gsm->AdjustSLIScissorToHeight(m_SLIScissorSpec, gsm->GetHeight());

    // Callwlate SLI scissor rectangles for all subdevices
    UINT32 y = 0;
    for (UINT32 j=0; j < numSubdevs; j++)
    {
        // Compute rectangle for SLI scissor
        ScissorRect rect = {0, gsm->GetWidth(), y, y+scanlineCounts[j]};
        y = rect.yMax;

        // Apply rectangle defined by regular scissor
        if (m_EnableSLIScissor && scissorEnable)
        {
            rect.xMin = scissorXmin;
            rect.xMax = scissorXmin + scissorWidth;
            if ((rect.yMin >= scissorYmin+scissorHeight) ||
                (rect.yMax <= scissorYmin))
            {
                rect.yMax = rect.yMin;
            }
            else
            {
                rect.yMin = max(rect.yMin, scissorYmin);
                rect.yMax = min(rect.yMax, scissorYmin+scissorHeight);
            }
        }

        // Save rectangle
        m_SLIScissorRects.push_back(rect);
    }

    if (m_EnableSLIScissor)
    {
        // Disable regular scissor - it's been taken care of
        scissorEnable = false;
    }

    return OK;
}

static inline UINT32 GetMethod(UINT32 subchannel, UINT32 method, UINT32 count)
{
    return
        REF_NUM(LW_FIFO_DMA_OPCODE,            LW_FIFO_DMA_OPCODE_METHOD) |
        REF_NUM(LW_FIFO_DMA_METHOD_COUNT,      count) |
        REF_NUM(LW_FIFO_DMA_METHOD_SUBCHANNEL, subchannel) |
        REF_NUM(LW_FIFO_DMA_METHOD_ADDRESS,    method >> 2) |
        REF_NUM(LW_FIFO_DMA_SEC_OP,            LW_FIFO_DMA_SEC_OP_INC_METHOD);
}

namespace {

    class InsertScissor: public Massager
    {
    public:
        InsertScissor(const vector<Trace3DTest::ScissorRect>& scissors, const int fake_subdev)
            : m_Scissors(scissors), m_FakeSubdev(fake_subdev) { }
        virtual void Massage(PushBufferMessage& message)
        {
            // Get scissor configuration from message in pushbuffer
            Trace3DTest::ScissorRect newScissor = {0, 0, 0, 0};
            bool isHorizontal = false;
            bool isVertical = false;
            bool isDisabled = false;
            UINT32 subChannel = message.GetSubChannelNum();
            for (UINT32 ipayload=0; ipayload < message.GetPayloadSize(); ipayload++)
            {
                const UINT32 method = message.GetMethodOffset(ipayload);
                if (method == LW9097_SET_SCISSOR_ENABLE(0))
                {
                    isDisabled = true;
                }
                else if (method == LW9097_SET_SCISSOR_HORIZONTAL(0))
                {
                    const UINT32 data = message.GetPayload(ipayload);
                    newScissor.xMin = DRF_VAL(9097, _SET_SCISSOR_HORIZONTAL, _XMIN, data);
                    newScissor.xMax = DRF_VAL(9097, _SET_SCISSOR_HORIZONTAL, _XMAX, data);
                    isHorizontal = true;
                }
                else if (method == LW9097_SET_SCISSOR_VERTICAL(0))
                {
                    const UINT32 data = message.GetPayload(ipayload);
                    newScissor.yMin = DRF_VAL(9097, _SET_SCISSOR_VERTICAL, _YMIN, data);
                    newScissor.yMax = DRF_VAL(9097, _SET_SCISSOR_VERTICAL, _YMAX, data);
                    isVertical = true;
                }
            }

            vector< vector<UINT32> > newMessages;

            // Set default scissors if disabled by current message
            if (isDisabled)
            {
                message.ClearMessage();
                newMessages = GetScissorRects(subChannel);
            }
            // Set new scissors honoring scissor settings from current message
            else if (isHorizontal || isVertical)
            {
                message.ClearMessage();
                newMessages = GetIntersectedScissorRects(newScissor, isHorizontal, isVertical, subChannel);
            }
            // Set initial scissors
            else if (message.GetPushBufferOffset() == 0)
            {
                newMessages = GetScissorRects(subChannel);
            }

            // Add new messages if any
            if (m_FakeSubdev > 0 && !newMessages.empty())
            {
                // If -sli_sfr_subdev is used, we apply scissor/clip of subdev n in single gpu mode
                message.AddSubdevMessage(0, newMessages[m_FakeSubdev]);
            }
            else
            {
                for (UINT32 subdev=0; subdev < newMessages.size(); subdev++)
                {
                    message.AddSubdevMessage(subdev, newMessages[subdev]);
                }
            }
        }
        vector< vector<UINT32> > GetScissorRects(UINT32 subchnum)
        {
            const Trace3DTest::ScissorRect bigrect = {0, 0, 0, 0};
            return GetIntersectedScissorRects(bigrect, false, false, subchnum);
        }
        vector< vector<UINT32> > GetIntersectedScissorRects(const Trace3DTest::ScissorRect& newRect, bool isHorizontal, bool isVertical, UINT32 subchnum)
        {
            vector< vector<UINT32> > newMessages;
            newMessages.resize(m_Scissors.size());
            for (UINT32 subdev=0; subdev < m_Scissors.size(); subdev++)
            {
                // Create new rectangle intersecting with the new one
                Trace3DTest::ScissorRect rect = m_Scissors[subdev];
                if (isHorizontal)
                {
                    if ((newRect.xMax <= rect.xMin) || (newRect.xMin >= rect.xMax))
                    {
                        rect.xMax = rect.xMin;
                    }
                    else
                    {
                        rect.xMin = max(rect.xMin, newRect.xMin);
                        rect.xMax = min(rect.xMax, newRect.xMax);
                    }
                }
                if (isVertical)
                {
                    if ((newRect.yMax <= rect.yMin) || (newRect.yMin >= rect.yMax))
                    {
                        rect.yMax = rect.yMin;
                    }
                    else
                    {
                        rect.yMin = max(rect.yMin, newRect.yMin);
                        rect.yMax = min(rect.yMax, newRect.yMax);
                    }
                }

                // Create scissor commands for this subdevice
                newMessages[subdev].push_back(
                        GetMethod(subchnum, LW9097_SET_SCISSOR_HORIZONTAL(0), 2));
                newMessages[subdev].push_back(
                        DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMIN, rect.xMin) |
                        DRF_NUM(9097, _SET_SCISSOR_HORIZONTAL, _XMAX, rect.xMax));
                newMessages[subdev].push_back(
                        DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMIN, rect.yMin) |
                        DRF_NUM(9097, _SET_SCISSOR_VERTICAL, _YMAX, rect.yMax));
                newMessages[subdev].push_back(
                        GetMethod(subchnum, LW9097_SET_SCISSOR_ENABLE(0), 1));
                newMessages[subdev].push_back(
                        LW9097_SET_SCISSOR_ENABLE_V_TRUE);
                DebugPrintf(MSGID(), "Massaging in scissor for subdevice %u: x=[%u..%u] y=[%u..%u]\n",
                        subdev, rect.xMin, rect.xMax, rect.yMin, rect.yMax);
            }
            return newMessages;
        }

    private:
        vector<Trace3DTest::ScissorRect> m_Scissors;
        int m_FakeSubdev;
    };

    class InsertSurfaceClip: public Massager
    {
    public:
        InsertSurfaceClip(const vector<Trace3DTest::ScissorRect>& clips, const int fake_subdev)
            : m_Clips(clips), m_FakeSubdev(fake_subdev) { }
        virtual void Massage(PushBufferMessage& message)
        {
            Trace3DTest::ScissorRect newClip = {0, 0, 0, 0};

            bool isHorizontal = false;
            bool isVertical = false;
            UINT32 subChannel = message.GetSubChannelNum();
            for (UINT32 ipayload=0; ipayload < message.GetPayloadSize(); ipayload++)
            {
                const UINT32 method = message.GetMethodOffset(ipayload);
                const UINT32 data = message.GetPayload(ipayload);
                switch (method)
                {
                    case LW9097_SET_SURFACE_CLIP_HORIZONTAL:
                        newClip.xMin = DRF_VAL(9097, _SET_SURFACE_CLIP_HORIZONTAL, _X, data);
                        newClip.xMax = DRF_VAL(9097, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, data) + newClip.xMin;
                        isHorizontal = true;
                        break;
                    case LW9097_SET_SURFACE_CLIP_VERTICAL:
                        newClip.yMin = DRF_VAL(9097, _SET_SURFACE_CLIP_VERTICAL, _Y, data);
                        newClip.yMax = DRF_VAL(9097, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, data) + newClip.yMin;
                        isVertical = true;
                        break;
                    default:
                        // no action
                        break;
                }
            }

            vector< vector<UINT32> > newMessages;

            if (isHorizontal || isVertical)
            {
                message.ClearMessage();
                newMessages = GetIntersectedClips(newClip, isHorizontal, isVertical, subChannel);
            }
            else if (message.GetPushBufferOffset() == 0)
            {
                // Init methods
                newMessages = GetIntersectedClips(newClip, false, false, subChannel);
            }

            // Add new messages if any
            if (m_FakeSubdev > 0 && !newMessages.empty())
            {
                // If -sli_sfr_subdev is used, we apply scissor/clip of subdev n in single gpu mode
                message.AddSubdevMessage(0, newMessages[m_FakeSubdev]);
            }
            else
            {
                for (UINT32 subdev=0; subdev < newMessages.size(); subdev++)
                {
                    message.AddSubdevMessage(subdev, newMessages[subdev]);
                }
            }
        }

        vector< vector<UINT32> > GetIntersectedClips(const Trace3DTest::ScissorRect& newRect,
            bool isHorizontal, bool isVertical, UINT32 subchnum)
        {
            vector< vector<UINT32> > newMessages;
            newMessages.resize(m_Clips.size());
            for (UINT32 subdev=0; subdev < m_Clips.size(); subdev++)
            {
                // Create new rectangle intersecting with the new one
                Trace3DTest::ScissorRect rect = m_Clips[subdev];
                if (isHorizontal)
                {
                    if ((newRect.xMax <= rect.xMin) || (newRect.xMin >= rect.xMax))
                    {
                        rect.xMax = rect.xMin;
                    }
                    else
                    {
                        rect.xMin = max(rect.xMin, newRect.xMin);
                        rect.xMax = min(rect.xMax, newRect.xMax);
                    }
                }
                if (isVertical)
                {
                    if ((newRect.yMax <= rect.yMin) || (newRect.yMin >= rect.yMax))
                    {
                        rect.yMax = rect.yMin;
                    }
                    else
                    {
                        rect.yMin = max(rect.yMin, newRect.yMin);
                        rect.yMax = min(rect.yMax, newRect.yMax);
                    }
                }

                // Create scissor commands for this subdevice
                newMessages[subdev].push_back(
                    GetMethod(subchnum, LW9097_SET_SURFACE_CLIP_HORIZONTAL, 2));
                newMessages[subdev].push_back(
                    DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _X, rect.xMin) |
                    DRF_NUM(9097, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, rect.xMax-rect.xMin));
                newMessages[subdev].push_back(
                    DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _Y, rect.yMin) |
                    DRF_NUM(9097, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, rect.yMax-rect.yMin));
                DebugPrintf(MSGID(), "Massaging in surface clip for subdevice %u: x=[%u..%u] y=[%u..%u]\n",
                    subdev, rect.xMin, rect.xMax-rect.xMin, rect.yMin, rect.yMax-rect.yMin);
            }
            return newMessages;
        }

    private:
        vector<Trace3DTest::ScissorRect> m_Clips;
        int m_FakeSubdev;
    };
}

void Trace3DTest::ApplySLIScissorToChannel(TraceChannel* pTraceChannel)
{
    if (m_EnableSLIScissor || m_EnableSLISurfClip)
    {
        int fake_subdev = -1;
        if (params->ParamPresent("-sli_sfr_subdev") > 0)
        {
            fake_subdev = (int)(params->ParamUnsigned("-sli_sfr_subdev"));
        }

        TraceModules::iterator iMod;
        for (iMod = pTraceChannel->ModBegin(); iMod != pTraceChannel->ModEnd(); ++iMod)
        {
            DebugPrintf(MSGID(), "MassagePushBuffer: Trace3DTest::ApplySLIScissor()\n");
            unique_ptr<Massager> sli_massager(
                m_EnableSLISurfClip?((Massager*)new InsertSurfaceClip(m_SLIScissorRects, fake_subdev)):
                ((Massager*)new InsertScissor(m_SLIScissorRects, fake_subdev)));

            (*iMod)->MassagePushBuffer(*sli_massager);
            (*iMod)->PrintPushBuffer("SLIPB", true);
        }
    }
}

void Trace3DTest::ApplySLIScissor()
{
    // Massage pushbuffer of every tracemodule on every channel
    TraceChannels::iterator iCh;
    for (iCh = m_TraceCpuManagedChannels.begin();
         iCh != m_TraceCpuManagedChannels.end();
         ++iCh)
    {
        ApplySLIScissorToChannel(*iCh);
    }
}
