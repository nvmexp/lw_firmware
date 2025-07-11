/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TESTVECTORECDSAP256SIGNING_H
#define TESTVECTORECDSAP256SIGNING_H

#define SIZE_IN_BYTES_ECDSA_P256 32

#if 1
    /* Test vector obtained from : https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/dolwments/dss/186-4ecdsatestvectors.zip (See file SigGen.txt, and the first one under "[P-256,SHA-256]".
       Should give a PASS. */

    /*
        All values are in BIG ENDIAN.

        --------------
        Message input:
        --------------
        Hex Msg (MESSAGE TO BE SIGNED) = 5905238877c77421f73e43ee3da6f2d9e2ccad5fc942dcec0cbd25482935faaf416983fe165b1a045ee2bcd2e6dca3bdf46c4310a7461f9a37960ca672d3feb5473e253605fb1ddfd28065b53cb5858a8ad28175bf9bd386a5e471ea7a65c17cc934a9d791e91491eb3754d03799790fe2d308d16146d5c9b0d0debd97d79ce8
        MsgHash (SHA-256 HASH OF ABOVE MESSAGE) = 44acf6b7e36c1342c2c5897204fe09504e1e2efb1a900377dbc4e7a6a133ec56
        Tool used to get hash of hex message : https://emn178.github.io/online-tools/sha256.html (Make sure to select 'Hex' from input type dropdown)

        ----------
        Key input:
        ----------
        d (PRIVATE KEY) = 519b423d715f8b581f4fa8ee59f4771a5b44c8130b4e3eacca54a56dda72b464

        Public key (not used for signing):
        Qx (PUBLIC KEY X COORDINATE) = 1ccbe91c075fc7f4f033bfa248db8fccd3565de94bbfb12f3c59ff46c271bf83
        Qy (PUBLIC KEY Y COORDINATE) = ce4014c68811f9a21a1fdb2c0e6113e06db7ca93b7404e78dc7ccd5ca89a4ca9

        --------------
        Random number: (picked for colwenience and to compare with final result)
        --------------
        k (RANDOM K FACTOR FOR SIGNING) = 94a1bbb14b906a61a280f245f9e93c7f3b4a6247824f5d33b9670787642a68de

        -------
        Output:
        -------
        R (SIGNATURE COMPONENT R) = f3ac8061b514795b8843e3d6629527ed2afd6b1f6a555a7acabb5e6f79c8c2ac
        S (SIGNATURE COMPONENT S) = 8bf77819ca05a6b2786c76262bf7371cef97b218e96f175a3ccdda2acc058903
    */

    uint8_t msgHash[SIZE_IN_BYTES_ECDSA_P256] = {
        0x44, 0xac, 0xf6, 0xb7, 0xe3, 0x6c, 0x13, 0x42, 0xc2, 0xc5, 0x89, 0x72, 0x04, 0xfe, 0x09, 0x50, 
        0x4e, 0x1e, 0x2e, 0xfb, 0x1a, 0x90, 0x03, 0x77, 0xdb, 0xc4, 0xe7, 0xa6, 0xa1, 0x33, 0xec, 0x56,
    };

    uint8_t privateKey[SIZE_IN_BYTES_ECDSA_P256] = {
        0x51, 0x9b, 0x42, 0x3d, 0x71, 0x5f, 0x8b, 0x58, 0x1f, 0x4f, 0xa8, 0xee, 0x59, 0xf4, 0x77, 0x1a,
        0x5b, 0x44, 0xc8, 0x13, 0x0b, 0x4e, 0x3e, 0xac, 0xca, 0x54, 0xa5, 0x6d, 0xda, 0x72, 0xb4, 0x64,
    };

/*
    uint8_t Qx[SIZE_IN_BYTES_ECDSA_P256] = {
        0x1c, 0xcb, 0xe9, 0x1c, 0x07, 0x5f, 0xc7, 0xf4, 0xf0, 0x33, 0xbf, 0xa2, 0x48, 0xdb, 0x8f, 0xcc,
        0xd3, 0x56, 0x5d, 0xe9, 0x4b, 0xbf, 0xb1, 0x2f, 0x3c, 0x59, 0xff, 0x46, 0xc2, 0x71, 0xbf, 0x83,
    };

    uint8_t Qy[SIZE_IN_BYTES_ECDSA_P256] = {
        0xce, 0x40, 0x14, 0xc6, 0x88, 0x11, 0xf9, 0xa2, 0x1a, 0x1f, 0xdb, 0x2c, 0x0e, 0x61, 0x13, 0xe0,
        0x6d, 0xb7, 0xca, 0x93, 0xb7, 0x40, 0x4e, 0x78, 0xdc, 0x7c, 0xcd, 0x5c, 0xa8, 0x9a, 0x4c, 0xa9,
    };
*/

    uint8_t randNum[SIZE_IN_BYTES_ECDSA_P256] = {
        0x94, 0xa1, 0xbb, 0xb1, 0x4b, 0x90, 0x6a, 0x61, 0xa2, 0x80, 0xf2, 0x45, 0xf9, 0xe9, 0x3c, 0x7f,
        0x3b, 0x4a, 0x62, 0x47, 0x82, 0x4f, 0x5d, 0x33, 0xb9, 0x67, 0x07, 0x87, 0x64, 0x2a, 0x68, 0xde,
    };

    uint8_t R[SIZE_IN_BYTES_ECDSA_P256] = {
        0xf3, 0xac, 0x80, 0x61, 0xb5, 0x14, 0x79, 0x5b, 0x88, 0x43, 0xe3, 0xd6, 0x62, 0x95, 0x27, 0xed,
        0x2a, 0xfd, 0x6b, 0x1f, 0x6a, 0x55, 0x5a, 0x7a, 0xca, 0xbb, 0x5e, 0x6f, 0x79, 0xc8, 0xc2, 0xac,
    };

    uint8_t S[SIZE_IN_BYTES_ECDSA_P256] = {
        0x8b, 0xf7, 0x78, 0x19, 0xca, 0x05, 0xa6, 0xb2, 0x78, 0x6c, 0x76, 0x26, 0x2b, 0xf7, 0x37, 0x1c,
        0xef, 0x97, 0xb2, 0x18, 0xe9, 0x6f, 0x17, 0x5a, 0x3c, 0xcd, 0xda, 0x2a, 0xcc, 0x05, 0x89, 0x03,
    };
#endif


#if 0
    /* Test vector obtained from : https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/dolwments/dss/186-4ecdsatestvectors.zip (See file SigGen.txt, and the second one under "[P-256,SHA-256]".
       Should give a PASS. */

    /*
        All values are in BIG ENDIAN.

        --------------
        Message input:
        --------------
        Hex Msg (MESSAGE TO BE SIGNED) = c35e2f092553c55772926bdbe87c9796827d17024dbb9233a545366e2e5987dd344deb72df987144b8c6c43bc41b654b94cc856e16b96d7a821c8ec039b503e3d86728c494a967d83011a0e090b5d54cd47f4e366c0912bc808fbb2ea96efac88fb3ebec9342738e225f7c7c2b011ce375b56621a20642b4d36e060db4524af1
        MsgHash (SHA-256 HASH OF ABOVE MESSAGE) = 9b2db89cb0e8fa3cc7608b4d6cc1dec0114e0b9ff4080bea12b134f489ab2bbc 
        Tool used to get hash of hex message : https://emn178.github.io/online-tools/sha256.html (Make sure to select 'Hex' from input type dropdown)

        ----------
        Key input:
        ----------
        d (PRIVATE KEY) = 0f56db78ca460b055c500064824bed999a25aaf48ebb519ac201537b85479813

        Public key (not used for signing):
        Qx (PUBLIC KEY X COORDINATE) = e266ddfdc12668db30d4ca3e8f7749432c416044f2d2b8c10bf3d4012aeffa8a
        Qy (PUBLIC KEY Y COORDINATE) = bfa86404a2e9ffe67d47c587ef7a97a7f456b863b4d02cfc6928973ab5b1cb39

        --------------
        Random number: (picked for colwenience and to compare with final result)
        --------------
        k (RANDOM K FACTOR FOR SIGNING) = 6d3e71882c3b83b156bb14e0ab184aa9fb728068d3ae9fac421187ae0b2f34c6

        -------
        Output:
        -------
        R (SIGNATURE COMPONENT R) = 976d3a4e9d23326dc0baa9fa560b7c4e53f42864f508483a6473b6a11079b2db
        S (SIGNATURE COMPONENT S) = 1b766e9ceb71ba6c01dcd46e0af462cd4cfa652ae5017d4555b8eeefe36e1932
    */

    uint8_t msgHash[SIZE_IN_BYTES_ECDSA_P256] = {
        0x9b, 0x2d, 0xb8, 0x9c, 0xb0, 0xe8, 0xfa, 0x3c, 0xc7, 0x60, 0x8b, 0x4d, 0x6c, 0xc1, 0xde, 0xc0,
        0x11, 0x4e, 0x0b, 0x9f, 0xf4, 0x08, 0x0b, 0xea, 0x12, 0xb1, 0x34, 0xf4, 0x89, 0xab, 0x2b, 0xbc,
    };

    uint8_t privateKey[SIZE_IN_BYTES_ECDSA_P256] = {
        0x0f, 0x56, 0xdb, 0x78, 0xca, 0x46, 0x0b, 0x05, 0x5c, 0x50, 0x00, 0x64, 0x82, 0x4b, 0xed, 0x99,
        0x9a, 0x25, 0xaa, 0xf4, 0x8e, 0xbb, 0x51, 0x9a, 0xc2, 0x01, 0x53, 0x7b, 0x85, 0x47, 0x98, 0x13,
    };

/*
    uint8_t Qx[SIZE_IN_BYTES_ECDSA_P256] = {
        0xe2, 0x66, 0xdd, 0xfd, 0xc1, 0x26, 0x68, 0xdb, 0x30, 0xd4, 0xca, 0x3e, 0x8f, 0x77, 0x49, 0x43,
        0x2c, 0x41, 0x60, 0x44, 0xf2, 0xd2, 0xb8, 0xc1, 0x0b, 0xf3, 0xd4, 0x01, 0x2a, 0xef, 0xfa, 0x8a,
    };

    uint8_t Qy[SIZE_IN_BYTES_ECDSA_P256] = {
        0xbf, 0xa8, 0x64, 0x04, 0xa2, 0xe9, 0xff, 0xe6, 0x7d, 0x47, 0xc5, 0x87, 0xef, 0x7a, 0x97, 0xa7,
        0xf4, 0x56, 0xb8, 0x63, 0xb4, 0xd0, 0x2c, 0xfc, 0x69, 0x28, 0x97, 0x3a, 0xb5, 0xb1, 0xcb, 0x39,
    };
*/

    uint8_t randNum[SIZE_IN_BYTES_ECDSA_P256] = {
        0x6d, 0x3e, 0x71, 0x88, 0x2c, 0x3b, 0x83, 0xb1, 0x56, 0xbb, 0x14, 0xe0, 0xab, 0x18, 0x4a, 0xa9,
        0xfb, 0x72, 0x80, 0x68, 0xd3, 0xae, 0x9f, 0xac, 0x42, 0x11, 0x87, 0xae, 0x0b, 0x2f, 0x34, 0xc6,
    };

    uint8_t R[SIZE_IN_BYTES_ECDSA_P256] = {
        0x97, 0x6d, 0x3a, 0x4e, 0x9d, 0x23, 0x32, 0x6d, 0xc0, 0xba, 0xa9, 0xfa, 0x56, 0x0b, 0x7c, 0x4e,
        0x53, 0xf4, 0x28, 0x64, 0xf5, 0x08, 0x48, 0x3a, 0x64, 0x73, 0xb6, 0xa1, 0x10, 0x79, 0xb2, 0xdb,
    };

    uint8_t S[SIZE_IN_BYTES_ECDSA_P256] = {
        0x1b, 0x76, 0x6e, 0x9c, 0xeb, 0x71, 0xba, 0x6c, 0x01, 0xdc, 0xd4, 0x6e, 0x0a, 0xf4, 0x62, 0xcd,
        0x4c, 0xfa, 0x65, 0x2a, 0xe5, 0x01, 0x7d, 0x45, 0x55, 0xb8, 0xee, 0xef, 0xe3, 0x6e, 0x19, 0x32,
    };
#endif

/* More test vectors can be added here if needed */

#endif // TESTVECTORECDSAP256SIGNING_H
