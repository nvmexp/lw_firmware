/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "extech42570.h"
#include "core/include/serial.h"
#include "core/include/tasker.h"

const UINT16 START_KEY_VALUE = 0xFFFF;
const UINT08 STOP_KEY_VALUE = 0xAA;
const UINT08 FLAGS_FAHRENHEIT = 0x1;

#pragma pack(1)
struct Extech42570Packet
{
    UINT16 StartKey;
    UINT08 Emissivity;
    UINT08 IRTempH; // IR = Infra-Red measurement
    UINT08 IRTempL;
    UINT08 TKTempH; // TK - Thermocouple (when attached) measurement
    UINT08 TKTempL;
    UINT08 MaxTempH;
    UINT08 MaxTempL;
    UINT08 MinTempH;
    UINT08 MinTempL;
    UINT08 DifferenceTempH;
    UINT08 DifferenceTempL;
    UINT08 AverageTempH;
    UINT08 AverageTempL;
    UINT08 Flags;
    UINT08 StopKey;
};
#pragma pack()

Extech42570::~Extech42570()
{
    for (UINT32 instrIdx = 0; instrIdx < m_Instruments.size(); instrIdx++)
    {
        m_Instruments[instrIdx]->Uninitialize(SerialConst::CLIENT_IRTHERMOMETER);
    }
}

static bool SearchForExtech42570Packet
(
    const UINT08 *rawBuffer,
    UINT32 size,
    const Extech42570Packet **packet
)
{
    if (size < sizeof(Extech42570Packet))
        return false;

    for (UINT32 packetStartIdx = 0;
        packetStartIdx < (1 + size - sizeof(Extech42570Packet));
        packetStartIdx++)
    {
        *packet = (const Extech42570Packet *)&rawBuffer[packetStartIdx];
        if (((*packet)->StartKey == START_KEY_VALUE) &&
            ((*packet)->StopKey == STOP_KEY_VALUE))
        {
            return true;
        }
    }

    return false;
}

static void ReadToRawBuffer
(
    Serial *pCom,
    UINT08 *rawBuffer,
    UINT32 size,
    UINT32 *readBytes
)
{
    *readBytes = 0;

    while (*readBytes < size)
    {
        UINT32 byte;
        if (OK != pCom->Get(SerialConst::CLIENT_IRTHERMOMETER, &byte))
        {
            break;
        }
        rawBuffer[*readBytes] = byte;
        (*readBytes)++;
    }
}

RC Extech42570::FindInstruments(UINT32 *numInstruments)
{
    RC rc;

    *numInstruments = 0;

    for (UINT32 portIdx = 1; portIdx <= Serial::GetHighestComPort(); portIdx++)
    {
        Serial *pCom = GetSerialObj::GetCom(portIdx);

        RC iniRC = pCom->Initialize(SerialConst::CLIENT_IRTHERMOMETER, true);
        if (iniRC != OK)
        {
            pCom->Uninitialize(SerialConst::CLIENT_IRTHERMOMETER);
            continue;
        }

        CHECK_RC(pCom->SetBaud(SerialConst::CLIENT_IRTHERMOMETER, 9600));

        Tasker::Sleep(120); // The instrument makes a measurement every 100ms
                            // "120" should guarantee to receive at least one
                            // packet

        if (pCom->ReadBufCount() == 0)
        {
            pCom->Uninitialize(SerialConst::CLIENT_IRTHERMOMETER);
            continue;
        }

        UINT08 rawBuffer[3*sizeof(Extech42570Packet)];
        UINT32 readBytes = 0;

        Tasker::Sleep(100); // Extra "100" should guarantee to receive total of
                            // at least two packets as the first one is
                            // sometimes corrupted
                            // [probably after the baud switch]

        ReadToRawBuffer(pCom, rawBuffer, sizeof(rawBuffer), &readBytes);
        const Extech42570Packet *packet = nullptr;
        if (!SearchForExtech42570Packet(rawBuffer, readBytes, &packet))
        {
            pCom->Uninitialize(SerialConst::CLIENT_IRTHERMOMETER);
            continue;
        }

        m_Instruments.push_back(pCom);
    }

    *numInstruments = (UINT32)m_Instruments.size();

    return OK;

}

RC Extech42570::MeasureTemperature(UINT32 instrumentIdx, double *tempDegC)
{
    RC rc;

    if (instrumentIdx >= m_Instruments.size())
        return RC::SOFTWARE_ERROR;

    Serial *pCom = m_Instruments[instrumentIdx];

    CHECK_RC(pCom->ClearBuffers(SerialConst::CLIENT_IRTHERMOMETER));

    Tasker::Sleep(150); // The instrument makes a measurement every 100ms
                        // "150" should guarantee to receive at least one

    UINT08 rawBuffer[2*sizeof(Extech42570Packet)];
    UINT32 readBytes = 0;

    ReadToRawBuffer(pCom, rawBuffer, sizeof(rawBuffer), &readBytes);
    const Extech42570Packet *packet = nullptr;
    if (!SearchForExtech42570Packet(rawBuffer, readBytes, &packet))
    {
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    *tempDegC = ( (packet->IRTempH<<8) + packet->IRTempL ) / 10.0;

    if (packet->Flags & FLAGS_FAHRENHEIT)
        *tempDegC = 5.0/9.0 * (*tempDegC - 32.0);

    return OK;
}
