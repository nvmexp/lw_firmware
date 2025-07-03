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

#pragma once

#include <vector>
#include <cmath>

typedef unsigned int UINT32;

// CRC Callwlation algorithm: https://p4hw-swarm.lwpu.com/reviews/51222008/v1/
void crc(UINT32 crcIn[], UINT32 dataBin[], UINT32 crcOut[])
{

    crcOut[15] = dataBin[31] ^ dataBin[30] ^ dataBin[29] ^ dataBin[28] ^ dataBin[27] ^ dataBin[26]
               ^ dataBin[25] ^ dataBin[24] ^ dataBin[23] ^ dataBin[22] ^ dataBin[21] ^ dataBin[20]
               ^ dataBin[19] ^ dataBin[18] ^ dataBin[16] ^ crcIn[15] ^ dataBin[15] ^ crcIn[14]
               ^ dataBin[14] ^ crcIn[13] ^ dataBin[13] ^ crcIn[12] ^ dataBin[12] ^ crcIn[11]
               ^ dataBin[11] ^ crcIn[10] ^ dataBin[10] ^ crcIn[9] ^ dataBin[9] ^ crcIn[8]
               ^ dataBin[8] ^ crcIn[7] ^ dataBin[7] ^ crcIn[6] ^ dataBin[6] ^ crcIn[5] ^ dataBin[5]
               ^ crcIn[4] ^ dataBin[4] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    crcOut[14] = dataBin[30] ^ dataBin[29] ^ dataBin[28] ^ dataBin[27] ^ dataBin[26] ^ dataBin[25]
               ^ dataBin[24] ^ dataBin[23] ^ dataBin[22] ^ dataBin[21] ^ dataBin[20] ^ dataBin[19]
               ^ dataBin[18] ^ dataBin[17] ^ crcIn[15] ^ dataBin[15] ^ crcIn[14] ^ dataBin[14]
               ^ crcIn[13] ^ dataBin[13] ^ crcIn[12] ^ dataBin[12] ^ crcIn[11] ^ dataBin[11]
               ^ crcIn[10] ^ dataBin[10] ^ crcIn[9] ^ dataBin[9] ^ crcIn[8] ^ dataBin[8] ^ crcIn[7]
               ^ dataBin[7] ^ crcIn[6] ^ dataBin[6] ^ crcIn[5] ^ dataBin[5] ^ crcIn[4] ^ dataBin[4]
               ^ crcIn[3] ^ dataBin[3] ^ crcIn[0] ^ dataBin[0];

    crcOut[13] = dataBin[31] ^ dataBin[30] ^ dataBin[17] ^ crcIn[15] ^ dataBin[15] ^ crcIn[3]
               ^ dataBin[3] ^ crcIn[2] ^ dataBin[2] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0]
               ^ dataBin[0];

    crcOut[12] = dataBin[30] ^ dataBin[29] ^ dataBin[16] ^ crcIn[14] ^ dataBin[14] ^ crcIn[2]
               ^ dataBin[2] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    crcOut[11] = dataBin[29] ^ dataBin[28] ^ crcIn[15] ^ dataBin[15] ^ crcIn[13] ^ dataBin[13]
               ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];

    crcOut[10] = dataBin[28] ^ dataBin[27] ^ crcIn[14] ^ dataBin[14] ^ crcIn[12] ^ dataBin[12]
               ^ crcIn[0] ^ dataBin[0];

    crcOut[9] = dataBin[27] ^ dataBin[26] ^ crcIn[13] ^ dataBin[13] ^ crcIn[11] ^ dataBin[11];

    crcOut[8] = dataBin[26] ^ dataBin[25] ^ crcIn[12] ^ dataBin[12] ^ crcIn[10] ^ dataBin[10];

    crcOut[7] = dataBin[25] ^ dataBin[24] ^ crcIn[11] ^ dataBin[11] ^ crcIn[9] ^ dataBin[9];

    crcOut[6] = dataBin[24] ^ dataBin[23] ^ crcIn[10] ^ dataBin[10] ^ crcIn[8] ^ dataBin[8];

    crcOut[5] = dataBin[23] ^ dataBin[22] ^ crcIn[9] ^ dataBin[9] ^ crcIn[7] ^ dataBin[7];

    crcOut[4] = dataBin[22] ^ dataBin[21] ^ crcIn[8] ^ dataBin[8] ^ crcIn[6] ^ dataBin[6];

    crcOut[3] = dataBin[21] ^ dataBin[20] ^ crcIn[7] ^ dataBin[7] ^ crcIn[5] ^ dataBin[5];

    crcOut[2] = dataBin[20] ^ dataBin[19] ^ crcIn[6] ^ dataBin[6] ^ crcIn[4] ^ dataBin[4];

    crcOut[1] = dataBin[19] ^ dataBin[18] ^ crcIn[5] ^ dataBin[5] ^ crcIn[3] ^ dataBin[3];

    crcOut[0] = dataBin[31] ^ dataBin[30] ^ dataBin[29] ^ dataBin[28] ^ dataBin[27] ^ dataBin[26]
              ^ dataBin[25] ^ dataBin[24] ^ dataBin[23] ^ dataBin[22] ^ dataBin[21] ^ dataBin[20]
              ^ dataBin[19] ^ dataBin[17] ^ dataBin[16] ^ crcIn[15] ^ dataBin[15] ^ crcIn[14]
              ^ dataBin[14] ^ crcIn[13] ^ dataBin[13] ^ crcIn[12] ^ dataBin[12] ^ crcIn[11]
              ^ dataBin[11] ^ crcIn[10] ^ dataBin[10] ^ crcIn[9] ^ dataBin[9] ^ crcIn[8]
              ^ dataBin[8] ^ crcIn[7] ^ dataBin[7] ^ crcIn[6] ^ dataBin[6] ^ crcIn[5] ^ dataBin[5]
              ^ crcIn[2] ^ dataBin[2] ^ crcIn[1] ^ dataBin[1] ^ crcIn[0] ^ dataBin[0];
}

UINT32* i2b(UINT32 n)
{
    UINT32 *m = new UINT32[32];
    for (int i = 31; i >= 0; i--)
    {
        *(m+i) = (((n>>i) & 1));
    }

    return m;
}

UINT32 callwlateCRC(vector<UINT32> iffrecords)
{
    UINT32* crc_in = new UINT32[16];
    UINT32* crc_out = new UINT32[16];
    UINT32 output = 0;
    for (UINT32 i = 0; i < 16; i++)
    {
        crc_in[i] = 0;
    }

    for (auto i = iffrecords.begin(); i < iffrecords.end(); i++)
    {
        UINT32* data_bin=new UINT32[32];
        data_bin=i2b(*i);
        crc(crc_in, data_bin, crc_out);
        for (auto j = 0; j < 16; j++)
        {
            crc_in[j] = crc_out[j];
        }
    }
    //colwert results to int
    for (auto i=0; i<16;i++)
    {
        output += crc_out[i] * pow(2, i);
    }

    return output;
}