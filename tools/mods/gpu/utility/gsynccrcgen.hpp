/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * This file was automatically generated using:
 *
 * /home/lwtools/engr/2016/07/10_05_00_08/lwtools/bin/x86_64Linux/crcgen 16 24 -poly X^16+X^5+X^3+X^2+1 -c -name LWODCrc16_24Gen
 *
 * This module contains a combinatorial 16-bit CRC calculator.
 * The serial implementation of polynomial X^16+X^5+X^3+X^2+1 is:
 *
 *     crcOut[15] = crcIn[0] ^ dIn;
 *     crcOut[14] = crcIn[15];
 *     crcOut[13] = crcIn[14] ^ crcIn[0] ^ dIn;
 *     crcOut[12] = crcIn[13] ^ crcIn[0] ^ dIn;
 *     crcOut[11] = crcIn[12];
 *     crcOut[10] = crcIn[11] ^ crcIn[0] ^ dIn;
 *     crcOut[9] = crcIn[10];
 *     crcOut[8] = crcIn[9];
 *     crcOut[7] = crcIn[8];
 *     crcOut[6] = crcIn[7];
 *     crcOut[5] = crcIn[6];
 *     crcOut[4] = crcIn[5];
 *     crcOut[3] = crcIn[4];
 *     crcOut[2] = crcIn[3];
 *     crcOut[1] = crcIn[2];
 *     crcOut[0] = crcIn[1];
 *
 * This module contains the parallel implementation, which is
 * derived by cycling the serial implementation 24 times and
 * aclwmulating terms.  The total number of required XOR gates
 * is 105 with a maximum XOR-tree depth of 3.
 */
#define CRCWIDTH    16
#define DATAWIDTH    24

void LWODCrc16_24Gen(const UINT08 crcIn[], const UINT08 dIn[], UINT08 crcOut[])
{
    crcOut[15] = dIn[23] ^ crcIn[12] ^ dIn[12] ^ crcIn[10] ^ dIn[10] ^ crcIn[9] ^ dIn[9] ^ crcIn[7] ^ dIn[7] ^ crcIn[1] ^ dIn[1];

    crcOut[14] = dIn[22] ^ crcIn[11] ^ dIn[11] ^ crcIn[9] ^ dIn[9] ^ crcIn[8] ^ dIn[8] ^ crcIn[6] ^ dIn[6] ^ crcIn[0] ^ dIn[0];

    crcOut[13] = dIn[23] ^ dIn[21] ^ crcIn[12] ^ dIn[12] ^ crcIn[9] ^ dIn[9] ^ crcIn[8] ^ dIn[8] ^ crcIn[5] ^ dIn[5] ^ crcIn[1] ^ dIn[1];

    crcOut[12] = dIn[23] ^ dIn[22] ^ dIn[20] ^ crcIn[12] ^ dIn[12] ^ crcIn[11] ^ dIn[11] ^ crcIn[10] ^ dIn[10] ^ crcIn[9] ^ dIn[9] ^ crcIn[8] ^ dIn[8] ^ crcIn[4] ^ dIn[4] ^ crcIn[1] ^ dIn[1] ^ crcIn[0] ^ dIn[0];

    crcOut[11] = dIn[22] ^ dIn[21] ^ dIn[19] ^ crcIn[11] ^ dIn[11] ^ crcIn[10] ^ dIn[10] ^ crcIn[9] ^ dIn[9] ^ crcIn[8] ^ dIn[8] ^ crcIn[7] ^ dIn[7] ^ crcIn[3] ^ dIn[3] ^ crcIn[0] ^ dIn[0];

    crcOut[10] = dIn[23] ^ dIn[21] ^ dIn[20] ^ dIn[18] ^ crcIn[12] ^ dIn[12] ^ crcIn[8] ^ dIn[8] ^ crcIn[6] ^ dIn[6] ^ crcIn[2] ^ dIn[2] ^ crcIn[1] ^ dIn[1];

    crcOut[9] = dIn[22] ^ dIn[20] ^ dIn[19] ^ dIn[17] ^ crcIn[11] ^ dIn[11] ^ crcIn[7] ^ dIn[7] ^ crcIn[5] ^ dIn[5] ^ crcIn[1] ^ dIn[1] ^ crcIn[0] ^ dIn[0];

    crcOut[8] = dIn[21] ^ dIn[19] ^ dIn[18] ^ dIn[16] ^ crcIn[10] ^ dIn[10] ^ crcIn[6] ^ dIn[6] ^ crcIn[4] ^ dIn[4] ^ crcIn[0] ^ dIn[0];

    crcOut[7] = dIn[20] ^ dIn[18] ^ dIn[17] ^ crcIn[15] ^ dIn[15] ^ crcIn[9] ^ dIn[9] ^ crcIn[5] ^ dIn[5] ^ crcIn[3] ^ dIn[3];

    crcOut[6] = dIn[19] ^ dIn[17] ^ dIn[16] ^ crcIn[14] ^ dIn[14] ^ crcIn[8] ^ dIn[8] ^ crcIn[4] ^ dIn[4] ^ crcIn[2] ^ dIn[2];

    crcOut[5] = dIn[18] ^ dIn[16] ^ crcIn[15] ^ dIn[15] ^ crcIn[13] ^ dIn[13] ^ crcIn[7] ^ dIn[7] ^ crcIn[3] ^ dIn[3] ^ crcIn[1] ^ dIn[1];

    crcOut[4] = dIn[17] ^ crcIn[15] ^ dIn[15] ^ crcIn[14] ^ dIn[14] ^ crcIn[12] ^ dIn[12] ^ crcIn[6] ^ dIn[6] ^ crcIn[2] ^ dIn[2] ^ crcIn[0] ^ dIn[0];

    crcOut[3] = dIn[16] ^ crcIn[14] ^ dIn[14] ^ crcIn[13] ^ dIn[13] ^ crcIn[11] ^ dIn[11] ^ crcIn[5] ^ dIn[5] ^ crcIn[1] ^ dIn[1];

    crcOut[2] = crcIn[15] ^ dIn[15] ^ crcIn[13] ^ dIn[13] ^ crcIn[12] ^ dIn[12] ^ crcIn[10] ^ dIn[10] ^ crcIn[4] ^ dIn[4] ^ crcIn[0] ^ dIn[0];

    crcOut[1] = crcIn[14] ^ dIn[14] ^ crcIn[12] ^ dIn[12] ^ crcIn[11] ^ dIn[11] ^ crcIn[9] ^ dIn[9] ^ crcIn[3] ^ dIn[3];

    crcOut[0] = crcIn[13] ^ dIn[13] ^ crcIn[11] ^ dIn[11] ^ crcIn[10] ^ dIn[10] ^ crcIn[8] ^ dIn[8] ^ crcIn[2] ^ dIn[2];

}
#undef CRCWIDTH
#undef DATAWIDTH
