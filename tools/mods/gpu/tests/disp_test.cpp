/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
//      disp_test.cpp - DispTest class / namespace definitions
//      Copyright (c) 1999-2012 by LWPU Corporation.
//      All rights reserved.
//
//      Written by: Matt Craighead, et al.
//      Date:       29 July 2004
//
//      Routines in this module:
//      Frame Class Methods:
//      ~Frame                          Frame Deconstructor; frees allocated resources
//      AddPixel                        Adds a pixel to the frame
//      Data                            Returns the current frame ptr; allocates and initializes one if necessary
//      SanityCheck                     Checks if all pixels are present
//
//      RTLOutput Class methods:
//      ~RTLOutput                      RTLOutput Deconstructor; closes the file and frees allocated resources
//      GetFirst                        Get the pixels in the frame
//      GetNext                         Get the next pixels in the frame
//
//      DispTest Namespace Methods / Routines:
//      SetGlobalTimeout                Sets the GlobalTimeoutMs value in the namespace
//      GetGlobalTimeout                Gets the value of the GlobalTimeoutMs variable
//      SetDebugMessageDisplay          Turn on/off verbose debug messages during test exelwtion
//      Initialize                      Allocate Root, Device, Evo Object, Memory and create a CtxDMA for error notification and reporting for the RM
//      Setup                           Allocate Memory for the Channel PushBuffer and Create a CtxDma and allocate the Channel from RM (This includes connecting the push buffer)
//      Release                         Release all the allocated objects (Notifiers, IsoSurfaces, Semaphores, etc) that have bound to the channel
//      SetPioChannelTimeout            Set the default timeout for this PIO channel operations
//      SwitchToChannel                 Switch to the given display channel
//      IdleChannelControl              Manage the idle channel settings
//      SetChnStatePollNotif            Set Channel State for Poll Notifier
//      SetChnStatePollNotifBlocking    Set Channel Poll Notifier to block RM
//      EnqMethod                       Enqueue Method into the Channel PushBuffer and enqueue the Data associated with the Method
//      EnqUpdateAndMode                Enqueue Method into the Channel PushBuffer; enqueue the Data associated with the Method; set the HeadMode field in the crc_head_info structure
//      EnqCoreUpdateAndMode            Enqueue Method into the Channel PushBuffer; enqueue the Data associated with the Method; set the HeadMode field in the crc_head_info structure
//      EnqMethodMulti                  Enqueue Method into the Channel PushBuffer and enqueue the Data associated with the Method
//      EnqMethodNonInc                 Enqueue Method into the Channel PushBuffer and enqueue the Data associated with the Method (non-incrementing)
//      EnqMethodNonIncMulti            Enqueue Method into the Channel PushBuffer and enqueue the Data associated with the Method (non-incrementing)
//      EnqMethodNop                    Enqueue a Nop Method into the Channel PushBuffer
//      EnqMethodJump                   Enqueue a Jump Method into the Channel PushBuffer
//      EnqMethodSetSubdevice           Enqueue a SetSubDevice Method into the Channel PushBuffer
//      SetSwapHeads                    Set the swap heads setting for the current device
//      HandleSwapHead                  If DoSwapHeads is true, massages the Method and Data to swap head 0 and head 1
//      SetUseHead                      Set the head setting for the current device
//      HandleUseHead                   If UseHead is true, massages the Method and Data to use vga_head
//      EnqMethodOpcode                 Enqueue Method into the Channel PushBuffer and enqueue the Data associated with the Method
//      SetAutoFlush                    Update the Push Buffer Put Pointer
//      Flush                           Update the Push Buffer Put Pointer
//      AddDmaContext                   Set the DMA context for the current device
//      GetDmaContext                   Get the DMA context from the current device
//      AddChannelContext               Add a new channel context to the current device
//      GetChannelContext               Get the channel context from the current device
//      SearchList                      Determine if the given list contains the given element
//      CreateNotifierContextDma        Allocate Memory and create a CtxDMA
//      CreateSemaphoreContextDma       Allocate Memory and create a CtxDMA for the Semaphore
//      CreateContextDma                Allocate Memory and create a CtxDMA
//      DeleteContextDma                Release the handle and free the allocated memory
//      SetContextDmaDebugRegs          Write the Context Dma Params to the Debug Registers
//      GetChannelNum                   Get the channel number based on the given channel class and channel instance
//      WriteVal32                      Resolve the Ctx DMA (Get Base and Size) and write Value at Base+Offset (Check if Offset < Size)
//      ReadVal32                       Resolve the Ctx DMA (Get Base and Size) and read from Base+Offset (Check if Offset < Size) and return the Value
//      Cleanup                         Deallocate memory used for DispTest data structures
//      IsoDumpImages                   Saves output produced by rtl into tga files
//      P4Action                        Performs a Perforce (p4) system command
//      RmaRegWr32                      Performs 32-bit RMA writes in order to access priv regs
//      RmaRegRd32                      Performs 32-bit RMA reads in order to access priv regs
//      SkipDsiSupervisor2EventHandler  Event handler helper function for "SetSkipDsiSupervisor2Event"
//      SetSkipDsiSupervisor2Event      Set LW_CFGEX_EVENT_HANDLE_SKIP_DSI_SUPERVISOR_2_EVENT
//      SupervisorRestartEventHandler   Event handler helper function for "SetSupervisorRestartMode"
//      SetSupervisorRestartMode        Set SUPERVISOR_RESTART_EVENT_HANDLE
//      GetSupervisorRestartMode        Retrieve current setting of the supervisor restart mode
//      ExceptionRestartEventHandler    Event handler helper function for "SetExceptionRestartMode"
//      SetExceptionRestartMode         Set LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE
//      GetExceptionRestartMode         Set LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE and return retrieved values
//      GetInternalTVRasterTimings      Retrieve the current TV raster timings
//      SetVPLLRef                      Set the VPLL clock reference
//      SetVPLLArchType                 Set LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE
//      ControlSetSorSequence           Set LW5070_CTRL_CMD_SET_SOR_SEQ_CTL
//      ControlGetSorSequence           Set LW5070_CTRL_CMD_GET_SOR_SEQ_CTL and return retrieved values
//      ControlSetPiorSequence          Set LW5070_CTRL_CMD_SET_PIOR_SEQ_CTL
//      ControlGetPiorSequence          Set LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL and return retrieved values
//      ControlSetSorOpMode             Set LW5070_CTRL_CMD_SET_SOR_OP_MODE
//      ControlSetPiorOpMode            Set LW5070_CTRL_CMD_SET_PIOR_OP_MODE
//      ControlGetPiorOpMode            Set LW5070_CTRL_CMD_GET_PIOR_OP_MODE and return retrieved values
//      ControlSetDacConfig             Set LW5070_CTRL_CMD_SET_DAC_CONFIG
//      ControlSetErrorMask             Set LW5070_CTRL_CMD_SET_ERRMASK
//      ControlSetSorPwmMode            Set LW5070_CTRL_CMD_SET_SOR_PWM
//      ControlGetDacPwrMode            Set LW5070_CTRL_CMD_GET_DAC_PWR
//      ControlSetDacPwrMode            Set LW5070_CTRL_CMD_SET_DAC_PWR
//      ControlSetRgUnderflowProp       Set LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP
//      ControlSetSemaAcqDelay          Set LW5070_CTRL_CMD_SET_SEMA_ACQ_DELAY
//      ControlGetSorOpMode             Set LW5070_CTRL_CMD_GET_SOR_OP_MODE and return retrieved values
//      ControlSetRgFliplockProp        Set LW5070_CTRL_CMD_SET_RG_FLIPLOCK_PROP
//      ControlGetPrevModeSwitchFlags   Set LW5070_CTRL_CMD_GET_PREV_MODESWITCH_FLAGS
//      GetHDMICapableDisplayIdForSor   Returns the display ID which is capable of HDMI and uses the given sor
//      SetHdmiEnable                   Sets HDMI enable/disble for a given displayId
//      ControlGetDacLoad               Set LW5070_CTRL_CMD_GET_DAC_LOAD
//      ControlSetDacLoad               Set LW5070_CTRL_CMD_SET_DAC_LOAD
//      ControlGetOverlayFlipCount      Set LW5070_CTRL_CMD_GET_OVERLAY_FLIPCOUNT
//      ControlSetOverlayFlipCount      Set LW5070_CTRL_CMD_SET_OVERLAY_FLIPCOUNT
//      ControlSetDmiElv                Set LW5070_CTRL_CMD_SET_DMI_ELV
//      GetHDMICapableDisplayIdForSor   Returns the display ID which is capable of HDMI and uses the given sor
//      SetHdmiEnable                   Sets HDMI enable/disble for a given displayId
//      GetHDMICapableDisplayIdForSor   Returns the display ID which is capable of HDMI and uses the given sor
//      SetHdmiEnable                   Sets HDMI enable/disble for a given displayId
//      SetSkipFreeCount                Set the skip free count on the given channel
//      VbiosAttentionEventHandler      Event handler for "ControlSetVbiosAttentionEvent"
//      ControlSetVbiosAttentionEvent   Set LW5070_CTRL_CMD_SET_VBIOS_ATTENTION_EVENT
//      LowPower                        Enable power management
//      LowerPower                      Enable power management
//      LowestPower                     Enable power management
//      BlockLevelClockGating           No op
//      SlowLwclk                       Set the LWCLK to slow mode
//      ControlPrepForResumeFromUnfrUpd Call LW5070_CTRL_CMD_PREP_FOR_RESUME_FROM_UNFRIENDLY_UPDATE
//      ControlSetUnfrUpdEvent          Set LW5070_CTRL_CMD_SET_UNFR_UPD_EVENT
//      UnfrUpdEventHandler             Event handler for "ControlSetUnfrUpdEvent"
//      SetInterruptHandlerName         Set custom interrupt handler function name
//      GetInterruptHandlerName         Get custom interrupt handler function name
//
//      Helper functions in this module:
//      AllocVidHeapMemory              Video heap memory allocation
//      AllocSystemMemory               System memory allocation
//      CreateVidHeapContextDma         Context DMA allocation on video heap
//      CreateSystemContextDma          Context DMA allocation in system memory.
//      TimeSlotsCtl                    Sets a boolean value that controls if SF & BFM will be updated with new timeslots when add_stream is called
//      ActivesymCtl                    Sets a boolean value that controls if LW_PDISP_SF_DP_CONFIG_ACTIVESYM_CNTL_MODE has to be set to DISABLE. Supported only in DTB
//      UpdateTimeSlots                 SF & BFM registers will be updated with new timeslots parameters
//      sorSetFlushMode                 Enable or disable flush mode in SOR. This is used if RM wants to link train without disconnecting SOR from head. Bug : 562336
//      InstallErrorLoggerFilter        Allow tests to add a ErrorLogger filter to skip expected interrupts from asserting
//
//

#include "Lwcm.h"
#include "lwcmrsvd.h"

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/dispchan.h"
#include "core/include/color.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "core/include/imagefil.h"
#include "random.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/utility/surf2d.h"
#include "core/include/evntthrd.h"
#include "core/include/cmdline.h"
#include <math.h>
#include <list>
#include <map>
#include <errno.h>
//#include <direct.h>

#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/js/dispt_sr.h"
#else
#include "gpu/js/dispt_sr.h"
#endif

#include "core/utility/errloggr.h"

#include "kepler/gk104/dev_therm.h"

#include "class/cl507a.h"
#include "class/cl507b.h"
#include "class/cl507c.h"
#include "class/cl507d.h"
#include "class/cl507e.h"
#include "ctrl/ctrl5070.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0073.h"

#include "class/cl8270.h"
#include "class/cl827a.h"
#include "class/cl827b.h"
#include "class/cl827c.h"
#include "class/cl827d.h"
#include "class/cl827e.h"

#include "class/cl8370.h"
#include "class/cl837c.h"
#include "class/cl837d.h"
#include "class/cl837e.h"

#include "class/cl8870.h"
#include "class/cl887d.h"

#include "class/cl8570.h"
#include "class/cl857a.h"
#include "class/cl857b.h"
#include "class/cl857c.h"
#include "class/cl857d.h"
#include "class/cl857e.h"

#include "class/cl9070.h"
#include "class/cl907a.h"
#include "class/cl907b.h"
#include "class/cl907c.h"
#include "class/cl907d.h"
#include "class/cl907e.h"
#include "class/cl0073.h"

#include "class/cl9170.h"
#include "class/cl917a.h"
#include "class/cl917b.h"
#include "class/cl917c.h"
#include "class/cl917d.h"
#include "class/cl917e.h"

#include "class/cl9270.h"
#include "class/cl927c.h"
#include "class/cl927d.h"

#include "class/cl9470.h"
#include "class/cl947d.h"

#include "class/cl9570.h"
#include "class/cl957d.h"

#include "class/cl9770.h"
#include "class/cl977d.h"

#include "class/cl9870.h"
#include "class/cl9878.h"
#include "class/cl987d.h"

#define DISPTEST_PRIVATE
#include "core/include/disp_test.h"
#include "gpu/include/vgacrc.h"

//the headers for all the class specific implementations of DispTest::Display
#ifdef USE_DTB_DISPLAY_CLASSES
#include "gpu/tests/disptest/display_01xx.h"
#include "gpu/tests/disptest/display_015x.h"
#include "gpu/tests/disptest/display_02xx.h"
#include "gpu/tests/disptest/display_021x.h"
#include "gpu/tests/disptest/display_022x.h"
#include "gpu/tests/disptest/display_024x.h"
#include "gpu/tests/disptest/display_025x.h"
#include "gpu/tests/disptest/display_027x.h"
#include "gpu/tests/disptest/display_028x.h"
#else
#include "gpu/tests/disptest/display_01xx.h"
#include "gpu/tests/disptest/display_015x.h"
#include "gpu/tests/disptest/display_02xx.h"
#include "gpu/tests/disptest/display_021x.h"
#include "gpu/tests/disptest/display_022x.h"
#include "gpu/tests/disptest/display_024x.h"
#include "gpu/tests/disptest/display_025x.h"
#include "gpu/tests/disptest/display_027x.h"
#include "gpu/tests/disptest/display_028x.h"
#endif
/*
#include "gpu/tests/disptest/display_03xx.h"
.
.
.
*/

#ifdef USE_DTB_DISPLAY_CLASSES
#include "disptest/display_02xx_dtb.h"
#include "disptest/display_021x_dtb.h"
#include "disptest/display_022x_dtb.h"
#include "disptest/display_024x_dtb.h"
#include "disptest/display_025x_dtb.h"
#include "disptest/display_027x_dtb.h"
#include "disptest/display_028x_dtb.h"
#endif

UINT32 DispTest::activeSubdeviceIndex = 0;    // active subdevice for control call

///a vector of pointers to the display devices
std::vector<DispTest::Display *> DispTest::m_display_devices;

////////////////////////////////////////////////////////////////////////////////////////////
//
//  Frame - Class definition, data structures, defines and method definitions
//
//  Notes:  Used by the RTLOutput class and by "DispTest:IsoDumpImages"
//
class Frame
{
public:

    Frame();                                                            // Constructor
    ~Frame();                                                           // Deconstructor
    void AddPixel(UINT32 x, UINT32 y, UINT32 r, UINT32 g, UINT32 b);    // Adds a pixel to the frame
    bool SanityCheck() const;                                           // Checks if all pixels are present
    UINT32 Width() const {return m_width+1;}                            // Returns current frame width
    UINT32 Height() const {return m_height+1;}                          // Returns current frame height
    void*  Data();                                                      //
    void    SetFormat(ColorUtils::Format);                              // asserts if format is set incompatible
    ColorUtils::Format Format()     { return m_format; }

private:
    ColorUtils::Format m_format;
struct Pixel
{
    UINT16  m_r;
    UINT16  m_g;
    UINT16  m_b;

    Pixel()
    {
        m_r = m_g = m_b = 0;
    }
    Pixel(const Pixel& pixel)
    {
        (*this) = pixel;
    }
    Pixel(UINT32 r, UINT32 g, UINT32 b)
    {
        m_r = r;
        m_g = g;
        m_b = b;
    }

    Pixel& operator= (const Pixel& pixel)
    {
        m_r = pixel.m_r;
        m_g = pixel.m_g;
        m_b = pixel.m_b;

        return *this;
    }
};
    typedef map<UINT32, Pixel>      LineType;                           // Pixels have format in variable Format
    typedef map<UINT32, LineType>   FrameType;

    UINT32 m_width;
    UINT32 m_height;
    UINT32* m_data;
    FrameType m_pixels;
};

Frame::Frame() : m_width(0), m_height(0), m_data(0)
{
    // default is 10bpc format
    m_format = ColorUtils::A2B10G10R10;
}

/****************************************************************************************
 * Frame::~Frame
 *
 *  Functional Description:
 *  - Frame Deconstructor; frees allocated resources
 ****************************************************************************************/
Frame::~Frame()
{
    if (m_data)
        delete[] m_data;
}

void Frame::SetFormat(ColorUtils::Format format)
{
    //
    // if we're already using a 16bpc format don't go back to a 10bpc format
    if(m_format != ColorUtils::RU16_GU16_BU16_AU16)
        m_format = format;
}

/****************************************************************************************
 * Frame::AddPixel
 *
 *  Arguments Description:
 *  - x:     X coordinate within the frame
 *  - y:     Y coordinate within the frame
 *  - color: Pixel color
 *
 *  Functional Description:
 *  - Adds a pixel to the frame
 ****************************************************************************************/
void Frame::AddPixel(UINT32 x, UINT32 y, UINT32 r, UINT32 g, UINT32 b)
{
    m_width = m_width > x ? m_width : x;
    m_height= m_height> y ? m_height: y;
    m_pixels[y][x] = Pixel(r,g,b);
}

/****************************************************************************************
 * Frame::Data
 *
 *  Functional Description:
 *  - Returns the current frame ptr; allocates and initializes one if necessary
 ****************************************************************************************/
void* Frame::Data()
{
    if (!m_data)
    {
        // assumed default is ColorUtils::A2B10G10R10
        int bpp = 4;
        if(m_format == ColorUtils::RU16_GU16_BU16_AU16)
            bpp = 8;
        m_data = new UINT32[Width() * Height() * bpp / 4];
        for (FrameType::const_iterator j = m_pixels.begin(); j != m_pixels.end(); ++j)
        {
            for (LineType::const_iterator i = j->second.begin(); i != j->second.end(); ++i)
            {
                switch(m_format)
                {
                case ColorUtils::A2B10G10R10:
                    m_data[j->first*Width() + i->first] = (i->second.m_b << 20) | (i->second.m_g << 10) | (i->second.m_r);
                    break;
                case ColorUtils::RU16_GU16_BU16_AU16:
                    {
                        m_data[j->first*Width() * 2 + i->first * 2 + 1] = i->second.m_b;
                        m_data[j->first*Width() * 2 + i->first * 2] = (i->second.m_g << 16) | i->second.m_r;
                    }
                    break;
                default:
                    // unsupported pixel format
                    MASSERT(!"Unsupported pixel format.");
                }
            }
        }
    }

    return (void*) m_data;
}

/****************************************************************************************
 * Frame::SanityCheck
 *
 *  Functional Description:
 *  - Checks if all pixels are present
 ****************************************************************************************/
bool Frame::SanityCheck() const
{
    UINT32 x_sum = 0;
    UINT32 y_sum = 0;
    for (FrameType::const_iterator j = m_pixels.begin(); j != m_pixels.end(); ++j)
    {
        y_sum += j->first;
        x_sum = 0;
        for (LineType::const_iterator i = j->second.begin(); i != j->second.end(); ++i)
            x_sum += i->first;
        if (x_sum != m_width*(m_width+1)/2) // sum of arithmetic progression
        {
            Printf(Tee::PriError, "Frame::SanityCheck - row %d does not have all pixels\n", j->first);
            return false;
        }
    }
    if (y_sum != m_height*(m_height+1)/2) // sum of arithmetic progression
    {
        Printf(Tee::PriError, "Frame::SanityCheck - a frame has missing lines\n");
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
//  RTLOutput - Class definition, data structures, defines and method definitions
//
//  Notes:  Depends on the Frame class and is used by "DispTest:IsoDumpImages"
//
class RTLOutput
{
public:
    RTLOutput(const string filename) : m_filename(filename),
                                       m_file(0),
                                       m_buff(new char[line_size])
                                       {}                           // Constructor
    ~RTLOutput();                                                   // Deconstructor
    Frame* GetFirst();                                              // Get the pixels in the frame
    Frame* GetNext();                                               // Get the next pixels in the frame

private:
    static const int line_size = 256;

    string m_filename;
    FILE* m_file;
    char* m_buff;
};

/****************************************************************************************
 * RTLOutput::~RTLOutput
 *
 *  Functional Description:
 *  - RTLOutput Deconstructor; closes the file and frees allocated resources
 ****************************************************************************************/
RTLOutput::~RTLOutput()
{
    if (m_file)
        fclose(m_file);
    if (m_buff)
        delete[] m_buff;
}

/****************************************************************************************
 * RTLOutput::GetFirst
 *
 *  Functional Description:
 *  - Get the pixels in the frame
 ****************************************************************************************/
Frame* RTLOutput::GetFirst()
{
    // reset
    if (m_file)
        fclose(m_file);

    if (OK != Utility::OpenFile(m_filename, &m_file, "r"))
    {
        m_file = 0;
        return 0;
    }

    // reading the first pixel
    if (0 == fgets(m_buff, line_size, m_file))
    {
        Printf(Tee::PriWarn, "Can't read first pixel from file %s\n",
                m_filename.c_str());
    }

    return GetNext();
}

/****************************************************************************************
 * RTLOutput::GetNext
 *
 *  Functional Description:
 *  - Get the next pixels in the frame
 ****************************************************************************************/
Frame* RTLOutput::GetNext()
{
    if (!m_file)
        return 0;
    // RTLOutput file is a text file, containing one pixel per line
    // here is an example:
    // @ rgen_2orif i 0 1023 2 1 1023 1023 1023        # (143847 ns)
    // which describes a pixel with (x, y) = (2, 1) having max components
    // in all three channels

    // m_buff always contains the first pixel of the frame returned by GetNext()
    // that allows not to use fseek

    Frame* frame = new Frame;
    UINT32 scale, x, y, r, g, b;
    char dumb[32];
    char* more_pixels = m_buff;
    bool frame_is_not_done = true;
    for (UINT32 i = 0; frame_is_not_done; ++i)
    {
        // extracting pixel parameters from the line
        sscanf(m_buff, "@ %s i 0 %d %d %d %d %d %d", dumb, &scale, &x, &y, &r, &g, &b);

        // supports 10bpc and 16bpc
        frame->SetFormat(scale == 1023  ? ColorUtils::A2B10G10R10 :
                         scale == 4095  ? ColorUtils::RU16_GU16_BU16_AU16 :
                         scale == 65535 ? ColorUtils::RU16_GU16_BU16_AU16 :
                                          ColorUtils::LWFMT_NONE);

        //
        // rtl uses the scale that fits the content
        // so need to upscale if we've started using 16bpc color format
        if(scale == 1023 && frame->Format() == ColorUtils::RU16_GU16_BU16_AU16)
        {
            r = r << 6;
            g = g << 6;
            b = b << 6;
        }
        //
        // we're using 16bpc color format above for 12bpc images ... but scale them up here
        if(scale == 4095)
        {
            r = r << 4;
            g = g << 4;
            b = b << 4;
        }

        frame->AddPixel(x, y, r, g, b);

        more_pixels = fgets(m_buff, line_size, m_file);

        sscanf(m_buff, "@ %s i 0 %d %d %d %d %d %d", dumb, &scale, &x, &y, &r, &g, &b);
        frame_is_not_done = (i == 0 || x != 0 || y != 0) && more_pixels;
    }

    if (!more_pixels)  // from now on GetNext() returns 0
    {                  // untill next GetFirst() call
        fclose(m_file);
        m_file = 0;
    }

    if (!frame->SanityCheck())
    {
        delete frame;
        return 0;
    }

    return frame;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
//  DispTest - Namespace definition, data structures, and API prototypes (method definitions)methods
//
namespace DispTest
{
    std::vector<DispTestDeviceInfo*> m_DeviceInfo;
    std::map<DispTestDeviceInfo*, int> m_DeviceIndex;

    int m_nLwrrentDevice = 0;
    int m_nNumDevice = 0;

    bool m_bLogicHeadsSupported = false;          // logical head support definition
    /*
     * variables global to all devices.
     */

    /* DispTest Timeout */
    FLOAT64 GlobalTimeoutMs = 1000;

    /* display verbose debug messages? */
    bool debug_messages = false;

    GpuDevice    *m_pGpuDevice = NULL;
    GpuSubdevice *m_pGpuSubdevice = NULL;

    /* for auto clk gating - used only in DTB*/
    UINT32 Head0DClkMode = 0xFFFFFFFF;
    UINT32 Head1DClkMode = 0xFFFFFFFF;
    UINT32 Head2DClkMode = 0xFFFFFFFF;
    UINT32 Head3DClkMode = 0xFFFFFFFF;
    UINT32 PClkMode = 0xFFFFFFFF;

    // Used SkipDsiSupervisor2Event callback
    string s_SkipDsiSupervisor2EventJSFunc;
    EventThread s_SkipDsiSupervisor2EventThread;

    string s_SupervisorRestartEventJSFunc;
    EventThread s_SupervisorRestartEventThread;

    string s_ExceptionRestartEventJSFunc;
    EventThread s_ExceptionRestartEventThread;

    // Used by the callback function for VBIOS attention event
    string s_SetVbiosAttentionEventJSFunc;
    EventThread s_VbiosAttentionEventThread;

    // Used by UnfrUpdEventHandler
    string s_SetUnfrUpdEventJSFunc;
    EventThread s_UnfrUpdEventThread;

    /*
     * internal methods
     */
    //moved to header file for class migration
    //RC GetChannelNum(UINT32 channelClass, UINT32 channelInstance, UINT32 *channelNum);

    /*
     * API methods
     */
    DispTestDeviceInfo* LwrrentDevice()
    {
        return ((int)m_DeviceInfo.size() > m_nLwrrentDevice) ?
            m_DeviceInfo[m_nLwrrentDevice] : NULL;
    }
    DispTestDeviceInfo* GetDeviceInfo(int index)
    {
        return ((int)m_DeviceInfo.size() > index) ?
            m_DeviceInfo[index] : NULL;
    }

    RC BindDevice(UINT32 index)
    {
        if ((index != (UINT32) m_nLwrrentDevice) ||
            (m_pGpuDevice == NULL) || (m_pGpuSubdevice == NULL))
        {
            RC rc;
            GpuDevMgr *pDevMgr = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;

            m_nLwrrentDevice = index;
            CHECK_RC(pDevMgr->GetDevice(m_nLwrrentDevice, (Device **)&m_pGpuDevice));
            m_pGpuSubdevice = m_pGpuDevice->GetSubdevice(Gpu::MasterSubdevice-1);
        }

        return OK;
    };

    // tells us what disptest initialized the display class to.
    UINT32 LwrrentDisplayClass() { return LwrrentDevice()->unInitializedClass; };

    bool bIsCoreChannel(UINT32 channelClass) {
        UINT32 lwrrDispClass = LwrrentDisplayClass();
        return ( ( lwrrDispClass == LW9870_DISPLAY && channelClass == LW987D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW9770_DISPLAY && channelClass == LW977D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW9570_DISPLAY && channelClass == LW957D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW9470_DISPLAY && channelClass == LW947D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW9270_DISPLAY && channelClass == LW927D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW9170_DISPLAY && channelClass == LW917D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW9070_DISPLAY && channelClass == LW907D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == GT214_DISPLAY  && channelClass == LW857D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == G82_DISPLAY    && channelClass == LW827D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == GT200_DISPLAY  && channelClass == LW837D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == G94_DISPLAY    && channelClass == LW887D_CORE_CHANNEL_DMA ) ||
                 ( lwrrDispClass == LW50_DISPLAY   && channelClass == LW507D_CORE_CHANNEL_DMA ) );
    }

    // Helper function prototypes
    RC AllocVidHeapMemory(UINT32 *pRtnMemoryHandle, void **pBase, UINT64 Size, UINT32 Type, UINT32 Attr, UINT64 * pOffset);
    RC AllocSystemMemory(string MemoryLocation, UINT32 *pRtnMemoryHandle, void **pBase, UINT32 Size);
    RC CreateVidHeapContextDma(LwRm::Handle ChannelHandle, UINT32 Flags, UINT64 Size, UINT32 MemType, UINT32 MemAttr, LwRm::Handle *pRtnHandle);
    RC CreateSystemContextDma(LwRm::Handle ChannelHandle, UINT32 Flags, UINT32 Size, string MemoryLocation, LwRm::Handle *pRtnHandle);

    RC CreateVidHeapContextDma(LwRm::Handle ChannelHandle, UINT32 Flags, UINT64 Size, UINT32 MemType, UINT32 MemAttr, LwRm::Handle *pRtnHandle, bool bindCtx);
    RC CreateSystemContextDma(LwRm::Handle ChannelHandle, UINT32 Flags, UINT32 Size, string MemoryLocation, LwRm::Handle *pRtnHandle, bool bindCtx);

    // API variables
    template <class T> bool SearchList ( T* , list<T> *);

    // Local Polling routines

    // DPU Ucode addresses
    UINT64 UcodePhyAddr = 0x0;

}

////////////////////////////////////////////////////////////////////////////////////////////
//
//  DispTest methods
//

/****************************************************************************************
 * DispTest::SetGlobalTimeout
 *
 *  Arguments Description:
 *  - timeout_ms: Value of the timeout
 *
 *  Functional Description:
 *  - Sets the GlobalTimeoutMs value in the namespace
 ****************************************************************************************/
RC DispTest::SetGlobalTimeout(FLOAT64 timeoutMs)
{
    GlobalTimeoutMs = timeoutMs;
    return OK;
}

/****************************************************************************************
 * DispTest::GetGlobalTimeout
 *
 *  Arguments Description:
 *  -
 *
 *  Functional Description:
 *  - Gets the value of the GlobalTimeoutMs variable
 ****************************************************************************************/
FLOAT64 DispTest::GetGlobalTimeout()
{
    return DispTest::GlobalTimeoutMs;
}

/****************************************************************************************
 * DispTest::SetDClkMode
 *
 *  Arguments Description:
 *  - head: head index
 *  - clkMode: Value of head's clk mode.
 *
 *  Functional Description:
 *  - Sets the HeadxDClkMode value in namespace
 ****************************************************************************************/

RC DispTest::SetDClkMode(UINT32 head, UINT32 clkMode)
{
    switch (head) {
            case 0: DispTest::Head0DClkMode = clkMode;
              break;
            case 1: DispTest::Head1DClkMode = clkMode;
              break;
            case 2: DispTest::Head2DClkMode = clkMode;
              break;
            case 3: DispTest::Head3DClkMode = clkMode;
              break;
            default: DispTest::Head0DClkMode = clkMode;
               break;
    }
    return OK;
}

/****************************************************************************************
 * DispTest::GetDClkMode
 *
 *  Arguments Description:
 *  - head: head index
 *  - clkmode: clkmode for that head.
 *
 *  Functional Description:
 *  - Gets the value of the head's DClkMode variable
 ****************************************************************************************/

RC DispTest::GetDClkMode(UINT32 head, UINT32* clkmode)
{
    UINT32 clkmode_tmp;
    switch(head) {
      case 0: clkmode_tmp = DispTest::Head0DClkMode;
              break;
      case 1: clkmode_tmp = DispTest::Head1DClkMode;
              break;
      case 2: clkmode_tmp = DispTest::Head2DClkMode;
              break;
      case 3: clkmode_tmp = DispTest::Head3DClkMode;
              break;
      default: clkmode_tmp = DispTest::Head0DClkMode;
               break;
    }
    *clkmode = clkmode_tmp;
    return OK;
}

/****************************************************************************************
 * DispTest::SetPClkMode
 *
 *  Arguments Description:
 *  - clkMode: Value of pclk mode.
 *
 *  Functional Description:
 *  - Sets the PClkMode value in namespace
 ****************************************************************************************/

RC DispTest::SetPClkMode(UINT32 clkMode)
{
    PClkMode = clkMode;
    return OK;
}

/****************************************************************************************
 * DispTest::GetPClkMode
 *
 *  Arguments Description:
 *  -
 *
 *  Functional Description:
 *  - Gets the value of the PClkMode variable
 ****************************************************************************************/
UINT32 DispTest::GetPClkMode()
{
    return DispTest::PClkMode;
}

/****************************************************************************************
 * DispTest::SetDebugMessageDisplay
 *
 *  Arguments Description:
 *  - Verbose : true iff verbose messages are desired
 *
 *  Functional Description:
 *  - Turn on/off verbose debug messages during test exelwtion
 ****************************************************************************************/
RC DispTest::SetDebugMessageDisplay(bool Verbose)
{
    DispTest::debug_messages = Verbose;
    return OK;
}

/****************************************************************************************
 * DispTest::Initialize
 *
 *  Arguments Description:
 *  - TestName : The test being created
 *  - SubTestName : Name of the subtest
 *
 *  Functional Description:
 *  - Allocate Root
 *  - Allocate Device
 *  - Allocate Evo Object
 *  - Allocate Memory and create a CtxDMA for error notification and reporting for the RM
 ****************************************************************************************/
RC DispTest::Initialize(string TestName, string SubTestName)
{
    RC rc = OK;

    // Lots of display tests have JS surface2D leaks. So enable garbage collector
    JSSurf2DTracker::EnableGarbageCollector();

    // initialize LwRm
    GpuDevMgr *pGpuDevMgr = (GpuDevMgr *)DevMgrMgr::d_GraphDevMgr;
    LwRmPtr pLwRm;

    MASSERT(GpuPtr()->IsInitialized() == true);

    m_nNumDevice = 0;
    activeSubdeviceIndex = 0;   // active subdevice index to broadcast

    BindDevice(m_nLwrrentDevice);

    LwU32 classPreference[] = { LW9870_DISPLAY, LW9770_DISPLAY, LW9570_DISPLAY, LW9470_DISPLAY, LW9270_DISPLAY, LW9170_DISPLAY, LW9070_DISPLAY, GT214_DISPLAY, G94_DISPLAY, GT200_DISPLAY, G82_DISPLAY, LW50_DISPLAY };
    LwU32 classDesired = 0;
    UINT32 BoundDevice = pGpuDevMgr->GetFirstGpuDevice()->GetDeviceInst();

    if (m_pGpuDevice != NULL)
    {
        BoundDevice = m_pGpuDevice->GetDeviceInst();
    }

    if (LwrrentDisplayClass() == LW9070_DISPLAY) {
         m_bLogicHeadsSupported = true;
        }

    for (GpuSubdevice *pGpuSubdevice = pGpuDevMgr->GetFirstGpu();
         pGpuSubdevice != NULL;
         pGpuSubdevice = pGpuDevMgr->GetNextGpu(pGpuSubdevice))
    {
        #if !defined (USE_DTB_DISPLAY_CLASSES) || defined (LW_DTB_USE_MULTI_DISP_TB)
        //TODO: dtb need to implement equivalent classes to support this code
        GpuDevice *pGpuDevice = pGpuSubdevice->GetParentDevice();
        BindDevice(pGpuDevice->GetDeviceInst());
        #endif

        classDesired = 0;
        for (unsigned int i = 0; i < sizeof(classPreference)/sizeof(LwU32); i++) {
            if ( pLwRm->IsClassSupported(classPreference[i], GetBoundGpuDevice()) ) {
                classDesired = classPreference[i];
                break;
            }
        }

        if (classDesired) {
            #ifndef USE_DTB_DISPLAY_CLASSES
            Printf(Tee::PriNormal,
                   "Display allocating class 0x%08x on device %d\n",
                   (UINT32) classDesired, pGpuDevice->GetDeviceInst());
            #endif

            // allocate Evo Object, the root is picked up automatically
            rc = pLwRm->Alloc(
                pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                &(LwrrentDevice()->ObjectHandle),
                classDesired,        // Defined in sdk/lwpu/inc/class/cl5070.h
                &(LwrrentDevice()->DispAllocParams));
            if (rc != OK)
            {
                Printf(Tee::PriError, "DispTest::Initialize - Can't allocate display object from the RM\n");
                pLwRm->Free(pLwRm->GetDeviceHandle(GetBoundGpuDevice()));
                return rc;
            }

            // allocate common display object
            rc = pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                              &(LwrrentDevice()->DispCommonHandle),
                              LW04_DISPLAY_COMMON,  // Defined in sdk/lwpu/inc/class/cl0073.h
                              NULL);
            if (rc != OK)
            {
                 Printf(Tee::PriError, "DispTest::Initialize - Can't allocate display common object from the RM\n");
                 pLwRm->Free(pLwRm->GetDeviceHandle(GetBoundGpuDevice()));
                 return rc;
            }

            ///allocate an entry in m_display_devices of the correct class type
            Display * my_display = NULL;

            //temporary solution for dtb (dtb and full-chip share this file)
#ifdef USE_DTB_DISPLAY_CLASSES
            if(classDesired == LW9070_DISPLAY)
            {
                my_display = new Display_02xx_dtb(m_nNumDevice);
            }
            else if(classDesired == LW9170_DISPLAY)
            {
                my_display = new Display_021x_dtb(m_nNumDevice);
            }
            else if(classDesired == LW9270_DISPLAY)
            {
                my_display = new Display_022x_dtb(m_nNumDevice);
            }
            else if(classDesired == LW9470_DISPLAY)
            {
                my_display = new Display_024x_dtb(m_nNumDevice);
            }
            else if(classDesired == LW9570_DISPLAY)
            {
                my_display = new Display_025x_dtb(m_nNumDevice);
            }
            else if(classDesired == LW9770_DISPLAY)
            {
                my_display = new Display_027x_dtb(m_nNumDevice);
            }
            else if(classDesired == LW9870_DISPLAY)
            {
                my_display = new Display_028x_dtb(m_nNumDevice);
            }
#else
            if((classDesired == LW50_DISPLAY)
                || (classDesired == G82_DISPLAY)
                || (classDesired == GT200_DISPLAY)
                || (classDesired == G94_DISPLAY)
                )
            {
                my_display = new Display_01xx(m_nNumDevice);
            }
            else if(classDesired == GT214_DISPLAY)
            {
                my_display = new Display_015x(m_nNumDevice);
            }
            else if(classDesired == LW9070_DISPLAY)
            {
                my_display = new Display_02xx(m_nNumDevice);
            }
            else if(classDesired == LW9170_DISPLAY)
            {
                my_display = new Display_021x(m_nNumDevice);
            }
            else if(classDesired == LW9270_DISPLAY)
            {
                my_display = new Display_022x(m_nNumDevice);
            }
            else if(classDesired == LW9470_DISPLAY)
            {
                my_display = new Display_024x(m_nNumDevice);
            }
            else if(classDesired == LW9570_DISPLAY)
            {
                my_display = new Display_025x(m_nNumDevice);
            }
            else if(classDesired == LW9770_DISPLAY)
            {
                my_display = new Display_027x(m_nNumDevice);
            }
            else if(classDesired == LW9870_DISPLAY)
            {
                my_display = new Display_028x(m_nNumDevice);
            }
            /*else if(classDesired == something really cool that doesn't exist yet)
            {
                m_display_devices.push_back(new Display_030x());
            }
            else if(classDesired == something really cool that doesn't exist yet)
            {
                m_display_devices.push_back(new Display_031x());
            }
            .
            .
            .
            */
#endif
            else
            {
                Printf(Tee::PriWarn, "DispTest::Display - Unrecognized Display Class. Not Initializing\n");
            }

            if(my_display)
            {
                rc = my_display->Initialize();
                if(rc != OK)
                {
                    Printf(Tee::PriError, "Failed to initialize DTB Display class\n");
                    delete(my_display);
                    return(rc);
                }
                m_display_devices.push_back(my_display);
            }

        } else {
            //If there's no valid display...
            //We want to test pmgr functionality
            Printf(Tee::PriWarn, "NOTE: DispTest found devices %d with no supported display! You will not be able to use display related API on this device, but polling & register access is still working\n", m_nNumDevice);
        }

        LwrrentDevice()->unInitializedClass = classDesired;

        m_nNumDevice++;

        // initialize crc manager
        // default to "none" for the test owner
        CHECK_RC_MSG
        (
            DispTest::CrcInitialize(TestName, SubTestName, string(DISPTEST_CRC_DEFAULT_TEST_OWNER)),
            "***ERROR: DispTest::Initialize - Error Initializing CRC Manager"
        );

    }

    // Rebind whatever device was initially bound
    BindDevice(BoundDevice);

   // RC == OK at this point
    Printf(Tee::PriNormal, "DispTest found %d devices\n", m_nNumDevice);
    return rc;
}

/***********
 * DispTest::AllocDispMemory
 *
 *  Arguments Description:
 *  - Size:: The memory size to be allocated
 *
 *  Functional Description:
 *  - Allocate a certain value of memory for
 *    falcon ucode
 ***********
 **/
UINT64 DispTest::AllocDispMemory(UINT64 Size)
{
     RC rc = OK;
     UINT32 hMem;
     void *pBase;
     UINT64 Offset;

     //CHECK RC

     if (OK != (rc = AllocVidHeapMemory(&hMem, &pBase, Size, LWOS32_TYPE_IMAGE, (DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB)), &Offset)))
     {
        Printf(Tee::PriWarn, "Fail to allocate Memory for Display\n");
        return 0;
     }

     Printf(Tee::PriNormal, "Allocate Memory for Display, memory handler is 0x%x, FB physical address is 0x%llx, systerm virtual address is %p, Size is 0x%llx\n", hMem, Offset, pBase, Size);

     MASSERT(pBase != NULL);

     DispTest::UcodePhyAddr = Offset;

     return  UINT64(pBase);
}

/***********
 * DispTest::GetUcodePhyAddr
 *
 *  Arguments Description:
 *  -
 *
 *  Functional Description:
 *  - Returns the physical address of the memory allocated for
 *    falcon ucode
 ***********
 **/
UINT64 DispTest::GetUcodePhyAddr()
{
     Printf(Tee::PriNormal, "DispTest::GetUcodePhyAddr() :: FB physical address is 0x%llx\n", DispTest::UcodePhyAddr);
     return DispTest::UcodePhyAddr;
}

/***********
 * DispTest::BootupFalcon
 * Arguments Description:
 * - hMem is the memory handle point to the allocated mem in DFB.
 * - BootVec is the startup PC  address for Falcon.
 ****************************************************************************/
RC DispTest::BootupFalcon(UINT64 bootVec,
                          UINT64 bl_dmem_offset,
                          string Falcon)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::Release - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->BootupFalcon(DispTest::UcodePhyAddr, bootVec, bl_dmem_offset, Falcon));
}

/***********
 * DispTest::BootupAFalcon
 * Arguments Description:
 * - hMem is the memory handle point to the allocated mem in DFB.
 * - BootVec is the startup PC  address for Falcon.
 ****************************************************************************/
RC DispTest::BootupAFalcon(UINT32 hMem,
                          UINT64 bootVec,
                          UINT64 bl_dmem_offset)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::Release - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->BootupAFalcon(hMem, bootVec, bl_dmem_offset));
}

/***********
 * DispTest::StoreBin2FB
 * Arguments Description:
 * - fileName is path of the BIN file you want to write into DFB
 * - hMem is the memory handle point to the allocated mem in DFB.
 * - Offset is the offset from the base address, defined by hMem.
 ****************************************************************************/
void DispTest::StoreBin2FB(string filename,
        UINT64 SysAddr,
        UINT64 Offset,
        UINT32 fset)
{
     FileHolder this_file;
     int n;
     char buffer[1024];
     char FullBinFile[512];
     char * ActualAddr;

     string BINPath = CommandLine::ScriptFilePath();
     if (fset) {
       sprintf(FullBinFile, "%s", filename.c_str());
     } else {
       sprintf(FullBinFile, "%s%s",BINPath.c_str(),filename.c_str());
     }

     ActualAddr = (char *)(SysAddr + Offset);
     Printf(Tee::PriNormal, "DispTest::StoreBin2FB: File going to write to FB is %s, system address is %p\n", FullBinFile, ActualAddr);

     if (OK != this_file.Open(FullBinFile,"rb"))
     {
        Printf(Tee::PriWarn, "DispTest::StoreBin2FB: No existing BIN file to write to FB\n");
        return;
     }else{
        Printf(Tee::PriNormal, "DispTest::StoreBin2FB: open file %s to write to FB\n", filename.c_str());
     }

    for (;;)
    {
        n = (int)fread(buffer, sizeof(char), 256, this_file.GetFile());
        if (!n)
            break;
        Platform::VirtualWr(ActualAddr,buffer,n);
        ActualAddr = ActualAddr + n;
    }
}

/***********
 * DispTest::LoadFB2Bin
 * Arguments Description:
 * - fileName is path of the BIN file you want to store the data  from DFB
 * - hMem is the memory handle point to the allocated mem in DFB.
 * - Offset is the offset from the base address, defined by hMem.
 * - length is how many bytes you want read
 ****************************************************************************/
void DispTest::LoadFB2Bin(string filename,
        UINT64 SysAddr,
        UINT64 Offset,
        int length)
{
     FileHolder this_file;

     int n;
     char buffer[1024];
     char FullBinFile[512];
     char * ActualAddr;

     string BINPath = CommandLine::ScriptFilePath();

     //sprintf(FullBinFile, "%s%s",BINPath.c_str(),filename.c_str());
     sprintf(FullBinFile, "%s", filename.c_str());

     ActualAddr = (char *)(SysAddr + Offset);
     Printf(Tee::PriNormal, "LoadFB2Bin write to file %s, system address %p\n", FullBinFile, ActualAddr);

     if (OK != this_file.Open(FullBinFile,"wb"))
     {
        Printf(Tee::PriWarn, "Can't Open file  to store data from FB\n");
        return;
     }

     while(length > 0)
     {
        if (length > 1024)
        {
            n=1024;
        }else{
            n=length;
        }
        length -= 1024;
        Platform::VirtualRd(ActualAddr,buffer,n);
        ActualAddr = ActualAddr + n;
        if (static_cast<size_t>(n) !=
                fwrite(buffer,sizeof(char),n,this_file.GetFile()))
        {
            Printf(Tee::PriWarn, "Can't write to file %s\n", FullBinFile);
        }
     }
}

/****************************************************************************
 * DispTest::ConfigZPW
 * Arguments Description:
 * - enable is if we enable ZPW context save/restore in display, default 0
 * - fast_mode is if we ilwoke fast context save/restore in display, default 0
 ****************************************************************************/
RC DispTest::ConfigZPW(bool zpw_enable, bool zpw_ctx_mode, bool zpw_fast_mode, string ucode_dir)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ConfigZPW - Current device class not allocated.");
    }

    return (m_display_devices[index]->ConfigZPW(zpw_enable, zpw_ctx_mode, zpw_fast_mode, ucode_dir));
}

/****************************************************************************************
 * DispTest::Setup
 *
 *  Arguments Description:
 *  - ChannelName is a string in the following list:
 *      "core"
 *      "overlay"
 *      "overlay_imm"
 *      "base"
 *      "cursor"
 *      "capture_core"
 *      "capture_video"
 *      "capture_audio"
 *    It indicates the channel to be configured.
 *  - PbLocation is one of the targets defined in dispTypes_01.mfs
 *    (VIRTUAL, LWM, PCI, PCI_COHERENT) where the PB should be allocated.
 *  - Head which head, 0 or 1. For the Core channel, must be 0.
 *
 *  Functional Description:
 *  - Allocate Memory for the Channel PushBuffer and Create a CtxDma
 *  - Allocate the Channel from RM (This includes connecting the push buffer)
 *  - Returns a Handle for the Channel.
 ****************************************************************************************/
RC DispTest::Setup(string ChannelName,
                   string PbLocation,
                   UINT32 Head,
                   LwRm::Handle *pRtnChannelHandle)
//TODO: put this into the classes so as not to have to ifdef
#ifdef USE_DTB_DISPLAY_CLASSES
{
    ///make sure we have a place to write the return handle
    MASSERT(pRtnChannelHandle != NULL);

    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::Setup - Current device class not allocated.");
    }

    ///and forward the call to the class
    *pRtnChannelHandle = m_display_devices[index]->Setup(ChannelName, PbLocation, Head);
    return(OK);
}
#else
{
    //TODO; make this only check for the current devices, then call its
    //DispTest::Display class specific Setup function
    RC rc = OK;
    MASSERT(pRtnChannelHandle != NULL);

    if (LwrrentDevice()->DoSwapHeads) {
        if (Head == 0) Head = 1;
        else if (Head == 1) Head = 0;
    }
    if (LwrrentDevice()->UseHead) {
        Head = LwrrentDevice()->VgaHead;
    }
    // If this is the core channel, then the head has to be 0.
    if (ChannelName == "core")
        Head = 0;

    // channel type
    UINT32  ClassType = 0;
    bool    DmaChannel = true;
    // push buffer base and context DMA handle
    void *pPushBufferBase = NULL;
    LwRm::Handle hPBCtxDma = 0;
    // lwrm
    LwRmPtr pLwRm;
    // Local Variables
    UINT32 PBCtxDmaFlags = 0;
    LwRm::Handle hPBMem;
    UINT64  PBOffset = 0;
    Memory::Location PushbufMemSpace = Memory::Coherent;

    //////////////////////////////////////////////////////////////////////////
    // Is the class a DMA or Pio channel?  Is it base, core, overlay, etc.?
    CHECK_RC_MSG
    (
        GetClassType(ChannelName, &ClassType, &DmaChannel),
        "***ERROR: DispTest::Setup failed to determine what the class type should be."
    );

    //////////////////////////////////////////////////////////////////////////
    // PUSH BUFFER allocation
    if( DmaChannel )
    {
        CHECK_RC_MSG
        (
            AllocatePushBufferMemoryAndContextDma(pLwRm, &hPBMem, &PBOffset, &pPushBufferBase, &hPBCtxDma, &PBCtxDmaFlags, &PushbufMemSpace, PbLocation),
            "***ERROR: DispTest::Setup failed to allocate push-buffer."
        );
    }

    //////////////////////////////////////////////////////////////////////////
    // ERROR NOTIFIER allocation
    void * pErrorNotifierBase;
    UINT32 hErrorNotifierMem;
    LwRm::Handle hErrorNotifierCtxDma;    // error notifier handle
    UINT64 ErrorNotifierOffset = 0;
    UINT32 ErrNotifierFlags = DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE);

    CHECK_RC_MSG
    (
        AllocateErrorNotifierMemoryAndContextDma(pLwRm, &hErrorNotifierMem, &ErrorNotifierOffset, &pErrorNotifierBase, &hErrorNotifierCtxDma, ErrNotifierFlags),
        "***ERROR: DispTest::Setup failed to allocate error notifier."
    );

    //////////////////////////////////////////////////////////////////////////
    // CHANNEL NOTIFIER allocation
    void *pChannelNotifierBase;
    LwRm::Handle hChannelNotifierMem, hChannelNotifierCtxDma;
    UINT32 ChannelNotifierCtxDmaFlags = DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) | DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER);
    UINT64 ChannelNotifierOffset = 0;

    CHECK_RC_MSG
    (
        AllocateChannelNotifierMemoryAndContextDma(pLwRm, &hChannelNotifierMem, &ChannelNotifierOffset, &pChannelNotifierBase, &hChannelNotifierCtxDma, ChannelNotifierCtxDmaFlags),
        "***ERROR: DispTest::Setup failed to allocate channel notifier."
    );

    //////////////////////////////////////////////////////////////////////////
    // Allocate the channel from the RM
    if (DmaChannel)
    {
#ifdef LW_VERIF_FEATURES
        UINT32 flag = DRF_DEF(50VAIO_CHANNELDMA_ALLOCATION, _FLAGS, _ALLOW_MULTIPLE_PB_FOR_CLIENT, _YES);
#else
        UINT32 flag = 0;
#endif

        // Allow multiple push buffers for a single client by default so we
        // can test channel grabbing
        CHECK_RC_MSG
        (
            pLwRm->AllocDisplayChannelDma (LwrrentDevice()->ObjectHandle, pRtnChannelHandle,
                ClassType, Head, hErrorNotifierCtxDma, pErrorNotifierBase, hPBCtxDma,
                pPushBufferBase, DISPTEST_PB_SIZE, 0, flag, GetBoundGpuDevice()),
            "***ERROR: DispTest::Setup failed to allocate channel DMA."
         );
    }
    else    // Not dma channel
    {
        CHECK_RC_MSG
        (
            pLwRm->AllocDisplayChannelPio (LwrrentDevice()->ObjectHandle, pRtnChannelHandle,
                ClassType, Head, hErrorNotifierCtxDma, pErrorNotifierBase, 0, GetBoundGpuDevice()),
            "***ERROR: DispTest::Setup failed to allocate channel PIO."
        );

        // Set the default global timeout for this PIO Channel
        DispTest::SetPioChannelTimeout(*pRtnChannelHandle,DispTest::GlobalTimeoutMs);
    }

    // Set the push buffer memory space in the channel.
    DisplayChannel * channel = pLwRm->GetDisplayChannel(*pRtnChannelHandle);
    channel->SetPushbufMemSpace(PushbufMemSpace);

    // Connect the channel notifier
    CHECK_RC_MSG
    (
        pLwRm->BindContextDma (*pRtnChannelHandle, hChannelNotifierCtxDma),
        "***ERROR: DispTest::Setup failed to connect the channel notifier."
    );

    // disable auto-flushing for DMA channels
    if (DmaChannel && (!GetBoundGpuSubdevice()->IsDFPGA()))
    {
        CHECK_RC_MSG
        (
            DispTest::SetAutoFlush (*pRtnChannelHandle, false, 1),
            "***ERROR: DispTest::Setup failed to disable auto-flushing for the channel."
        );
    }

    //////////////////////////////////////////////////////////////////////////
    // enable lookup of the channel name from the handle
    DispTest::LwrrentDevice()->ChannelNameMap[*pRtnChannelHandle] = ChannelName;

    UINT32 ChannelNum;
    GetChannelNum(ClassType, Head, &ChannelNum);
    LwrrentDevice()->ChannelNumberMap[*pRtnChannelHandle] = ChannelNum;

    //////////////////////////////////////////////////////////////////////////
    // Register all the context Dma's to the channel
    // First the push-buffer context dma
    CHECK_RC_MSG
    (
        AddDmaContext(
            hPBCtxDma,
            PbLocation.c_str(),
            PBCtxDmaFlags,
            pPushBufferBase,
            DISPTEST_PB_SIZE - 1,
            PBOffset),
        "***ERROR: DispTest::Setup failed to add DMA context for the push buffer."
    );

    // Next the error notifier context dma
    CHECK_RC_MSG
    (
        AddDmaContext(
            hErrorNotifierCtxDma,
            "lwm",
            ErrNotifierFlags,
            pErrorNotifierBase,
            DISPTEST_ERR_NOT_SIZE - 1,
            ErrorNotifierOffset),
        "***ERROR: DispTest::Setup failed to add DMA context for the error notifier."
    );

    // Now the channel notifier context dma
    CHECK_RC_MSG
    (
        AddDmaContext(
            hChannelNotifierCtxDma,
            "lwm",
            ChannelNotifierCtxDmaFlags,
            pChannelNotifierBase,
            DISPTEST_PB_SIZE - 1,
            ChannelNotifierOffset),
        "***ERROR: DispTest::Setup failed to add DMA context for the channel notifier."
    );

    // This information is used for channel grabbing,
    // which is a DMA channel concept
    if (DmaChannel)
    {
        AddChannelContext(*pRtnChannelHandle, ClassType, Head, hErrorNotifierCtxDma, hPBCtxDma);
    }

    LwrrentDevice()->ChannelClassMap[*pRtnChannelHandle] = ClassType;
    LwrrentDevice()->ChannelHeadMap[*pRtnChannelHandle] = Head;

    // RC == OK at this point
    return rc;
}
#endif

/****************************************************************************************
* DispTest::GetClassType
*
*  Arguments Description:
*  - ChannelName is a string in the following list:
*      "core"
*      "overlay"
*      "overlay_imm"
*      "base"
*      "cursor"
*  - ClassType: Return value indicating what the class type is.
*  - DmaChannel: Return value indicating whether this is a dma channel or not.
*
*  Functional Description:
*  - This function will fill in what the ClassType is (Dma or Pio, which channel)
*    and also fill in whether this is a dma channel or not.
****************************************************************************************/
RC DispTest::GetClassType(string ChannelName, UINT32 *pClassType, bool *pDmaChannel)
{
    RC rc = OK;

    switch (LwrrentDisplayClass())
    {
    case LW9870_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW987D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW927C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW917A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW917E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "wrbk")
        {
            *pClassType = LW9878_WRITEBACK_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW9770_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW977D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW927C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW917A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW917E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW9570_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW957D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW927C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW917A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW917E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW9470_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW947D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW927C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW917A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW917E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW9270_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW927D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW927C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW917A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW917E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW9170_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW917D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW917C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW917A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW917E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW9070_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW907D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW907C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW907A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW907B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW907E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case GT214_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW857D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW857C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW857A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW857B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW857E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case G94_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW887D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW837C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW827A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW827B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW837E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case GT200_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW837D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW837C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW827A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW827B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW837E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case G82_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW827D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW827C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW827A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW827B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW827E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    case LW50_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            *pClassType = LW507D_CORE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "base")
        {
            *pClassType = LW507C_BASE_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else if (ChannelName == "cursor")
        {
            *pClassType = LW507A_LWRSOR_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay_imm")
        {
            *pClassType = LW507B_OVERLAY_IMM_CHANNEL_PIO;
            *pDmaChannel = false;
        }
        else if (ChannelName == "overlay")
        {
            *pClassType = LW507E_OVERLAY_CHANNEL_DMA;
            *pDmaChannel = true;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Setup - Invalid Channel Name.");
        }
        break;
    default:
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::Setup - Invalid Class.");
        break;
    }

    return rc;
}

/****************************************************************************************
* DispTest::AllocatePushBufferMemoryAndContextDma
*
*  Arguments Description:
*  - pLwRm: Pointer to the RM
*  - phPBMem: A pointer for a handle for the Push-Buffer Memory
*  - pPBOffset: A pointer for the Push-Buffer Offset
*  - ppPushBufferBase:  A pointer to a pointer for the Push-Buffer Base
*  - phPBCtxDma: A pointer for a handle to the Push-Buffer Context DMA
*  - pPBCtxDmaFlags: A pointer to flags regarding the Context DMA.
*  - pPushbufMemSpace: A pointer describing what memory space the Push-Buffer should use.
*  - PBLocation: A string describing the location of the Push-Buffer
*
*  Functional Description:
*  - This function will allocate memory for the push buffer and create the context dma.
****************************************************************************************/
RC DispTest::AllocatePushBufferMemoryAndContextDma(LwRmPtr pLwRm,
                                                   LwRm::Handle *phPBMem,
                                                   UINT64 *pPBOffset,
                                                   void **ppPushBufferBase,
                                                   LwRm::Handle *phPBCtxDma,
                                                   UINT32 *pPBCtxDmaFlags,
                                                   Memory::Location *pPushbufMemSpace,
                                                   string PbLocation)
{
    RC rc = OK;

    // allocate memory for the channel push buffer
    if ((PbLocation == "lwm") ||
        (PbLocation == "vid"))
    {
        // allocate push buffer in frame buffer memory
        // for Fermi we must allocate with 4KB page swizzle
        CHECK_RC_MSG
        (
            pLwRm->VidHeapAllocSize (LWOS32_TYPE_DMA,
                DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB), LWOS32_ATTR2_NONE,
                DISPTEST_PB_SIZE, phPBMem, pPBOffset, NULL, NULL, NULL,
                GetBoundGpuDevice()),
             "***ERROR: DispTest::Setup failed to allocate channel push buffer."
         );
         *pPushbufMemSpace = Memory::Fb;
    }
    else if ((PbLocation == "pci") ||
             (PbLocation == "ncoh"))
    {
        // alloate push buffer in system memory
        CHECK_RC_MSG
        (
            pLwRm->AllocSystemMemory (phPBMem, DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_COMBINE), DISPTEST_PB_SIZE,
                GetBoundGpuDevice()),
            "***ERROR: DispTest::Setup failed to allocate channel push buffer."
        );
        *pPBCtxDmaFlags |= DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE);
        *pPushbufMemSpace = Memory::NonCoherent;
    }
    else if ((PbLocation == "pci_coherent") ||
             (PbLocation == "coh"))
    {
        // alloate push buffer in system memory
        CHECK_RC_MSG
        (
            pLwRm->AllocSystemMemory (phPBMem, DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED), DISPTEST_PB_SIZE,
                GetBoundGpuDevice()),
            "***ERROR: DispTest::Setup failed to allocate channel push buffer."
        );
        *pPBCtxDmaFlags |= DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE);
        *pPushbufMemSpace = Memory::Coherent;
    }
    else
    {
        RETURN_RC_MSG (RC::BAD_COMMAND_LINE_ARGUMENT, "***ERROR: DispTest::Setup - Invalid PB Location");
    }

    CHECK_RC_MSG
    (
        pLwRm->MapMemory (*phPBMem, 0, DISPTEST_PB_SIZE, ppPushBufferBase, 0,
                          GetBoundGpuSubdevice()),
        "***ERROR: DispTest::Setup failed to map memory for the channel push buffer."
    );
    LwrrentDevice()->MemMap[*phPBMem] = *ppPushBufferBase;

    // Clear the last location in the PushBuffer. This is needed to test the PushBuffer wrapping.
    // Since the DmaDisplayChannel routines prevent the pushbuffer contents from wrapping around incorrectly by inserting
    // a Jump instruction at the last Pushbuffer location, we are basically setting the last location to a NOP here so that
    // the test (pushbuffer_overflow) can trigger the overflow condition.
    MEM_WR32((void*)((UINT32 *)(*ppPushBufferBase) + DISPTEST_PB_SIZE /4 - 1), 0);

    // allocate context dma for push buffer
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma (phPBCtxDma, *pPBCtxDmaFlags, *phPBMem, 0, DISPTEST_PB_SIZE - 1),
        "***ERROR: DispTest::Setup failed to allocate memory for the context dma for the push buffer."
    );

    return rc;
}

/****************************************************************************************
* DispTest::AllocateErrorNotifierMemoryAndContextDma
*
*  Arguments Description:
*  - pLwRm: Pointer to the RM
*  - phErrorNotifierMem: A pointer for a handle for the Error Notifier Memory
*  - pErrorNotifierOffset: A pointer for the Error Notifier Offset
*  - ppErrorNotifierBase:  A pointer to a pointer for the Error Notifier Base
*  - phErrorNotifierCtxDma: A pointer for a handle to the Error Notifier Context DMA
*  - ErrNotifierFlags: Flags regarding the Context DMA.
*
*  Functional Description:
*  - This function will allocate memory for the Error Notifier and create the context dma.
****************************************************************************************/
RC DispTest::AllocateErrorNotifierMemoryAndContextDma(LwRmPtr pLwRm,
                                                      UINT32 *phErrorNotifierMem,
                                                      UINT64 *pErrorNotifierOffset,
                                                      void **ppErrorNotifierBase,
                                                      LwRm::Handle *phErrorNotifierCtxDma,
                                                      UINT32 ErrNotifierFlags)
{
    RC rc = OK;

    // Allocate memory in FB for error notifier
    CHECK_RC_MSG
    (
        // for Fermi we must allocate with 4KB page swizzle
        pLwRm->VidHeapAllocSize (LWOS32_TYPE_IMAGE,
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB), LWOS32_ATTR2_NONE,
            DISPTEST_ERR_NOT_SIZE,
            phErrorNotifierMem, pErrorNotifierOffset, NULL, NULL, NULL, GetBoundGpuDevice()),
        "***ERROR: DispTest::Setup failed to allocate FB memory for the error notifier."
    );

    // Map the memory in the RM.
    CHECK_RC_MSG
    (
        pLwRm->MapMemory (*phErrorNotifierMem, 0, DISPTEST_ERR_NOT_SIZE, ppErrorNotifierBase,
                          0, GetBoundGpuSubdevice()),
        "***ERROR: DispTest::Setup failed to map FB memory for the error notifier."
    );

    LwrrentDevice()->MemMap[*phErrorNotifierMem] = *ppErrorNotifierBase;

    // Allocate ctx DMA for the error notifier
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma (phErrorNotifierCtxDma, ErrNotifierFlags, *phErrorNotifierMem,
            0, DISPTEST_ERR_NOT_SIZE - 1),
        "***ERROR: DispTest::Setup failed to allocate the context DMA for the error notifier."
    );

    return rc;
}

/****************************************************************************************
* DispTest::AllocateErrorNotifierMemoryAndContextDma
*
*  Arguments Description:
*  - pLwRm: Pointer to the RM
*  - phChannelNotifierMem: A pointer for a handle for the Channel Notifier Memory
*  - pChannelNotifierOffset: A pointer for the Channel Notifier Offset
*  - ppChannelNotifierBase:  A pointer to a pointer for the Channel Notifier Base
*  - phChannelNotifierCtxDma: A pointer for a handle to the Channel Notifier Context DMA
*  - ChannelNotifierCtxDmaFlags: Flags regarding the Context DMA.
*
*  Functional Description:
*  - This function will allocate memory for the Channel Notifier and create the context dma.
****************************************************************************************/
RC DispTest::AllocateChannelNotifierMemoryAndContextDma(LwRmPtr pLwRm,
                                                        LwRm::Handle *phChannelNotifierMem,
                                                        UINT64 *pChannelNotifierOffset,
                                                        void **ppChannelNotifierBase,
                                                        LwRm::Handle *phChannelNotifierCtxDma,
                                                        UINT32 ChannelNotifierCtxDmaFlags)
{
    RC rc = OK;

    // Allocate FB memory and context DMA for channel notifier
    CHECK_RC_MSG
    (
        // for Fermi we must allocate with 4KB page swizzle
        pLwRm->VidHeapAllocSize (LWOS32_TYPE_DMA,
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB), LWOS32_ATTR2_NONE,
            DISPTEST_PB_SIZE,
            phChannelNotifierMem, pChannelNotifierOffset, NULL, NULL, NULL,
            GetBoundGpuDevice()),
        "***ERROR: DispTest::Setup failed to allocate FB memory for the channel notifier."
    );

    // Map the memory in the RM.
    CHECK_RC_MSG
    (
        pLwRm->MapMemory (*phChannelNotifierMem, 0, DISPTEST_PB_SIZE, ppChannelNotifierBase,
                          0, GetBoundGpuSubdevice()),
        "***ERROR: DispTest::Setup failed to map FB memory for the channel notifier."
    );

    LwrrentDevice()->MemMap[*phChannelNotifierMem] = *ppChannelNotifierBase;

    // Allocate ctx DMA for the channel notifier.
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma (phChannelNotifierCtxDma, ChannelNotifierCtxDmaFlags, *phChannelNotifierMem,
            0, DISPTEST_PB_SIZE - 1),
        "***ERROR: DispTest::Setup failed to allocate the context DMA for the channel notifier."
    );

    return rc;
}

/****************************************************************************************
 * DispTest::Release
 *
 *  Arguments Description:
 *  - ChannelHandle: Handle for the channel to be released
 *
 *  Functional Description:
 *  - Release all the allocated objects (Notifiers, IsoSurfaces, Semaphores, etc)
 *    that have bound to the channel.
 *  - Release the channel handle
 *  - Deallocate channel from the RM.
 *  - Free the channel PB CtxDMA and memory
 *  - Deallocate memory used for CRCs
 ****************************************************************************************/
RC DispTest::Release(LwRm::Handle ChannelHandle)
{
    // release all objects bound to channel
    LwRmPtr pLwRm;
    pLwRm->Free(ChannelHandle);

    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::Release - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->Release(ChannelHandle));
}

/****************************************************************************************
 * DispTest::SetPioChannelTimeout
 *
 *  Arguments Description:
 *  - ChannelHandle: Handle for the channel to be released
 *
 *  Functional Description:
 *  - Set the default timeout for this PIO channel operations
 ****************************************************************************************/
RC DispTest::SetPioChannelTimeout
(
    LwRm::Handle ChannelHandle,
    FLOAT64 TimeoutMs
)
{
    LwRmPtr pLwRm;
    // Recover the channel pointer
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);

    if (!channel)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::SetPioChannelTimeout - Bad channel handle.");
    }

    // Set the default Timeout
    channel->SetDefaultTimeoutMs(TimeoutMs);
    return OK;
}

/****************************************************************************************
 * DispTest::SwitchToChannel
 *
 *  Arguments Description:
 *  - hDispChan - Handle for a previously allocated (Setup) DMA display channel
 *
 *  Functional Description:
 *  - Switch to the given display channel
 ****************************************************************************************/
RC DispTest::SwitchToChannel(LwRm::Handle hDispChan)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SwitchToChannel - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SwitchToChannel(hDispChan));
}

/****************************************************************************************
 * DispTest::IdleChannelControl
 *
 *  Arguments Description:
 *  - channelHandle - the handle returned by a DispTest.Setup() Call.
 *  - desiredChannelStateMask - channel state mask setting
 *  - accel - channel accel setting
 *  - timeout - channel timeout setting
 *
 *  Functional Description:
 *  - Manage the idle channel settings
 ****************************************************************************************/
RC DispTest::IdleChannelControl(UINT32 channelHandle, UINT32 desiredChannelStateMask,
                               UINT32 accel, UINT32 timeout)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::IdleChannelControl - Current device class not allocated.");
    }

    // And forward the call to the class.
    return (m_display_devices[index]->IdleChannelControl(channelHandle, desiredChannelStateMask,
                               accel, timeout) );
}

/****************************************************************************************
 * DispTest::SetChnStatePollNotif
 *
 *  Arguments Description:
 *  - channelHandle - the handle returned by a DispTest.Setup() Call.
 *  - NotifChannelState - channel state to trigger notifier
 *  - hNotifierCtxDma - notifier context dma handle
 *  - offset - notifier offset
 *
 *  Functional Description:
 *  - set a channel state to trigger notifier
 ****************************************************************************************/
RC DispTest::SetChnStatePollNotif(UINT32 channelHandle, UINT32 NotifChannelState,
                                  UINT32 hNotifierCtxDma, UINT32 offset)
{
    UINT32 channelClass = LwrrentDevice()->ChannelClassMap[channelHandle];
    UINT32 channelHead = LwrrentDevice()->ChannelHeadMap[channelHandle];

    LwRmPtr pLwRm;

    LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_NOTIFICATION *pNotif = 0;

    LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_PARAMS params = {
        {LwV32(activeSubdeviceIndex)},
        LwV32(channelClass),
        LwV32(channelHead),
        LwV32(NotifChannelState),
        LwV32(hNotifierCtxDma),
        LwV32(offset)
    };

    UINT32 writeOffset;

    //The offset passed into WriteVal32 is the number of 32-byte words of offset
    //writeOffset needs to be divided by 4
    writeOffset = offset + (UINT32)reinterpret_cast<LwUPtr>(&pNotif->blockRMDuringPoll);
    DispTest::WriteVal32(hNotifierCtxDma, (writeOffset>>2), 0);

    writeOffset = offset + (UINT32)reinterpret_cast<LwUPtr>(&pNotif->rmIsBusyPolling);
    DispTest::WriteVal32(hNotifierCtxDma, (writeOffset>>2), 0);

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::SetChnStatePollNotif
 *
 *  Arguments Description:
 *  - ChannelName is a string in the following list:
 *      "core"
 *      "overlay"
 *      "overlay_imm"
 *      "base"
 *      "cursor"
 *  - channelInstance:
 *  - NotifChannelState - channel state to trigger notifier
 *  - hNotifierCtxDma - notifier context dma handle
 *  - offset - notifier offset
 *
 *  Functional Description:
 *  - set a channel state to trigger notifier
 ****************************************************************************************/
RC DispTest::SetChnStatePollNotif(string ChannelName, UINT32 channelInstance,
                                  UINT32 NotifChannelState, UINT32 hNotifierCtxDma,
                                  UINT32 offset)
{

    UINT32  ClassType = 0;

    switch (LwrrentDisplayClass())
    {
    case LW9870_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW987D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW927C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW917A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW917E_OVERLAY_CHANNEL_DMA;
        }
        else if (ChannelName == "wrbk")
        {
            ClassType = LW9878_WRITEBACK_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW9770_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW977D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW927C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW917A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW917E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW9570_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW957D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW927C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW917A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW917E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW9470_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW947D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW927C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW917A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW917E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW9270_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW927D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW927C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW917A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW917E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW9170_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW917D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW917C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW917A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW917B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW917E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW9070_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW907D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW907C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW907A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW907B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW907E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case GT214_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW857D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW857C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW857A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW857B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW857E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case G94_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW887D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW837C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW827A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW827B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW837E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case GT200_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW837D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW837C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW827A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW827B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW837E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case G82_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW827D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW827C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW827A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW827B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW827E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    case LW50_DISPLAY:
        // set the correct class type depending on the channel requested
        if (ChannelName == "core")
        {
            ClassType = LW507D_CORE_CHANNEL_DMA;
        }
        else if (ChannelName == "base")
        {
            ClassType = LW507C_BASE_CHANNEL_DMA;
        }
        else if (ChannelName == "cursor")
        {
            ClassType = LW507A_LWRSOR_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay_imm")
        {
            ClassType = LW507B_OVERLAY_IMM_CHANNEL_PIO;
        }
        else if (ChannelName == "overlay")
        {
            ClassType = LW507E_OVERLAY_CHANNEL_DMA;
        }
        else
        {
            RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Channel Name.");
        }
        break;
    default:
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::SetChnStatePollNotif - Invalid Class.");
        break;
    }

    UINT32 channelHead = channelInstance;

    LwRmPtr pLwRm;

    LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_NOTIFICATION *pNotif = 0;

    LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_PARAMS params = {
        {LwV32(activeSubdeviceIndex)},
        LwV32(ClassType),
        LwV32(channelHead),
        LwV32(NotifChannelState),
        LwV32(hNotifierCtxDma),
        LwV32(offset)
    };

    UINT32 writeOffset;

    writeOffset = offset + (UINT32)reinterpret_cast<LwUPtr>(&pNotif->blockRMDuringPoll);
    DispTest::WriteVal32(hNotifierCtxDma, (writeOffset>>2), 0);

    writeOffset = offset + (UINT32)reinterpret_cast<LwUPtr>(&pNotif->rmIsBusyPolling);
    DispTest::WriteVal32(hNotifierCtxDma, (writeOffset>>2), 0);

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::SetChnStatePollNotifBlocking
 *
 *  Arguments Description:
 *  - hNotifierCtxDma - notifier context dma handle
 *  - value true = block, false = unblock
 *
 *  Functional Description:
 *  - set a channel state to trigger notifier
 ****************************************************************************************/
RC DispTest::SetChnStatePollNotifBlocking(UINT32 hNotifierCtxDma, UINT32 offset, UINT32 value)
{
    LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_NOTIFICATION *pNotif = 0;

    UINT32 writeOffset;

    writeOffset = offset + (UINT32)reinterpret_cast<LwUPtr>(&pNotif->blockRMDuringPoll);
    DispTest::WriteVal32(hNotifierCtxDma, (writeOffset>>2), value);

    return OK;
}

/****************************************************************************************
 * DispTest::SetChnStatePollNotifBlocking
 *
 *  Arguments Description:
 *  - hNotifierCtxDma - notifier context dma handle
 *  - value true = block, false = unblock
 *
 *  Functional Description:
 *  - set a channel state to trigger notifier
 ****************************************************************************************/
RC DispTest::GetChnStatePollNotifRMPolling
(
    UINT32 hNotifierCtxDma,
    UINT32 offset,
    FLOAT64 timeoutMs
)
{

    LW5070_CTRL_CMD_SET_CHN_STATE_POLL_NOTIF_NOTIFICATION *pNotif = 0;

    UINT32 readOffset;

    readOffset = offset + (UINT32)reinterpret_cast<LwUPtr>(&pNotif->rmIsBusyPolling);

    //The offset expected by PollDone is in 4-byte words.  readOffset is in bytes, so
    //it must be divided by 4 before being sent to PollDone
    return DispTest::PollDone(hNotifierCtxDma, (readOffset>>2), 0, true, timeoutMs);
}

/****************************************************************************************
 * DispTest::EnqMethod
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Method is a Method Name as defined in the class headers.
 *  - Data is a UINT32. It is the Data field associated with Method
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method
 ****************************************************************************************/
RC DispTest::EnqMethod(LwRm::Handle ChannelHandle, UINT32 Method, UINT32 Data)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::EnqMethod - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->EnqMethod(ChannelHandle, Method, Data));
}

/****************************************************************************************
 * DispTest::Set/GetVPLLLockDelay
 *
 * Sent through DispTest so that Gpu in dtb doesn't have to use manuals.
 ****************************************************************************************/
RC DispTest::GetVPLLLockDelay(UINT32* delay)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetVPLLLockDelay - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->GetVPLLLockDelay(delay));
}

RC DispTest::SetVPLLLockDelay(UINT32 delay)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetVPLLLockDelay - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetVPLLLockDelay(delay));
}

/****************************************************************************************
 * DispTest::EnqUpdateAndMode
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Method is a Method Name as defined in the class headers.
 *  - Data is a UINT32. It is the Data field associated with Method
 *  - Mode is the current raster mode for the head
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method
 *  - Set the HeadMode field in the crc_head_info structure
 ****************************************************************************************/
RC DispTest::EnqUpdateAndMode(LwRm::Handle ChannelHandle, UINT32 Method, UINT32 Data, const char * Mode)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::EnqUpdateAndMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->EnqUpdateAndMode(ChannelHandle, Method, Data, Mode));
}

/****************************************************************************************
 * DispTest::EnqCoreUpdateAndMode
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Method is a Method Name as defined in the class headers.
 *  - Data is a UINT32. It is the Data field associated with Method
 *  - Head0Mode is the current raster mode for head 0
 *  - Head1Mode is the current raster mode for head 1
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method
 *  - Set the HeadMode field in the crc_head_info structure
 ****************************************************************************************/
RC DispTest::EnqCoreUpdateAndMode(LwRm::Handle ChannelHandle, UINT32 Method, UINT32 Data, const char * Heada_Mode, const char * Headb_Mode, const char * Headc_Mode, const char * Headd_Mode)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::EnqCoreUpdateAndMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->EnqCoreUpdateAndMode(ChannelHandle, Method, Data, Heada_Mode, Headb_Mode, Headc_Mode, Headd_Mode));
}

/****************************************************************************************
 * DispTest::EnqCrcModes
 *
 *  Functional Description:
 *  - Enqueue just CRC mode  astring for a given head but do not enqueue an update.
 #    This is used to compensate for snnoze frame tag incrementing that is otherwise
 #    unexpected.
 ****************************************************************************************/
RC DispTest::EnqCrcMode(UINT32 headnum, const char * Head_Mode)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::EnqCrcMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->EnqCrcMode(headnum, Head_Mode));
}

/****************************************************************************************
 * DispTest::EnqMethodMulti
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Method is a Method Name as defined in the class headers.
 *  - Count is the number of data items
 *  - DataPtr is the pointer to the list of data items of "Count" length
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method
 ****************************************************************************************/
RC DispTest::EnqMethodMulti(LwRm::Handle ChannelHandle, UINT32 Method, UINT32 Count, const UINT32 * DataPtr)
{
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::EnqMethodMulti - Can't find Channel.  Did you configure the channel already?");
    }

    // write method to channel
    return channel->Write(Method, Count, DataPtr);
}

/****************************************************************************************
 * DispTest::EnqMethodNonInc
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Method is a Method Name as defined in the class headers.
 *  - Data is a UINT32. It is the Data field associated with Method
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method (non-incrementing)
 ****************************************************************************************/
RC DispTest::EnqMethodNonInc(LwRm::Handle ChannelHandle, UINT32 Method, UINT32 Data)
{
    /*
     * EnqMethod is non-incrementing by default, but some legacy tests still call this
     * older API.  Just forward the call to the up-to-date version.
     */
    return DispTest::EnqMethod(ChannelHandle, Method, Data);
}

/****************************************************************************************
 * DispTest::EnqMethodNonIncMulti
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Method is a Method Name as defined in the class headers.
 *  - Count is the number of data items
 *  - DataPtr is a UINT32*. It is the Data Array associated with Method
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method (non-incrementing)
 ****************************************************************************************/
RC DispTest::EnqMethodNonIncMulti(LwRm::Handle ChannelHandle, UINT32 Method, UINT32 Count, const UINT32 *DataPtr)
{
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::EnqMethodNonIncMulti - Can't find Channel.  Did you configure the channel already?");
    }

    // write method to channel
    return channel->WriteNonInc(Method, Count, DataPtr);
}

/****************************************************************************************
 * DispTest::EnqMethodNop
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *
 *  Functional Description:
 *  - Enqueue a Nop Method into the Channel PushBuffer
 ****************************************************************************************/
RC DispTest::EnqMethodNop(LwRm::Handle ChannelHandle)
{
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::EnqMethodNop - Can't find Channel.  Did you configure the channel already?");
    }

    // write method to channel
    return channel->WriteNop();
}

/****************************************************************************************
 * DispTest::EnqMethodJump
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Offset is the offset to jump to.
 *
 *  Functional Description:
 *  - Enqueue a Jump Method into the Channel PushBuffer
 ****************************************************************************************/
RC DispTest::EnqMethodJump(LwRm::Handle ChannelHandle, UINT32 Offset)
{
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::EnqMethodJump - Can't find Channel.  Did you configure the channel already?");
    }

    // write method to channel
    return channel->WriteJump(Offset);
}

/****************************************************************************************
 * DispTest::EnqMethodSetSubdevice
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Subdevice is the subdevice mask.
 *
 *  Functional Description:
 *  - Enqueue a SetSubDevice Method into the Channel PushBuffer
 ****************************************************************************************/
RC DispTest::EnqMethodSetSubdevice(LwRm::Handle ChannelHandle, UINT32 Subdevice)
{
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::EnqMethodSetSubdevice - Can't find Channel.  Did you configure the channel already?");
    }

    // write method to channel
    return channel->WriteSetSubdevice(Subdevice);
}

/****************************************************************************************
 * DispTest::SetSwapHeads
 *
 *  Arguments Description:
 *  - swap : boolean value to denote swap or not swap
 *
 *  Functional Description:
 *  - Set the swap heads setting for the current device
 ****************************************************************************************/
RC DispTest::SetSwapHeads(bool swap)
{
    LwrrentDevice()->DoSwapHeads = swap;

    return OK;
}

/****************************************************************************************
 * DispTest::HandleSwapHead
 *
 *  Arguments Description:
 *  - Method to be modified
 *  - Data to be modified
 *
 *  Functional Description:
 *  - if DoSwapHeads is true, massages the Method and Data to swap head 0 and head 1
 ****************************************************************************************/
RC DispTest::HandleSwapHead(UINT32& Method, UINT32& Data)
{
    if (! LwrrentDevice()->DoSwapHeads) return OK;

    UINT32 OldMethod = Method;
    UINT32 OldData = Data;

    if(LwrrentDisplayClass() == LW9070_DISPLAY)
    {
            RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::HandleSwapHead - class020x not supported right now.");
    }

    switch (LwrrentDisplayClass()) {
    case GT214_DISPLAY:
    case GT200_DISPLAY:
    case G94_DISPLAY:
    case G82_DISPLAY:
    switch (Method) {
    case LW827D_HEAD_SET_PRESENT_CONTROL(0):
    case LW827D_HEAD_SET_PIXEL_CLOCK(0):
    case LW827D_HEAD_SET_CONTROL(0):
    case LW827D_HEAD_SET_LOCK_OFFSET(0):
    case LW827D_HEAD_SET_OVERSCAN_COLOR(0):
    case LW827D_HEAD_SET_RASTER_SIZE(0):
    case LW827D_HEAD_SET_RASTER_SYNC_END(0):
    case LW827D_HEAD_SET_RASTER_BLANK_END(0):
    case LW827D_HEAD_SET_RASTER_BLANK_START(0):
    case LW827D_HEAD_SET_RASTER_VERT_BLANK2(0):
    case LW827D_HEAD_SET_RASTER_VERT_BLANK_DMI(0):
    case LW827D_HEAD_SET_DEFAULT_BASE_COLOR(0):
    case LW827D_HEAD_SET_CRC_CONTROL(0):
    case LW827D_HEAD_SET_CONTEXT_DMA_CRC(0):
    case LW827D_HEAD_SET_CONTEXT_DMAS_ISO(0,0):
    case LW827D_HEAD_SET_CONTEXT_DMAS_ISO(0,1):
    case LW827D_HEAD_SET_CONTEXT_DMA_LWRSOR(0):
    case LW827D_HEAD_SET_CONTEXT_DMA_LUT(0):
    case LW827D_HEAD_SET_BASE_LUT_LO(0):
    case LW827D_HEAD_SET_OUTPUT_LUT_LO(0):
    case LW827D_HEAD_SET_OFFSET(0,0):
    case LW827D_HEAD_SET_OFFSET(0,1):
    case LW827D_HEAD_SET_SIZE(0):
    case LW827D_HEAD_SET_STORAGE(0):
    case LW827D_HEAD_SET_PARAMS(0):
    case LW827D_HEAD_SET_CONTROL_LWRSOR(0):
    case LW827D_HEAD_SET_OFFSET_LWRSOR(0):
    case LW827D_HEAD_SET_DITHER_CONTROL(0):
    case LW827D_HEAD_SET_CONTROL_OUTPUT_SCALER(0):
    case LW827D_HEAD_SET_PROCAMP(0):
    case LW827D_HEAD_SET_VIEWPORT_POINT_IN(0,0):
    case LW827D_HEAD_SET_VIEWPORT_POINT_IN(0,1):
    case LW827D_HEAD_SET_VIEWPORT_SIZE_IN(0):
    case LW827D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(0):
    case LW827D_HEAD_SET_VIEWPORT_SIZE_OUT(0):
    case LW827D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(0):
    case LW827D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(0):
    case LW827D_HEAD_SET_OVERLAY_USAGE_BOUNDS(0):
    case LW827D_HEAD_SET_PROCESSING(0):
    case LW827D_HEAD_SET_COLWERSION(0):
    case LW827D_HEAD_SET_BASE_LUT_HI(0):
    case LW827D_HEAD_SET_OUTPUT_LUT_HI(0):
    case LW827D_HEAD_SET_LEGACY_CRC_CONTROL(0):
        Method += 0x00000400;
        break;
    case LW827D_HEAD_SET_PRESENT_CONTROL(1):
    case LW827D_HEAD_SET_PIXEL_CLOCK(1):
    case LW827D_HEAD_SET_CONTROL(1):
    case LW827D_HEAD_SET_LOCK_OFFSET(1):
    case LW827D_HEAD_SET_OVERSCAN_COLOR(1):
    case LW827D_HEAD_SET_RASTER_SIZE(1):
    case LW827D_HEAD_SET_RASTER_SYNC_END(1):
    case LW827D_HEAD_SET_RASTER_BLANK_END(1):
    case LW827D_HEAD_SET_RASTER_BLANK_START(1):
    case LW827D_HEAD_SET_RASTER_VERT_BLANK2(1):
    case LW827D_HEAD_SET_RASTER_VERT_BLANK_DMI(1):
    case LW827D_HEAD_SET_DEFAULT_BASE_COLOR(1):
    case LW827D_HEAD_SET_CRC_CONTROL(1):
    case LW827D_HEAD_SET_CONTEXT_DMA_CRC(1):
    case LW827D_HEAD_SET_CONTEXT_DMAS_ISO(1,0):
    case LW827D_HEAD_SET_CONTEXT_DMAS_ISO(1,1):
    case LW827D_HEAD_SET_CONTEXT_DMA_LWRSOR(1):
    case LW827D_HEAD_SET_CONTEXT_DMA_LUT(1):
    case LW827D_HEAD_SET_BASE_LUT_LO(1):
    case LW827D_HEAD_SET_OUTPUT_LUT_LO(1):
    case LW827D_HEAD_SET_OFFSET(1,0):
    case LW827D_HEAD_SET_OFFSET(1,1):
    case LW827D_HEAD_SET_SIZE(1):
    case LW827D_HEAD_SET_STORAGE(1):
    case LW827D_HEAD_SET_PARAMS(1):
    case LW827D_HEAD_SET_CONTROL_LWRSOR(1):
    case LW827D_HEAD_SET_OFFSET_LWRSOR(1):
    case LW827D_HEAD_SET_DITHER_CONTROL(1):
    case LW827D_HEAD_SET_CONTROL_OUTPUT_SCALER(1):
    case LW827D_HEAD_SET_PROCAMP(1):
    case LW827D_HEAD_SET_VIEWPORT_POINT_IN(1,0):
    case LW827D_HEAD_SET_VIEWPORT_POINT_IN(1,1):
    case LW827D_HEAD_SET_VIEWPORT_SIZE_IN(1):
    case LW827D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(1):
    case LW827D_HEAD_SET_VIEWPORT_SIZE_OUT(1):
    case LW827D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(1):
    case LW827D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(1):
    case LW827D_HEAD_SET_OVERLAY_USAGE_BOUNDS(1):
    case LW827D_HEAD_SET_PROCESSING(1):
    case LW827D_HEAD_SET_COLWERSION(1):
    case LW827D_HEAD_SET_BASE_LUT_HI(1):
    case LW827D_HEAD_SET_OUTPUT_LUT_HI(1):
    case LW827D_HEAD_SET_LEGACY_CRC_CONTROL(1):
        Method -= 0x00000400;
        break;
    case LW827D_DAC_SET_CONTROL(0):
    case LW827D_DAC_SET_CONTROL(1):
    case LW827D_DAC_SET_CONTROL(2):
        switch (REF_VAL(LW827D_DAC_SET_CONTROL_OWNER, Data)) {
        case LW827D_DAC_SET_CONTROL_OWNER_HEAD0:
            Data = FLD_SET_DRF_NUM(827D,_DAC_SET_CONTROL,_OWNER,
                                   LW827D_DAC_SET_CONTROL_OWNER_HEAD1, Data);
            break;
        case LW827D_DAC_SET_CONTROL_OWNER_HEAD1:
            Data = FLD_SET_DRF_NUM(827D,_DAC_SET_CONTROL,_OWNER,
                                   LW827D_DAC_SET_CONTROL_OWNER_HEAD0, Data);
            break;
        default:
            break;
        }
        break;
    case LW827D_SOR_SET_CONTROL(0):
    case LW827D_SOR_SET_CONTROL(1):
    // adding support for new SORs:
    case LW827D_SOR_SET_CONTROL(2):
    case LW827D_SOR_SET_CONTROL(3):
        switch (REF_VAL(LW827D_SOR_SET_CONTROL_OWNER, Data)) {
        case LW827D_SOR_SET_CONTROL_OWNER_HEAD0:
            Data = FLD_SET_DRF_NUM(827D,_SOR_SET_CONTROL,_OWNER,
                                   LW827D_SOR_SET_CONTROL_OWNER_HEAD1, Data);
            break;
        case LW827D_SOR_SET_CONTROL_OWNER_HEAD1:
            Data = FLD_SET_DRF_NUM(827D,_SOR_SET_CONTROL,_OWNER,
                                   LW827D_SOR_SET_CONTROL_OWNER_HEAD0, Data);
            break;
        default:
            break;
        }
        break;
    case LW827D_PIOR_SET_CONTROL(0):
    case LW827D_PIOR_SET_CONTROL(1):
    case LW827D_PIOR_SET_CONTROL(2):
        switch (REF_VAL(LW827D_PIOR_SET_CONTROL_OWNER, Data)) {
        case LW827D_PIOR_SET_CONTROL_OWNER_HEAD0:
            Data = FLD_SET_DRF_NUM(827D,_PIOR_SET_CONTROL,_OWNER,
                                   LW827D_PIOR_SET_CONTROL_OWNER_HEAD1, Data);
            break;
        case LW827D_PIOR_SET_CONTROL_OWNER_HEAD1:
            Data = FLD_SET_DRF_NUM(827D,_PIOR_SET_CONTROL,_OWNER,
                                   LW827D_PIOR_SET_CONTROL_OWNER_HEAD0, Data);
            break;
        default:
            break;
        }
        break;

    case LW827D_UPDATE:
        if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_BASE0, Data) &&
            REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_BASE1, Data)) {
            break;
        } else {
            if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_BASE0, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_BASE0,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_BASE1,1,Data);
            } else if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_BASE1, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_BASE1,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_BASE0,1,Data);
            }
        }
        if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_LWRSOR0, Data) &&
            REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_LWRSOR1, Data)) {
            break;
        } else {
            if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_LWRSOR0, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_LWRSOR0,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_LWRSOR1,1,Data);
            } else if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_LWRSOR1, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_LWRSOR1,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_LWRSOR0,1,Data);
            }
        }
        if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY0, Data) &&
            REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY1, Data)) {
            break;
        } else {
            if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY0, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY0,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY1,1,Data);
            } else if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY1, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY1,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY0,1,Data);
            }
        }
        if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM0, Data) &&
            REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM1, Data)) {
            break;
        } else {
            if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM0, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM0,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM1,1,Data);
            } else if (REF_VAL(LW827D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM1, Data)) {
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM1,0,Data);
                Data = FLD_SET_DRF_NUM(827D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM0,1,Data);
            }
        }
        break;
    default:
        // do nothing
        break;
    }
    break;
    case LW50_DISPLAY:
    switch (Method) {
    case LW507D_HEAD_SET_PRESENT_CONTROL(0):
    case LW507D_HEAD_SET_PIXEL_CLOCK(0):
    case LW507D_HEAD_SET_CONTROL(0):
    case LW507D_HEAD_SET_LOCK_OFFSET(0):
    case LW507D_HEAD_SET_OVERSCAN_COLOR(0):
    case LW507D_HEAD_SET_RASTER_SIZE(0):
    case LW507D_HEAD_SET_RASTER_SYNC_END(0):
    case LW507D_HEAD_SET_RASTER_BLANK_END(0):
    case LW507D_HEAD_SET_RASTER_BLANK_START(0):
    case LW507D_HEAD_SET_RASTER_VERT_BLANK2(0):
    case LW507D_HEAD_SET_RASTER_VERT_BLANK_DMI(0):
    case LW507D_HEAD_SET_DEFAULT_BASE_COLOR(0):
    case LW507D_HEAD_SET_CRC_CONTROL(0):
    case LW507D_HEAD_SET_CONTEXT_DMA_CRC(0):
    case LW507D_HEAD_SET_CONTEXT_DMA_ISO(0):
    case LW507D_HEAD_SET_BASE_LUT_LO(0):
    case LW507D_HEAD_SET_OUTPUT_LUT_LO(0):
    case LW507D_HEAD_SET_OFFSET(0,0):
    case LW507D_HEAD_SET_OFFSET(0,1):
    case LW507D_HEAD_SET_SIZE(0):
    case LW507D_HEAD_SET_STORAGE(0):
    case LW507D_HEAD_SET_PARAMS(0):
    case LW507D_HEAD_SET_CONTROL_LWRSOR(0):
    case LW507D_HEAD_SET_OFFSET_LWRSOR(0):
    case LW507D_HEAD_SET_DITHER_CONTROL(0):
    case LW507D_HEAD_SET_CONTROL_OUTPUT_SCALER(0):
    case LW507D_HEAD_SET_PROCAMP(0):
    case LW507D_HEAD_SET_VIEWPORT_POINT_IN(0,0):
    case LW507D_HEAD_SET_VIEWPORT_POINT_IN(0,1):
    case LW507D_HEAD_SET_VIEWPORT_SIZE_IN(0):
    case LW507D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(0):
    case LW507D_HEAD_SET_VIEWPORT_SIZE_OUT(0):
    case LW507D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(0):
    case LW507D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(0):
    case LW507D_HEAD_SET_OVERLAY_USAGE_BOUNDS(0):
    case LW507D_HEAD_SET_BASE_LUT_HI(0):
    case LW507D_HEAD_SET_OUTPUT_LUT_HI(0):
    case LW507D_HEAD_SET_LEGACY_CRC_CONTROL(0):
        Method += 0x00000400;
        break;
    case LW507D_HEAD_SET_PRESENT_CONTROL(1):
    case LW507D_HEAD_SET_PIXEL_CLOCK(1):
    case LW507D_HEAD_SET_CONTROL(1):
    case LW507D_HEAD_SET_LOCK_OFFSET(1):
    case LW507D_HEAD_SET_OVERSCAN_COLOR(1):
    case LW507D_HEAD_SET_RASTER_SIZE(1):
    case LW507D_HEAD_SET_RASTER_SYNC_END(1):
    case LW507D_HEAD_SET_RASTER_BLANK_END(1):
    case LW507D_HEAD_SET_RASTER_BLANK_START(1):
    case LW507D_HEAD_SET_RASTER_VERT_BLANK2(1):
    case LW507D_HEAD_SET_RASTER_VERT_BLANK_DMI(1):
    case LW507D_HEAD_SET_DEFAULT_BASE_COLOR(1):
    case LW507D_HEAD_SET_CRC_CONTROL(1):
    case LW507D_HEAD_SET_CONTEXT_DMA_CRC(1):
    case LW507D_HEAD_SET_CONTEXT_DMA_ISO(1):
    case LW507D_HEAD_SET_BASE_LUT_LO(1):
    case LW507D_HEAD_SET_OUTPUT_LUT_LO(1):
    case LW507D_HEAD_SET_OFFSET(1,0):
    case LW507D_HEAD_SET_OFFSET(1,1):
    case LW507D_HEAD_SET_SIZE(1):
    case LW507D_HEAD_SET_STORAGE(1):
    case LW507D_HEAD_SET_PARAMS(1):
    case LW507D_HEAD_SET_CONTROL_LWRSOR(1):
    case LW507D_HEAD_SET_OFFSET_LWRSOR(1):
    case LW507D_HEAD_SET_DITHER_CONTROL(1):
    case LW507D_HEAD_SET_CONTROL_OUTPUT_SCALER(1):
    case LW507D_HEAD_SET_PROCAMP(1):
    case LW507D_HEAD_SET_VIEWPORT_POINT_IN(1,0):
    case LW507D_HEAD_SET_VIEWPORT_POINT_IN(1,1):
    case LW507D_HEAD_SET_VIEWPORT_SIZE_IN(1):
    case LW507D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(1):
    case LW507D_HEAD_SET_VIEWPORT_SIZE_OUT(1):
    case LW507D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(1):
    case LW507D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(1):
    case LW507D_HEAD_SET_OVERLAY_USAGE_BOUNDS(1):
    case LW507D_HEAD_SET_BASE_LUT_HI(1):
    case LW507D_HEAD_SET_OUTPUT_LUT_HI(1):
    case LW507D_HEAD_SET_LEGACY_CRC_CONTROL(1):
        Method -= 0x00000400;
        break;
    case LW507D_DAC_SET_CONTROL(0):
    case LW507D_DAC_SET_CONTROL(1):
    case LW507D_DAC_SET_CONTROL(2):
        switch (REF_VAL(LW507D_DAC_SET_CONTROL_OWNER, Data)) {
        case LW507D_DAC_SET_CONTROL_OWNER_HEAD0:
            Data = FLD_SET_DRF_NUM(507D,_DAC_SET_CONTROL,_OWNER,
                                   LW507D_DAC_SET_CONTROL_OWNER_HEAD1, Data);
            break;
        case LW507D_DAC_SET_CONTROL_OWNER_HEAD1:
            Data = FLD_SET_DRF_NUM(507D,_DAC_SET_CONTROL,_OWNER,
                                   LW507D_DAC_SET_CONTROL_OWNER_HEAD0, Data);
            break;
        default:
            break;
        }
        break;
    case LW507D_SOR_SET_CONTROL(0):
    case LW507D_SOR_SET_CONTROL(1):
        switch (REF_VAL(LW507D_PIOR_SET_CONTROL_OWNER, Data)) {
        case LW507D_SOR_SET_CONTROL_OWNER_HEAD0:
            Data = FLD_SET_DRF_NUM(507D,_SOR_SET_CONTROL,_OWNER,
                                   LW507D_SOR_SET_CONTROL_OWNER_HEAD1, Data);
            break;
        case LW507D_SOR_SET_CONTROL_OWNER_HEAD1:
            Data = FLD_SET_DRF_NUM(507D,_SOR_SET_CONTROL,_OWNER,
                                   LW507D_SOR_SET_CONTROL_OWNER_HEAD0, Data);
            break;
        default:
            break;
        }
        break;
    case LW507D_PIOR_SET_CONTROL(0):
    case LW507D_PIOR_SET_CONTROL(1):
    case LW507D_PIOR_SET_CONTROL(2):
        switch (REF_VAL(LW507D_PIOR_SET_CONTROL_OWNER, Data)) {
        case LW507D_PIOR_SET_CONTROL_OWNER_HEAD0:
            Data = FLD_SET_DRF_NUM(507D,_PIOR_SET_CONTROL,_OWNER,
                                   LW507D_PIOR_SET_CONTROL_OWNER_HEAD1, Data);
            break;
        case LW507D_PIOR_SET_CONTROL_OWNER_HEAD1:
            Data = FLD_SET_DRF_NUM(507D,_PIOR_SET_CONTROL,_OWNER,
                                   LW507D_PIOR_SET_CONTROL_OWNER_HEAD0, Data);
            break;
        default:
            break;
        }
        break;

    case LW507D_UPDATE:
        if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_BASE0, Data) &&
            REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_BASE1, Data)) {
            break;
        } else {
            if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_BASE0, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_BASE0,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_BASE1,1,Data);
            } else if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_BASE1, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_BASE1,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_BASE0,1,Data);
            }
        }
        if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_LWRSOR0, Data) &&
            REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_LWRSOR1, Data)) {
            break;
        } else {
            if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_LWRSOR0, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_LWRSOR0,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_LWRSOR1,1,Data);
            } else if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_LWRSOR1, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_LWRSOR1,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_LWRSOR0,1,Data);
            }
        }
        if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY0, Data) &&
            REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY1, Data)) {
            break;
        } else {
            if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY0, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY0,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY1,1,Data);
            } else if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY1, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY1,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY0,1,Data);
            }
        }
        if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM0, Data) &&
            REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM1, Data)) {
            break;
        } else {
            if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM0, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM0,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM1,1,Data);
            } else if (REF_VAL(LW507D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM1, Data)) {
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM1,0,Data);
                Data = FLD_SET_DRF_NUM(507D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM0,1,Data);
            }
        }
        break;
    default:
        // do nothing
        break;
    }
    break;
    default:
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::Setup - Invalid Class.");
        break;
    }

    Printf(Tee::PriNormal, "Method: 0x%08x -> 0x%08x - Data: 0x%08x -> 0x%08x\n",
           OldMethod, Method, OldData, Data);

    return OK;
}

/****************************************************************************************
 * DispTest::SetUseHead
 *
 * Arguments Description:
 *  - use_head : boolean value to denote default head or from commandline
 *
 * Functional Description:
 *  - Set the use head setting for the current device
 ****************************************************************************************/
RC DispTest::SetUseHead (bool use_head, UINT32 vga_head)
{
    LwrrentDevice()->UseHead = use_head;
    LwrrentDevice()->VgaHead = vga_head;

    return OK;
}

/****************************************************************************************
 * DispTest::HandleUseHead
 *
 *  Arguments Description:
 *  - Method to be modified
 *  - Data to be modified
 *
 *  Functional Description:
 *  - if UseHead is true, massages the Method and Data to use vga_head
 ****************************************************************************************/
RC DispTest::HandleUseHead(UINT32& Method, UINT32& Data)
{
    if (! LwrrentDevice()->UseHead) return OK;
    UINT32 vga_head = LwrrentDevice()->VgaHead;

    if(LwrrentDisplayClass() == LW9070_DISPLAY)
    {
      switch (Method) {
      case LW907D_HEAD_SET_PRESENT_CONTROL(0):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(0):
      case LW907D_HEAD_SET_CONTROL(0):
      case LW907D_HEAD_SET_LOCK_OFFSET(0):
      case LW907D_HEAD_SET_OVERSCAN_COLOR(0):
      case LW907D_HEAD_SET_RASTER_SIZE(0):
      case LW907D_HEAD_SET_RASTER_SYNC_END(0):
      case LW907D_HEAD_SET_RASTER_BLANK_END(0):
      case LW907D_HEAD_SET_RASTER_BLANK_START(0):
      case LW907D_HEAD_SET_RASTER_VERT_BLANK2(0):
      case LW907D_HEAD_SET_LOCK_CHAIN(0):
      case LW907D_HEAD_SET_DEFAULT_BASE_COLOR(0):
      case LW907D_HEAD_SET_CRC_CONTROL(0):
      case LW907D_HEAD_SET_LEGACY_CRC_CONTROL(0):
      case LW907D_HEAD_SET_CONTEXT_DMA_CRC(0):
      case LW907D_HEAD_SET_BASE_LUT_LO(0):
      case LW907D_HEAD_SET_BASE_LUT_HI(0):
      case LW907D_HEAD_SET_OUTPUT_LUT_LO(0):
      case LW907D_HEAD_SET_OUTPUT_LUT_HI(0):
      case LW907D_HEAD_SET_PIXEL_CLOCK_FREQUENCY(0):
      case LW907D_HEAD_SET_PIXEL_CLOCK_CONFIGURATION(0):
      case LW907D_HEAD_SET_CONTEXT_DMA_LUT(0):
      case LW907D_HEAD_SET_OFFSET(0):
      case LW907D_HEAD_SET_SIZE(0):
      case LW907D_HEAD_SET_STORAGE(0):
      case LW907D_HEAD_SET_PARAMS(0):
      case LW907D_HEAD_SET_CONTEXT_DMAS_ISO(0):
      case LW907D_HEAD_SET_CONTROL_LWRSOR(0):
      case LW907D_HEAD_SET_OFFSET_LWRSOR(0):
      case LW907D_HEAD_SET_CONTEXT_DMA_LWRSOR(0):
      case LW907D_HEAD_SET_DITHER_CONTROL(0):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_SCALER(0):
      case LW907D_HEAD_SET_PROCAMP(0):
      case LW907D_HEAD_SET_VIEWPORT_POINT_IN(0):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_IN(0):
      case LW907D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(0):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(0):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MAX(0):
      case LW907D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(0):
      case LW907D_HEAD_SET_OVERLAY_USAGE_BOUNDS(0):
      case LW907D_HEAD_SET_PROCESSING(0):
      case LW907D_HEAD_SET_COLWERSION_RED(0):
      case LW907D_HEAD_SET_COLWERSION_GRN(0):
      case LW907D_HEAD_SET_COLWERSION_BLU(0):
      case LW907D_HEAD_SET_CSC_RED2RED(0):
      case LW907D_HEAD_SET_CSC_GRN2RED(0):
      case LW907D_HEAD_SET_CSC_BLU2RED(0):
      case LW907D_HEAD_SET_CSC_CONSTANT2RED(0):
      case LW907D_HEAD_SET_CSC_RED2GRN(0):
      case LW907D_HEAD_SET_CSC_GRN2GRN(0):
      case LW907D_HEAD_SET_CSC_BLU2GRN(0):
      case LW907D_HEAD_SET_CSC_CONSTANT2GRN(0):
      case LW907D_HEAD_SET_CSC_RED2BLU(0):
      case LW907D_HEAD_SET_CSC_GRN2BLU(0):
      case LW907D_HEAD_SET_CSC_BLU2BLU(0):
      case LW907D_HEAD_SET_CSC_CONSTANT2BLU(0):
      case LW907D_HEAD_SET_DISPLAY_ID(0,0):
      case LW907D_HEAD_SET_DISPLAY_ID(0,1):
      case LW907D_HEAD_SET_SW_SPARE_A(0):
      case LW907D_HEAD_SET_SW_SPARE_B(0):
      case LW907D_HEAD_SET_SW_SPARE_C(0):
      case LW907D_HEAD_SET_SW_SPARE_D(0):
      case LW907D_HEAD_SET_SPARE(0):
      case LW907D_HEAD_SET_SPARE_NOOP(0,0):
      case LW907D_HEAD_SET_SPARE_NOOP(0,1):
          switch (vga_head) {
          case 1: Method += 0x00000300; break;
          case 2: Method += 0x00000600; break;
          case 3: Method += 0x00000900; break;
          default: break;
          }
          break;
      case LW907D_HEAD_SET_PRESENT_CONTROL(1):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(1):
      case LW907D_HEAD_SET_CONTROL(1):
      case LW907D_HEAD_SET_LOCK_OFFSET(1):
      case LW907D_HEAD_SET_OVERSCAN_COLOR(1):
      case LW907D_HEAD_SET_RASTER_SIZE(1):
      case LW907D_HEAD_SET_RASTER_SYNC_END(1):
      case LW907D_HEAD_SET_RASTER_BLANK_END(1):
      case LW907D_HEAD_SET_RASTER_BLANK_START(1):
      case LW907D_HEAD_SET_RASTER_VERT_BLANK2(1):
      case LW907D_HEAD_SET_LOCK_CHAIN(1):
      case LW907D_HEAD_SET_DEFAULT_BASE_COLOR(1):
      case LW907D_HEAD_SET_CRC_CONTROL(1):
      case LW907D_HEAD_SET_LEGACY_CRC_CONTROL(1):
      case LW907D_HEAD_SET_CONTEXT_DMA_CRC(1):
      case LW907D_HEAD_SET_BASE_LUT_LO(1):
      case LW907D_HEAD_SET_BASE_LUT_HI(1):
      case LW907D_HEAD_SET_OUTPUT_LUT_LO(1):
      case LW907D_HEAD_SET_OUTPUT_LUT_HI(1):
      case LW907D_HEAD_SET_PIXEL_CLOCK_FREQUENCY(1):
      case LW907D_HEAD_SET_PIXEL_CLOCK_CONFIGURATION(1):
      case LW907D_HEAD_SET_CONTEXT_DMA_LUT(1):
      case LW907D_HEAD_SET_OFFSET(1):
      case LW907D_HEAD_SET_SIZE(1):
      case LW907D_HEAD_SET_STORAGE(1):
      case LW907D_HEAD_SET_PARAMS(1):
      case LW907D_HEAD_SET_CONTEXT_DMAS_ISO(1):
      case LW907D_HEAD_SET_CONTROL_LWRSOR(1):
      case LW907D_HEAD_SET_OFFSET_LWRSOR(1):
      case LW907D_HEAD_SET_CONTEXT_DMA_LWRSOR(1):
      case LW907D_HEAD_SET_DITHER_CONTROL(1):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_SCALER(1):
      case LW907D_HEAD_SET_PROCAMP(1):
      case LW907D_HEAD_SET_VIEWPORT_POINT_IN(1):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_IN(1):
      case LW907D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(1):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(1):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MAX(1):
      case LW907D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(1):
      case LW907D_HEAD_SET_OVERLAY_USAGE_BOUNDS(1):
      case LW907D_HEAD_SET_PROCESSING(1):
      case LW907D_HEAD_SET_COLWERSION_RED(1):
      case LW907D_HEAD_SET_COLWERSION_GRN(1):
      case LW907D_HEAD_SET_COLWERSION_BLU(1):
      case LW907D_HEAD_SET_CSC_RED2RED(1):
      case LW907D_HEAD_SET_CSC_GRN2RED(1):
      case LW907D_HEAD_SET_CSC_BLU2RED(1):
      case LW907D_HEAD_SET_CSC_CONSTANT2RED(1):
      case LW907D_HEAD_SET_CSC_RED2GRN(1):
      case LW907D_HEAD_SET_CSC_GRN2GRN(1):
      case LW907D_HEAD_SET_CSC_BLU2GRN(1):
      case LW907D_HEAD_SET_CSC_CONSTANT2GRN(1):
      case LW907D_HEAD_SET_CSC_RED2BLU(1):
      case LW907D_HEAD_SET_CSC_GRN2BLU(1):
      case LW907D_HEAD_SET_CSC_BLU2BLU(1):
      case LW907D_HEAD_SET_CSC_CONSTANT2BLU(1):
      case LW907D_HEAD_SET_DISPLAY_ID(1,0):
      case LW907D_HEAD_SET_DISPLAY_ID(1,1):
      case LW907D_HEAD_SET_SW_SPARE_A(1):
      case LW907D_HEAD_SET_SW_SPARE_B(1):
      case LW907D_HEAD_SET_SW_SPARE_C(1):
      case LW907D_HEAD_SET_SW_SPARE_D(1):
      case LW907D_HEAD_SET_SPARE(1):
      case LW907D_HEAD_SET_SPARE_NOOP(1,0):
      case LW907D_HEAD_SET_SPARE_NOOP(1,1):
          switch (vga_head) {
          case 0: Method -= 0x00000300; break;
          case 2: Method += 0x00000300; break;
          case 3: Method += 0x00000600; break;
          default: break;
          }
          break;

      case LW907D_HEAD_SET_PRESENT_CONTROL(2):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(2):
      case LW907D_HEAD_SET_CONTROL(2):
      case LW907D_HEAD_SET_LOCK_OFFSET(2):
      case LW907D_HEAD_SET_OVERSCAN_COLOR(2):
      case LW907D_HEAD_SET_RASTER_SIZE(2):
      case LW907D_HEAD_SET_RASTER_SYNC_END(2):
      case LW907D_HEAD_SET_RASTER_BLANK_END(2):
      case LW907D_HEAD_SET_RASTER_BLANK_START(2):
      case LW907D_HEAD_SET_RASTER_VERT_BLANK2(2):
      case LW907D_HEAD_SET_LOCK_CHAIN(2):
      case LW907D_HEAD_SET_DEFAULT_BASE_COLOR(2):
      case LW907D_HEAD_SET_CRC_CONTROL(2):
      case LW907D_HEAD_SET_LEGACY_CRC_CONTROL(2):
      case LW907D_HEAD_SET_CONTEXT_DMA_CRC(2):
      case LW907D_HEAD_SET_BASE_LUT_LO(2):
      case LW907D_HEAD_SET_BASE_LUT_HI(2):
      case LW907D_HEAD_SET_OUTPUT_LUT_LO(2):
      case LW907D_HEAD_SET_OUTPUT_LUT_HI(2):
      case LW907D_HEAD_SET_PIXEL_CLOCK_FREQUENCY(2):
      case LW907D_HEAD_SET_PIXEL_CLOCK_CONFIGURATION(2):
      case LW907D_HEAD_SET_CONTEXT_DMA_LUT(2):
      case LW907D_HEAD_SET_OFFSET(2):
      case LW907D_HEAD_SET_SIZE(2):
      case LW907D_HEAD_SET_STORAGE(2):
      case LW907D_HEAD_SET_PARAMS(2):
      case LW907D_HEAD_SET_CONTEXT_DMAS_ISO(2):
      case LW907D_HEAD_SET_CONTROL_LWRSOR(2):
      case LW907D_HEAD_SET_OFFSET_LWRSOR(2):
      case LW907D_HEAD_SET_CONTEXT_DMA_LWRSOR(2):
      case LW907D_HEAD_SET_DITHER_CONTROL(2):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_SCALER(2):
      case LW907D_HEAD_SET_PROCAMP(2):
      case LW907D_HEAD_SET_VIEWPORT_POINT_IN(2):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_IN(2):
      case LW907D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(2):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(2):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MAX(2):
      case LW907D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(2):
      case LW907D_HEAD_SET_OVERLAY_USAGE_BOUNDS(2):
      case LW907D_HEAD_SET_PROCESSING(2):
      case LW907D_HEAD_SET_COLWERSION_RED(2):
      case LW907D_HEAD_SET_COLWERSION_GRN(2):
      case LW907D_HEAD_SET_COLWERSION_BLU(2):
      case LW907D_HEAD_SET_CSC_RED2RED(2):
      case LW907D_HEAD_SET_CSC_GRN2RED(2):
      case LW907D_HEAD_SET_CSC_BLU2RED(2):
      case LW907D_HEAD_SET_CSC_CONSTANT2RED(2):
      case LW907D_HEAD_SET_CSC_RED2GRN(2):
      case LW907D_HEAD_SET_CSC_GRN2GRN(2):
      case LW907D_HEAD_SET_CSC_BLU2GRN(2):
      case LW907D_HEAD_SET_CSC_CONSTANT2GRN(2):
      case LW907D_HEAD_SET_CSC_RED2BLU(2):
      case LW907D_HEAD_SET_CSC_GRN2BLU(2):
      case LW907D_HEAD_SET_CSC_BLU2BLU(2):
      case LW907D_HEAD_SET_CSC_CONSTANT2BLU(2):
      case LW907D_HEAD_SET_DISPLAY_ID(2,0):
      case LW907D_HEAD_SET_DISPLAY_ID(2,1):
      case LW907D_HEAD_SET_SW_SPARE_A(2):
      case LW907D_HEAD_SET_SW_SPARE_B(2):
      case LW907D_HEAD_SET_SW_SPARE_C(2):
      case LW907D_HEAD_SET_SW_SPARE_D(2):
      case LW907D_HEAD_SET_SPARE(2):
      case LW907D_HEAD_SET_SPARE_NOOP(2,0):
      case LW907D_HEAD_SET_SPARE_NOOP(2,1):
          switch (vga_head) {
          case 0: Method -= 0x00000600; break;
          case 1: Method -= 0x00000300; break;
          case 3: Method += 0x00000300; break;
          default: break;
          }
          break;
      case LW907D_HEAD_SET_PRESENT_CONTROL(3):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(3):
      case LW907D_HEAD_SET_CONTROL(3):
      case LW907D_HEAD_SET_LOCK_OFFSET(3):
      case LW907D_HEAD_SET_OVERSCAN_COLOR(3):
      case LW907D_HEAD_SET_RASTER_SIZE(3):
      case LW907D_HEAD_SET_RASTER_SYNC_END(3):
      case LW907D_HEAD_SET_RASTER_BLANK_END(3):
      case LW907D_HEAD_SET_RASTER_BLANK_START(3):
      case LW907D_HEAD_SET_RASTER_VERT_BLANK2(3):
      case LW907D_HEAD_SET_LOCK_CHAIN(3):
      case LW907D_HEAD_SET_DEFAULT_BASE_COLOR(3):
      case LW907D_HEAD_SET_CRC_CONTROL(3):
      case LW907D_HEAD_SET_LEGACY_CRC_CONTROL(3):
      case LW907D_HEAD_SET_CONTEXT_DMA_CRC(3):
      case LW907D_HEAD_SET_BASE_LUT_LO(3):
      case LW907D_HEAD_SET_BASE_LUT_HI(3):
      case LW907D_HEAD_SET_OUTPUT_LUT_LO(3):
      case LW907D_HEAD_SET_OUTPUT_LUT_HI(3):
      case LW907D_HEAD_SET_PIXEL_CLOCK_FREQUENCY(3):
      case LW907D_HEAD_SET_PIXEL_CLOCK_CONFIGURATION(3):
      case LW907D_HEAD_SET_CONTEXT_DMA_LUT(3):
      case LW907D_HEAD_SET_OFFSET(3):
      case LW907D_HEAD_SET_SIZE(3):
      case LW907D_HEAD_SET_STORAGE(3):
      case LW907D_HEAD_SET_PARAMS(3):
      case LW907D_HEAD_SET_CONTEXT_DMAS_ISO(3):
      case LW907D_HEAD_SET_CONTROL_LWRSOR(3):
      case LW907D_HEAD_SET_OFFSET_LWRSOR(3):
      case LW907D_HEAD_SET_CONTEXT_DMA_LWRSOR(3):
      case LW907D_HEAD_SET_DITHER_CONTROL(3):
      case LW907D_HEAD_SET_CONTROL_OUTPUT_SCALER(3):
      case LW907D_HEAD_SET_PROCAMP(3):
      case LW907D_HEAD_SET_VIEWPORT_POINT_IN(3):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_IN(3):
      case LW907D_HEAD_SET_VIEWPORT_POINT_OUT_ADJUST(3):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MIN(3):
      case LW907D_HEAD_SET_VIEWPORT_SIZE_OUT_MAX(3):
      case LW907D_HEAD_SET_BASE_CHANNEL_USAGE_BOUNDS(3):
      case LW907D_HEAD_SET_OVERLAY_USAGE_BOUNDS(3):
      case LW907D_HEAD_SET_PROCESSING(3):
      case LW907D_HEAD_SET_COLWERSION_RED(3):
      case LW907D_HEAD_SET_COLWERSION_GRN(3):
      case LW907D_HEAD_SET_COLWERSION_BLU(3):
      case LW907D_HEAD_SET_CSC_RED2RED(3):
      case LW907D_HEAD_SET_CSC_GRN2RED(3):
      case LW907D_HEAD_SET_CSC_BLU2RED(3):
      case LW907D_HEAD_SET_CSC_CONSTANT2RED(3):
      case LW907D_HEAD_SET_CSC_RED2GRN(3):
      case LW907D_HEAD_SET_CSC_GRN2GRN(3):
      case LW907D_HEAD_SET_CSC_BLU2GRN(3):
      case LW907D_HEAD_SET_CSC_CONSTANT2GRN(3):
      case LW907D_HEAD_SET_CSC_RED2BLU(3):
      case LW907D_HEAD_SET_CSC_GRN2BLU(3):
      case LW907D_HEAD_SET_CSC_BLU2BLU(3):
      case LW907D_HEAD_SET_CSC_CONSTANT2BLU(3):
      case LW907D_HEAD_SET_DISPLAY_ID(3,0):
      case LW907D_HEAD_SET_DISPLAY_ID(3,1):
      case LW907D_HEAD_SET_SW_SPARE_A(3):
      case LW907D_HEAD_SET_SW_SPARE_B(3):
      case LW907D_HEAD_SET_SW_SPARE_C(3):
      case LW907D_HEAD_SET_SW_SPARE_D(3):
      case LW907D_HEAD_SET_SPARE(3):
      case LW907D_HEAD_SET_SPARE_NOOP(3,0):
      case LW907D_HEAD_SET_SPARE_NOOP(3,1):
          switch (vga_head) {
          case 0: Method -= 0x00000900; break;
          case 1: Method -= 0x00000600; break;
          case 2: Method -= 0x00000300; break;
          default: break;
          }
          break;
      case LW907D_DAC_SET_CONTROL(0):
      case LW907D_DAC_SET_CONTROL(1):
      case LW907D_DAC_SET_CONTROL(2):
          switch (vga_head) {
          case 0: Data = FLD_SET_DRF_NUM(907D,_DAC_SET_CONTROL,_OWNER_MASK,LW907D_DAC_SET_CONTROL_OWNER_MASK_HEAD0, Data); break;
          case 1: Data = FLD_SET_DRF_NUM(907D,_DAC_SET_CONTROL,_OWNER_MASK,LW907D_DAC_SET_CONTROL_OWNER_MASK_HEAD1, Data); break;
          case 2: Data = FLD_SET_DRF_NUM(907D,_DAC_SET_CONTROL,_OWNER_MASK,LW907D_DAC_SET_CONTROL_OWNER_MASK_HEAD2, Data); break;
          case 3: Data = FLD_SET_DRF_NUM(907D,_DAC_SET_CONTROL,_OWNER_MASK,LW907D_DAC_SET_CONTROL_OWNER_MASK_HEAD3, Data); break;
          default: break;
          }
          break;
      case LW907D_SOR_SET_CONTROL(0):
      case LW907D_SOR_SET_CONTROL(1):
      case LW907D_SOR_SET_CONTROL(2):
      case LW907D_SOR_SET_CONTROL(3):
          switch (vga_head) {
          case 0: Data = FLD_SET_DRF_NUM(907D,_SOR_SET_CONTROL,_OWNER_MASK,LW907D_SOR_SET_CONTROL_OWNER_MASK_HEAD0, Data); break;
          case 1: Data = FLD_SET_DRF_NUM(907D,_SOR_SET_CONTROL,_OWNER_MASK,LW907D_SOR_SET_CONTROL_OWNER_MASK_HEAD1, Data); break;
          case 2: Data = FLD_SET_DRF_NUM(907D,_SOR_SET_CONTROL,_OWNER_MASK,LW907D_SOR_SET_CONTROL_OWNER_MASK_HEAD2, Data); break;
          case 3: Data = FLD_SET_DRF_NUM(907D,_SOR_SET_CONTROL,_OWNER_MASK,LW907D_SOR_SET_CONTROL_OWNER_MASK_HEAD3, Data); break;
          default: break;
          }
          break;
      case LW907D_PIOR_SET_CONTROL(0):
      case LW907D_PIOR_SET_CONTROL(1):
      case LW907D_PIOR_SET_CONTROL(2):
          switch (vga_head) {
          case 0: Data = FLD_SET_DRF_NUM(907D,_PIOR_SET_CONTROL,_OWNER_MASK,LW907D_PIOR_SET_CONTROL_OWNER_MASK_HEAD0, Data); break;
          case 1: Data = FLD_SET_DRF_NUM(907D,_PIOR_SET_CONTROL,_OWNER_MASK,LW907D_PIOR_SET_CONTROL_OWNER_MASK_HEAD1, Data); break;
          case 2: Data = FLD_SET_DRF_NUM(907D,_PIOR_SET_CONTROL,_OWNER_MASK,LW907D_PIOR_SET_CONTROL_OWNER_MASK_HEAD2, Data); break;
          case 3: Data = FLD_SET_DRF_NUM(907D,_PIOR_SET_CONTROL,_OWNER_MASK,LW907D_PIOR_SET_CONTROL_OWNER_MASK_HEAD3, Data); break;
          default: break;
          }
          break;
      case LW907D_UPDATE:
          if(REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_BASE0, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_BASE1, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_BASE2, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_BASE3, Data)) {
             break;
          }
          else {
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE0,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE1,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE2,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE3,0,Data);
               switch (vga_head) {
               case 0: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE0,1,Data); break;
               case 1: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE1,1,Data); break;
               case 2: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE2,1,Data); break;
               case 3: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_BASE3,1,Data); break;
               default: break;
               }
          }
          if(REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_LWRSOR0, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_LWRSOR1, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_LWRSOR2, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_LWRSOR3, Data)) {
             break;
          }
          else {
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR0,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR1,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR2,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR3,0,Data);
               switch (vga_head) {
               case 0: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR0,1,Data); break;
               case 1: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR1,1,Data); break;
               case 2: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR2,1,Data); break;
               case 3: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_LWRSOR3,1,Data); break;
               default: break;
               }
          }
          if(REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY0, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY1, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY2, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY3, Data)) {
             break;
          }
          else {
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY0,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY1,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY2,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY3,0,Data);
               switch (vga_head) {
               case 0: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY0,1,Data); break;
               case 1: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY1,1,Data); break;
               case 2: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY2,1,Data); break;
               case 3: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY3,1,Data); break;
               default: break;
               }
          }
          if(REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM0, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM1, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM2, Data) &&
             REF_VAL(LW907D_UPDATE_INTERLOCK_WITH_OVERLAY_IMM3, Data)) {
             break;
          }
          else {
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM0,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM1,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM2,0,Data);
               Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM3,0,Data);
               switch (vga_head) {
               case 0: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM0,1,Data); break;
               case 1: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM1,1,Data); break;
               case 2: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM2,1,Data); break;
               case 3: Data = FLD_SET_DRF_NUM(907D,_UPDATE,_INTERLOCK_WITH_OVERLAY_IMM3,1,Data); break;
               default: break;
               }
          }
          break;
      default: break;
      }
    }
    return OK;
}

/****************************************************************************************
 * DispTest::EnqMethodOpcode
 *
 *  Arguments Description:
 *  - ChannelHandle is a handle returned by a DispTest.Setup() Call.
 *  - Opcode is the OPCODE to send down
 *  - Method is a Method Name as defined in the class headers.
 *  - Count is the number of data items
 *  - DataPtr is the pointer to the list of data items of "Count" length
 *
 *  Functional Description:
 *  - Enqueue Method into the Channel PushBuffer
 *  - Enqueue the Data associated with the Method
 ****************************************************************************************/
RC DispTest::EnqMethodOpcode(LwRm::Handle ChannelHandle, UINT32 Opcode,
                             UINT32 Method, UINT32 Count, const UINT32 * DataPtr)
{
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::EnqMethodOpcode - Can't find Channel.  Did you configure the channel already?");
    }

    // write method to channel
    return channel->WriteOpcode(Opcode, Method, Count, DataPtr);
}

/****************************************************************************************
 * DispTest::SetAutoFlush
 *
 *  Arguments Description:
 *  - ChannelHandle : a handle returned by a DispTest.Setup() Call.
 *  - AutoFlushEnable: boolean indicating whether to enable auto-flush
 *  - AutoFlushThreshold: the number of enqueued methods required to trigger an auto-flush
 *
 *  Functional Description
 *  - Update the Push Buffer Put Pointer
 *
 *  Note that AutoFlush is disabled by default (this is done in DispTest::Setup)
 ****************************************************************************************/
RC DispTest::SetAutoFlush(LwRm::Handle ChannelHandle, bool AutoFlushEnable, UINT32 AutoFlushThreshold)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetAutoFlush - Can't find Channel.  Did you configure the channel already?");
    }

    // set the auto-flush behavior
    CHECK_RC_MSG
    (
        channel->SetAutoFlush(AutoFlushEnable, AutoFlushThreshold),
        "***ERROR: DispTest::SetAutoFlush - Can't Set AutoFlush Parameters!!!"
    );

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::Flush
 *
 *  Arguments Description:
 *  - ChannelHandle : a handle returned by a DispTest.Setup() Call.
 *
 *  Functional Description
 *  - Update the Push Buffer Put Pointer
 ****************************************************************************************/
RC DispTest::Flush(LwRm::Handle ChannelHandle)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    // get the channel
    DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::Flush - Can't find Channel.  Did you configure the channel already?");
    }

    // flush the cached put pointer and update channel PUT Priv Reg
    CHECK_RC_MSG
    (
        channel->Flush(),
        "***ERROR: DispTest::Flush - Can't update the Channel Put Pointer !!!"
    );

    // RC == OK at this point
    return rc;
}

RC DispTest::GetBaseChannelScanline(LwRm::Handle ChannelHandle, UINT32 *pRtlwalue)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    string ChannelName;

    // get the channel
    DmaDisplayChannel* channel =
        static_cast<DmaDisplayChannel*>(pLwRm->GetDisplayChannel(ChannelHandle));
      //DisplayChannel * channel = pLwRm->GetDisplayChannel(ChannelHandle);
    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::GetBaseChannelScanline - Can't find Channel.  Did you configure the channel already?");
    }

    ChannelName = LwrrentDevice()->ChannelNameMap[ChannelHandle].c_str();

    if (!(ChannelName == "base")) {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::GetBaseChannelScanline - Channel must be base");
    }

    *pRtlwalue = channel->ScanlineRead(0);
    return rc;

}

RC DispTest::GetChannelPut(LwRm::Handle ChannelHandle, UINT32 *pRtlwalue)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    string ChannelName;

    // get the channel
    DmaDisplayChannel* channel =
        static_cast<DmaDisplayChannel*>(pLwRm->GetDisplayChannel(ChannelHandle));

    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::GetChannelPut - Can't find Channel.  Did you configure the channel already?");
    }

    ChannelName = LwrrentDevice()->ChannelNameMap[ChannelHandle].c_str();

    if (!((ChannelName == "core") || (ChannelName == "base") ||  (ChannelName == "overlay"))) {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::GetChannelPut - Channel must be core, base or overlay");
    }

    *pRtlwalue = channel->GetPut(ChannelName);
    return rc;

}

RC DispTest::SetChannelPut(LwRm::Handle ChannelHandle, UINT32 newPut)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    string ChannelName;

    // get the channel
    DmaDisplayChannel* channel =
        static_cast<DmaDisplayChannel*>(pLwRm->GetDisplayChannel(ChannelHandle));

    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChannelPut - Can't find Channel.  Did you configure the channel already?");
    }

    ChannelName = LwrrentDevice()->ChannelNameMap[ChannelHandle].c_str();

    if (!((ChannelName == "core") || (ChannelName == "base") ||  (ChannelName == "overlay"))) {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChannelPut - Channel must be core, base or overlay");
    }

    channel->SetPut(ChannelName, newPut);
    return rc;

}

RC DispTest::GetChannelGet(LwRm::Handle ChannelHandle, UINT32 *pRtlwalue)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    string ChannelName;

    // get the channel
    DmaDisplayChannel* channel =
        static_cast<DmaDisplayChannel*>(pLwRm->GetDisplayChannel(ChannelHandle));

    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::GetChannelGet - Can't find Channel.  Did you configure the channel already?");
    }

    ChannelName = LwrrentDevice()->ChannelNameMap[ChannelHandle].c_str();

    if (!((ChannelName == "core") || (ChannelName == "base") ||  (ChannelName == "overlay"))) {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::GetChannelGet - Channel must be core, base or overlay");
    }

    *pRtlwalue = channel->GetGet(ChannelName);
    return rc;

}

RC DispTest::SetChannelGet(LwRm::Handle ChannelHandle, UINT32 newGet)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    string ChannelName;

    // get the channel
    DmaDisplayChannel* channel =
        static_cast<DmaDisplayChannel*>(pLwRm->GetDisplayChannel(ChannelHandle));

    if (!channel)
    {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChannelGet - Can't find Channel.  Did you configure the channel already?");
    }

    ChannelName = LwrrentDevice()->ChannelNameMap[ChannelHandle].c_str();

    if (!((ChannelName == "core") || (ChannelName == "base") ||  (ChannelName == "overlay"))) {
        RETURN_RC_MSG (RC::LWRM_ILWALID_CHANNEL, "***ERROR: DispTest::SetChannelGet - Channel must be core, base or overlay");
    }

    channel->SetGet(ChannelName, newGet);
    return rc;

}

RC DispTest::GetPtimer(UINT32 *TimeHi, UINT32 *TimeLo)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::EnqUpdateAndMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->GetPtimer(TimeHi, TimeLo));
}

/****************************************************************************************
 * DispTest::AddDmaContext
 *
 *  Arguments Description:
 *  - Handle
 *  - Target
 *  - Flags
 *  - Address
 *  - Limit
 *  - Offset
 *
 *  Functional Description
 *  - Set the DMA context for the current device
 ****************************************************************************************/
RC DispTest::AddDmaContext(LwRm::Handle Handle,
                           const char * Target,
                           UINT32 Flags,
                           void *Address,
                           UINT64 Limit,
                           UINT64 Offset)
{
    DmaContext *DmaCtx = new DmaContext;
    if (!DmaCtx)
        RETURN_RC_MSG (RC::CANNOT_ALLOCATE_MEMORY, "***ERROR: DispTest::AddDmaContext - Unable to create DmaContext.");
    DmaCtx->Target = new string(Target);
    DmaCtx->Flags   = Flags;
    DmaCtx->Address = Address;
    DmaCtx->Limit   = Limit;
    DmaCtx->Offset  = Offset;
    LwrrentDevice()->DmaContexts[Handle] = DmaCtx;
    return OK;
}

/****************************************************************************************
 * DispTest::GetDmaContext
 *
 *  Arguments Description:
 *  - Handle
 *
 *  Functional Description
 *  - Get the DMA context from the current device
 ****************************************************************************************/
DispTest::DmaContext *DispTest::GetDmaContext(LwRm::Handle Handle)
{
    if (LwrrentDevice()->DmaContexts.find(Handle) != LwrrentDevice()->DmaContexts.end())
    {
        return LwrrentDevice()->DmaContexts[Handle];
    }

    return NULL;
}

/****************************************************************************************
 * DispTest::AddChannelContext
 *
 *  Arguments Description:
 *  - DispChannelHandle
 *  - Class
 *  - Head
 *  - hErrCtxDma
 *  - hPushBufferCtxDma
 *
 *  Functional Description
 *  - Add a new channel context to the current device
 ****************************************************************************************/
void DispTest::AddChannelContext
(
    LwRm::Handle DispChannelHandle,
    UINT32 Class,
    UINT32 Head,
    LwRm::Handle hErrCtxDma,
    LwRm::Handle hPushBufferCtxDma
)
{
    ChannelContextInfo *newCtx = new ChannelContextInfo;
    newCtx->Class = Class;
    newCtx->Head = Head;
    newCtx->hErrorNotifierCtxDma = hErrCtxDma;
    newCtx->hPBCtxDma = hPushBufferCtxDma;

    LwrrentDevice()->d_ChannelCtxInfo[DispChannelHandle] = newCtx;
}

/****************************************************************************************
 * DispTest::GetChannelContext
 *
 *  Arguments Description:
 *  - hCh
 *
 *  Functional Description
 *  - Get the channel context from the current device
 ****************************************************************************************/
DispTest::ChannelContextInfo *DispTest::GetChannelContext(LwRm::Handle hCh)
{
    if(LwrrentDevice()->d_ChannelCtxInfo.find(hCh) != LwrrentDevice()->d_ChannelCtxInfo.end())
    {
        return LwrrentDevice()->d_ChannelCtxInfo[hCh];
    }

    return NULL;
}

/****************************************************************************************
 * DispTest::SearchList
 *
 *  Arguments Description:
 *  - Element
 *  - List
 *
 *  Functional Description:
 *  - Determine if the given list contains the given element
 ****************************************************************************************/
template <class T> bool DispTest::SearchList (T *Element,
                                              list<T> *List)
{
    const T *Current = new T();
    for (typename list<T>::iterator iter = List->begin(); iter != List->end(); iter++) {
        Current = &(List->front());
        if (*Current == *Element) {
            return true;
        } else {
            List->push_back(*Current);
            List->pop_front();
        }

    }
    delete(Current);
    return false;
}

/****************************************************************************************
 * DispTest::CreateNotifierContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle: Handle to the channel the surface will be associated to
 *    (This is purely for book keeping purposes).
 *  - MemoryLocation: Area where the memory resides ("vid", "ncoh", "coh").
 *  - pRtnHandle
 *
 *  Functional Description:
 *  - Allocate Memory and create a CtxDMA
 *  - Return a Handle for the Notifier
 ****************************************************************************************/
RC DispTest::CreateNotifierContextDma(LwRm::Handle ChannelHandle,
                                      string MemoryLocation,
                                      LwRm::Handle *pRtnHandle)
{
    // for video heap memory, specify the correct type and attributes
    if ((MemoryLocation == "lwm") ||
        (MemoryLocation == "vid"))
    {
        // for Fermi we must allocate with 4KB page swizzle
        return CreateVidHeapContextDma(
            ChannelHandle,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
            DISPTEST_NOTIFIER_SIZE,
            LWOS32_TYPE_IMAGE,
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB),
            pRtnHandle);
    }
    else
    {
        return CreateSystemContextDma(
            ChannelHandle,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
            DISPTEST_NOTIFIER_SIZE,
            MemoryLocation,
            pRtnHandle);
    }
}

/****************************************************************************************
 * DispTest::CreateNotifierContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle: Handle to the channel the surface will be associated to
 *    (This is purely for book keeping purposes).
 *  - MemoryLocation: Area where the memory resides ("vid", "ncoh", "coh").
 *  - pRtnHandle
 *  - bindCtx: boolean defining if the context DMA should be bound to the ChannelHandle
 *
 *  Functional Description:
 *  - Allocate Memory and create a CtxDMA
 *  - Return a Handle for the Notifier
 ****************************************************************************************/
RC DispTest::CreateNotifierContextDma(LwRm::Handle ChannelHandle,
                                      string MemoryLocation,
                                      LwRm::Handle *pRtnHandle,
                                      bool bindCtx)
{
    // for video heap memory, specify the correct type and attributes
    if ((MemoryLocation == "lwm") ||
        (MemoryLocation == "vid"))
    {
        // for Fermi we must allocate with 4KB page swizzle
        return CreateVidHeapContextDma(
            ChannelHandle,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
            DISPTEST_NOTIFIER_SIZE,
            LWOS32_TYPE_IMAGE,
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB),
            pRtnHandle,
            bindCtx);
    }
    else
    {
        return CreateSystemContextDma(
            ChannelHandle,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
            DISPTEST_NOTIFIER_SIZE,
            MemoryLocation,
            pRtnHandle,
            bindCtx);
    }
}

/****************************************************************************************
 * DispTest::CreateSemaphoreContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle: Handle to the channel the surface will be associated to
 *    (This is purely for book keeping purposes).
 *  - MemoryLocation: Area where the surface memory resides ("vid", "ncoh", "coh").
 *  - pRtnHandle
 *
 *  Functional Description:
 *  - Allocate Memory and create a CtxDMA for the Semaphore
 *  - Return a Handle for the CtxDMA
 ****************************************************************************************/
RC DispTest::CreateSemaphoreContextDma(LwRm::Handle ChannelHandle,
                                       string MemoryLocation,
                                       LwRm::Handle *pRtnHandle)
{
    // for video heap memory, specify the correct type and attributes
    if ((MemoryLocation == "lwm") ||
        (MemoryLocation == "vid"))
    {
        // for Fermi we must allocate with 4KB page swizzle
        return CreateVidHeapContextDma(
            ChannelHandle,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
            DISPTEST_SEMAPHORE_SIZE,
            LWOS32_TYPE_IMAGE,
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB),
            pRtnHandle);
    }
    else
    {
        return CreateSystemContextDma(
            ChannelHandle,
            DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
            DRF_DEF(OS03, _FLAGS, _TYPE, _NOTIFIER),
            DISPTEST_SEMAPHORE_SIZE,
            MemoryLocation,
            pRtnHandle);
    }
}

/****************************************************************************************
 * DispTest::CreateContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle: Handle to the channel the surface will be associated to
 *     (This is purely for book keeping purposes).
 *  - Flags: OR the different settings together (The settings are defined in
 *    dev_ram.ref)
 *      - Access:
 *          Read Only: LW_DMA_ACCESS_READ_ONLY
 *          Read/Write: LW_DMA_ACCESS_READ_AND_WRITE
 *          From PTE: LW_DMA_ACCESS_FROM_PTE
 *      - Compression
 *          None: LW_DMA_COMP_TAGS_NONE
 *          Type A: LW_DMA_COMP_TAGS_TYPE_A
 *          Type B: LW_DMA_COMP_TAGS_TYPE_B
 *          From PTE: LW_DMA_COMP_TAGS_FROM_PTE
 *
 *  - Size : Memory Size to be allocated
 *  - MemoryLocation: One of the possible targets defined in dispTypes_01.mfs
 *    ("vid", "ncoh", "coh").
 *  - pRtnHandle
 *
 *  Functional Description
 *  - Allocate Memory and create a CtxDMA
 *  - Return a Handle for the CtxDMA
 ****************************************************************************************/
RC DispTest::CreateContextDma(LwRm::Handle ChannelHandle,
                              UINT32 Flags,
                              UINT32 Size,
                              string MemoryLocation,
                              LwRm::Handle *pRtnHandle)
{
    // for video heap memory, specify the correct type and attributes
    if ((MemoryLocation == "lwm") ||
        (MemoryLocation == "vid"))
    {
        return CreateVidHeapContextDma(
            ChannelHandle,
            Flags,
            Size,
            LWOS32_TYPE_INSTANCE,
            LWOS32_ATTR_NONE,
            pRtnHandle);
    }
    else
    {
        return CreateSystemContextDma(
            ChannelHandle,
            Flags,
            Size,
            MemoryLocation,
            pRtnHandle);
    }
}

/****************************************************************************************
 * DispTest::DeleteContextDma
 *
 *  Arguments Description:
 *  - Handle: Handle to a pre-allocated surface
 *
 *  Functional Description:
 *  - Release the handle and free the allocated memory
 ****************************************************************************************/
RC DispTest::DeleteContextDma(LwRm::Handle Handle)
{
    LwRmPtr pLwRm;

    // release the context dma handle
    pLwRm->Free(Handle);
    LwrrentDevice()->DmaContexts.erase(Handle);

    return OK;
}

/****************************************************************************************
 * DispTest::SetContextDmaDebugRegs
 *
 *  Arguments Description:
 *  - Handle: Handle to a pre-allocated surface
 *
 *  Functional Description:
 *  - Write the Context Dma Params to the Debug Registers
 ****************************************************************************************/
RC DispTest::SetContextDmaDebugRegs(LwRm::Handle Handle)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetContextDmaDebugRegs - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetContextDmaDebugRegs(Handle));
}

/****************************************************************************************
 * DispTest::GetChannelNum
 *
 *  Arguments Description:
 *  - channelClass:
 *  - channelInstance:
 *  - channelNum:
 *
 *  Functional Description:
 *  - Get the channel number based on the given channel class and channel instance
 ****************************************************************************************/
RC DispTest::GetChannelNum(UINT32 channelClass, UINT32 channelInstance, UINT32 *channelNum)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetChannelNum - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->GetChannelNum(channelClass, channelInstance, channelNum));
}

/****************************************************************************************
 * DispTest::WriteVal32
 *
 *  Arguments Description:
 *  - Handle: handle to CtxDMA describing the memory area of interest.
 *  - Offset: offset within the memory area
 *  - Value: data to be written at Offset.
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Write Value at Base+Offset (Check if Offset < Size)
 ****************************************************************************************/
RC DispTest::WriteVal32(LwRm::Handle Handle, UINT32 Offset, UINT32 Value)
{
    // get context dma for the handle
    DmaContext *HandleCtxDma = GetDmaContext(Handle);
    if (!HandleCtxDma)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::WriteVal32 - Can't Find Ctx DMA for Handle.");
    }

    // get base pointer and limit
    UINT32 *pHandleBase = (UINT32*)(HandleCtxDma->Address);
    UINT64 HandleLimit = HandleCtxDma->Limit;

    // check that offset is within limit
    if ((Offset << 2) >= HandleLimit)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::WriteVal32 - Offset Greater than Limit of Memory Area.");
    }

    // write the value
    MEM_WR32((void *)(pHandleBase + Offset), Value);
    return OK;
}

/****************************************************************************************
 * DispTest::ReadVal32
 *
 *  Arguments Description:
 *  - Handle: handle to CtxDMA describing the memory area of interest.
 *  - Offset: offset within the memory area
 *
 *  Functional Description:
 *  - Resolve the Ctx DMA (Get Base and Size)
 *  - Read from Base+Offset (Check if Offset < Size) and return the Value.
 ***************************************************************************************/
RC DispTest::ReadVal32(LwRm::Handle Handle, UINT32 Offset, UINT32 *pRtlwalue)
{
    // get context dma for the handle
    DmaContext *HandleCtxDma = GetDmaContext(Handle);
    if (!HandleCtxDma)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::ReadVal32 - Can't Find Ctx DMA for Handle.");
    }

    // get base pointer and limit
    UINT32 *pHandleBase = (UINT32*)(HandleCtxDma->Address);
    UINT64 HandleLimit = HandleCtxDma->Limit;

    // check that offset is within limit
    if ((Offset << 2)  >= HandleLimit)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::ReadVal32 - Offset Greater than Limit of Memory Area.");
    }

    // read the value
    *pRtlwalue = MEM_RD32((void *)(pHandleBase + Offset));
    return OK;
}

/****************************************************************************************
 * DispTest::Cleanup
 *
 *  Functional Description:
 *  - Deallocate memory used for DispTest data structures
 ***************************************************************************************/
void DispTest::Cleanup()
{
    LwRmPtr pLwRm;

    while (! (LwrrentDevice()->DmaContexts.empty()) )
    {
        DeleteContextDma(LwrrentDevice()->DmaContexts.begin()->first);
    }
    LwrrentDevice()->d_ChannelCtxInfo.clear();

    map<LwRm::Handle, void*>::iterator it;
    for (it = LwrrentDevice()->MemMap.begin(); it != LwrrentDevice()->MemMap.end(); it++)
    {
        pLwRm->Free(it->first);
    }

    if (LwrrentDevice()->unInitializedClass)
    {
        // Free the Evo Object
        pLwRm->Free(LwrrentDevice()->ObjectHandle);

        // Free common display object
        pLwRm->Free(LwrrentDevice()->DispCommonHandle);
    }
}

/****************************************************************************************
 * DispTest::IsoDumpImages
 *
 *  Arguments Description:
 *  - rtl_pixel_trace_file:
 *  - testname:
 *  - add2p4:
 *
 *  Functional Description:
 *  - Saves output produced by rtl into tga files
 *
 *  Notes:
 *  - Depends on the Frame and RTLOutput classes
 ***************************************************************************************/
RC DispTest::IsoDumpImages(const string& rtl_pixel_trace_file, const string& testname, bool add2p4)
{
    Printf(Tee::PriNormal, "Info : creating images out of %s...\n", rtl_pixel_trace_file.c_str());

    RTLOutput out(rtl_pixel_trace_file);

    RC rc = OK;
    unsigned i = 0;
    FILE* fp;
    string fprefix;

    /* Get the prefix for final image filenames
     * For the dafault configuration, ie dac1, there is not prefix */
    i = unsigned(rtl_pixel_trace_file.find("_", 0));
    fprefix = rtl_pixel_trace_file;
    fprefix.erase(i+1, fprefix.length());
    if (fprefix == "dac1_") fprefix.erase(0, fprefix.length());
    i = 0;
    for (Frame* frame = out.GetFirst(); frame; frame = out.GetNext())
    {
        char filename[512];
        sprintf(filename, "%s_%s%02x.tga", testname.c_str(), fprefix.c_str(), i++);
        Printf(Tee::PriNormal, "Info: saving %s...\n", filename);

        bool p4_edited = false;
        if (add2p4)
        {
            if (OK == Utility::OpenFile(filename, &fp, "r"))
            {
                if (DispTest::P4Action("edit", filename))
                    Printf(Tee::PriError, "DispTest::IsoDumpImages - p4 edit %s failed\n", filename);
                else
                {
                    Printf(Tee::PriDebug, "p4 edit %s ... done\n", filename);
                    p4_edited = true;
                }
                fclose(fp);
            }
        }

        rc = ImageFile::WriteTga(filename,
                                 frame->Format(),
                                 frame->Data(),
                                 frame->Width(),
                                 frame->Height(),
                                 ColorUtils::TgaPixelBytes(frame->Format())*frame->Width(),
                                 false,
                                 false,
                                 false);
        if (rc != OK)
        {
            Printf(Tee::PriError, "DispTest::IsoDumpImages - Can't Write Tga File: (%s)\n", filename);
            delete frame;
            return rc;
        }

        if (add2p4 && !p4_edited)
        {
            if (DispTest::P4Action("add", filename))
                Printf(Tee::PriError, "DispTest::IsoDumpImages - p4 add %s failed\n", filename);
            else
                Printf(Tee::PriDebug, "p4 add %s ... done\n", filename);
        }

        delete frame;
    }

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::P4Action
 *
 *  Arguments Description:
 *  - action:    Perforce command to perform
 *  - filename:  File to be acted upon
 *
 *  Functional Description:
 *  - Performs a Perforce (p4) system command
 *
 *  Notes:
 *  - TODO!
 *  - this is a copy of P4Action() in mdiag dir (which is in golobal namespace)
 *    but it can not be used here since some configs are compiled witout mdiag
 *    perhaps we need to have only one copy... not sure where to put it right now
 ***************************************************************************************/
int DispTest::P4Action(const char *action, const char *filename)
{
    const char *absfilename;

    // colwert the filename into an absolute path filename
#if defined(_WIN32)
    char buf2[512];
    absfilename = _fullpath(buf2,filename,sizeof(buf2));
#else
    absfilename = filename;
#endif
    if (absfilename==NULL)
        return errno;

    char buf[512];
    sprintf(buf, "p4 %s %s", action, absfilename);
    return system(buf);
}

/****************************************************************************************
 * DispTest::RmaRegWr32
 *
 *  Arguments Description:
 *  - addr: address
 *  - data: data
 *
 *  Functional Description:
 *  - Performs 32-bit RMA writes in order to access priv regs.
 *    Not intended for memory accesses.
***************************************************************************************/
#include "gt215/dev_disp.h"
RC DispTest::RmaRegWr32(UINT32 addr, UINT32 data)
{
    RC rc = OK;
    bool color;
    UINT08 vio_misc;
    UINT08 save_index;

    // Make sure the Address is within the addressable space
    if ((addr & 0xE0000000) != 0)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::RmaRegWr32 - RMA Address out of range.");
    }

    // Read the MISC register to decide whether to use monochrome (3Bx) or color (3DX)
    // register space.
    CHECK_RC_MSG
    (
        Platform::PioRead08(LW_VIO_MISC__READ, &vio_misc),
        "***ERROR: DispTest::RmaRegWr32 unable to read LW_VIO_MISC__READ."
    );
    color = (REF_VAL(LW_VIO_MISC_IO_ADR,vio_misc) == LW_VIO_MISC_IO_ADR_03DX);

    // Save the index.
    CHECK_RC_MSG
    (
        Platform::PioRead08(color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO, &save_index),
        "***ERROR: DispTest::RmaRegWr32 unable to read LW_CIO_CRX__????."
    );

    // Set RMA register to point to the PTR field.
    CHECK_RC_MSG
    (
        DispTest::IsoIndexedRegWrVGA (color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO,
                                         LW_CIO_CRE_RMA__INDEX,
                                         DRF_NUM(_CIO, _CRE_RMA, _ENAB_RMA, 1) |
                                         DRF_DEF(_CIO, _CRE_RMA, _SEL_RMA, _PTR)),
        "***ERROR: DispTest::RmaRegWr32 unable to write LW_CIO_CRX__???? (PTR field)."
    );

    // Write the RMA address.
    CHECK_RC_MSG
    (
        Platform::PioWrite32(0x3d0, addr),
        "***ERROR: DispTest::RmaRegWr32 unable to write 0x3D0 (RMA address)."
    );

    // Set RMA register to point to the data32 field.
    CHECK_RC_MSG
    (
        DispTest::IsoIndexedRegWrVGA (color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO,
                                         LW_CIO_CRE_RMA__INDEX,
                                         DRF_NUM(_CIO, _CRE_RMA, _ENAB_RMA, 1) |
                                         DRF_DEF(_CIO, _CRE_RMA, _SEL_RMA, _DATA32)),
        "***ERROR: DispTest::RmaRegWr32 unable to write LW_CIO_CRX__???? (data32 field)."
    );

    // Write the RMA data
    CHECK_RC_MSG
    (
        Platform::PioWrite32(0x3d0, data),
        "***ERROR: DispTest::RmaRegWr32 unable to write 0x3D0 (RMA data)."
    );

    // Restore the index.
    CHECK_RC_MSG
    (
        Platform::PioWrite08(color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO, save_index),
        "***ERROR: DispTest::RmaRegWr32 unable to write LW_CIO_CRX__????."
    );

    return OK;
}

/****************************************************************************************
 * DispTest::RmaRegRd32
 *
 *  Arguments Description:
 *  - addr: address
 *  - pData: pointer to data
 *
 *  Functional Description:
 *  - Performs 32-bit RMA reads in order to access priv regs.
 *    Not intended for memory accesses.
 ***************************************************************************************/
RC DispTest::RmaRegRd32(UINT32 addr, UINT32 * pData)
{
    RC rc = OK;
    bool color;
    UINT08 vio_misc;
    UINT08 save_index;

    // Make sure the Address is within the addressable space
    if ((addr & 0xE0000000) != 0)
    {
        RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::RmaRegRd32 - RMA Address out of range.");
    }

    // Read the MISC register to decide whether to use monochrome (3Bx) or color (3DX)
    // register space.
    CHECK_RC_MSG
    (
        Platform::PioRead08(LW_VIO_MISC__READ, &vio_misc),
        "***ERROR: DispTest::RmaRegRd32 unable to read LW_VIO_MISC__READ."
    );
    color = (REF_VAL(LW_VIO_MISC_IO_ADR,vio_misc) == LW_VIO_MISC_IO_ADR_03DX);

    // Save the index.
    CHECK_RC_MSG
    (
        Platform::PioRead08(color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO, &save_index),
        "***ERROR: DispTest::RmaRegRd32 unable to read LW_CIO_CRX__????."
    );

    // Set RMA register to point to the PTR field.
    CHECK_RC_MSG
    (
        DispTest::IsoIndexedRegWrVGA (color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO,
                                         LW_CIO_CRE_RMA__INDEX,
                                         DRF_NUM(_CIO, _CRE_RMA, _ENAB_RMA, 1) |
                                         DRF_DEF(_CIO, _CRE_RMA, _SEL_RMA, _PTR)),
        "***ERROR: DispTest::RmaRegRd32 unable to write LW_CIO_CRX__???? (PTR field)."
    );

    // Write the RMA address.
    CHECK_RC_MSG
    (
        Platform::PioWrite32(0x3d0, addr),
        "***ERROR: DispTest::RmaRegRd32 unable to write 0x3D0 (RMA address)."
    );

    // Set RMA register to point to the data field used for reads.
    CHECK_RC_MSG
    (
        DispTest::IsoIndexedRegWrVGA (color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO,
                                         LW_CIO_CRE_RMA__INDEX,
                                         DRF_NUM(_CIO, _CRE_RMA, _ENAB_RMA, 1) |
                                         DRF_DEF(_CIO, _CRE_RMA, _SEL_RMA, _DATA)),
        "***ERROR: DispTest::RmaRegRd32 unable to write LW_CIO_CRX__???? (data32 field)."
    );

    // Read the RMA data
    CHECK_RC_MSG
    (
        Platform::PioRead32(0x3d0, pData),
        "***ERROR: DispTest::RmaRegRd32 unable to read 0x3D0 (RMA data)."
    );

    // Restore the index.
    CHECK_RC_MSG
    (
        Platform::PioWrite08(color?LW_CIO_CRX__COLOR:LW_CIO_CRX__MONO, save_index),
        "***ERROR: DispTest::RmaRegRd32 unable to write LW_CIO_CRX__????."
    );

    return OK;
}

/****************************************************************************************
 * DispTest::SkipDsiSupervisor2EventHandler
 *
 *  Functional Description
 *  - Event handler helper function for "SetSkipDsiSupervisor2Event"
 ****************************************************************************************/
void DispTest::SkipDsiSupervisor2EventHandler()
{
    JavaScriptPtr pJs;

    if (DispTest::s_SkipDsiSupervisor2EventJSFunc != "") {
        pJs->CallMethod(pJs->GetGlobalObject(), DispTest::s_SkipDsiSupervisor2EventJSFunc);
    } else {
        Printf(Tee::PriWarn, "DispTest::SkipDsiSupervisor2EventHandler - Spurious SkipDsiSupervisor2Event received from RM\n");
    }
}

/****************************************************************************************
 * DispTest::SetSkipDsiSupervisor2Event
 *
 *  Arguments Description:
 *  - funcname:
 *
 *  Functional Description:
 *  - Set LW_CFGEX_EVENT_HANDLE_SKIP_DSI_SUPERVISOR_2_EVENT
 ****************************************************************************************/
RC DispTest::SetSkipDsiSupervisor2Event(string funcname)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetSkipDsiSupervisor2Event - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetSkipDsiSupervisor2Event(funcname));
}

/****************************************************************************************
 * DispTest::SupervisorRestartEventHandler
 *
 *  Functional Description:
 *  - Event handler helper function for "SetSupervisorRestartMode"
 ****************************************************************************************/
void DispTest::SupervisorRestartEventHandler()
{
    JavaScriptPtr pJs;

    if (DispTest::s_SupervisorRestartEventJSFunc != "") {
        pJs->CallMethod(pJs->GetGlobalObject(), DispTest::s_SupervisorRestartEventJSFunc);
    } else {
        Printf(Tee::PriWarn, "DispTest::SupervisorRestartEventHandler - Spurious SupervisorRestartEvent received from RM\n");
    }
}

/****************************************************************************************
 * DispTest::SetSupervisorRestartMode
 *
 *  Arguments Description:
 *  - WhichSv:
 *  - RestartMode:
 *  - exelwteRmSvCode:
 *  - funcname:
 *  - clientRestart:
 *
 *  Functional Description:
 *  - Set SUPERVISOR_RESTART_EVENT_HANDLE
 ****************************************************************************************/
RC DispTest::SetSupervisorRestartMode(UINT32 WhichSv, UINT32 RestartMode,
                                     bool exelwteRmSvCode, string funcname, bool clientRestart)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetSupervisorRestartMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetSupervisorRestartMode(WhichSv, RestartMode, exelwteRmSvCode, funcname, clientRestart));
}

/****************************************************************************************
 * DispTest::GetSupervisorRestartMode
 *
 *  Arguments Description:
 *  - WhichSv:
 *  - RestartMode:
 *  - exelwteRmSvCode:
 *  - pFuncname:
 *  - clientRestart:
 *
 *  Functional Description:
 *  - Retrieve current setting of the supervisor restart mode
 ****************************************************************************************/
RC DispTest::GetSupervisorRestartMode(UINT32 WhichSv,
                                     UINT32* RestartMode, bool* exelwteRmSvCode,
                                     string *pFuncname, bool* clientRestart)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_GET_SV_RESTART_MODE_PARAMS params = {
        {LwV32(activeSubdeviceIndex)},
        LwU32(WhichSv), LwU32(0),
        LwU32(0),
        LW_PTR_TO_LwP64(NULL),
        LwU32(0)
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                        LW5070_CTRL_CMD_GET_SV_RESTART_MODE,
                        (void*)&params,
                        sizeof(params)),
        "***ERROR: DispTest::GetSupervisorRestartMode failed to get restart mode."
    );

    *RestartMode = params.restartMode;

    *pFuncname = s_SupervisorRestartEventJSFunc;

    if (params.exelwteRmSvCode == LW5070_CTRL_CMD_GET_SV_RESTART_MODE_EXELWTE_RM_SV_CODE_YES)
        *exelwteRmSvCode = true;
    else
        *exelwteRmSvCode = false;

    if (params.clientRestart == LW5070_CTRL_CMD_GET_SV_RESTART_MODE_CLIENT_RESTART_YES)
        *clientRestart = true;
    else
        *clientRestart = false;

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::ExceptionRestartEventHandler
 *
 *  Functional Description
 *  - Event handler helper function for "SetExceptionRestartMode"
 ****************************************************************************************/
void DispTest::ExceptionRestartEventHandler()
{
    JavaScriptPtr pJs;

    if (DispTest::s_ExceptionRestartEventJSFunc != "") {
        pJs->CallMethod(pJs->GetGlobalObject(), DispTest::s_ExceptionRestartEventJSFunc);
    } else {
        Printf(Tee::PriWarn, "DispTest::ExceptionRestartEventHandler - Spurious ExceptionRestartEvent received from RM\n");
    }

}

/****************************************************************************************
 * DispTest::SetExceptionRestartMode
 *
 *  Arguments Description:
 *  - Channel:
 *  - Reason:
 *  - RestartMode:
 *  - Assert:
 *  - ResumeArg:
 *  - Override:
 *  - funcname:
 *  - manualRestart:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_EXCEPTION_RESTART_MODE
 ****************************************************************************************/
RC DispTest::SetExceptionRestartMode(UINT32 Channel,
                                     UINT32 Reason,
                                     UINT32 RestartMode,
                                     bool Assert,
                                     bool ResumeArg,
                                     UINT32 Override,
                                     string funcname,
                                     bool manualRestart)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetContextDmaDebugRegs - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetExceptionRestartMode(Channel,
                                                             Reason,
                                                             RestartMode,
                                                             Assert,
                                                             ResumeArg,
                                                             Override,
                                                             funcname,
                                                             manualRestart));
}

/****************************************************************************************
 * DispTest::GetExceptionRestartMode
 *
 *  Arguments Description:
 *  - Channel:
 *  - Reason:
 *  - RestartMode:
 *  - Assert:
 *  - ResumeArg:
 *  - Override:
 *  - pFuncname:
 *  - manualRestart:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_EXCEPTION_RESTART_MODE and return retrieved values
 ****************************************************************************************/
RC DispTest::GetExceptionRestartMode(UINT32 Channel,
                                     UINT32 Reason,
                                     UINT32* RestartMode,
                                     bool* Assert,
                                     bool* ResumeArg,
                                     UINT32* Override,
                                     string *pFuncname,
                                     bool* manualRestart)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetContextDmaDebugRegs - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->GetExceptionRestartMode(Channel,
                                                             Reason,
                                                             RestartMode,
                                                             Assert,
                                                             ResumeArg,
                                                             Override,
                                                             pFuncname,
                                                             manualRestart));
}

/****************************************************************************************
 * DispTest::GetInternalTVRasterTimings
 *
 *  Arguments Description:
 *  - Protocol:
 *  - PClkFreqKHz:
 *  - HActive:
 *  - VActive:
 *  - Width:
 *  - Height:
 *  - SyncEndX:
 *  - SyncEndY:
 *  - BlankEndX:
 *  - BlankEndY:
 *  - BlankStartX:
 *  - BlankStartY:
 *  - Blank2EndY:
 *  - Blank2StartY:
 *
 *  Functional Description
 *  - Retrieve the current TV raster timings
 ****************************************************************************************/
RC DispTest::GetInternalTVRasterTimings(UINT32 Protocol,
                                        UINT32* PClkFreqKHz,     // [OUT]
                                        UINT32* HActive,         // [OUT]
                                        UINT32* VActive,         // [OUT]
                                        UINT32* Width,           // [OUT]
                                        UINT32* Height,          // [OUT]
                                        UINT32* SyncEndX,        // [OUT]
                                        UINT32* SyncEndY,        // [OUT]
                                        UINT32* BlankEndX,       // [OUT]
                                        UINT32* BlankEndY,       // [OUT]
                                        UINT32* BlankStartX,     // [OUT]
                                        UINT32* BlankStartY,     // [OUT]
                                        UINT32* Blank2EndY,      // [OUT]
                                        UINT32* Blank2StartY    // [OUT]
    )
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetInternalTVRasterTimings - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->GetInternalTVRasterTimings(
                                        Protocol,
                                        PClkFreqKHz,     // [OUT]
                                        HActive,         // [OUT]
                                        VActive,         // [OUT]
                                        Width,           // [OUT]
                                        Height,          // [OUT]
                                        SyncEndX,        // [OUT]
                                        SyncEndY,        // [OUT]
                                        BlankEndX,       // [OUT]
                                        BlankEndY,       // [OUT]
                                        BlankStartX,     // [OUT]
                                        BlankStartY,     // [OUT]
                                        Blank2EndY,      // [OUT]
                                        Blank2StartY    // [OUT]

    ));
}

/****************************************************************************************
 * DispTest::GetRGStatus
 *
 *  Arguments Description:
 *  - Head:
 *  - scanLocked:
 *  - flipLocked:
 *
 *  Functional Description
 *  - Get the scanlocked and fliplocked status from RG
 ****************************************************************************************/
RC DispTest::GetRGStatus(UINT32 Head,
                         UINT32* scanLocked,     // [OUT]
                         UINT32* flipLocked      // [OUT]
    )
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetRGStatus - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->GetRGStatus(Head, scanLocked, flipLocked));
}

/****************************************************************************************
 * DispTest::SetVPLLRef
 *
 *  Arguments Description:
 *  - Head:
 *  - RefName:
 *  - Frequency:
 *
 *  Functional Description
 *  - Set the VPLL clock reference
 ****************************************************************************************/
RC DispTest::SetVPLLRef(UINT32 Head, UINT32 RefName, UINT32 Frequency)
{
    LwRmPtr pLwRm;

    LW0080_CTRL_CMD_SET_VPLL_REF_PARAMS params = {
        LwU32(Head),
        LwU32(RefName),
        LwU32(Frequency)
    };

    return pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          LW0080_CTRL_CMD_SET_VPLL_REF,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::SetVPLLArchType
 *
 *  Arguments Description:
 *  - Head
 *  - ArchType
 *
 *  Functional Description:
 *  - Set LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE
 ****************************************************************************************/
RC DispTest::SetVPLLArchType(UINT32 Head, UINT32 ArchType)
{
    LwRmPtr pLwRm;

    LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE_PARAMS params = {
        LwU32(Head),
        LwU32(ArchType)
    };

    return pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          LW0080_CTRL_CMD_SET_VPLL_ARCH_TYPE,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::ControlSetSorSequence
 *
 *  Arguments Description:
 *  - OrNum:
 *  - pdPc_specified:
 *  - pdPc:
 *  - puPc_specified:
 *  - puPc:
 *  - puPcAlt_specified:
 *  - puPcAlt:
 *  - normalStart_specified:
 *  - normalStart:
 *  - safeStart_specified:
 *  - safeStart:
 *  - normalState_specified:
 *  - normalState:
 *  - safeState_specified:
 *  - safeState:
 *  - SkipWaitForVsync:
 *  - SeqProgPresent:
 *  - SequenceProgram:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_SOR_SEQ_CTL
 ****************************************************************************************/
RC DispTest::ControlSetSorSequence(UINT32 OrNum,
                                   bool pdPc_specified,        UINT32 pdPc,
                                   bool puPc_specified,        UINT32 puPc,
                                   bool puPcAlt_specified,     UINT32 puPcAlt,
                                   bool normalStart_specified, UINT32 normalStart,
                                   bool safeStart_specified,   UINT32 safeStart,
                                   bool normalState_specified, UINT32 normalState,
                                   bool safeState_specified,   UINT32 safeState,
                                   bool SkipWaitForVsync,
                                   bool SeqProgPresent,
                                   UINT32* SequenceProgram
                                   )
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetSorSequence - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetSorSequence(OrNum,
                                   pdPc_specified,        pdPc,
                                   puPc_specified,        puPc,
                                   puPcAlt_specified,     puPcAlt,
                                   normalStart_specified, normalStart,
                                   safeStart_specified,   safeStart,
                                   normalState_specified, normalState,
                                   safeState_specified,   safeState,
                                   SkipWaitForVsync,
                                   SeqProgPresent,
                                   SequenceProgram
                                   ));
}

/****************************************************************************************
 * DispTest::ControlGetSorSequence
 *
 *  Arguments Description:
 *  - OrNum:
 *  - puPcAlt:
 *  - pdPc:
 *  - pdPcAlt:
 *  - normalStart:
 *  - safeStart:
 *  - normalState:
 *  - safeState:
 *  - GetSeqProg:
 *  - seqProgram:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_SOR_SEQ_CTL and return retrieved values
 ****************************************************************************************/
RC DispTest::ControlGetSorSequence(UINT32 OrNum,
                                   UINT32* puPcAlt,
                                   UINT32* pdPc,
                                   UINT32* pdPcAlt,
                                   UINT32* normalStart,
                                   UINT32* safeStart,
                                   UINT32* normalState,
                                   UINT32* safeState,
                                   bool  GetSeqProg,
                                   UINT32* seqProgram
                                   )
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_PARAMS params = {
        {LwV32(activeSubdeviceIndex)},
        LwU32(OrNum),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(GetSeqProg),
        {
            LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0),
            LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0),
        }
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                        LW5070_CTRL_CMD_GET_SOR_SEQ_CTL,
                        (void*)&params,
                        sizeof(params)),
        "***ERROR: DispTest::ControlGetSorSequence failed to get SOR sequence."
    );

    *puPcAlt = params.puPcAlt;
    *pdPc = params.pdPc;
    *pdPcAlt = params.pdPcAlt;
    *normalStart = params.normalStart;
    *safeStart = params.safeStart;
    *normalState = params.normalState;
    *safeState = params.safeState;
    *normalState = params.normalState;

    if (GetSeqProg) {
        MASSERT(seqProgram != NULL);

        for (UINT32 i = 0; i < LW5070_CTRL_CMD_GET_SOR_SEQ_CTL_SEQ_PROG_SIZE; i++) {
            seqProgram[i] = params.seqProgram[i];
        }
    }

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::ControlSetPiorSequence
 *
 *  Arguments Description:
 *  - OrNum:
 *  - pdPc_specified:
 *  - pdPc:
 *  - puPc_specified:
 *  - puPc:
 *  - puPcAlt_specified:
 *  - puPcAlt:
 *  - normalStart_specified:
 *  - normalStart:
 *  - safeStart_specified:
 *  - safeStart:
 *  - normalState_specified:
 *  - normalState:
 *  - safeState_specified:
 *  - safeState:
 *  - SkipWaitForVsync:
 *  - SeqProgPresent:
 *  - SequenceProgram:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_PIOR_SEQ_CTL
 ****************************************************************************************/
RC DispTest::ControlSetPiorSequence(UINT32 OrNum,
                                   bool pdPc_specified,        UINT32 pdPc,
                                   bool puPc_specified,        UINT32 puPc,
                                   bool puPcAlt_specified,     UINT32 puPcAlt,
                                   bool normalStart_specified, UINT32 normalStart,
                                   bool safeStart_specified,   UINT32 safeStart,
                                   bool normalState_specified, UINT32 normalState,
                                   bool safeState_specified,   UINT32 safeState,
                                   bool SkipWaitForVsync,
                                   bool SeqProgPresent,
                                   UINT32* SequenceProgram
                                   )
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetPiorSequence - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetPiorSequence(OrNum,
                                   pdPc_specified,        pdPc,
                                   puPc_specified,        puPc,
                                   puPcAlt_specified,     puPcAlt,
                                   normalStart_specified, normalStart,
                                   safeStart_specified,   safeStart,
                                   normalState_specified, normalState,
                                   safeState_specified,   safeState,
                                   SkipWaitForVsync,
                                   SeqProgPresent,
                                   SequenceProgram
                                   ));
}

/****************************************************************************************
 * DispTest::ControlGetPiorSequence
 *
 *  Arguments Description:
 *  - OrNum:
 *  - puPcAlt:
 *  - pdPc:
 *  - pdPcAlt:
 *  - normalStart:
 *  - safeStart:
 *  - normalState:
 *  - safeState:
 *  - GetSeqProg:
 *  - seqProgram:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL and return retrieved values
 ****************************************************************************************/
RC DispTest::ControlGetPiorSequence(UINT32 OrNum,
                                    UINT32* puPcAlt,
                                    UINT32* pdPc,
                                    UINT32* pdPcAlt,
                                    UINT32* normalStart,
                                    UINT32* safeStart,
                                    UINT32* normalState,
                                    UINT32* safeState,
                                    bool  GetSeqProg,
                                    UINT32* seqProgram
                                   )
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_PARAMS params = {
        {LwV32(activeSubdeviceIndex)},
        LwU32(OrNum),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(GetSeqProg),
        {
            LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0), LwU32(0),
        }
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL,
                          (void*)&params,
                          sizeof(params)),
        "***ERROR: DispTest::ControlGetPiorSequence failed to get PIOR sequence."
    );

    *puPcAlt = params.puPcAlt;
    *pdPc = params.pdPc;
    *pdPcAlt = params.pdPcAlt;
    *normalStart = params.normalStart;
    *safeStart = params.safeStart;
    *normalState = params.normalState;
    *safeState = params.safeState;
    *normalState = params.normalState;

    if (GetSeqProg) {
        MASSERT(seqProgram != NULL);

        for (UINT32 i = 0; i < LW5070_CTRL_CMD_GET_PIOR_SEQ_CTL_SEQ_PROG_SIZE; i++) {
            seqProgram[i] = params.seqProgram[i];
        }
    }

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::ControlSetSorOpMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - category:
 *  - puTxda:
 *  - puTxdb:
 *  - puTxca:
 *  - puTxcb:
 *  - upper:
 *  - mode:
 *  - linkActA:
 *  - linkActB:
 *  - lvdsEn:
 *  - dupSync:
 *  - newMode:
 *  - balanced:
 *  - plldiv:
 *  - rotClk:
 *  - rotDat:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_SOR_OP_MODE
 ****************************************************************************************/
RC DispTest::ControlSetSorOpMode(UINT32 orNumber, UINT32 category,
                                 UINT32 puTxda, UINT32 puTxdb,
                                 UINT32 puTxca, UINT32 puTxcb,
                                 UINT32 upper, UINT32 mode,
                                 UINT32 linkActA, UINT32 linkActB,
                                 UINT32 lvdsEn, UINT32 lvdsDual, UINT32 dupSync,
                                 UINT32 newMode, UINT32 balanced,
                                 UINT32 plldiv,
                                 UINT32 rotClk, UINT32 rotDat)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetSorOpMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetSorOpMode(orNumber, category,
                                 puTxda, puTxdb,
                                 puTxca, puTxcb,
                                 upper, mode,
                                 linkActA, linkActB,
                                 lvdsEn, lvdsDual, dupSync,
                                 newMode, balanced,
                                 plldiv,
                                 rotClk, rotDat));
}

/****************************************************************************************
 * DispTest::ControlSetPiorOpMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - category:
 *  - clkPolarity:
 *  - clkMode:
 *  - clkPhs:
 *  - unusedPins:
 *  - polarity:
 *  - dataMuxing:
 *  - clkDelay:
 *  - dataDelay:
 *  - master:
 *  - pin_set:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_PIOR_OP_MODE
 ****************************************************************************************/
RC DispTest::ControlSetPiorOpMode(UINT32 orNumber, UINT32 category,
                                  UINT32 clkPolarity, UINT32 clkMode,
                                  UINT32 clkPhs, UINT32 unusedPins,
                                  UINT32 polarity, UINT32 dataMuxing,
                                  UINT32 clkDelay, UINT32 dataDelay,
                                  UINT32 DroMaster, UINT32 DroPinset)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetPiorOpMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetPiorOpMode(orNumber, category,
                                  clkPolarity, clkMode,
                                  clkPhs, unusedPins,
                                  polarity, dataMuxing,
                                  clkDelay, dataDelay,
                                  DroMaster, DroPinset));
}

/****************************************************************************************
 * DispTest::ControlGetPiorOpMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - category:
 *  - clkPolarity:
 *  - clkMode:
 *  - clkPhs:
 *  - unusedPins:
 *  - polarity:
 *  - dataMuxing:
 *  - clkDelay:
 *  - dataDelay:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_PIOR_OP_MODE and return retrieved values
 ****************************************************************************************/
RC DispTest::ControlGetPiorOpMode(UINT32 orNumber,
                                  UINT32* category,
                                  UINT32* clkPolarity,
                                  UINT32* clkMode,
                                  UINT32* clkPhs,
                                  UINT32* unusedPins,
                                  UINT32* polarity,
                                  UINT32* dataMuxing,
                                  UINT32* clkDelay,
                                  UINT32* dataDelay)
{
    RC  rc = OK;

    return (rc);
}

/****************************************************************************************
 * DispTest::ControlSetUseTestPiorSettings
 *
 *  Arguments Description:
 *  - enable:
 *
 *  Functional Description
 *  - LW0073_CTRL_SYSTEM_USE_TEST_PIOR_SETTINGS
 ****************************************************************************************/
RC DispTest::ControlSetUseTestPiorSettings(UINT32 unEnable)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetUseTestPiorSettings - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetUseTestPiorSettings(unEnable));
}

/****************************************************************************************
 * DispTest::ControlSetSfBlank
 *
 *  Arguments Description:
 *  - sfNumber:
 *  - transition:
 *  - status:
 *  - waitForCompletion:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_SF_BLANK
 *  - basically the same as ControletOrBlank, but for SFs
 ****************************************************************************************/
RC DispTest::ControlSetSfBlank(UINT32 sfNumber, UINT32 transition,
                               UINT32 status, UINT32 waitForCompletion)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetSfBlank - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetSfBlank(sfNumber, transition,
                                                       status, waitForCompletion));
}

/****************************************************************************************
 * DispTest::ControlSetDacConfig
 *
 *  Arguments Description:
 *  - orNumber:
 *  - cpstDac0Src:
 *  - cpstDac1Src:
 *  - cpstDac2Src:
 *  - cpstDac3Src:
 *  - compDac0Src:
 *  - compDac1Src:
 *  - compDac2Src:
 *  - compDac3Src:
 *  - driveSync:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_DAC_CONFIG
 ****************************************************************************************/
RC DispTest::ControlSetDacConfig(UINT32 orNumber,
                                 UINT32 cpstDac0Src,
                                 UINT32 cpstDac1Src,
                                 UINT32 cpstDac2Src,
                                 UINT32 cpstDac3Src,
                                 UINT32 compDac0Src,
                                 UINT32 compDac1Src,
                                 UINT32 compDac2Src,
                                 UINT32 compDac3Src,
                                 UINT32 driveSync)

{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetDacConfig - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetDacConfig(orNumber,
                                 cpstDac0Src,
                                 cpstDac1Src,
                                 cpstDac2Src,
                                 cpstDac3Src,
                                 compDac0Src,
                                 compDac1Src,
                                 compDac2Src,
                                 compDac3Src,
                                 driveSync));

}

/****************************************************************************************
 * DispTest::ControlSetErrorMask
 *
 *  Arguments Description:
 *  - channelHandle:
 *  - method:
 *  - mode:
 *  - errorCode:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_ERRMASK
 ****************************************************************************************/
RC DispTest::ControlSetErrorMask(UINT32 channelHandle,
                                 UINT32 method,
                                 UINT32 mode,
                                 UINT32 errorCode)

{
    LwRmPtr pLwRm;

    UINT32 channelClass = LwrrentDevice()->ChannelClassMap[channelHandle];
    UINT32 channelHead = LwrrentDevice()->ChannelHeadMap[channelHandle];

    LW5070_CTRL_CMD_SET_ERRMASK_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LwU32(channelClass),
        LwU32(channelHead),
        LwU32(method),
        LwU32(mode),
        LwU32(errorCode)
    };

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_ERRMASK,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::ControlSetSorPwmMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - targetFreq:
 *  - actualFreq:
 *  - div:
 *  - resolution:
 *  - dutyCycle:
 *  - flags:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_SOR_PWM
 ****************************************************************************************/
RC DispTest::ControlSetSorPwmMode(UINT32 orNumber, UINT32 targetFreq,
                                  UINT32 *actualFreq, UINT32 div,
                                  UINT32 resolution, UINT32 dutyCycle,
                                  UINT32 flags)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_SET_SOR_PWM_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LwU32(orNumber),
        LwU32(targetFreq),
        LwU32(LwUPtr(actualFreq)),
        LwU32(div),
        LwU32(resolution),
        LwU32(dutyCycle),
        LwU32(flags)
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_SOR_PWM,
                          (void*)&params,
                          sizeof(params)),
        "***ERROR: DispTest::ControlSetSorPwmMode failed to set SOR mode."
    );
    *actualFreq = params.actualFreq;

    // RC == OK at this point
    return rc;

}

/****************************************************************************************
 * DispTest::ControlGetDacPwrMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - normalHSync:
 *  - normalVSync:
 *  - normalData:
 *  - normalPower:
 *  - safeHSync:
 *  - safeVSync:
 *  - safeData:
 *  - safePower:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_DAC_PWR
 ****************************************************************************************/
RC DispTest::ControlGetDacPwrMode(UINT32 orNumber,
                                  UINT32* normalHSync, UINT32* normalVSync,
                                  UINT32* normalData,  UINT32* normalPower,
                                  UINT32* safeHSync,   UINT32* safeVSync,
                                  UINT32* safeData,    UINT32* safePower)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_GET_DAC_PWR_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LwU32(orNumber),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0)
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_GET_DAC_PWR,
                          (void*)&params,
                          sizeof(params)),
        "***ERROR: DispTest::ControlGetDacPwrMode failed to get DAC power mode."
    );

    *normalHSync = params.normalHSync;
    *normalVSync = params.normalVSync;
    *normalData  = params.normalData;
    *normalPower = params.normalPower;

    *safeHSync = params.safeHSync;
    *safeVSync = params.safeVSync;
    *safeData  = params.safeData;
    *safePower = params.safePower;

    // RC == OK at this point
    return rc;

}

/****************************************************************************************
 * DispTest::ControlSetDacPwrMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - normalHSync:
 *  - normalVSync:
 *  - normalData:
 *  - normalPower:
 *  - safeHSync:
 *  - safeVSync:
 *  - safeData:
 *  - safePower:
 *  - flags:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_DAC_PWR
 ****************************************************************************************/
RC DispTest::ControlSetDacPwrMode(UINT32 orNumber, UINT32 normalHSync,
                                  UINT32 normalVSync, UINT32 normalData,
                                  UINT32 normalPower, UINT32 safeHSync,
                                  UINT32 safeVSync, UINT32 safeData,
                                  UINT32 safePower, UINT32 flags)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_SET_DAC_PWR_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LwU32(orNumber),
        LwU32(normalHSync),
        LwU32(normalVSync),
        LwU32(normalData),
        LwU32(normalPower),
        LwU32(safeHSync),
        LwU32(safeVSync),
        LwU32(safeData),
        LwU32(safePower),
        LwU32(flags)
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_DAC_PWR,
                          (void*)&params,
                          sizeof(params)),
        "***ERROR: DispTest::ControlSetDacPwrMode failed to set DAC power mode."
    );

    // RC == OK at this point
    return rc;

}

/****************************************************************************************
 * DispTest::ControlSetRgUnderflowProp
 *
 *  Arguments Description:
 *  - head:
 *  - enable:
 *  - clearUnderflow:
 *  - mode:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP
 ****************************************************************************************/
RC DispTest::ControlSetRgUnderflowProp(UINT32 head,
                                       UINT32 enable,
                                       UINT32 clearUnderflow,
                                       UINT32 mode)
{
    LwRmPtr pLwRm;
    LW5070_CTRL_CMD_UNDERFLOW_PARAMS underflowProp =
    {
        LwU32(head),
        LwU32(enable),
        LwU32(clearUnderflow),
        LwU32(mode)
    };
    LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LW5070_CTRL_CMD_UNDERFLOW_PARAMS(underflowProp)
    };

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_RG_UNDERFLOW_PROP,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::ControlSetSemaAcqDelay
 *
 *  Arguments Description:
 *  - delayUs:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_SEMA_ACQ_DELAY
 ****************************************************************************************/
RC DispTest::ControlSetSemaAcqDelay(UINT32 delayUs)
{
    LwRmPtr pLwRm;

    LW5070_CTRL_CMD_SET_SEMA_ACQ_DELAY_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LwU32(delayUs)
    };

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_SEMA_ACQ_DELAY,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::ControlGetSorOpMode
 *
 *  Arguments Description:
 *  - orNumber:
 *  - category:
 *  - puTxda:
 *  - puTxdb:
 *  - puTxca:
 *  - puTxcb:
 *  - upper:
 *  - mode:
 *  - linkActA:
 *  - linkActB:
 *  - lvdsEn:
 *  - dupSync:
 *  - newMode:
 *  - balanced:
 *  - plldiv:
 *  - rotClk:
 *  - rotDat:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_SOR_OP_MODE and return retrieved values
 ****************************************************************************************/
RC DispTest::ControlGetSorOpMode(UINT32 orNumber, UINT32 category,
                                 UINT32* puTxda, UINT32* puTxdb,
                                 UINT32* puTxca, UINT32* puTxcb,
                                 UINT32* upper, UINT32* mode,
                                 UINT32* linkActA, UINT32* linkActB,
                                 UINT32* lvdsEn, UINT32* lvdsDual, UINT32* dupSync,
                                 UINT32* newMode, UINT32* balanced,
                                 UINT32* plldiv,
                                 UINT32* rotClk, UINT32* rotDat)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW5070_CTRL_CMD_GET_SOR_OP_MODE_PARAMS params = {
        {LwV32(activeSubdeviceIndex)},
        LwU32(orNumber),
        LwU32(category),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
        LwU32(0),
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                        LW5070_CTRL_CMD_GET_SOR_OP_MODE,
                        (void*)&params,
                        sizeof(params)),
        "***ERROR: DispTest::ControlGetSorOpMode failed to get SOR OP mode."
    );

    *puTxda = params.puTxda;
    *puTxdb = params.puTxdb;
    *puTxca = params.puTxca;
    *puTxcb = params.puTxcb;
    *upper = params.upper;
    *mode = params.mode;
    *linkActA = params.linkActA;
    *linkActB = params.linkActB;
    *lvdsEn = params.lvdsEn;
    *lvdsDual = params.lvdsDual;
    *dupSync = params.dupSync;
    *newMode = params.newMode;
    *balanced = params.balanced;
    *plldiv = params.plldiv;
    *rotClk = params.rotClk;
    *rotDat = params.rotDat;

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::SetSkipFreeCount
 *
 *  Arguments Description:
 *  - Channel:
 *  - skip:
 *
 *  Functional Description
 *  - Set the skip free count on the given channel
 ****************************************************************************************/
RC DispTest::SetSkipFreeCount(UINT32 Channel, bool skip)
{
    LwRmPtr pLwRm;

    DisplayChannel * channel = static_cast<DisplayChannel*> (pLwRm->GetDisplayChannel(Channel));

    return channel->SetSkipFreeCount(skip);
}

/****************************************************************************************
 * DispTest::VbiosAttentionEventHandler
 *
 *  Arguments Description:
 *  - funcname:
 *
 *  Functional Description
 *  - Event handler for "ControlSetVbiosAttentionEvent"
 ****************************************************************************************/
void DispTest::VbiosAttentionEventHandler()
{
    JavaScriptPtr pJs;

    if (DispTest::s_SetVbiosAttentionEventJSFunc != "") {
        pJs->CallMethod(pJs->GetGlobalObject(), DispTest::s_SetVbiosAttentionEventJSFunc);
    } else {
        Printf(Tee::PriWarn, "DispTest::VbiosAttentionEventHandler - Spurious SetVbiosAttentionEvent received from RM\n");
    }

}

/****************************************************************************************
 * DispTest::ControlSetVbiosAttentionEvent
 *
 *  Arguments Description:
 *  - funcname:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_VBIOS_ATTENTION_EVENT
 ****************************************************************************************/
RC DispTest::ControlSetVbiosAttentionEvent(string funcname)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    s_SetVbiosAttentionEventJSFunc = funcname;
    CHECK_RC_MSG
    (
        s_VbiosAttentionEventThread.SetHandler(
                DispTest::VbiosAttentionEventHandler),
        "***ERROR: DispTest::ControlSetVbiosAttentionEvent - SetHandler failed!"
    );

    void* const pOsEvent = s_VbiosAttentionEventThread.GetOsEvent(
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    LW5070_CTRL_CMD_SET_VBIOS_ATTENTION_EVENT_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LW_PTR_TO_LwP64(pOsEvent)
    };

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_VBIOS_ATTENTION_EVENT,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::ControlSetRgFliplockProp
 *
 *  Arguments Description:
 *  - head:
 *  - maxSwapLockoutSkew:
 *  - swapLockoutStart:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_RG_FLIPLOCK_PROP
 ****************************************************************************************/
RC DispTest::ControlSetRgFliplockProp(UINT32 head,
                                      UINT32 maxSwapLockoutSkew,
                                      UINT32 swapLockoutStart)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetRgFliplockProp - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetRgFliplockProp(head,
                                      maxSwapLockoutSkew,
                                      swapLockoutStart));
}

/****************************************************************************************
 * DispTest::ControlGetPrevModeSwitchFlags
 *
 *  Arguments Description:
 *  - WhichHead:
 *  - blank    :
 *  - shutdown :
 *
 *  Functional Description
 *  - Gets the ModeSwitchFlags state variable in LW_PDISP_SUPERVISOR_HEAD for a given head.
 ****************************************************************************************/
RC DispTest::ControlGetPrevModeSwitchFlags(UINT32 WhichHead,
                                           UINT32* blank, UINT32* shutdown)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlGetPrevModeSwitchFlags - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlGetPrevModeSwitchFlags(WhichHead, blank, shutdown));
}

/****************************************************************************************
 * DispTest::ControlGetDacLoad
 *
 *  Arguments Description:
 *  - orNumber:
 *  - mode:
 *  - valDCCrt:
 *  - valDCTv:
 *  - valACCrt:
 *  - valACTv:
 *  - perAuto:
 *  - perDCCrt:
 *  - perDCTv:
 *  - perSampleCrt:
 *  - perSampleTv:
 *  - load:
 *  - status:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_GET_DAC_LOAD
 ****************************************************************************************/
RC DispTest::ControlGetDacLoad(UINT32  orNumber, UINT32* mode,
                               UINT32* valDCCrt, UINT32 *valACCrt,
                               UINT32* perDCCrt, UINT32 *perSampleCrt,
                               UINT32* valDCTv, UINT32 *valACTv,
                               UINT32* perDCTv, UINT32 *perSampleTv,
                               UINT32* perAuto,
                               UINT32* load,
                               UINT32* status)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlGetDacLoad - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlGetDacLoad(orNumber, mode,
                                                       valDCCrt, valACCrt,
                                                       perDCCrt, perSampleCrt,
                                                       valDCTv, valACTv,
                                                       perDCTv, perSampleTv,
                                                       perAuto,
                                                       load,
                                                       status)
    );
}

/****************************************************************************************
 * DispTest::ControlSetDacLoad
 *
 *  Arguments Description:
 *  - orNumber:
 *  - mode:
 *  - valDCCrt:
 *  - valDCTv:
 *  - valACCrt:
 *  - valACTv:
 *  - perAuto:
 *  - perDCCrt:
 *  - perDCTv:
 *  - perSampleCrt:
 *  - perSampleTv:
 *  - flags:
 *
 *  Functional Description
 *  - Set LW5070_CTRL_CMD_SET_DAC_LOAD
 ****************************************************************************************/
RC DispTest::ControlSetDacLoad(UINT32 orNumber, UINT32 mode,
                               UINT32 valDCCrt, UINT32 valACCrt,
                               UINT32 perDCCrt, UINT32 perSampleCrt,
                               UINT32 valDCTv, UINT32 valACTv,
                               UINT32 perDCTv, UINT32 perSampleTv,
                               UINT32 perAuto,
                               UINT32 flags)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetDacLoad - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetDacLoad(orNumber, mode,
                                                       valDCCrt, valACCrt,
                                                       perDCCrt, perSampleCrt,
                                                       valDCTv, valACTv,
                                                       perDCTv, perSampleTv,
                                                       perAuto,
                                                       flags)
    );
}

/****************************************************************************************
 * DispTest::ControlGetOverlayFlipCount
 *
 *  Arguments Description:
 *  - channelInstance:
 *  - value:
 *
 *  Functional Description
 *  - Call LW5070_CTRL_CMD_GET_OVERLAY_FLIPCOUNT
 ****************************************************************************************/
RC DispTest::ControlGetOverlayFlipCount(UINT32 channelInstance, UINT32* value)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlGetOverlayFlipCount - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlGetOverlayFlipCount(channelInstance, value));
}

/****************************************************************************************
 * DispTest::ControlSetOverlayFlipCount
 *
 *  Arguments Description:
 *  - channelInstance:
 *  - forceCount:
 *
 *  Functional Description
 *  - Call LW5070_CTRL_CMD_SET_OVERLAY_FLIPCOUNT
 ****************************************************************************************/
RC DispTest::ControlSetOverlayFlipCount(UINT32 channelInstance, UINT32 forceCount)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetOverlayFlipCount - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetOverlayFlipCount(channelInstance, forceCount));
}
/****************************************************************************************
 * DispTest::ControlSetActiveSubdevice
 *
 *  Arguments Description:
 *  - index:
 *
 ****************************************************************************************/
RC DispTest::ControlSetActiveSubdevice(UINT32 subdeviceIndex)
{
    activeSubdeviceIndex = subdeviceIndex;
    return OK;
}

/****************************************************************************************
 * DispTest::ControlSetDmiElv
 *
 *  Arguments Description:
 *  - channelInstance:
 *  - field:
 *  - value
 *
 *  Functional Description
 *  - Call LW5070_CTRL_CMD_SET_DMI_ELV
 ****************************************************************************************/
RC DispTest::ControlSetDmiElv(UINT32 channelInstance, UINT32 field, UINT32 value)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LwU32 rm_what;

    switch (field) {
        case LW_DISPTEST_CONTROL_SET_DMI_ELV_ADVANCE:
            rm_what = LW5070_CTRL_CMD_SET_DMI_ELV_ADVANCE;
            break;

        case LW_DISPTEST_CONTROL_SET_DMI_ELV_MIN_DELAY:
            rm_what = LW5070_CTRL_CMD_SET_DMI_ELV_MIN_DELAY;
            break;

        default:
            RETURN_RC_MSG (RC::BAD_PARAMETER, "***ERROR: DispTest::ControlSetDmiElv - invalid field.");
    }

    LW5070_CTRL_CMD_SET_DMI_ELV_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LwU32(channelInstance),
        LwU32(rm_what),
        LwU32(value)
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_DMI_ELV,
                          (void*)&params,
                          sizeof(params)),
        "***ERROR: DispTest::ControlSetDmiElv failed."
    );

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::LowPower
 *
 *  Functional Description
 *  - Enable power management
 *
 *  Notes:
 *  - Here we set the engines to power save mode but we rely on bits in
 *    pbus_debug_4 and we need the engines to be enabled in pmc_enableSet
 *    LW5070_CTRL_CMD_SET_VBIOS_ATTENTION_EVENT
 ****************************************************************************************/
RC DispTest::LowPower()
{
    // set sw_therm_slowdown to div2.  this should pass in emulation.
    UINT32 tmp = 0x1;
    GpuSubdevice * pSubdev = GetBoundGpuSubdevice();

    pSubdev->RegWr32(LW_THERM_CONFIG, tmp);
    return OK;
}

/****************************************************************************************
 * DispTest::LowerPower
 *
 *  Functional Description
 *  - Enable power management
 ****************************************************************************************/
RC DispTest::LowerPower()
{
    return OK;
}

/****************************************************************************************
 * DispTest::LowestPower
 *
 *  Functional Description
 *  - Enable power management
 ****************************************************************************************/
RC DispTest::LowestPower()
{
    // set sw_therm_slowdown to div8.  this should fail in emulation.
    UINT32 tmp = 0x3;
    GetBoundGpuSubdevice()->RegWr32(LW_THERM_CONFIG, tmp);

    return OK;
}

/****************************************************************************************
 * DispTest::BlockLevelClockGating
 *
 *  Functional Description
 *  - No op
 ****************************************************************************************/
RC DispTest::BlockLevelClockGating()
{
    return OK;
}

/****************************************************************************************
 * DispTest::SlowLwclk
 *
 *  Functional Description
 *  - Set the LWCLK to slow mode
 ****************************************************************************************/
RC DispTest::SlowLwclk()
{
    UINT32 tmp = GetBoundGpuSubdevice()->RegRd32(LW_THERM_CONFIG);
 Printf(Tee::PriNormal, "read LW_THERM_CONFIG %x\n", tmp);
    tmp &= ~(0x02000000);
 Printf(Tee::PriNormal, "writing LW_THERM_CONFIG %x\n", tmp);
    GetBoundGpuSubdevice()->RegWr32(LW_THERM_CONFIG, tmp);

    return OK;
}

/****************************************************************************************
 * DispTest::ControlPrepForResumFromUnfrUpd
 *
 *  Functional Description
 *  - Checks with the RM to see if it is safe to do a modeset.
 *  - This function is used after the vbios has signaled that it has performed
 *    and unfriendly modechange
 ****************************************************************************************/
RC DispTest::ControlPrepForResumeFromUnfrUpd()
{
    LwRmPtr pLwRm;
    RC rc = OK;

    //lwrrently unused
    LW5070_CTRL_CMD_PREP_FOR_RESUME_FROM_UNFR_UPD_PARAMS params = {
        {LwV32(activeSubdeviceIndex)}
    };

    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->ObjectHandle,
                        LW5070_CTRL_CMD_PREP_FOR_RESUME_FROM_UNFR_UPD,
                        (void*)&params,
                        sizeof(params)),
        "***ERROR: DispTest::ControlPrepForResumeFromUnfrUpd failed."
    );

    return OK;
}

/****************************************************************************************
 * DispTest::ControlSetUnfrUpdEvent
 *
 *  Functional Description
 *    Set the JS function that will be called when the RM wants to signal the CPL that
 *    an unfriendly vbios grab oclwred.  This sets DispTest::UnfrUpdEventHandler to be the
 *    function that the RM exelwtes.
 *
 ****************************************************************************************/
RC DispTest::ControlSetUnfrUpdEvent(string funcname)
{
    LwRmPtr pLwRm;
    RC rc = OK;

    s_SetUnfrUpdEventJSFunc = funcname;
    CHECK_RC_MSG
    (
        s_UnfrUpdEventThread.SetHandler(DispTest::UnfrUpdEventHandler),
        "***ERROR: DispTest::ControlSetUnfrUpdEvent - SetHandler failed!"
    );

    void* const pOsEvent = s_UnfrUpdEventThread.GetOsEvent(
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    LW5070_CTRL_CMD_SET_UNFR_UPD_EVENT_PARAMS params =
    {
        {LwV32(activeSubdeviceIndex)},
        LW_PTR_TO_LwP64(pOsEvent)
    };

    return pLwRm->Control(LwrrentDevice()->ObjectHandle,
                          LW5070_CTRL_CMD_SET_UNFR_UPD_EVENT,
                          (void*)&params,
                          sizeof(params));
}

/****************************************************************************************
 * DispTest::UnfrUpdEventHandler
 *
 *  Functional Description
 *   Call the JS function that was specified in ControlSetUnfrUpdEvent.
 *
 ****************************************************************************************/
void DispTest::UnfrUpdEventHandler()
{
    JavaScriptPtr pJs;

    if (DispTest::s_SetUnfrUpdEventJSFunc != "") {
        pJs->CallMethod(pJs->GetGlobalObject(), DispTest::s_SetUnfrUpdEventJSFunc);
    } else {
        Printf(Tee::PriWarn, "DispTest::UnfrUpdEventHandler - Spurious SetUnfrUpdEvent received from RM\n");
    }

}

/****************************************************************************************
 * DispTest::SetInterruptHandlerName
 *
 *  Arguments Description:
 *  - intrHandleSrc: which kind of interrupt. There are totally 4 type:
 *         DTB_INTERRUPT_SOURCE_DISPLAY      1
 *         DTB_INTERRUPT_SOURCE_FALCON2HOST  2
 *         DTB_INTERRUPT_SOURCE_FALCON2PWR   3
 *         DTB_INTERRUPT_SOURCE_DSI2PMU      4
 *  - funcname: name of the custom define handler function.
 *
 *  Functional Description
 *  - Set custom interrupt handler function name for a kind of interrupt.
 ****************************************************************************************/
RC DispTest::SetInterruptHandlerName(UINT32 intrHandleSrc, const string& funcname)
{
    // check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if ((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetInterruptHandlerName - Current device class not allocated.");
    }

    //forward the call to the class
    return(m_display_devices[index]->SetInterruptHandlerName(intrHandleSrc, funcname));
}

/****************************************************************************************
 * DispTest::GetInterruptHandlerName
 *
 *  Arguments Description:
 *  - intrHandleSrc: which kind of interrupt. There are totally 4 type:
 *         DTB_INTERRUPT_SOURCE_DISPLAY      1
 *         DTB_INTERRUPT_SOURCE_FALCON2HOST  2
 *         DTB_INTERRUPT_SOURCE_FALCON2PWR   3
 *         DTB_INTERRUPT_SOURCE_DSI2PMU      4
 *  - funcname: interrupt handler function name.
 *
 *  Functional Description
 *  - Get custom interrupt handler function name for a kind of interrupt.
 ****************************************************************************************/
RC DispTest::GetInterruptHandlerName(UINT32 intrHandleSrc, string &funcname)
{
    // check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();
    if ((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetInterruptHandlerName - Current device class not allocated.");
    }

    // forward the call to the class
    return(m_display_devices[index]->GetInterruptHandlerName(intrHandleSrc, funcname));
}

////////////////////////////////////////////////////////////////////////////////////////////
//
//  DispTest - Memory Allocation and Context DMA Allocation Helper Functions.
//

/****************************************************************************************
 * AllocVidHeapMemory
 *
 *  Arguments Description:
 *  - pRtnMemoryHandle
 *  - pBase
 *  - Size
 *  - Type
 *  - Attr
 *  - pOffset
 *
 *  Functional Description
 *  - Video heap memory allocation:
 *    - Allocate Size bytes of memory at the given MemoryLocation.
 *    - Specify the Type and Attributes of the memory.
 *    - Return a pointer to the start of the allocated memory.
 ****************************************************************************************/
RC DispTest::AllocVidHeapMemory(UINT32 *pRtnMemoryHandle,
                                void **pBase,
                                UINT64 Size,
                                UINT32 Type,
                                UINT32 Attr,
                                UINT64 *pOffset)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    // TODO : We must add support for ATTR2 parameter
    // This paramater specifies GPU_CACHEABLE and ZBC flags
    // Lwrrently keeping it LWOS32_ATTR2_NONE
    UINT32  Attr2 = LWOS32_ATTR2_NONE;
    // allocate memory
    CHECK_RC_MSG
    (
        pLwRm->VidHeapAllocSizeEx (Type, LWOS32_ALLOC_FLAGS_ALIGNMENT_FORCE,
                                   &Attr, &Attr2, Size, 0x1000, NULL, NULL, NULL,
                                   pRtnMemoryHandle, pOffset, NULL, NULL, NULL,
                                   0, 0, GetBoundGpuDevice()),
        "***ERROR: AllocVidHeapMemory failed to allocate video memory."
    );
    CHECK_RC_MSG
    (
        pLwRm->MapMemory (*pRtnMemoryHandle, 0, Size, pBase, 0, GetBoundGpuSubdevice()),
        "***ERROR: AllocVidHeapMemory failed to map video memory."
    );

    DispTest::LwrrentDevice()->MemMap[*pRtnMemoryHandle] = *pBase;

    return OK;
}

/****************************************************************************************
 * AllocSystemMemory
 *
 *  Arguments Description:
 *  - MemoryLocation
 *  - pRtnMemoryHandle
 *  - pBase
 *  - Size
 *
 *  Functional Description
 *  - System memory allocation:
 *    - Allocate Size bytes of memory at the given MemoryLocation.
 *    - Return a pointer to the start of the allocated memory.
 *
 *  Notes:
 *  - MemoryLocation must be either "ncoh" or "coh".
 ****************************************************************************************/
RC DispTest::AllocSystemMemory(string MemoryLocation,
                               UINT32 *pRtnMemoryHandle,
                               void **pBase,
                               UINT32 Size)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    // allocate memory
    if ((MemoryLocation == "pci") ||
        (MemoryLocation == "ncoh"))
    {
        CHECK_RC_MSG
        (
            pLwRm->AllocSystemMemory(
                pRtnMemoryHandle,
                DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _WRITE_COMBINE),
                Size,
                GetBoundGpuDevice()),
            "***ERROR: AllocSystemMemory failed to allocate system memory."
        );
    }
    else if ((MemoryLocation == "pci_coherent") ||
             (MemoryLocation == "coh"))
    {
        CHECK_RC_MSG
        (
            pLwRm->AllocSystemMemory(
                pRtnMemoryHandle,
                DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
                DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED),
                Size,
                GetBoundGpuDevice()),
            "***ERROR: AllocSystemMemory failed to allocate system memory."
        );
    }
    else
    {
        Printf(Tee::PriError, "AllocSystemMemory - Unknown System MemoryLocation: %s\n", MemoryLocation.c_str());
        return RC::BAD_PARAMETER;
    }
    CHECK_RC_MSG
    (
        pLwRm->MapMemory (*pRtnMemoryHandle, 0, Size, pBase, 0, GetBoundGpuSubdevice()),
        "***ERROR: AllocSystemMemory failed to map system memory."
    );
    DispTest::LwrrentDevice()->MemMap[*pRtnMemoryHandle] = *pBase;

    return OK;
}

/****************************************************************************************
 * CreateVidHeapContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle
 *  - Flags
 *  - Size
 *  - MemType
 *  - MemAttr
 *  - pRtnHandle
 *
 *  Functional Description
 *  - Context DMA allocation on video heap.
 *    - Specify the Flags for the context DMA and the Type and Attributes for the memory region.
 ****************************************************************************************/
RC DispTest::CreateVidHeapContextDma(LwRm::Handle ChannelHandle,
                                     UINT32 Flags,
                                     UINT64 Size,
                                     UINT32 MemType,
                                     UINT32 MemAttr,
                                     LwRm::Handle *pRtnHandle)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    // allocate memory for the context dma
    UINT32 hMem;
    void *pBase;
    UINT64 Offset;
    // Ensure that size is a multiple of 4K. Note that all iso ctx dmas are
    // 4K aligned and multiple of 4K in size.
    // We will always alloc memory in multiple of 4K and align to 4K. This is
    // not the most efficient scheme for iso surface (memory) since it can be
    // 256 byte aligned and multiple of 256 bytes in size (even though the ctx
    // dma describing it is actually 4K aligned). The reason we want to do
    // this is that, otherwise, the user of this function will need to use
    // SetOffset method to specify the correct offset and DispTest returns
    // back only ctx dma handle.
    Size = (Size + 0xFFF) & ~(0xFFF);

    CHECK_RC_MSG
    (
        AllocVidHeapMemory (&hMem, &pBase, Size, MemType, MemAttr, &Offset),
        "***ERROR: CreateVidHeapContextDma failed to allocate video memory."
    );

    // allocate the context dma
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma (pRtnHandle, Flags, hMem, 0, Size - 1),
        "***ERROR: CreateVidHeapContextDma failed to allocate context DMA."
    );
    CHECK_RC_MSG
    (
        DispTest::AddDmaContext (*pRtnHandle, "lwm", Flags, pBase, Size - 1,Offset),
        "***ERROR: CreateVidHeapContextDma failed to add context DMA."
    );

    // bind the context dma to the channel
    CHECK_RC_MSG
    (
        pLwRm->BindContextDma(ChannelHandle, *pRtnHandle),
        "***ERROR: CreateVidHeapContextDma failed to bind context DMA."
    );

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * CreateVidHeapContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle
 *  - Flags
 *  - Size
 *  - MemType
 *  - MemAttr
 *  - pRtnHandle
 *  - bindCtx: boolean defining if the context DMA should be bound to the ChannelHandle
 *
 *  Functional Description
 *  - Context DMA allocation on video heap.
 *    - Specify the Flags for the context DMA and the Type and Attributes for the memory region.
 ****************************************************************************************/
RC DispTest::CreateVidHeapContextDma(LwRm::Handle ChannelHandle,
                                     UINT32 Flags,
                                     UINT64 Size,
                                     UINT32 MemType,
                                     UINT32 MemAttr,
                                     LwRm::Handle *pRtnHandle,
                                     bool bindCtx)
{
    RC rc = OK;
    LwRmPtr pLwRm;

    // allocate memory for the context dma
    UINT32 hMem;
    void *pBase;
    UINT64 Offset;
    // Ensure that size is a multiple of 4K. Note that all iso ctx dmas are
    // 4K aligned and multiple of 4K in size.
    // We will always alloc memory in multiple of 4K and align to 4K. This is
    // not the most efficient scheme for iso surface (memory) since it can be
    // 256 byte aligned and multiple of 256 bytes in size (even though the ctx
    // dma describing it is actually 4K aligned). The reason we want to do
    // this is that, otherwise, the user of this function will need to use
    // SetOffset method to specify the correct offset and DispTest returns
    // back only ctx dma handle.
    Size = (Size + 0xFFF) & ~(0xFFF);

    CHECK_RC_MSG
    (
        AllocVidHeapMemory (&hMem, &pBase, Size, MemType, MemAttr, &Offset),
        "***ERROR: CreateVidHeapContextDma failed to allocate video memory."
    );

    // allocate the context dma
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma (pRtnHandle, Flags, hMem, 0, Size - 1),
        "***ERROR: CreateVidHeapContextDma failed to allocate context DMA."
    );
    CHECK_RC_MSG
    (
        DispTest::AddDmaContext (*pRtnHandle, "lwm", Flags, pBase, Size - 1,Offset),
        "***ERROR: CreateVidHeapContextDma failed to add context DMA."
    );

    if(bindCtx)
    {
        // bind the context dma to the channel
        CHECK_RC_MSG
            (
             pLwRm->BindContextDma(ChannelHandle, *pRtnHandle),
             "***ERROR: CreateVidHeapContextDma failed to bind context DMA."
            );
    }

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * CreateSystemContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle
 *  - Flags
 *  - Size
 *  - MemLocation
 *  - pRtnHandle
 *
 * DispTest object, properties and methods.
 ***************************************************************************************/

RC DispTest::CreateSystemContextDma(LwRm::Handle ChannelHandle,
                                    UINT32 Flags,
                                    UINT32 Size,
                                    string MemoryLocation,
                                    LwRm::Handle *pRtnHandle)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    UINT32 hMem;
    void *pBase;

    // adjust flags to match cache coherency of memory
    if ((MemoryLocation == "pci") ||
        (MemoryLocation == "ncoh"))
    {
        Flags = (Flags & ~DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE)) |
            DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE);
    }
    else if ((MemoryLocation == "pci_coherent") ||
             (MemoryLocation == "coh"))
    {
        Flags = (Flags & ~DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE)) |
            DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE);
    }
    else
    {
        Printf(Tee::PriError, "CreateSystemContextDma - Unknown System MemoryLocation: %s\n", MemoryLocation.c_str());
        return RC::BAD_PARAMETER;
    }

    // allocate memory for the context dma
    CHECK_RC_MSG
    (
        AllocSystemMemory (MemoryLocation, &hMem, &pBase, Size),
        "***ERROR: CreateSystemContextDma failed to allocate system memory for the context DMA."
    );

    // allocate the context dma
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma(pRtnHandle, Flags, hMem, 0, Size - 1),
        "***ERROR: CreateSystemContextDma failed to allocate context DMA."
    );
    CHECK_RC_MSG
    (
        DispTest::AddDmaContext(*pRtnHandle, MemoryLocation.c_str(), Flags, pBase, Size - 1, 0),
        "***ERROR: CreateSystemContextDma failed to add context DMA."
    );

    // bind the context dma to the channel
    CHECK_RC_MSG
    (
        pLwRm->BindContextDma(ChannelHandle, *pRtnHandle),
        "***ERROR: CreateSystemContextDma failed to bind the context DMA to the channel."
    );

    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * CreateSystemContextDma
 *
 *  Arguments Description:
 *  - ChannelHandle
 *  - Flags
 *  - Size
 *  - MemLocation
 *  - pRtnHandle
 *  - bindCtx: boolean defining if the context DMA should be bound to the ChannelHandle
 *
 * DispTest object, properties and methods.
 ***************************************************************************************/

RC DispTest::CreateSystemContextDma(LwRm::Handle ChannelHandle,
                                    UINT32 Flags,
                                    UINT32 Size,
                                    string MemoryLocation,
                                    LwRm::Handle *pRtnHandle,
                                    bool bindCtx)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    UINT32 hMem;
    void *pBase;

    // adjust flags to match cache coherency of memory
    if ((MemoryLocation == "pci") ||
        (MemoryLocation == "ncoh"))
    {
        Flags = (Flags & ~DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE)) |
            DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE);
    }
    else if ((MemoryLocation == "pci_coherent") ||
             (MemoryLocation == "coh"))
    {
        Flags = (Flags & ~DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE)) |
            DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE);
    }
    else
    {
        Printf(Tee::PriError, "CreateSystemContextDma - Unknown System MemoryLocation: %s\n", MemoryLocation.c_str());
        return RC::BAD_PARAMETER;
    }

    // allocate memory for the context dma
    CHECK_RC_MSG
    (
        AllocSystemMemory (MemoryLocation, &hMem, &pBase, Size),
        "***ERROR: CreateSystemContextDma failed to allocate system memory for the context DMA."
    );

    // allocate the context dma
    CHECK_RC_MSG
    (
        pLwRm->AllocContextDma(pRtnHandle, Flags, hMem, 0, Size - 1),
        "***ERROR: CreateSystemContextDma failed to allocate context DMA."
    );
    CHECK_RC_MSG
    (
        DispTest::AddDmaContext(*pRtnHandle, MemoryLocation.c_str(), Flags, pBase, Size - 1, 0),
        "***ERROR: CreateSystemContextDma failed to add context DMA."
    );
    if(bindCtx)
    {
        // bind the context dma to the channel
        CHECK_RC_MSG
            (
             pLwRm->BindContextDma(ChannelHandle, *pRtnHandle),
             "***ERROR: CreateSystemContextDma failed to bind the context DMA to the channel."
            );
    }
    // RC == OK at this point
    return rc;
}

/****************************************************************************************
 * DispTest::GetBoundGpuSubdevice
 *
 *  Arguments Description:
 *  -
 *
 *  Functional Description:
 *  - Gets a pointer to the GpuSubdevice under test
 ****************************************************************************************/
GpuSubdevice *DispTest::GetBoundGpuSubdevice() { return m_pGpuSubdevice; }

RC DispTest::GetHDMICapableDisplayIdForSor(UINT32 sorIndex, UINT32 *pRtnDispId)
{
    LwRmPtr     pLwRm;
    RC          rc            = OK;
    UINT32      dispMask;
    UINT32      lwrDispMask = 0;
    bool        bFoundDisplay = false;
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams = {0};

    // get supported displays
    dispSupportedParams.subDeviceInstance   = activeSubdeviceIndex;
    dispSupportedParams.displayMask         = 0;
    dispSupportedParams.displayMaskDDC      = 0;
    CHECK_RC_MSG
    (
        pLwRm->Control(LwrrentDevice()->DispCommonHandle,
                         LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                         &dispSupportedParams, sizeof (dispSupportedParams)),
            "***ERROR: GetHDMICapableDisplayIdForSor Failed to find all the supported devices\n"
    );

    dispMask = dispSupportedParams.displayMask;

    while (dispMask)
    {

        lwrDispMask = LOWESTBIT(dispMask);
        dispMask    &= ~lwrDispMask;

        // Check if the current DFP supports HDMI and has TMDS protocol
        LW0073_CTRL_DFP_GET_INFO_PARAMS     dfpParams = {0};
        dfpParams.subDeviceInstance = activeSubdeviceIndex;
        dfpParams.displayId         = lwrDispMask;
        dfpParams.flags             = 0;

        rc =  pLwRm->Control(LwrrentDevice()->DispCommonHandle,
                             LW0073_CTRL_CMD_DFP_GET_INFO,
                             &dfpParams, sizeof (dfpParams));
        if (rc != OK)
        {
            // continue if we cannot fetch DFP related information
            continue;
        }

        // check if the display supports TMDS protocol and is HDMI capable
        if(FLD_TEST_DRF(0073_CTRL_DFP, _FLAGS, _SIGNAL, _TMDS, dfpParams.flags) &&
           FLD_TEST_DRF(0073_CTRL_DFP, _FLAGS, _HDMI_CAPABLE, _TRUE, dfpParams.flags) )
        {
            LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS params;
            memset(&params, 0, sizeof(params));
            params.subDeviceInstance = activeSubdeviceIndex;
            params.displayId = lwrDispMask;
            CHECK_RC(pLwRm->Control(LwrrentDevice()->DispCommonHandle,
                                    LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                                    &params, sizeof(params)));

            // check if the or index matches with the sorIndex provided
            if (params.index == sorIndex)
            {
                Printf(Tee::PriNormal,
                    "DispTest: GetHDMICapableDisplayIdForSor Found HDMI Capable display on displayId: 0x%x for sor %d\n", lwrDispMask, sorIndex);
                bFoundDisplay = true;
                break;
            }
        }
        else
        {
            continue; //DFP supported, but is not HDMI capable...continue
        }
    }

    if (bFoundDisplay && lwrDispMask)
    {
        *pRtnDispId =   lwrDispMask;
        rc = OK;
    }
    else
    {
        *pRtnDispId =   0;
        RETURN_RC_MSG (RC::SOFTWARE_ERROR, "***ERROR: Cannot find a HDMI capable display for given sorIndex");
    }

    return rc;
}

RC DispTest::SetHDMIEnable(UINT32 displayId, bool enable)
{
    LwRmPtr     pLwRm;
    RC          rc            = OK;
    LW0073_CTRL_SPECIFIC_SET_HDMI_ENABLE_PARAMS hdmiParams = {0};

    // Enable HDMI on the given display mask
    hdmiParams.subDeviceInstance = activeSubdeviceIndex;
    hdmiParams.displayId         = displayId;
    if (enable)
    {
        hdmiParams.enable            = LW_TRUE;
    }
    else
    {
        hdmiParams.enable            = LW_FALSE;
    }
    CHECK_RC_MSG
        (
        pLwRm->Control(LwrrentDevice()->DispCommonHandle,
        LW0073_CTRL_CMD_SPECIFIC_SET_HDMI_ENABLE,
        &hdmiParams, sizeof (hdmiParams)),
        "*** ERROR: SetHDMIEnable Failed to set HDMI Enable\n"
        );
    Printf(Tee::PriNormal,
        "DispTest: SetHDMIEnable Enabled HDMI on displayId: 0x%x\n", displayId);

    return rc;
}

RC DispTest::SetHDMISinkCaps(UINT32 displayId, UINT32 caps)
{
    LwRmPtr     pLwRm;
    RC          rc            = OK;
    LW0073_CTRL_SPECIFIC_SET_HDMI_SINK_CAPS_PARAMS hdmiParams = {0};

    // HDMI SinkCaps parameters
    hdmiParams.subDeviceInstance = activeSubdeviceIndex;
    hdmiParams.displayId         = displayId;
    hdmiParams.caps              = caps;

    CHECK_RC_MSG
        (
        pLwRm->Control(LwrrentDevice()->DispCommonHandle,
        LW0073_CTRL_CMD_SPECIFIC_SET_HDMI_SINK_CAPS,
        &hdmiParams, sizeof (hdmiParams)),
        "*** ERROR: SetHDMISinkCaps Failed to set HDMI Sink CAPS\n"
        );
    Printf(Tee::PriNormal,
        "DispTest: SetHDMISinkCaps set HDMI sink CAPS on displayId: 0x%x\n", displayId);

    return rc;
}

/****************************************************************************************
 * DispTest::GetBoundGpuDevice
 *
 *  Arguments Description:
 *  -
 *
 *  Functional Description:
 *  - Gets a pointer to the GpuDevice under test
 ****************************************************************************************/
GpuDevice    *DispTest::GetBoundGpuDevice() { return m_pGpuDevice; }

/****************************************************************************************
 * DispTest::GetPinsetCount
 *
 *  Arguments Description:
 *  - UINT32 *puPinsetCount: returns the pinset count.
 *
 *  Functional Description
 *  - Get the number of pinsets.
 ****************************************************************************************/
RC DispTest::GetPinsetCount (UINT32 *puPinsetCount)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetPinsetCount - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->GetPinsetCount(puPinsetCount));
}

/****************************************************************************************
 * DispTest::GetPinsetLockPins
 *
 *  Arguments Description:
 *  - UINT32 uPinset:        the pinset.
 *  - UINT32 *puScanLockPin: returns the scan-lock pin.
 *  - UINT32 *puFlipLockPin: returns the flip-lock pin.
 *
 *  Functional Description
 *  - Get the scan-lock and flip-lock pins for the specified pinset.
 ****************************************************************************************/
RC DispTest::GetPinsetLockPins (UINT32 uPinset, UINT32 *puScanLockPin, UINT32 *puFlipLockPin)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetPinsetLockPins - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->GetPinsetLockPins(uPinset, puScanLockPin, puFlipLockPin));
}

/****************************************************************************************
 * DispTest::GetStereoPin
 *
 *  Arguments Description:
 *  - UINT32 *puStereoPin: returns the stereo pin.
 *
 *  Functional Description
 *  - Get the stereo pin.
 ****************************************************************************************/
RC DispTest::GetStereoPin (UINT32 *puStereoPin)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetStereoPin - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->GetStereoPin(puStereoPin));
}

/****************************************************************************************
 * DispTest::GetExtSyncPin
 *
 *  Arguments Description:
 *  - UINT32 *puExtSyncPin: returns the external sync pin.
 *
 *  Functional Description
 *  - Get the external sync pin.
 ****************************************************************************************/
RC DispTest::GetExtSyncPin (UINT32 *puExtSyncPin)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetExtSyncPin - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->GetExtSyncPin(puExtSyncPin));
}

/****************************************************************************************
 * DispTest::SetDsiForceBits
 *
 *  Functional Description
 *  - Sets the Dsi force bits in LW_PDISP_SUPERVISOR_HEAD for a given head.
 ****************************************************************************************/
RC DispTest::SetDsiForceBits(UINT32 WhichHead,
                             UINT32 ChangeVPLL, UINT32 NochangeVPLL,
                             UINT32 Blank, UINT32 NoBlank,
                             UINT32 Shutdown, UINT32 NoShutdown,
                             UINT32 NoBlankShutdown, UINT32 NoBlankWakeup
    )
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetDsiForceBits - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->SetDsiForceBits(WhichHead,
                                                     ChangeVPLL, NochangeVPLL,
                                                     Blank, NoBlank,
                                                     Shutdown, NoShutdown,
                                                     NoBlankShutdown, NoBlankWakeup)
        );
}

/****************************************************************************************
 * DispTest::GetDsiForceBits
 *
 *  Functional Description
 *  - Gets the Dsi force bits in LW_PDISP_SUPERVISOR_HEAD for a given head.
 ****************************************************************************************/
RC DispTest::GetDsiForceBits(UINT32 WhichHead,
                             UINT32* ChangeVPLL, UINT32* NochangeVPLL,
                             UINT32* Blank, UINT32* NoBlank,
                             UINT32* Shutdown, UINT32* NoShutdown,
                             UINT32* NoBlankShutdown, UINT32* NoBlankWakeup
    )
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::GetDsiForceBits - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->GetDsiForceBits(WhichHead,
                                                     ChangeVPLL, NochangeVPLL,
                                                     Blank, NoBlank,
                                                     Shutdown, NoShutdown,
                                                     NoBlankShutdown, NoBlankWakeup)
        );
}

/****************************************************************************************
 * DispTest::TimeSlotsCtl
 *
 *  Functional Description
 *      Sets a boolean value that controls if SF & BFM will be updated with new timeslots when add_stream is called. Supported only in DTB
 ****************************************************************************************/
RC DispTest::TimeSlotsCtl (bool value)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::TimeSlotsCtl - Current device class not allocated.");
    }
    // And forward the call to the class.
    return(m_display_devices[index]->TimeSlotsCtl(value));
}

/****************************************************************************************
 * DispTest::ActivesymCtl
 *
 *  Functional Description
 *      Sets a boolean value that controls if LW_PDISP_SF_DP_CONFIG_ACTIVESYM_CNTL_MODE has to be set to DISABLE. Supported only in DTB
 ****************************************************************************************/
RC DispTest::ActivesymCtl (bool value)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ActivesymCtl - Current device class not allocated.");
    }
    // And forward the call to the class.
    return(m_display_devices[index]->ActivesymCtl(value));
}

/****************************************************************************************
 * DispTest::UpdateTimeSlots
 *
 *  Functional Description
 *      SF & BFM registers will be updated with new timeslots parameters. Supported only in DTB
 ****************************************************************************************/
RC DispTest::UpdateTimeSlots (UINT32 sornum, UINT32 headnum)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::TimeSlotsCtl - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->UpdateTimeSlots(sornum,headnum));
}

/****************************************************************************************
 * DispTest::sorSetFlushMode
 *
 *  Functional Description
 *      Program SOR to enter or exit flush mode.
 *
 ****************************************************************************************/
RC DispTest::sorSetFlushMode
(
    UINT32 sornum,
    bool enable,
    bool bImmediate,
    bool bFireAndForget,
    bool bForceRgDiv,
    bool bUseBFM
)
{
    // Check that this device has a Display class.
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size()) || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::sorSetFlushMode - Current device class not allocated.");
    }

    // And forward the call to the class.
    return(m_display_devices[index]->sorSetFlushMode(sornum,enable,bImmediate,bFireAndForget,bForceRgDiv,bUseBFM));
}

/****************************************************************************************
 * DispTest::SetSPPLLSpreadPercentage
 *
 *  Functional Description
 *      Sets the spread percentage value for VPLLs.  Bug 653760
 *
 ****************************************************************************************/
RC DispTest::SetSPPLLSpreadPercentage(UINT32 spread_value)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetSPPLLSpreadPercentage - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetSPPLLSpreadPercentage(spread_value));
}

/****************************************************************************************
 * DispTest::SetMscg
 *
 *  Functional Description
 *  enable or disable MSCG
 *
 ****************************************************************************************/
RC DispTest::SetMscg(UINT32 spread_value)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetMscg - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetMscg(spread_value));
}

/****************************************************************************************
 * DispTest::SetForceDpTimeslot
 *
 *  Functional Description
 *      Forces the DP multistream timeslot value
 *
 ****************************************************************************************/
RC DispTest::SetForceDpTimeslot(UINT32 sor_num, UINT32 headnum, UINT32 timeslot_value, bool update_alloc_pbn)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::SetForceDpTimeslot - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->SetForceDpTimeslot(sor_num, headnum, timeslot_value, update_alloc_pbn));
}

/****************************************************************************************
 * DispTest::DisableAutoSetDisplayId
 *
 *  Functional Description
 *      Disables the DispTest method snooping code that automatically tracks and
 *      enqueues SetDisplayId methods.  Use this if you want to manage the methods
 *      manually.
 *  NOTE: this function assumes that LW_PDISP_SOR_DP_LINKCTL_FORMAT_MODE has already been
 *      setup correctly.
 *
 ****************************************************************************************/
RC DispTest::DisableAutoSetDisplayId()
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::DisableAutoSetDisplayId - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->DisableAutoSetDisplayId());
}

/****************************************************************************************
 * DispTest::LookupDisplayId
 *
 *  Functional Description
 *      Lookup the displayId for a given or configuration, as specified in the args.
 *      the headnum argument is only used for multistream, so for normal OR configs
 *      it is a don't care.
 *  NOTE: this function assumes that LW_PDISP_SOR_DP_LINKCTL_FORMAT_MODE has already been
 *      setup correctly.
 *
 ****************************************************************************************/
UINT32 DispTest::LookupDisplayId(UINT32 ortype, UINT32 ornum, UINT32 protocol, UINT32 headnum, UINT32 streamnum)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        Printf(Tee::PriError, "DispTest::LookupDisplayId - Current device class not allocated.");
        MASSERT(0);
    }

    ///and forward the call to the class
    return(m_display_devices[index]->LookupDisplayId(ortype, ornum, protocol, headnum, streamnum));
}

/****************************************************************************************
 * DispTest::DpCtrl
 *
 *  Functional Description
 *      Do DP link training via LW0073_CTRL_CMD_DP_CTRL api.
 *
 ****************************************************************************************/
RC DispTest::DpCtrl (UINT32 sornum, UINT32 protocol,
            bool laneCount_Specified, UINT32 *pLaneCount, bool *pLaneCount_Error,
            bool linkBw_Specified, UINT32 *pLinkBw, bool *pLinkBw_Error,
            bool enhancedFraming_Specified, bool enhancedFraming,
            bool isMultiStream, bool disableLinkConfigCheck, bool fakeLinkTrain,
            bool *pLinkTrain_Error, bool *pIlwalidParam_Error, UINT32 *pRetryTimeMs
            )
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        Printf(Tee::PriError, "DispTest::LookupDisplayId - Current device class not allocated.");
        MASSERT(0);
    }

    ///and forward the call to the class
    return(m_display_devices[index]->DpCtrl(sornum, protocol, laneCount_Specified, pLaneCount, pLaneCount_Error,
               linkBw_Specified, pLinkBw, pLinkBw_Error, enhancedFraming_Specified, enhancedFraming,
               isMultiStream, disableLinkConfigCheck, fakeLinkTrain, pLinkTrain_Error, pIlwalidParam_Error, pRetryTimeMs));
}

/****************************************************************************************
 * DispTest::ControlSetSfDualStream
 *
 *  Arguments Description:
 *  - headNum:
 *  - enableDualMst:
 *
 *  Functional Description
 *  - Setup single-head dual stream mode
 ****************************************************************************************/
RC DispTest::ControlSetSfDualMst(UINT32 sorNum,
                                 UINT32 headNum,
                                 UINT32 enableDualMst,
                                 UINT32 isDPB)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetSorOpMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetSfDualMst(sorNum, headNum, enableDualMst, isDPB));
}

/****************************************************************************************
 * DispTest::ControlSetSorXBar
 *
 *  Arguments Description:
 *  - sorNum:
 *  - link_p:
 *  - link_s:
 *
 *  Functional Description
 *  - Setup the SOR XBAR to connect the SOR and analog link
 ****************************************************************************************/
RC DispTest::ControlSetSorXBar(UINT32 sorNum,
                               UINT32 protocolNum,
                               UINT32 linkPrimary,
                               UINT32 linkSecondary)
{
    ///check that this device has a Display class
    UINT32 index = LwrrentDevice()->GetDeviceIndex();

    if((index >= m_display_devices.size())
        || (m_display_devices[index] == NULL))
    {
        RETURN_RC_MSG(RC::ILWALID_DEVICE_ID, "***ERROR: DispTest::ControlSetSorOpMode - Current device class not allocated.");
    }

    ///and forward the call to the class
    return(m_display_devices[index]->ControlSetSorXBar(sorNum, protocolNum, linkPrimary, linkSecondary));
}

/****************************************************************************************
 * DispTest::InstallErrorLoggerFilter
 *
 *  Arguments Description:
 *  - pattern: The string pattern to match, interrupt with matching pattern will be
 *             skipped by the error logger
 *
 *  Functional Description
 *  - Helper function for tests to add a ErrorLogger filter
 ****************************************************************************************/
std::string DispTest::s_ErrLoggerIgnorePattern;

void DispTest::InstallErrorLoggerFilter(std::string pattern)
{
    if (!s_ErrLoggerIgnorePattern.empty())
    {
        Printf(Tee::PriNormal, "There is already a ErrorLogger filter installed, will skip new installation");
        return;
    }

    s_ErrLoggerIgnorePattern = pattern;

    ErrorLogger::InstallErrorLogFilter(ErrorLogFilter);
}

bool DispTest::ErrorLogFilter(const char *errMsg)
{
    if (Utility::MatchWildCard(errMsg, s_ErrLoggerIgnorePattern.c_str()))
        return false;

    return true;
}

//      Copyright (c) 1999-2005 by LWPU Corporation.
//      All rights reserved.
//
