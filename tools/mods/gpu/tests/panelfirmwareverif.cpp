/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "panelfirmwareverif.h"
#include "core/include/display.h"
#include "ctrl/ctrl0073.h"
#include "gpu/include/gpudev.h"
#include <boost/algorithm/string.hpp>

PanelFirmwareVerif::PanelFirmwareVerif()
{
    SetName("PanelFirmwareVerif");
}

bool PanelFirmwareVerif::IsSupported()
{
    bool capable;

    if (CheckDDSCapability(&capable) != RC::OK)
    {
        //TODO: Call CheckDDSCapability in Setup to fail the test and check further.
        return true;
    }

    return capable;
}

RC PanelFirmwareVerif::Setup()
{
    RC rc;

    bool capable;
    CHECK_RC(CheckDDSCapability(&capable));

    CHECK_RC(ParseInputChecksumFile());

    CHECK_RC(GpuTest::Setup());

    // Allocate display
    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetBoundGpuDevice()->GetDisplay();

    m_pDynamicDisplayMuxHelper = make_unique<DynamicDisplayMuxHelper>(GetBoundGpuDevice(),
                m_pDisplay, GetVerbosePrintPri());

    CHECK_RC(SetEDP());

    return rc;
}

RC PanelFirmwareVerif::Run()
{
    RC rc;

    CHECK_RC(m_pDynamicDisplayMuxHelper->InitAcpiIdMap(m_EDPDisplayId));

    UINT16 manfId = 0x0;
    UINT16 prodId = 0x0;
    CHECK_RC(m_pDynamicDisplayMuxHelper->GetManfProdId(m_EDPDisplayId, &manfId, &prodId));

    // Initialize Mux
    CHECK_RC(m_pDynamicDisplayMuxHelper->InitializeMux(m_EDPDisplayId, manfId, prodId));

    LW0073_CTRL_CMD_DFP_DISPMUX_GET_TCON_INFO_PARAMS params = {};
    params.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();
    params.displayId = m_EDPDisplayId;

    CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_DFP_DISPMUX_GET_TCON_INFO,
                (void*)&params, sizeof(params)));

    CHECK_RC(VerifyChecksum(manfId, prodId, params.softwareRev));

    return rc;
}

RC PanelFirmwareVerif::CheckDDSCapability(bool *pCapable)
{
    RC rc;
    MASSERT(pCapable);

    *pCapable = false;

    LW0073_CTRL_CMD_SYSTEM_CHECK_SIDEBAND_I2C_SUPPORT_PARAMS i2cSupportParams = { };
    i2cSupportParams.subDeviceInstance = GetBoundGpuSubdevice()->GetSubdeviceInst();

    CHECK_RC(GetBoundGpuDevice()->GetDisplay()->RmControl(LW0073_CTRL_CMD_SYSTEM_CHECK_SIDEBAND_I2C_SUPPORT,
                &i2cSupportParams, sizeof (i2cSupportParams)));

    *pCapable = i2cSupportParams.bIsSidebandI2cSupported;

    return rc;
}

RC PanelFirmwareVerif::ParseInputChecksumFile()
{
    RC rc;

    if (!Xp::DoesFileExist(m_PanelChecksumsFilePath))
    {
        Printf(Tee::PriError, "%s file does not exists\n", m_PanelChecksumsFilePath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    string fileContents;
    CHECK_RC(Xp::InteractiveFileRead(m_PanelChecksumsFilePath.c_str(), &fileContents));

    if (fileContents.empty())
    {
        Printf(Tee::PriError, "%s file is empty\n", m_PanelChecksumsFilePath.c_str());
        return RC::ILWALID_INPUT;
    }
    vector<string> fileLines;
    CHECK_RC(Utility::Tokenizer(fileContents, "\n", &fileLines));

    for (auto line : fileLines)
    {
        CHECK_RC(ParseEntry(line));
    }

    return rc;
}

RC PanelFirmwareVerif::ParseEntry(string& entry)
{
    RC rc;

    boost::algorithm::trim(entry);
    if (entry.empty() || entry[0] == '#')
    {
        return rc;
    }

    vector<string> words;
    CHECK_RC(Utility::Tokenizer(entry, " ", &words));
    if (words.size() != 3)
    {
        Printf(Tee::PriError, "Incorrect format for checksum entry\n");
        return RC::ILWALID_INPUT;
    }

    UINT16 manfId = static_cast<UINT16>(Utility::ColwertStrToHex(words[0]));
    UINT16 prodId = static_cast<UINT16>(Utility::ColwertStrToHex(words[1]));

    vector<string> checksums;
    CHECK_RC(Utility::Tokenizer(words[2], ",", &checksums));

    vector<UINT16> panelChecksum;
    for (auto checksum : checksums)
    {
        panelChecksum.push_back(static_cast<UINT16>(Utility::ColwertStrToHex(checksum)));
    }

    if (m_PanelChecksumMap.find(make_pair(manfId, prodId)) != m_PanelChecksumMap.end())
    {
        Printf(Tee::PriError, "Entry already exists for Manufacturer ID = 0x%x, Product ID = 0x%x\n", manfId, prodId);
        return RC::ILWALID_INPUT;
    }

    m_PanelChecksumMap[make_pair(manfId, prodId)] = panelChecksum;

    return rc;
}

RC PanelFirmwareVerif::SetEDP()
{
    RC rc;

    DisplayIDs connectors;
    CHECK_RC(GetBoundGpuDevice()->GetDisplay()->GetConnectors(&connectors));

    const UINT32 subdeviceInst = GetBoundGpuSubdevice()->GetSubdeviceInst();

    for (const auto& displayID : connectors)
    {
        LW0073_CTRL_SPECIFIC_GET_CONNECTOR_DATA_PARAMS dataParams = { };
        dataParams.subDeviceInstance = subdeviceInst;
        dataParams.displayId = displayID;
        CHECK_RC(GetBoundGpuDevice()->GetDisplay()->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_CONNECTOR_DATA,
                    &dataParams, sizeof (dataParams)));

        if (dataParams.data[0].type == LW0073_CTRL_SPECIFIC_CONNECTOR_DATA_TYPE_DP_INT)
        {
            m_EDPDisplayId = displayID;
            break;
        }
    }

    if (!m_EDPDisplayId)
    {
        Printf(Tee::PriError, "EDP Display not found\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return rc;
}

RC PanelFirmwareVerif::VerifyChecksum(UINT16 manfId, UINT16 prodId, UINT16 checksum)
{
    if (m_PanelChecksumMap.find(make_pair(manfId, prodId)) == m_PanelChecksumMap.end())
    {
        Printf(Tee::PriError, "Entry not found for Manufacturer ID = 0x%x, Product ID = 0x%x\n", manfId, prodId);
        return RC::GOLDEN_VALUE_NOT_FOUND;
    }

    vector<UINT16> panelChecksum = m_PanelChecksumMap[make_pair(manfId, prodId)];
    if (find(panelChecksum.begin(), panelChecksum.end(), checksum) == panelChecksum.end())
    {
        Printf(Tee::PriError, "Panel Checksum mismatch for Manufacturer ID = 0x%x, Product ID = 0x%x\n,"
                "Expected Value: 0x%x\n", manfId, prodId, checksum);
        return RC::GOLDEN_VALUE_MISCOMPARE;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(PanelFirmwareVerif, GpuTest, "PanelFirmwareVerif");

CLASS_PROP_READWRITE(PanelFirmwareVerif, PanelChecksumsFilePath, string,
    "Input Panel Checksums file path");

//-----------------------------------------------------------------------------
