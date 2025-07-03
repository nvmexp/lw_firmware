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

#include "gputest.h"
#include "core/include/utility.h"
#include "dynamicdisplaymuxhelper.h"

class PanelFirmwareVerif : public GpuTest
{
public:
    PanelFirmwareVerif();
    RC Setup() override;
    RC Run() override;
    bool IsSupported() override;

    SETGET_PROP(PanelChecksumsFilePath, string);

private:
    Display *m_pDisplay = nullptr;
    unique_ptr<DynamicDisplayMuxHelper> m_pDynamicDisplayMuxHelper;
    map<pair<UINT16, UINT16>, vector<UINT16>> m_PanelChecksumMap;
    string m_PanelChecksumsFilePath = "PanelChecksums.txt";
    DisplayID m_EDPDisplayId = 0;

    RC CheckDDSCapability(bool *pCapable);
    RC ParseInputChecksumFile();
    RC ParseEntry(string& entry);
    RC SetEDP();
    RC VerifyChecksum(UINT16 manfId, UINT16 prodId, UINT16 checksum);
};
