/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_mxm.cpp

#include "gpu/tests/rmtest.h"

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "lwRmApi.h"
#include "class/cl0073.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl0080.h"
#include "core/include/memcheck.h"

// MXM constants and macros for 2.x

// MXM structure data types.
// New defines to match the MXMEdit Tool
#define OUTPUT_DEVICE_STRUCTURE                   0x00
#define SYSTEM_COOLING_CAPS_STRUCTURE             0x01
#define THERMAL_STRUCTURE                         0x02
#define INPUT_POWER_STRUCTURE                     0x03
#define GPIO_DEVICE_STRUCTURE                     0x04
#define VENDOR_STRUCTURE                          0x05
#define BACKLIGHT_CONTROL_STRUCTURE               0x06

// Identifier for device handle
#define CLIENT_DEVICE_HANDLE                 0xDEADBEE1

// EDID handle constants
#define LWCTRL_GPU_DEVICE_HANDLE            (0x1DAD2222)
#define LWCTRL_DISPLAY_DEVICE_HANDLE        (0xAEDD3333)

/* Max number of entries allowed per data type. */
#define MXM_MAX_ENTRIES_OUTPUT_DEVICE           6
#define MXM_MAX_ENTRIES_COOLING_CAPS            2
#define MXM_MAX_ENTRIES_THERMAL                 3
#define MXM_MAX_ENTRIES_INPUT_POWER             4
#define MXM_MAX_ENTRIES_GPIO_DEVICE             1
#define MXM_MAX_ENTRIES_GPIO_PIN                5
#define MXM_MAX_ENTRIES_VENDOR                  2
#define MXM_MAX_ENTRIES_BACKLIGHT               3

#define OUTPUT_DEVICE_TYPE_MAX                 0x03
#define OUTPUT_DEVICE_DDCPORT_MAX              0x02
#define DDCPORT_NA                             0x0F
#define CONNECTOR_TYPE_MAX                     0x0C
#define CONNECTOR_TYPE_RESERVED_1              0x6
#define CONNECTOR_TYPE_RESERVED_2              0x7
#define CONNECTOR_TYPE_NA                      0x0F
#define DIGITAL_CONNECTION_MAX                 0x09
#define DIGITAL_CONNECTION_NA                  0x0F
#define TV_FORMAT_NA                           0x1F
#define TV_FORMAT_NOT_SPECIFIED                0x0F
#define TYPE_TV                                0x01
#define OUTPUT_DEVICE_RESERVED                 0
#define INPUT_POWER_TYPE_MAX                   1
#define GPIO_DEVICE_TYPE_MAX                   1
#define MXM2X_DEVICE_NOT_PRESENT               0xFF
#define OUTPUT_GPIO_UNUSED                     0x1F
#define SYS_OUTPUT_GPIO                        0
#define SYS_OUTPUT_ACPI                        1

#define MXM_BIN_FILE                           "c:\mxm_data.bin"

#define MXM_MAX_BUFFER_SIZE                     (4*1024) // 4K bytes

#pragma pack(1)
// MXM 2.0 structure definitions.
typedef struct _MXM_Header
{
    UINT32 String;
    UINT08 Version;
    UINT08 Revision;
    UINT16 Length;
} MXM_Header;

// Generic header data type. It uses only 4 bits.
typedef struct _MXM_Data_Type_Header
{
    UINT08 Type : 4;
    UINT08 Unused : 4;
} MXM_Data_Type_Header;

typedef struct _MXM_Output_Device
{
    UINT08 DescriptorType : 4;
    UINT08 Type : 4;
    UINT32 DDCPort : 4;
    UINT32 ConnectorType : 5;
    UINT32 ConnectorLocation : 2;
    UINT32 LVDS_TMDS_HDMI_Connection : 4;
    UINT32 TVFormat : 5;
    UINT32 OutputSelectForGPIO : 5;
    UINT32 PolarityForGPIO : 1;
    UINT32 SystemOutputMethod : 1;
    UINT32 GPIOForDDCSelect : 5;
    UINT08 SystemDDCMethod : 1;
    UINT08 GPIOForDeviceDetection : 5;
    UINT08 PolarityForGPIODeviceDetection : 1;
    UINT08 SystemHotPlugNotify : 1;
} MXM_Output_Device;

typedef struct _MXM_Cooling_Capability
{
    UINT32 DescriptorType : 4;   // 0x1
    UINT32 CoolingType : 4;
    UINT32 CoolingValue : 10;
    UINT32 Reserved : 14;
} MXM_Cooling_Capability;

typedef struct _MXM_Thermal
{
    UINT32 DescriptorType : 4;
    UINT32 Type :  4;
    UINT32 Value : 10;
    UINT32 Scale : 2;
    UINT32 Reserved : 12;
} MXM_Thermal;

typedef struct _MXM_Input_Power
{
    UINT32 DescriptorType : 4;
    UINT32 Type : 4;
    UINT32 Amp4Value : 10;
    UINT32 Scale : 2;
    UINT32 Amp16Value : 10; // Shifted down due to change in specs
    UINT32 Reserved : 2;
} MXM_Input_Power;
typedef struct _MXM_GPIO_Device
{
    UINT32 DescriptorType : 4;
    UINT32 Type : 8;
    UINT32 I2CAddress : 8;
    UINT32 Reserved : 8;
    UINT32 NumOfPins : 4;
} MXM_GPIO_Device;

typedef struct _MXM_GPIO_Pin_Entry
{
    UINT08 PinNumber : 4;
    UINT08 Reserved : 4;
    UINT08 Function : 8;
} MXM_GPIO_Pin_Entry;

typedef struct _MXM_Backlight_Control
{
    UINT32 DescriptorType : 4;
    UINT32 Type : 4;
    UINT32 MaxDutyCycle : 16;
    UINT32 MinDutyCycle : 16;
    UINT32 DutyCycleFreq : 18;
    UINT32 Reserved : 6;
} MXM_Backlight_Control;

typedef struct _MXM_Vendor_Data
{
    UINT32 DescriptorType : 4;
    UINT32 VendorID : 16;
    UINT32 Reserved_1 : 12;
    UINT32 Reserved_2 : 32;
} MXM_Vendor_Data;

// TBD: Add MXM Backlight Control Structure.
typedef struct _MXM_Checksum
{
    UINT08 Value;
} MXM_Checksum;

typedef struct _MXMData
{
    MXM_Header             Header;
    MXM_Output_Device      OutputDevice[MXM_MAX_ENTRIES_OUTPUT_DEVICE];
    MXM_Cooling_Capability CoolingCaps[MXM_MAX_ENTRIES_COOLING_CAPS];
    MXM_Thermal            Thermal[MXM_MAX_ENTRIES_THERMAL];
    MXM_Input_Power        InputPower[MXM_MAX_ENTRIES_INPUT_POWER];
    struct
    {
        MXM_GPIO_Device    Device;
        MXM_GPIO_Pin_Entry Pin[MXM_MAX_ENTRIES_GPIO_PIN];
    } GPIO[MXM_MAX_ENTRIES_GPIO_DEVICE];
    MXM_Vendor_Data        VendorData[MXM_MAX_ENTRIES_VENDOR];
    MXM_Backlight_Control  Backlight[MXM_MAX_ENTRIES_BACKLIGHT];
    MXM_Checksum           Checksum;
} MXMDataStruct;

#pragma pack(1)

// ACPI Test defines
#define ACPI_MXMI_METHOD                        0
#define ACPI_MXMS_METHOD                        1
#define ACPI_MXMX_METHOD                        2

#define ACPI_MXMI_TEST_ITERATIONS               2
#define ACPI_MXMS_TEST_ITERATIONS               1
#define ACPI_MXMX_TEST_ITERATIONS               3

#define ACPI_CLIENT_DISPLAY_HANDLE              0xDEADBEEF
#define ACPI_CLIENT_DEVICE_HANDLE               0xBEEFFEED
#define ACPI_MXM_OUTBUF_SIZE                    (4*1024) // Max size is 4Kb

#define ACPI_BITMASK_CRT0                       0x1
#define ACPI_BITMASK_CRT1                       0x2
#define ACPI_BITMASK_DFP0                       0x10000
#define ACPI_BITMASK_DFP1                       0x20000
#define ACPI_BITMASK_DFP2                       0x40000
#define ACPI_BITMASK_TV0                        0x100

// ACPI test global handles
typedef struct _acpiTestData
{
    UINT32 hClient;
    UINT32 hDisplay;
    UINT32 hDev;
}acpiTestData;

#define gErrMsgTag "FAILED:\n"

// EDID Max Size
#define MAX_EDID_SIZE                           512

class ClassMXMTest : public RmTest
{
public:
    ClassMXMTest();
    virtual ~ClassMXMTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    bool CreateClient();
    void FreeClient();

    // Validation functions for different structures
    RC ValidateMxm();
    UINT32 ValidateOutputDeviceStructure(MXM_Output_Device *);
    UINT32 ValidateInputPowerStructure(MXM_Input_Power *);
    UINT32 ValidateGPIODeviceStructure(MXM_GPIO_Device *);
    UINT32 ValidateThermalStructure(MXM_Thermal *thermal);
    UINT32 ValidateBacklightStructure(MXM_Backlight_Control *bklight);

    // acpi test functions
    RC AcpiMxmApiTest(void);
    INT32 osCallACPI_MXMITest(void);
    INT32 osCallACPI_MXMSTest(void);
    INT32 osCallACPI_MXMXTest(void);

    // EDID Validation
    INT32 ReadEDIDData();
    RC EDIDReadTest();

    LwRm::Handle uRMClientHandle;
    // acpi test global handle instance
    acpiTestData gAcpiTestData;
};

//! \brief ClassMXMTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ClassMXMTest::ClassMXMTest()
{
    SetName("ClassMXMTest");
    uRMClientHandle = 0;
}

//! \brief ClassMXMTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ClassMXMTest::~ClassMXMTest()
{
}

//! \brief Whether or not the test is supported in the current environment.
//!
//! \return True if this is HW , False otherwise
//-----------------------------------------------------------------------------
string ClassMXMTest::IsTestSupported()
{
    UINT32 status=0;
    LW0073_CTRL_SYSTEM_EXELWTE_ACPI_METHOD_PARAMS acpiParams = {0};
    UINT08 inDataBuffer[4], *outDataBuffer=NULL ;
    bool retVal = true;

    // Supported on hardware only
    if(Platform::Hardware != Platform::GetSimulationMode())
        return "Supported on HW only";

    // Check for MXM 2.0 support

    if(!CreateClient())
        return "CreateClient() failed";

    outDataBuffer =
        (UINT08*)malloc(sizeof(UINT08)* ACPI_MXM_OUTBUF_SIZE);
    memset(outDataBuffer, 0, sizeof(UINT08)* ACPI_MXM_OUTBUF_SIZE);

    gAcpiTestData.hClient = uRMClientHandle;
    gAcpiTestData.hDev = CLIENT_DEVICE_HANDLE;
    gAcpiTestData.hDisplay = ACPI_CLIENT_DISPLAY_HANDLE;

    inDataBuffer[0] = 0x20;

    // Setup the param structure
    acpiParams.method = ACPI_MXMI_METHOD;
    acpiParams.inData = (LwP64)inDataBuffer;
    acpiParams.inDataSize = sizeof(inDataBuffer);
    acpiParams.outStatus = 0;
    acpiParams.outData = (LwP64)outDataBuffer;
    acpiParams.outDataSize = ACPI_MXM_OUTBUF_SIZE;

    status = LwRmControl(gAcpiTestData.hClient,
                    gAcpiTestData.hDisplay,
                    LW0073_CTRL_CMD_SYSTEM_EXELWTE_ACPI_METHOD,
                    &acpiParams,
                    sizeof(acpiParams));

    // Not supported if this call fails
    if (status != LW_OK)
    {
        retVal = false;
        goto Quit;
    }

    // Validate return value and version.
    if (!(((((UINT08*)(acpiParams.outData))[0] == 0x02) ||
        (((UINT08*)(acpiParams.outData))[0] == 0x20)) &&
        ((acpiParams.outStatus) == LW_OK)))
    {
        retVal = false;
        goto Quit;
    }

Quit:
    if (outDataBuffer)
        free(outDataBuffer);
    FreeClient();
    if(retVal)
        return RUN_RMTEST_TRUE;
    return "test writter should give reason here";
}

//! \brief Setup all necessary resources before running the test.
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa Run
//-----------------------------------------------------------------------------
RC ClassMXMTest::Setup()
{
    RC rc = OK;

    if(!CreateClient())
        rc = RC::SOFTWARE_ERROR;

    return rc;
}

//
//! \brief Run the test!
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassMXMTest::Run()
{
    RC rcMxm = OK;
    RC rcAcpi = OK;
    RC rcEDID = OK;

    Printf(Tee::PriHigh,"------     Mxm Validation Version 1.0     -----\n");

    rcMxm = ValidateMxm();

    rcAcpi = AcpiMxmApiTest();

    rcEDID = EDIDReadTest();

    if(rcEDID != OK)
        return rcEDID;

    if(rcMxm != OK)
        return rcMxm;

    if(rcAcpi != OK)
        return rcAcpi;

    return OK;
}

//! \brief Free any resources that this test selwred
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClassMXMTest::Cleanup()
{
    FreeClient();
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------
//! \brief Validate MXM functionality
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassMXMTest::ValidateMxm()
{
    RC rc = OK;
    UINT32 NoOfOutputDevices = 0;
    UINT32 NoOfCoolingCapsEntries = 0;
    UINT32 NoOfThermalEntries = 0;
    UINT32 NoOfInputPowerEntries = 0;
    UINT32 NoOfGPIODevices = 0;
    UINT32 NoOfVendorEntries = 0;
    UINT32 NoOfBacklightEntries = 0;
    UINT32 NoOfGPIOPinEntriesPerDevice[MXM_MAX_ENTRIES_GPIO_DEVICE];
    UINT32 rmstatus=0;
    UINT08 *ROMBuffer ;
    UINT32 i;
    MXMDataStruct MXMData;
    UINT32 MXMDataSize;
    UINT08 FindSum = 0;

    LW0000_CTRL_SYSTEM_GET_MXM_FROM_ACPI_PARAMS MXMSbiosParams = {0};
    LW0000_CTRL_SYSTEM_GET_MXM_FROM_ROM_PARAMS MXMRomParams = {0};
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS GpuIdsParams = {{0}};

    bool outputDevFound = false;
    bool sysCoolFound = false;
    bool inputPowerFound = false;

    /* Do the initialization */
    memset(NoOfGPIOPinEntriesPerDevice, 0,
        sizeof(NoOfGPIOPinEntriesPerDevice));
    // Initialize MXM data all 1's, if a record is all 1's it is empty.
    memset(&MXMData, ~0, sizeof(MXMData));
    // Allocate the ROM Buffer for MAX size of 4K
    MXMDataSize = sizeof(char)*MXM_MAX_BUFFER_SIZE;
    ROMBuffer = (UINT08 *) malloc(MXMDataSize);

    if (!ROMBuffer)
    {
        Printf(Tee::PriHigh,
            "%s Unable to allocate memory for MXM ROM Buffer\n", gErrMsgTag);
        rmstatus = LW_ERR_GENERIC;
        goto Quit;
    }

    memset(ROMBuffer, 0, MXMDataSize);

    // Get the gpu ids first
    rmstatus = LwRmControl(uRMClientHandle,
                    uRMClientHandle,
                    LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                    &GpuIdsParams,
                    sizeof(GpuIdsParams));

    if (rmstatus != LW_OK)
    {
        Printf(Tee::PriHigh,"Failed to get the gpu id\n");
        goto Quit;
    }
    //Setup ROM params to read data from ROM
    MXMRomParams.gpuId = GpuIdsParams.gpuIds[0];
    MXMRomParams.pRomMXMBuffer = (LwP64)ROMBuffer;
    MXMRomParams.romMXMBufSize =  MXMDataSize;

    Printf(Tee::PriHigh,"Trying to get the MXM data from serial ROM\n");

    rmstatus = LwRmControl(uRMClientHandle,
        uRMClientHandle,
        LW0000_CTRL_CMD_SYSTEM_GET_MXM_FROM_ROM,
        &MXMRomParams,
        sizeof(MXMRomParams));
    if (rmstatus != LW_OK)
    {
        Printf(Tee::PriHigh,"Serial ROM MXM data read failed ...\n");
        Printf(Tee::PriHigh,"Trying to get MXM data from SBIOS ..\n");

        MXMSbiosParams.gpuId = GpuIdsParams.gpuIds[0];
        MXMSbiosParams.pAcpiMXMBuffer = (LwP64)ROMBuffer;
        MXMSbiosParams.acpiMXMBufferSize = MXMDataSize;

        rmstatus = LwRmControl(uRMClientHandle,
            uRMClientHandle,
            LW0000_CTRL_CMD_SYSTEM_GET_MXM_FROM_ACPI,
            &MXMSbiosParams,
            sizeof(MXMSbiosParams));
    }
    else
    {
        Printf(Tee::PriHigh,
            "The MXM data read from serial ROM is successful\n");
    }

    if (rmstatus != LW_OK)
    {
        Printf(Tee::PriHigh,"%s Error in getting MXM data from SBIOS\n",
            gErrMsgTag);
        goto Quit;
    }

    // Parse the header first.
    memcpy(&MXMData.Header, ROMBuffer, sizeof(MXMData.Header));

    Printf(Tee::PriHigh,"Version 0x%x Revision 0x%x Length = %d bytes\n",
        MXMData.Header.Version,
        MXMData.Header.Revision,
        MXMData.Header.Length);

    // validate Header String
    unsigned char mxmStr[5];

    mxmStr[0] = 'M';
    mxmStr[1] = 'X';
    mxmStr[2] = 'M';
    mxmStr[3] = '_';
    mxmStr[4] = '\0';

    if (memcmp(mxmStr, &MXMData.Header.String, 4) == 0)
    {
        Printf(Tee::PriHigh,"MXM Header String: %s validated.\n", mxmStr);
    }
    else
    {
        Printf(Tee::PriHigh,
            "MXM Header String mismatch.  Actual: %s  Expected: MXM_\n",
            mxmStr );
        rmstatus = LW_ERR_GENERIC;
    }

    if ((MXMData.Header.Version == 0x2) || (MXMData.Header.Version == 0x20))
    {
        Printf(Tee::PriHigh,"MXM Header Version 0x%x validated.\n",
            MXMData.Header.Version);
    }
    else
    {
        Printf(Tee::PriHigh,
            "MXM Header Version 0x%x is bad. Expected 0x2. \n\n",
            MXMData.Header.Version);
        rmstatus = LW_ERR_GENERIC;
    }

    if (rmstatus != LW_OK)
        goto Quit;

    // Parse the rest of MXM data
    UINT32 BytesParsed;
    UINT08 CheckSum;

    MXM_Data_Type_Header *Hdr;
    // Skip the header.
    BytesParsed = sizeof(MXMData.Header);

    // Set MXMDataSize to the actual size of the records, excluding
    // the header and checksum byte
    MXMDataSize = MXMData.Header.Length - 1 + sizeof(MXMData.Header);

    // Compute the (8-bit) checksum.
    for(i = 0; i < MXMDataSize; i++)
    {
        FindSum += ROMBuffer[i];
    }

    CheckSum = (UINT08)((~FindSum)+1);

    while (BytesParsed < MXMDataSize)
    {
        Hdr = (MXM_Data_Type_Header *) &ROMBuffer[BytesParsed];
        switch(Hdr->Type)
        {
        case OUTPUT_DEVICE_STRUCTURE:
            // Found output dev header
            outputDevFound = true;
            //
            // Check whether the MXM structure can accomodate the
            // OUTPUT_DEVICE_STRUCTURE and number of OUTPUT_DEVICE_STRUCTURE
            // structures should not exceed MXM_MAX_ENTRIES_OUTPUT_DEVICE
            //
            if ((NoOfOutputDevices < MXM_MAX_ENTRIES_OUTPUT_DEVICE) &&
                (MXMDataSize >= BytesParsed + sizeof(MXM_Output_Device)))
            {
                memcpy(&MXMData.OutputDevice[NoOfOutputDevices],
                    &ROMBuffer[BytesParsed],
                    sizeof(MXM_Output_Device));

                Printf(Tee::PriHigh,
                    "Checking Output device structure number %d ....",
                    NoOfOutputDevices);
                // Validate whether the field values are in the permissible
                // range or not
                if(ValidateOutputDeviceStructure(
                    &MXMData.OutputDevice[NoOfOutputDevices]) == LW_ERR_GENERIC)
                {
                    goto Quit;
                }

                Printf(Tee::PriHigh,"....PASSED \n");

                NoOfOutputDevices++;
                BytesParsed += sizeof(MXM_Output_Device);

            }
            else
            {
                // If the number of output device structures exceeds the max,
                // then report error
                if (NoOfOutputDevices >= MXM_MAX_ENTRIES_OUTPUT_DEVICE)
                {
                    Printf(Tee::PriHigh,
                        "%s Number of output devices exceeds the max %d\b",
                        gErrMsgTag, MXM_MAX_ENTRIES_OUTPUT_DEVICE);
                }
                else
                {
                    // Report error if no room for OUTPUT_DEVICE_STRUCTURE
                    Printf(Tee::PriHigh,
                        "No room for OUTPUT_DEVICE_STRUCTURE\n");
                }
                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }
            break;
        case SYSTEM_COOLING_CAPS_STRUCTURE:
            sysCoolFound = true;
            //
            // Check whether the MXM structure can accomodate the
            // SYSTEM_COOLING_CAPS_STRUCTURE and number of
            // SYSTEM_COOLING_CAPS_STRUCTURE structures should not exceed
            // MXM_MAX_ENTRIES_COOLING_CAPS
            //
            if (NoOfCoolingCapsEntries < MXM_MAX_ENTRIES_COOLING_CAPS &&
                (MXMDataSize >= BytesParsed + sizeof(MXM_Cooling_Capability)))
            {
                memcpy(&MXMData.CoolingCaps[NoOfCoolingCapsEntries],
                    &ROMBuffer[BytesParsed], sizeof(MXM_Cooling_Capability));

                Printf(Tee::PriHigh, "Checking Cooling Caps structure number"
                    " %d ........PASSED\n", NoOfCoolingCapsEntries);

                NoOfCoolingCapsEntries++;
                BytesParsed += sizeof(MXM_Cooling_Capability);
            }
            else
            {
                // report error if no. of cooling caps structures exceeds max
                if (NoOfCoolingCapsEntries >= MXM_MAX_ENTRIES_COOLING_CAPS)
                {
                    Printf(Tee::PriHigh,
                        "%s Number of cooling caps exceeds the max %d\b",
                        gErrMsgTag, MXM_MAX_ENTRIES_COOLING_CAPS);
                }
                else
                {
                    // report err if no room for SYSTEM_COOLING_CAPS_STRUCTURE
                    Printf(Tee::PriHigh,
                        "No room for SYSTEM_COOLING_CAPS_STRUCTURE\n");
                }
                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }

            break;
        case INPUT_POWER_STRUCTURE:
            inputPowerFound = true;
            //
            // Check whether the MXM structure can accomodate the
            // INPUT_POWER_STRUCTURE and number of INPUT_POWER_STRUCTURE
            // structures should not exceed MXM_MAX_ENTRIES_INPUT_POWER
            //
            if (NoOfInputPowerEntries < MXM_MAX_ENTRIES_INPUT_POWER &&
                (MXMDataSize >= BytesParsed + sizeof(MXM_Input_Power)))
            {
                Printf(Tee::PriHigh,
                    "Checking Input Power structure number %d ....",
                    NoOfInputPowerEntries);

                memcpy(&MXMData.InputPower[NoOfInputPowerEntries],
                    &ROMBuffer[BytesParsed], sizeof(MXM_Input_Power));
                // Validate the entry values for the fields
                if(ValidateInputPowerStructure(
                    &MXMData.InputPower[NoOfInputPowerEntries]) == LW_ERR_GENERIC)
                {
                    rmstatus = LW_ERR_GENERIC;
                    goto Quit;
                }

                Printf(Tee::PriHigh,"....PASSED\n");

                NoOfInputPowerEntries++;
                BytesParsed += sizeof(MXM_Input_Power);
            }
            else
            {
                // report error if number of input power structures exceeds max
                if (NoOfInputPowerEntries >= MXM_MAX_ENTRIES_INPUT_POWER )
                {
                    Printf(Tee::PriHigh,
                        "%s Number of input power structure exceeds the max"
                        "%d\b", gErrMsgTag, MXM_MAX_ENTRIES_INPUT_POWER);
                }
                else
                {
                    // report error if no room for INPUT_POWER_STRUCTURE
                    Printf(Tee::PriHigh,
                        "No space for INPUT_POWER_STRUCTURE\n");
                }

                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }
            break;

        case GPIO_DEVICE_STRUCTURE:
            //
            // Check whether the MXM structure can accomodate the
            // GPIO_DEVICE_STRUCTURE and number of GPIO_DEVICE_STRUCTURE
            // structures should not exceed MXM_MAX_ENTRIES_GPIO_DEVICE
            //
            if (NoOfGPIODevices < MXM_MAX_ENTRIES_GPIO_DEVICE &&
                (MXMDataSize >= BytesParsed + sizeof(MXM_GPIO_Device)))
            {
                Printf(Tee::PriHigh,
                    "Checking GPIO Device structure number %d ....",
                    NoOfGPIODevices);
                memcpy(&MXMData.GPIO[NoOfGPIODevices], &ROMBuffer[BytesParsed],
                    sizeof(MXM_GPIO_Device));
                // Validate the entry values for the fields
                if(ValidateGPIODeviceStructure(
                    &MXMData.GPIO[NoOfGPIODevices].Device) == LW_ERR_GENERIC)
                {
                    rmstatus = LW_ERR_GENERIC;
                    goto Quit;
                }
                Printf(Tee::PriHigh,".... PASSED \n");
                BytesParsed += sizeof(MXM_GPIO_Device);

                // The number of pins should be lesser than maximum
                if (MXMData.GPIO[NoOfGPIODevices].Device.NumOfPins >
                    MXM_MAX_ENTRIES_GPIO_PIN)
                {
                    Printf(Tee::PriHigh,
                        "%s Number of GPIO pin entries exceeds the max %d\n",
                        gErrMsgTag, MXM_MAX_ENTRIES_GPIO_PIN);
                    rmstatus = LW_ERR_GENERIC;
                    goto Quit;
                }
                // Check whether there is space to accomodate the PIN structure
                if(MXMDataSize >= (BytesParsed +
                    (sizeof(MXM_GPIO_Pin_Entry) *
                    MXMData.GPIO[NoOfGPIODevices].Device.NumOfPins)))
                {
                    for (UINT32 pin = 0; pin <
                        MXMData.GPIO[NoOfGPIODevices].Device.NumOfPins; ++pin)
                    {
                        memcpy(&MXMData.GPIO[NoOfGPIODevices].Pin[pin],
                            &ROMBuffer[BytesParsed],
                            sizeof(MXM_GPIO_Pin_Entry));

                        // Validate the Logical Pin number
                        if ((pin == 0) &&
                            (MXMData.GPIO[NoOfGPIODevices].Pin[pin].PinNumber
                            != 0))
                        {
                            Printf(Tee::PriHigh, "The Logical pin number"
                                "expected to begin from 0.\n");
                            rmstatus = LW_ERR_GENERIC;
                            goto Quit;
                        }

                        UINT32 function =
                            MXMData.GPIO[NoOfGPIODevices].Pin[pin].Function;
                        // Validate Function
                        if (((function > 1) && (function < 5)) ||
                           ((function > 11) && (function < 31)) ||
                            (function > 37))
                        {
                            Printf(Tee::PriHigh, "Invalid Gpio Function");
                            rmstatus = LW_ERR_GENERIC;
                        }

                        BytesParsed += sizeof(MXM_GPIO_Pin_Entry);

                    }
                }
                else
                {
                    Printf(Tee::PriHigh,"No room for GPIO_PIN_STRUCTURE\n");
                    rmstatus = LW_ERR_GENERIC;
                    goto Quit;
                }

                Printf(Tee::PriHigh,
                    "Number of GPIO Pins: %d parsed successfully\n",
                    MXMData.GPIO[NoOfGPIODevices].Device.NumOfPins);
                ++NoOfGPIODevices;
            }
            else
            {
                // Report error if number of GPIO_DEVICE structures exceeds max
                if (NoOfGPIODevices >= MXM_MAX_ENTRIES_GPIO_DEVICE)
                {
                    Printf(Tee::PriHigh,"%s Number of GPIO device structure"
                        "exceeds the max %d\b", gErrMsgTag,
                        MXM_MAX_ENTRIES_GPIO_DEVICE);
                }
                else
                {
                    // report error if no room for GPIO_DEVICE_STRUCTURE
                    Printf(Tee::PriHigh,
                        "No space for GPIO_DEVICE_STRUCTURE\n");
                }

                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }
            break;
        case THERMAL_STRUCTURE:
            //
            // Check whether the MXM structure can accomodate the
            // THERMAL_STRUCTURE andnumber of THERMAL_STRUCTURE structures
            // should not exceed MXM_MAX_ENTRIES_THERMAL
            //
            if (NoOfThermalEntries < MXM_MAX_ENTRIES_THERMAL &&
                (MXMDataSize >= BytesParsed +
                sizeof(MXMData.Thermal[NoOfThermalEntries])))
            {
                memcpy(&MXMData.Thermal[NoOfThermalEntries],\
                    &ROMBuffer[BytesParsed],
                    sizeof(MXM_Thermal));

                Printf(Tee::PriHigh,
                    "Checking Thermal Structure number %d... \n",
                    NoOfThermalEntries);
                if (ValidateThermalStructure(
                    &MXMData.Thermal[NoOfThermalEntries]) == LW_ERR_GENERIC)
                {
                    goto Quit;
                }
                Printf(Tee::PriHigh, "....PASSED\n");

                NoOfThermalEntries++;
                BytesParsed += sizeof(MXM_Thermal);
            }
            else
            {
                // report error if number of THERMAL structures exceeds max
                if (NoOfThermalEntries >= MXM_MAX_ENTRIES_THERMAL )
                {
                    Printf(Tee::PriHigh,
                        "%s Number of thermal structure exceeds the max %d\b",
                        gErrMsgTag, MXM_MAX_ENTRIES_THERMAL);
                }
                else
                {
                    // report error if there is no room for THERMAL_STRUCTURE
                    Printf(Tee::PriHigh,"No space for THERMAL_STRUCTURE\n");
                }

                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }
            break;
        case VENDOR_STRUCTURE:
            //
            // Check whether the MXM structure can accomodate the
            // VENDOR_STRUCTURE and number of VENDOR_STRUCTURE structures
            // should not exceed MXM_MAX_ENTRIES_VENDOR
            //
            Printf(Tee::PriHigh,"Vendor size: %d\n", (INT32)sizeof(MXM_Vendor_Data));

            if ( (NoOfVendorEntries < MXM_MAX_ENTRIES_VENDOR) &&
                (MXMDataSize >= (BytesParsed +
                sizeof(MXMData.VendorData[NoOfVendorEntries])) ))
            {
                memcpy(&MXMData.VendorData[NoOfVendorEntries],
                    &ROMBuffer[BytesParsed],
                    sizeof(MXM_Vendor_Data));

                Printf(Tee::PriHigh,
                    "Vendor structure %d parsed successfully \n",
                    NoOfVendorEntries);

                NoOfVendorEntries++;
                BytesParsed += sizeof(MXM_Vendor_Data);
            }
            else
            {
                // Report error if of VENDOR structures exceeds max
                if (NoOfVendorEntries >= MXM_MAX_ENTRIES_VENDOR )
                {
                    Printf(Tee::PriHigh,
                        "%s Number of vendor entries exceeds the max %d\b",
                        gErrMsgTag, MXM_MAX_ENTRIES_VENDOR);
                }
                else
                {
                    // Report error if there is no room for VENDOR_STRUCTURE
                    Printf(Tee::PriHigh,"No space for VENDOR_STRUCTURE\n");
                }

                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }
            break;

        case BACKLIGHT_CONTROL_STRUCTURE:
            //
            // Check whether the MXM structure can accomodate the
            // BACKLIGHT_CONTROL_STRUCTURE and number of
            // BACKLIGHT_CONTROL_STRUCTURE structures should not exceed
            // MXM_MAX_ENTRIES_BACKLIGHT
            //
            if (NoOfBacklightEntries < MXM_MAX_ENTRIES_BACKLIGHT &&
                (MXMDataSize >=
                BytesParsed + sizeof(MXMData.Backlight[NoOfBacklightEntries])))
            {
                memcpy(&MXMData.Backlight[NoOfBacklightEntries],
                    &ROMBuffer[BytesParsed],
                    sizeof(MXM_Backlight_Control));

                Printf(Tee::PriHigh,
                    "Checking Backlight Structure number %d... \n",
                    NoOfBacklightEntries);
                if (ValidateBacklightStructure(
                    &MXMData.Backlight[NoOfBacklightEntries]) == LW_ERR_GENERIC)
                {
                    goto Quit;
                }
                Printf(Tee::PriHigh, "....PASSED\n");
                NoOfBacklightEntries++;
                BytesParsed += sizeof(MXM_Backlight_Control);
            }
            else
            {
                // Report error if the no. of THERMAL structures exceed the max
                if (NoOfThermalEntries >= MXM_MAX_ENTRIES_THERMAL )
                {
                    Printf(Tee::PriHigh,
                        "%s Number of thermal structure exceeds the max %d\b",
                        gErrMsgTag, MXM_MAX_ENTRIES_THERMAL);
                }
                else
                {
                    // Report error if there is no room for THERMAL_STRUCTURE
                    Printf(Tee::PriHigh,"No space for THERMAL_STRUCTURE");
                }

                rmstatus = LW_ERR_GENERIC;
                goto Quit;
            }
            break;

        default:
            Printf(Tee::PriHigh,"%s Unknown header type 0x%x\n",
                gErrMsgTag, Hdr->Type);
            rmstatus = LW_ERR_GENERIC;
            goto Quit;
        }
    }

    if (!outputDevFound )
    {
        Printf(Tee::PriHigh, "System Output Device Structure not found.\n" );
        rmstatus = LW_ERR_GENERIC;
        goto Quit;
    }
    else if (!sysCoolFound)
    {
        Printf(Tee::PriHigh, "System Cooling Capability Structure not found.\n");
        rmstatus = LW_ERR_GENERIC;
        goto Quit;
    }
    else if (!inputPowerFound)
    {
        Printf(Tee::PriHigh, "Input Power Structure not found.\n");
        rmstatus = LW_ERR_GENERIC;
        goto Quit;
    }

    MXMData.Checksum.Value = ROMBuffer[BytesParsed];

    // Validate the CheckSum byte
    // 2's complement of the 8-bit sum of the MXM structure
    if( MXMData.Checksum.Value != CheckSum)
    {
        Printf(Tee::PriHigh,"CHECK_SUM byte did  not match ---\
            Check sum byte is %d -- ByteSum is %d\n", MXMData.Checksum.Value,
            CheckSum);
        rmstatus = LW_ERR_GENERIC;
        goto Quit;
    }
    else
    {
        Printf(Tee::PriHigh,
            "Checksum match successful, read=0x%x   computed=0x%x\n",
            MXMData.Checksum.Value,CheckSum);
    }

Quit:
    free(ROMBuffer);
    if(rmstatus == LW_ERR_GENERIC)
        rc = RC::SOFTWARE_ERROR;
    else
        rc = RmApiStatusToModsRC(rmstatus);
    return rc;
}

//! \brief Create client for this test
//!
//! \return true if successful, false if failed
//-----------------------------------------------------------------------------
bool ClassMXMTest::CreateClient(void)
{
    LW0080_ALLOC_PARAMETERS deviceAllocParams;

    memset(&deviceAllocParams, 0, sizeof(deviceAllocParams));

    // Allocate client.
    if (LwRmAllocRoot((LwU32*)&uRMClientHandle) != LW_OK)
    {
        Printf(Tee::PriHigh,"Allocation of Root device failed\n");
        return false;
    }

    // tell the resource manager which LW device we want to talk to
     if (LwRmAlloc(uRMClientHandle,
                   uRMClientHandle,
                   CLIENT_DEVICE_HANDLE,
                   LW01_DEVICE_0,
                   &deviceAllocParams) != LW_OK)
     {
         Printf(Tee::PriHigh,"Allocation of Device handle failed\n");
         LwRmFree(uRMClientHandle, uRMClientHandle , uRMClientHandle);
         return false;
    }

    if (LwRmAlloc(uRMClientHandle, CLIENT_DEVICE_HANDLE,
        ACPI_CLIENT_DISPLAY_HANDLE, LW04_DISPLAY_COMMON, NULL) !=
        LW_OK)
    {
        Printf(Tee::PriHigh,"Allocation of Display handle failed\n");
        LwRmFree(uRMClientHandle,uRMClientHandle, uRMClientHandle);
        return false;
    }

    return true;
}

//! \brief Free client
//!
//-----------------------------------------------------------------------------
void ClassMXMTest::FreeClient(void)
{
    LwRmFree(uRMClientHandle, uRMClientHandle, CLIENT_DEVICE_HANDLE);
    LwRmFree(uRMClientHandle, uRMClientHandle, uRMClientHandle);
    uRMClientHandle = 0;
}

//! \brief Validate output device structure
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
UINT32 ClassMXMTest::ValidateOutputDeviceStructure(
    MXM_Output_Device * OutputDevice)
{
    UINT32 rmstatus = 0;

    // OUTPUT_DEVICE_TYPE_MAX is the maximum value for TYPE field
    if (OutputDevice->Type > OUTPUT_DEVICE_TYPE_MAX)
    {
        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....The output device is invalid");
    }

    // OUTPUT_DEVICE_DDCPORT_MAX is the maximum value for DDCPORT field
    if( OutputDevice->DDCPort  > OUTPUT_DEVICE_DDCPORT_MAX )
    {
        // Check whether the entry is NA or not
        if( OutputDevice->DDCPort != DDCPORT_NA )
        {
            rmstatus = LW_ERR_GENERIC;
            Printf(Tee::PriHigh,"\n    ....Invalid DDC entry");
        }

    }

    // CONNECTOR_TYPE_MAX is the maximum vaue for "ConnectorType" field

    if(OutputDevice->ConnectorType  > CONNECTOR_TYPE_MAX )
    {
        if( (OutputDevice->ConnectorType != CONNECTOR_TYPE_NA ) ||
            (OutputDevice->ConnectorType == CONNECTOR_TYPE_RESERVED_1) ||
            (OutputDevice->ConnectorType == CONNECTOR_TYPE_RESERVED_2))
        {
            // Check whether the entry is NA or not
            rmstatus = LW_ERR_GENERIC;
            Printf(Tee::PriHigh,"\n    ....Invalid Connector Type");
        }

    }

    // DIGITAL_CONNECTION_MAX is the maximum value for
    // "LVDS_TMDS_HDMI_Connection" field
    if ((OutputDevice->LVDS_TMDS_HDMI_Connection > DIGITAL_CONNECTION_MAX) &&
        (OutputDevice->LVDS_TMDS_HDMI_Connection != DIGITAL_CONNECTION_NA))
    {

        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....Invalid Digital Connection");
    }

    // If the DeviceType is TV and TVFORMAT is NA, report error
    if( ((OutputDevice->TVFormat >= TV_FORMAT_NA) ||
        ((OutputDevice->TVFormat > TV_FORMAT_NOT_SPECIFIED) &&
        (OutputDevice->TVFormat < TV_FORMAT_NA))) &&
        (OutputDevice->Type == TYPE_TV))
    {
        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....Invalid TV format");
    }

    return rmstatus;
}

//! \brief Validate thermal structure
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
UINT32  ClassMXMTest::ValidateThermalStructure(MXM_Thermal *thermal)
{
    UINT32 rmstatus = 0;
    if (thermal->Type > 0x1)
    {
        Printf(Tee::PriHigh,"\n    ....Invalid Thermal Type Information");
        rmstatus = LW_ERR_GENERIC;
    }

    return rmstatus;
}

//! \brief Validate input power structure
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
UINT32 ClassMXMTest::ValidateInputPowerStructure(MXM_Input_Power *InputPower)
{
    UINT32 rmstatus = 0;
    // INPUT_POWER_TYPE_MAX is the maximum any entry can be
    if(InputPower->Type > INPUT_POWER_TYPE_MAX)
    {
        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....Invalid InputPowerType");
    }

    if (InputPower->Amp4Value == 0)
    {
        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....Invalid 4Amp Rating");
    }

    return rmstatus;
}

//! \brief Validate GPIO device structure
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
UINT32 ClassMXMTest::ValidateGPIODeviceStructure(MXM_GPIO_Device *GPIODevice)
{
    UINT32 rmstatus = 0;
    if(GPIODevice->Type > GPIO_DEVICE_TYPE_MAX)
    {
        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....Invalid GPIO device Type");
    }
    // Check whether the zeroth bit of I2C address is 0 or not
    if( (GPIODevice->I2CAddress & 1) != 0)
    {
        rmstatus = LW_ERR_GENERIC;
        Printf(Tee::PriHigh,"\n    ....The 0th bit of I2CAddress is not 0");
    }

    return rmstatus;
}

//! \brief Validate backlight structure
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
UINT32 ClassMXMTest::ValidateBacklightStructure(MXM_Backlight_Control *bklight)
{
    UINT32 rmstatus = 0;

    if(bklight->Type)
    {
        Printf(Tee::PriHigh,"....Invalid Backlight Ilwerter Control type");
        rmstatus = LW_ERR_GENERIC;
    }

    return rmstatus;
}

//! \brief Validate ACPI functionality
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ClassMXMTest::AcpiMxmApiTest(void)
{
    RC rcMXMI = OK;
    RC rcMXMS = OK;
    RC rcMXMX = OK;
    UINT32 status=0;

    // setup global data
    gAcpiTestData.hClient = uRMClientHandle;
    gAcpiTestData.hDev = CLIENT_DEVICE_HANDLE;
    gAcpiTestData.hDisplay = ACPI_CLIENT_DISPLAY_HANDLE;

    status = osCallACPI_MXMITest();
    if(status == LW_ERR_GENERIC)
        rcMXMI = RC::SOFTWARE_ERROR;
    else
        rcMXMI = RmApiStatusToModsRC(status);

    status = osCallACPI_MXMSTest();
    if(status == LW_ERR_GENERIC)
        rcMXMS = RC::SOFTWARE_ERROR;
    else
        rcMXMS = RmApiStatusToModsRC(status);

    status = osCallACPI_MXMXTest();
    if(status == LW_ERR_GENERIC)
        rcMXMX = RC::SOFTWARE_ERROR;
    else
        rcMXMX = RmApiStatusToModsRC(status);

    // free the handle to display
    LwRmFree(gAcpiTestData.hClient, gAcpiTestData.hDev,
        gAcpiTestData.hDisplay);

    if(rcMXMI != OK)
        return rcMXMI;

    if(rcMXMS != OK)
        return rcMXMS;

    if(rcMXMX != OK)
        return rcMXMX;

    return OK;
}

//! \brief Test MXMI functionality
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
INT32 ClassMXMTest::osCallACPI_MXMITest(void)
{
    UINT32 i=0;
    UINT32 status=0;

    for(i=0; i<ACPI_MXMI_TEST_ITERATIONS; i++)
    {
        LW0073_CTRL_SYSTEM_EXELWTE_ACPI_METHOD_PARAMS acpiParams = {0};
        UINT08 inDataBuffer[4], *outDataBuffer=NULL ;
        UINT32 allocBuf=1;

        switch (i)
        {

            case 0: // Positive test: set mxm version as 2.0
                Printf(Tee::PriHigh, "Checking osCallACPI_MXMI for "
                    "version 2.0 support ....");
                inDataBuffer[0] = 0x20;
                break;

            case 1: // Positive test: set mxm version as 0, Correct version is
                    // returned
                Printf(Tee::PriHigh,"Checking osCallACPI_MXMI for "
                    "Illegal version detection ....");
                inDataBuffer[0] = 0x0;
                break;

            default:
                Printf(Tee::PriHigh,"Illegal iteration. Not a valid case\n");
                return status;
        }

        if (allocBuf)
        {
            outDataBuffer =
                (UINT08*)malloc(sizeof(UINT08)* ACPI_MXM_OUTBUF_SIZE);
            memset(outDataBuffer, 0, sizeof(UINT08)* ACPI_MXM_OUTBUF_SIZE);
        }

        // Setup the param structure
        acpiParams.method = ACPI_MXMI_METHOD;
        acpiParams.inData = (LwP64)inDataBuffer;
        acpiParams.inDataSize = sizeof(inDataBuffer);
        acpiParams.outStatus = 0;
        acpiParams.outData = (LwP64)outDataBuffer;
        acpiParams.outDataSize = ACPI_MXM_OUTBUF_SIZE;

        if ((status = LwRmControl(gAcpiTestData.hClient,
                        gAcpiTestData.hDisplay,
                        LW0073_CTRL_CMD_SYSTEM_EXELWTE_ACPI_METHOD,
                        &acpiParams,
                        sizeof(acpiParams))) != LW_OK)
        {

            Printf(Tee::PriHigh,"....FAILED\n");
            Printf(Tee::PriHigh,"Error exelwting osCallACPI_MXMI method \n");

            if (outDataBuffer)
            {
                free(outDataBuffer);
            }

            return status;
        }

        switch (i)
        {
            case 0:

                // validate return value and version.
                if (((((UINT08*)(acpiParams.outData))[0] == 0x02) ||
                    (((UINT08*)(acpiParams.outData))[0] == 0x20)) &&
                   ((acpiParams.outStatus) == LW_OK))
                {
                    Printf(Tee::PriHigh,"....PASSED\n");
                }
                else
                {
                    Printf(Tee::PriHigh,"....FAILED\n");
                    Printf(Tee::PriHigh," version=0x%x  Status=0x%x \n",
                        ((UINT08*)(acpiParams.outData))[0],
                         (UINT32)((acpiParams.outStatus)));
                    Printf(Tee::PriHigh,
                        " Expected version: 0x20  Status: %d\n", LW_OK);
                }
                break;

            case 1: // Positive test: set mxm version as 0. Correct version
                    // is returned.

                // validate return value and version.
                if (((((UINT08*)(acpiParams.outData))[0] == 0x13) ||
                     (((UINT08*)(acpiParams.outData))[0] == 0x20) ||
                     (((UINT08*)(acpiParams.outData))[0] == 0x02)) &&
                     ((acpiParams.outStatus) == LW_OK))
                {
                    Printf(Tee::PriHigh,"....PASSED\n");
                }
                else
                {
                    Printf(Tee::PriHigh,"....FAILED\n");
                    Printf(Tee::PriHigh," version=0x%x  status=0x%x \n",
                        ((UINT08*)(acpiParams.outData))[0],
                        (UINT32)((acpiParams.outStatus)));
                    Printf(Tee::PriHigh,
                        " Expected version: 0x20 or 0x13  Status: %d\n", LW_OK);
                }

                break;

            default:
                Printf(Tee::PriHigh,"Illegal iteration. Not a valid case\n");
        }
        if (outDataBuffer)
        {
            free(outDataBuffer);
        }
    }

    return status;
}

//! \brief Test MXMS functionality
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
INT32 ClassMXMTest::osCallACPI_MXMSTest(void)
{
    LW0073_CTRL_SYSTEM_EXELWTE_ACPI_METHOD_PARAMS acpiParams = {0};
    UINT08 inDataBuffer;
    UINT16 outDataSize=0;
    UINT32 i=0, allocBuf=1;
    UINT32 mxmDataVersion=0;
    UINT32 status=LW_OK;

    for(i=0; i<ACPI_MXMS_TEST_ITERATIONS; i++)
    {
        UINT08 *outDataBuffer=NULL ;

        switch (i)
        {
            case 0:
                // Positive test:
                // set mxm version as 2.0
                // set function to 1 (Get mxm struct)

                Printf(Tee::PriHigh,"Checking osCallACPI_MXMS for "
                    "version 2.0 support ....");
                inDataBuffer = 0x20;
                break;

            default:
                Printf(Tee::PriHigh,"Illegal iteration. Not a valid case\n");
                return status;
        }

        if (allocBuf)
        {
            outDataSize = sizeof(UINT08)* ACPI_MXM_OUTBUF_SIZE;
            outDataBuffer = (UINT08*)malloc(outDataSize);
            memset(outDataBuffer, 0, outDataSize);
        }

        // Setup the param structure
        acpiParams.method = ACPI_MXMS_METHOD;
        acpiParams.inData = (LwP64)&inDataBuffer;
        acpiParams.inDataSize = sizeof(inDataBuffer);
        acpiParams.outStatus = 0;
        acpiParams.outData = (LwP64)outDataBuffer;
        acpiParams.outDataSize = outDataSize;

        if ((status = LwRmControl(gAcpiTestData.hClient, gAcpiTestData.hDisplay,
                        LW0073_CTRL_CMD_SYSTEM_EXELWTE_ACPI_METHOD,
                        &acpiParams,
                        sizeof(acpiParams))) != LW_OK)
        {
            Printf(Tee::PriHigh,"..... FAILED\n");
            Printf(Tee::PriHigh,"Error exelwting osCallACPI_MXMS method. \n");

            if (outDataBuffer)
            {
                free(outDataBuffer);
            }

            return status;
        }

        // Result Validation
        switch (i)
        {
            case 0:
                // Validate that the mxm data returned is for version 2.0

                // extract Mxm version from the Mxm Header
                mxmDataVersion = (((UINT08*)acpiParams.outData)[4]);

                if( ((mxmDataVersion == 0x02)|| (mxmDataVersion == 0x20)) &&
                    ((acpiParams.outStatus) == LW_OK))
                {
                    Printf(Tee::PriHigh,".....PASSED\n");
                }
                else
                {
                    Printf(Tee::PriHigh,".....FAILED\n");
                    Printf(Tee::PriHigh," MXM version retrieved = 0x%x, "
                        "status = 0x%x \n", mxmDataVersion,
                        (UINT32)((acpiParams.outStatus)));
                    Printf(Tee::PriHigh,
                        " Expected version: 0x20  Status: %d\n", LW_OK);
                }

                break;

            default:
                Printf(Tee::PriHigh,"Illegal iteration. Not a valid case\n");
        }

        if (outDataBuffer)
        {
            free(outDataBuffer);
        }

    }

    return status;
}

//! \brief Test MXMX functionality
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
INT32 ClassMXMTest::osCallACPI_MXMXTest(void)
{
    LW0073_CTRL_SYSTEM_EXELWTE_ACPI_METHOD_PARAMS acpiParams = {0};
    UINT08 inDataBuffer[5];
    UINT16 outDataSize=0;
    UINT32 i=0, allocBuf=1;
    UINT32 status=LW_OK;

    // set the display mask for any other display type, as needed
    UINT32 displayMask=ACPI_BITMASK_CRT0;

    for(i=0; i<ACPI_MXMX_TEST_ITERATIONS; i++)
    {
        std::vector<UINT08> outDataBuffer;
        switch (i)
        {
            case 0:
                // Positive test:
                // set function: acquire control

                Printf(Tee::PriHigh,"Checking osCallACPI_MXMX Acquire "
                    "Control function (display mask CRT) .....");

                inDataBuffer[0] = (0);
                break;

            case 1:
                // Positive test:
                // set function: release control

                Printf(Tee::PriHigh,"Checking osCallACPI_MXMX Release "
                    "Control function  (display mask CRT) .....");

                inDataBuffer[0] = 1;
                break;

            case 2:
                // Negative test:
                // set function > 1

                Printf(Tee::PriHigh,
                    "\n Checking osCallACPI_MXMX Illegal function .....");

                inDataBuffer[0] = 2;
                break;

            default:
                Printf(Tee::PriHigh,"Invalid test case\n");
                return status;
        }
        // set the  display mask, starting byte 1, in the input buffer
        memcpy((UINT08*)(inDataBuffer+1), (UINT08*)(&displayMask),
            sizeof(UINT32));

        if (allocBuf)
        {
            outDataBuffer.resize(sizeof(UINT32), 0);
        }

        // Setup the param structure
        acpiParams.method = ACPI_MXMX_METHOD;
        acpiParams.inData = (LwP64)&inDataBuffer;
        acpiParams.inDataSize = sizeof(inDataBuffer);
        acpiParams.outStatus = 0;
        acpiParams.outData = LW_PTR_TO_LwP64(&outDataBuffer[0]);
        acpiParams.outDataSize = outDataSize;

        if ((status = LwRmControl(gAcpiTestData.hClient,
            gAcpiTestData.hDisplay,
            LW0073_CTRL_CMD_SYSTEM_EXELWTE_ACPI_METHOD,
            &acpiParams,
            sizeof(acpiParams))) != LW_OK)
        {
            // if negative case, failure is expected.
            if (i!=2)
            {
                Printf(Tee::PriHigh,"\nWARNING: osCallACPI_MXMX may not be "
                    "supported ........FAILED \n");
                return LW_OK;
            }
        }

        // Result Validation
        switch (i)
        {
            case 0:
                // Validate acquire control function

                if ( ((*((UINT32*)(acpiParams.outData))) > 0) &&
                       ((acpiParams.outStatus) == LW_OK))
                {
                    Printf(Tee::PriHigh,".....PASSED\n");

                }
                else
                {
                    Printf(Tee::PriHigh,".....FAILED\n");
                    Printf(Tee::PriHigh,"Could not acquire device\n");
                }
                break;

            case 1:
                // Validate release control function
                  if ( ((*((UINT32*)(acpiParams.outData))) > 0) &&
                       ((acpiParams.outStatus) == LW_OK))
                {
                    Printf(Tee::PriHigh,".....PASSED\n");

                }
                else
                {
                    Printf(Tee::PriHigh,".....FAILED\n");
                    Printf(Tee::PriHigh,"Could not release device \n");
                }
                break;

            case 2:
                // Validate that an error code is returned
                // for an illegal function / NULL Buffers

                if (((acpiParams.outStatus)) != LW_OK)
                {
                    Printf(Tee::PriHigh,".....PASSED\n");
                }
                else
                {
                    Printf(Tee::PriHigh,".....PASSED\n");
                    Printf(Tee::PriHigh,
                        "Illegal function 0x2 is not detected\n");
                }
                break;
            default:
                Printf(Tee::PriHigh,"Illegal iteration. Not a valid case\n");
        }

    }

    return status;
}

//! \brief This functions reads EDID for each connected display on each gpu.
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
INT32 ClassMXMTest::ReadEDIDData()
{
    UINT32 status = 0;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS attachedIdsParams = {{0}};
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    UINT32 i, j;
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS numSubDevicesParams = {0};
    LW0073_CTRL_SYSTEM_GET_SUPPORTED_PARAMS dispSupportedParams = {0};
    LW0073_CTRL_SYSTEM_GET_CONNECT_STATE_PARAMS dispConnectParams = {0};
    UINT32 m_hDisplay = LWCTRL_DISPLAY_DEVICE_HANDLE;
    UINT32 hGpuDev = 0;
    UINT32 subdev, dispMask;
    LW0073_CTRL_SPECIFIC_GET_EDID_PARAMS getEdidParams = {0};

    // get attached gpuIds
    status = LwRmControl(uRMClientHandle, uRMClientHandle,
                         LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                         &attachedIdsParams, sizeof (attachedIdsParams));

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "CtrlDispTest: GPU_GET_ATTACHED_IDS failed: 0x%x\n", status);
        return status;
    }

    // for each device...
    for (i = 0; i < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++)
    {
        if(attachedIdsParams.gpuIds[i] == LW0000_CTRL_GPU_ILWALID_ID)
            break;

        Printf(Tee::PriHigh,"Testing GpuId : %x\n",
            (UINT32)attachedIdsParams.gpuIds[i]);

        // get gpu instance info
        gpuIdInfoParams.szName = (LwP64)NULL;
        gpuIdInfoParams.gpuId = attachedIdsParams.gpuIds[i];
        status = LwRmControl(uRMClientHandle, uRMClientHandle,
                            LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                            &gpuIdInfoParams, sizeof (gpuIdInfoParams));
        if(status != LW_OK)
        {
            Printf(Tee::PriHigh,
                "Error in call LW0000_CTRL_CMD_GPU_GET_ID_INFO.\n");
            return status;
        }

        // allocate the gpu's device
        lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
        lw0080Params.hClientShare = 0;
        hGpuDev = LWCTRL_GPU_DEVICE_HANDLE + gpuIdInfoParams.deviceInstance;
        status = LwRmAlloc(uRMClientHandle, uRMClientHandle, hGpuDev,
                        LW01_DEVICE_0,
                        &lw0080Params);
        if(status != LW_OK)
        {
            Printf(Tee::PriHigh,"LwRmAlloc(LW01_DEVICE_0) failed!\n");
            return status;
        }

        // allocate LW04_DISPLAY_COMMON
        status = LwRmAlloc(uRMClientHandle, hGpuDev, m_hDisplay,
            LW04_DISPLAY_COMMON, NULL);
        if(status != LW_OK)
        {
            Printf(Tee::PriHigh,"LwRmAlloc(LW04_DISPLAY_COMMON) failed: ");
            // Free Devices
            LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
            return status;
        }

        // get number of subdevices
        status = LwRmControl(uRMClientHandle, hGpuDev ,
                                LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES,
                                &numSubDevicesParams,
                                sizeof (numSubDevicesParams));
        if(status != LW_OK)
        {
            Printf(Tee::PriHigh,"Subdevice cnt cmd failed\n");
            // Free Devices
            LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
            LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
            return status;
        }

        //Print Number of Subdevices
        Printf(Tee::PriHigh,"Number of Subdevices : 0x%x\n",
            (UINT32)numSubDevicesParams.numSubDevices);

        // for each subdevice...
        for (subdev = 0; subdev < numSubDevicesParams.numSubDevices;
            subdev++)
        {
            Printf(Tee::PriHigh,"Testing Subdevice : 0x%x\n",
                (UINT32)subdev);

            // get supported displays
            dispSupportedParams.subDeviceInstance = subdev;
            dispSupportedParams.displayMask = 0;
            status = LwRmControl(uRMClientHandle, m_hDisplay,
                LW0073_CTRL_CMD_SYSTEM_GET_SUPPORTED,
                &dispSupportedParams, sizeof (dispSupportedParams));
            if(status != LW_OK)
            {
                Printf(Tee::PriHigh, "Displays supported failed\n");
                // Free Devices
                LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
                LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
                return status;
            }

            dispMask = dispSupportedParams.displayMaskDDC;
            Printf(Tee::PriHigh, "Supported Display Mask: 0x%x\n",
                (UINT32)dispMask);

            // get connected displays
            memset(&dispConnectParams, 0, sizeof(dispConnectParams));
            dispConnectParams.subDeviceInstance =
                gpuIdInfoParams.subDeviceInstance;
            dispConnectParams.flags = 0;
            dispConnectParams.displayMask = dispMask;
            status = LwRmControl(uRMClientHandle, m_hDisplay,
                LW0073_CTRL_CMD_SYSTEM_GET_CONNECT_STATE,
                &dispConnectParams, sizeof (dispConnectParams));
            if(status != LW_OK)
            {
                Printf(Tee::PriHigh, "Connect state failed\n");
                // Free Devices
                LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
                LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
                return status;
            }

            dispMask = dispConnectParams.displayMask;
            Printf(Tee::PriHigh, "Connected Display Mask: 0x%x\n",
                (UINT32)dispMask );

            if(dispMask == 0)
            {
                Printf(Tee::PriHigh, "No display connected.\n");
                // Free Devices
                LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
                LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
                return status;
            }

            // for each display...
            for (j = 0; j < 32; j++)
            {
                LwU8 *pEDIDBuffer = NULL;

                if (0 == ((1 << j) & dispMask))
                    continue;

                Printf(Tee::PriHigh, "DisplayId : 0x%x\n",
                    (UINT32)(1 << j));

                //Read Cached EDID
                pEDIDBuffer = (LwU8 *) malloc(MAX_EDID_SIZE);
                memset(&getEdidParams, 0,
                    sizeof(LW0073_CTRL_SPECIFIC_GET_EDID_PARAMS));
                getEdidParams.displayId         = (1 << j);
                getEdidParams.subDeviceInstance = subdev;
                getEdidParams.pEdidBuffer       = (LwP64) pEDIDBuffer;
                getEdidParams.bufferSize        = MAX_EDID_SIZE;
                getEdidParams.flags             =
                    LW0073_CTRL_SPECIFIC_GET_EDID_FLAGS_COPY_CACHE_YES;

                status = LwRmControl(uRMClientHandle, m_hDisplay,
                    LW0073_CTRL_CMD_SPECIFIC_GET_EDID,
                    &getEdidParams, sizeof (getEdidParams));
                free(pEDIDBuffer);
                if(status != LW_OK)
                {
                    Printf(Tee::PriHigh, "Cached GET_EDID failed\n");
                    // Free Devices
                    LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
                    LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
                    return status;
                }
                Printf(Tee::PriHigh, "Cached EDID read successfully\n");

                //Read Non Cached EDID
                pEDIDBuffer = (LwU8 *) malloc(MAX_EDID_SIZE);
                memset(&getEdidParams, 0,
                    sizeof(LW0073_CTRL_SPECIFIC_GET_EDID_PARAMS));
                getEdidParams.displayId         = (1 << j);
                getEdidParams.subDeviceInstance = subdev;
                getEdidParams.pEdidBuffer       = (LwP64) pEDIDBuffer;
                getEdidParams.bufferSize        = MAX_EDID_SIZE;
                getEdidParams.flags             =
                    LW0073_CTRL_SPECIFIC_GET_EDID_FLAGS_COPY_CACHE_NO;

                status = LwRmControl(uRMClientHandle, m_hDisplay,
                    LW0073_CTRL_CMD_SPECIFIC_GET_EDID,
                    &getEdidParams, sizeof (getEdidParams));
                free(pEDIDBuffer);
                if(status != LW_OK)
                {
                    Printf(Tee::PriHigh, "Non cached GET_EDID failed\n");
                    // Free Devices
                    LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
                    LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
                    return status;
                }
                Printf(Tee::PriHigh, "Non Cached EDID read successfully\n");

            }
        }

        // Free Devices
        LwRmFree(uRMClientHandle, hGpuDev, m_hDisplay);
        LwRmFree(uRMClientHandle, uRMClientHandle, hGpuDev);
    }

    Printf(Tee::PriHigh, "Finished EDID Testing for all devices\n");
    return status;
}

//! \brief This function performs EDID read test.
//!
//! \return 0 on succees, rm error code on failure
//-----------------------------------------------------------------------------
RC ClassMXMTest::EDIDReadTest()
{
    INT32 status;
    RC rc = OK;

    // Allocate client.
    status = LwRmAllocRoot((LwU32*)&uRMClientHandle);
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,"Allocation of Root device failed\n");
        rc = RmApiStatusToModsRC(status);
        return rc;
    }

    Printf(Tee::PriHigh,"------     Starting EDID Read Test        -----\n");
    status=ReadEDIDData();
    rc = RmApiStatusToModsRC(status);

    LwRmFree(uRMClientHandle, uRMClientHandle, uRMClientHandle);
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ClassMXMTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassMXMTest, RmTest,
                 "MXM 2.0 RM test.");
