#ifndef INCLUDED_DTIUTILS_H
#define INCLUDED_DTIUTILS_H

#include "ctrl/ctrl5070.h"
#include "ctrl/ctrl5070/ctrl5070rg.h"

#ifndef INCLUDED_DISPLAY_H
#include "core/include/display.h"
#endif

#ifndef INCLUDED_EVO_DISPLAY_H
#include "gpu/display/evo_disp.h"
#endif

#ifndef INCLUDED_EVO_CHANNELS_H
#include "gpu/display/evo_chns.h"
#endif

#ifndef __CRCCOMPARISON_H__
#include "crccomparison.h"
#endif

#ifndef INCLUDED_GPUSBDEV_H
#include "gpu/include/gpusbdev.h"
#endif

#ifndef INCLUDED_GPU_UTILITY_H
#include "gpu/utility/gpuutils.h"
#endif

#ifndef INCLUDED_UTILITY_H
#include "core/include/utility.h"
#endif

#include <fstream>

#include "ctrl/ctrl0073/ctrl0073system.h"  //For LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP

#define DTI_MANUALVERIF_TIMEOUT_MS 300000
#define DTI_MANUALVERIF_SLEEP_MS 15000

namespace DTIUtils
{
    typedef struct _SetImageConfig
    {
        DisplayID           displayId;
        Display::Mode       mode;
        UINT32              head;
        Display::ChannelID  channelId;

        _SetImageConfig() : displayId(), mode(), head(Display::ILWALID_HEAD), channelId(Display::CORE_CHANNEL_ID)
        {
        }

        _SetImageConfig(DisplayID            dispId,
                        Display::Mode        modeDetails,
                        UINT32               headNo,
                        Display::ChannelID   chanId)
        {
            displayId  = dispId;
            mode       = modeDetails;
            head       = headNo;
            channelId  = chanId;
        }
    }SetImageConfig;

    typedef struct
    {
        // Byte 0,1,2       : Pixel clock % 10,000. => 0.01 to 167,772.16 Mega Pixels per Sec
        UINT08  PClk10KhzLow;
        UINT08  PClk10KhzMid;
        UINT08  PClk10KhzHigh;

        // Byte 3           : Timing Options.
        UINT08  TimingFlags;

        // Byte 4,5         : Horizontal Active Image Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
        UINT08  HActiveLow;
        UINT08  HActiveHigh;

        // Byte 6,7         : Horizontal Blank Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
        UINT08  HBlankLow;
        UINT08  HBlankHigh;

        // Byte 8,9         :
        // High bits 14 - 8 : HFrontPorchHigh
        // Bit 15           : Horizontal Sync Polarity. [0 = -ve; 1= +ve]
        UINT08  HFrontPorchLow;
        UINT08  HFrontPorchHighAndPolarity;

        // Byte 10,11       : Horizontal Sync Width: 1 - 65,536 Pixels [0000h - FFFFh]
        UINT08  HSyncWidthLow;
        UINT08  HSyncWidthHigh;

        // Byte 12,13       : Vertical Active Image Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
        UINT08  VActiveLow;
        UINT08  VActiveHigh;

        // Byte 14,15       : Vertical Blank Pixels. 1 - 65,536 Pixels [0000h - FFFFh]
        UINT08  VBlankLow;
        UINT08  VBlankHigh;

        // Byte 16,17       :
        // Bits 14 - 8      : VFrontPorchHigh
        // Bit 15           : Vertical Sync Polarity. [0 = -ve; 1= +ve]
        UINT08  VFrontPorchLow;
        UINT08  VFrontPorchHighAndPolarity;

        // Byte 18,19       : Vertical Sync Width: 1 - 65,536 Pixels [0000h - FFFFh]
        UINT08  VSyncWidthLow;
        UINT08  VSyncWidthHigh;
    } DispIdExtBlockType1;

    // compile time assert to ensure the structure size if 20 bytes.
    // We will be replacing this block inside EDID Extn Block and
    // hence strickly need to ensure it is 20 bytes.
    ct_assert(20 == sizeof(DispIdExtBlockType1));

    typedef struct
    {
        // Byte 0,1       : Pixel clock % 10,000.
        UINT08  PClk10KhzLow;
        UINT08  PClk10KhzHigh;

        // byte 2: Horizontal Addressable Video in pixels --- contains lower 8 bits
        UINT08  HActiveLow;

        // byte 3: Horizontal Blanking in pixels --- contains lower 8 bits
        UINT08  HBlankLow;

        // byte 4: Horizontal Addressable Video in pixels - stored in Upper Nibble: contains upper 4 bits
        //         Horizontal Blanking in pixels          - stored in Lower Nibble: contains upper 4 bits
        UINT08  HActiveHighAndHBlankHigh;

        // byte 5: Vertical Addressable Video in lines --- contains lower 8 bits
        UINT08  VActiveLow;

        // byte 6: Vertical Blanking in lines --- contains lower 8 bits
        UINT08  VBlankLow;

        // byte 7: Vertical Addressable Video in lines -- stored in Upper Nibble: contains upper 4 bits
        //         Vertical Blanking in lines --- stored in Lower Nibble: contains upper 4 bits
        UINT08  VActiveHighAndVBlankHigh;

        // byte 8: Horizontal Front Porch in pixels --- contains lower 8 bits
        UINT08  HFrontPorchLow;

        // byte 9: Horizontal Sync Pulse Width in pixels --- contains lower 8 bits
        UINT08  HSyncWidthLow;

        //byte 10: Vertical Front Porch in Lines --- stored in Upper Nibble: contains lower 4 bits
        //         Vertical Sync Pulse Width in Lines --- stored in Lower Nibble: contains lower 4 bits
        UINT08  VFrontPorchHighAndVSyncWidthHigh;

        // byte 11: n n _ _ _ _ _ _ Horizontal Front Porch in pixels      --- contains upper 2 bits
        //          _ _ n n _ _ _ _ Horizontal Sync Pulse Width in Pixels --- contains upper 2 bits
        //          _ _ _ _ n n _ _ Vertical Front Porch in lines         --- contains upper 2 bits
        //          _ _ _ _ _ _ n n Vertical Sync Pulse Width in lines    --- contains upper 2 bits
        UINT08  FrontPorchSyncWidthHV;

        // size in mm = pixels * mmPerInch / pixelsPerInch;  //assuming 96 DPI monitor
        // byte 12: Horizontal Addressable Video Image Size in mm --- contains lower 8 bits
        UINT08  HVisibleInMMLow;

        // byte 13: Vertical Addressable Video Image Size in mm --- contains lower 8 bits
        UINT08  VVisibleInMMLow;

        // byte 14: Horizontal Addressable Video Image Size in mm --- stored in Upper Nibble: contains upper 4 bits
        //           Vertical Addressable Video Image Size in mm --- stored in Lower Nibble: contains upper 4 bits
        UINT08  HVActiveHigh;

        // byte 15: Horizontal Border in pixels
        UINT08  HBorder;

        // byte 16: Vertical Border in pixels
        UINT08  VBorder;

        // byte 17: Flag
        UINT08  TimingFlags;
    } DTDBlock;

    // compile time assert to ensure the structure size is 18 bytes.
    // We will be replacing this block inside EDID DTD Block and
    // hence strickly need to ensure it is exactly 18 bytes and no compiler padding is done
    ct_assert(18 == sizeof(DTDBlock));

    struct EDIDUtils
    {
        static int gethex(char ch1);
        static void FindNReplace(string &content,
                          string findstr,
                          string repstr);
        static RC GetEdidBufFromFile(string path,
                              UINT08 *&edidData,
                              UINT32 *edidSize);
        static RC FakeDisplay(Display       *pDisplay,
                              string        dispProtocol,
                              DisplayIDs    vbiosDisplays,
                              DisplayIDs    *pFakeDispIds,
                              string Edidfilename = "");
        static RC SetLwstomEdid(Display    *pDisplay,
                                DisplayID   Display,
                                bool        bSetEdid     = false,
                                string      Edidfilename = "");
        static RC GetSupportedRes(Display              *pDisplay,
                                  DisplayID             displayID,
                                  Display::Mode                  requiredRes,
                                  vector<Display::Mode>         *pSupportedRes);

        static RC GetListedModes(Display                  *pDisplay,
                                 DisplayID                 displayID,
                                 vector<Display::Mode>    *pListedRes,
                                 bool                      bGetResFromCPL = false);

        static RC GetListedModesFromCPL(Display             *pDisplay,
                                 DisplayID                 dispId,
                                 vector<Display::Mode>    *pModeList);

        static bool IsValidEdid(Display    *pDisplay,
                                DisplayID   dispId);

        static bool EdidHasResolutions(Display *pDisplay,
                                DisplayID dispId);

        //! function CreateEdidFileWithModes()
        //!     This function creates EDID file with the modes provided in argument populated in DTD blocks.
        //! params pDisplay               [in] : Pointer to display class.
        //! params referenceDispId        [in] : Pass the Fake disp Id to be used as reference for generating EDID.
        //!     Note: We assume the dispId passed has the edid "referenceEdidFileName_DFP" faked already.
        //!      Not doing any re-faking here so as to avoid additional modesets [to speed up for emu].
        //! params modeSettings           [in] : Modes needed for this config.
        //! params referenceEdidFileName_DFP [in] : EDID file to be used as base EDID file. Note: This file should be EDID ver1.4.
        //! params newEdidFileName        [in] : Filename of new EDID file to be generated.
        //! params pErrorStr              [in] : Char buffer where error string is to be passed back.
        static RC  CreateEdidFileWithModes(Display *pDisplay,
            DisplayID             referenceDispId,
            vector<Display::Mode> &modeSettings,
            string                referenceEdidFileName_DFP,
            string                newEdidFileName,
            char                 *pErrorStr);

        //! function CreateDispIdType1ExtBlock()
        //!     This function creates EDID Extension block using Display ID v1.3 spec.
        //!     This Extension block is needed to create EDID with modes 65k x 65k.
        //! params pDisplay               [in] : Pointer to display class.
        //! params referenceDispId        [in] : Pass the Fake disp Id to be used as reference for generating EDID.
        //!     Note: We assume the dispId passed has the edid "referenceEdidFileName_DFP" faked already.
        //!      Not doing any re-faking here so as to avoid additional modesets [to speed up for emu].
        //! params mode                   [in] : Mode for which extension block structure is to be created.
        //! params dispIdExtBlockType1    [out]: Display ID v1.3 Type 1 extension block.
        //! params pLwtTiming             [in] : LWT Timing, to be added to the DTD block
        static RC  CreateDispIdType1ExtBlock(Display *pDisplay,
            DisplayID             referenceDispId,
            Display::Mode         &mode,
            DispIdExtBlockType1   &dispIdExtBlockType1,
            LWT_TIMING            *pLwtTiming);

        //! function CreateDTDExtBlock()
        //!     This function creates EDID DTD Extension block.
        //!     This Extension block is needed to create EDID with modes upto 4k x 4k.
        //! params pDisplay               [in] : Pointer to display class.
        //! params referenceDispId        [in] : Pass the Fake disp Id to be used as reference for generating EDID.
        //!     Note: We assume the dispId passed has the edid "referenceEdidFileName_DFP" faked already.
        //!      Not doing any re-faking here so as to avoid additional modesets [to speed up for emu].
        //! params mode                   [in] : Mode for which extension block structure is to be created.
        //! params dtdExtBlock            [out]: EDID DTD extension block.
        //! params pLwtTiming             [in] : LWT Timing, to be added to the DTD block
        static RC  CreateDTDExtBlock(Display *pDisplay,
            DisplayID             referenceDispId,
            Display::Mode         &mode,
            DTDBlock              &dtdExtBlock,
            LWT_TIMING            *pLwtTiming);
    };

    struct VerifUtils
    {
        static RC manualVerification(Display* pDisplay,
                               DisplayID SetmodeDisplay,
                               Surface2D *Surf2Print,
                               string PromptMessage,
                               Display::Mode resolution,
                               UINT32 timeoutMS,
                               UINT32 sleepInterval,
                               UINT32 subdeviceInstance = 0);

        static RC showPrompt(DisplayID SetmodeDisplay,
                       Surface2D *Surf2Print,
                       string PromptMessage,
                       Display::Mode resolution,
                       UINT32 subdeviceInstance = 0);

        static RC acceptKey(UINT32 timeoutMS,
                      UINT32 sleepInterval);

        static RC autoVerification(Display      *pDisplay,
                             GpuDevice          *pGpuDev,
                             DisplayID           SetmodeDisplay,
                             UINT32              width,
                             UINT32              height,
                             UINT32              depth,
                             string              goldenFolderPath,
                             string              CrcFileName,
                             Surface2D          *pCoreImage = NULL,
                             VerifyCrcTree      *pCrcCompFlag = NULL);
        static bool IsSupportedProtocol(Display *pDisplay,
                                        DisplayID displayID,
                                        string desiredProtocolList);
        static RC IsModeSupportedOnMonitor(Display          *pDisplay,
                                           Display::Mode     mode,
                                           DisplayID         dispId,
                                           bool             *pModeSupportedOnMonitor,
                                           bool              bGetResFromCPL = false);

        static int upper(int c);
        static vector<string> Tokenize(const string &str,
                                const string &delimiters);
        static bool IsValidProtocolString(string desiredProtocol);
        static RC   checkDispUnderflow(Display *pDisplay, UINT32 numHeads, UINT32 *pUnderflowHead);

        // Get RG underflow state for head specified
        static RC  GetUnderflowState(Display *pDisplay,
            UINT32   head,
            LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP_PARAMS    *pGetUnderflowParams);

        // Set RG underflow state on head specified
        static RC SetUnderflowState(Display *pDisplay,
            UINT32 head,
            UINT32 enable           = LW5070_CTRL_CMD_UNDERFLOW_PROP_ENABLED_YES,
            UINT32 clearUnderflow   = LW5070_CTRL_CMD_UNDERFLOW_PROP_CLEAR_UNDERFLOW_YES,
            UINT32 mode             = LW5070_CTRL_CMD_UNDERFLOW_PROP_MODE_RED);

        static RC CompareSurfaces(Surface2D    *surf2Compare,
                                  Surface2D    *goldenSurface,
                                  UINT32        resultMode);
        static RC CreateGoldenAndCompare(GpuDevice       *pOwningDev,
                                         Surface2D    *surf2Compare,
                                         Surface2D    *GoldenSurface,
                                         UINT32        ImageWidth,
                                         UINT32        ImageHeight,
                                         UINT32        ImageDepth,
                                         string        ImageName="",
                                         UINT32        color=0);

    };

    enum ImageFormat
    {
        IMAGE_NONE,
        IMAGE_PNG,
        IMAGE_HDR,
        IMAGE_RAW,
    };

    struct ImageUtils
    {
        string ImageName;
        ImageFormat format;
        UINT32 ImageWidth;
        UINT32 ImageHeight;

        ImageUtils();
        ImageUtils(UINT32 width,UINT32 height);
        string GetImageName();
        UINT32 GetImageWidth();
        UINT32 GetImageHeight();
        static ImageUtils SelectImage(UINT32 width,UINT32 height);
        static ImageUtils SelectImage(ImageUtils *reqImg);
        static void UpdateImageArray(ImageUtils *imgArr);
    };

    struct Mislwtils
    {
        static ifstream * alloc_ifstream (vector<string> & searchpath, const string & fname);
        static RC GetHeadRoutingMask(Display *pDisplay,
                                     LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *HeadRoutingMap);
        static RC GetHeadFromRoutingMask(DisplayID dispayId,
                                  LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS HeadMapParams,
                                  UINT32 *pHead);

        static RC ColwertDisplayClassToString(UINT32 displayClass,
                                              string &displayClassStr);

        static std::string trim_right(const std::string &source, const std::string& t = " ");
        static std::string trim_left( const std::string& source, const std::string& t = " ");
        static std::string trim(const std::string& source,       const std::string& t = " ");
        static UINT32      getLogBase2(UINT32 nNum);
    };

    struct ReportUtils
    {
        static RC write2File(FILE *fp, const string & data);

        static RC CreateCSVReportHeader(string &headerString);

        static RC CreateCSVReportString(Display *pDisplay, vector<SetImageConfig> setImageConfig, ColorUtils::Format colorFormat,
                        bool bIMPpassed, bool bSetModeStatus, const string &comment, string &reportString);
        static RC GetLwrrTime(const char *timeFormat, char *lwrrTime, UINT32 lwrrTimeSize);
    };
};

#endif

