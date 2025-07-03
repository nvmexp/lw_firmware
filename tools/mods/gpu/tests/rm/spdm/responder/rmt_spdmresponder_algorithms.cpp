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

/*!
 * SpdmResponderTest: ALGORITHMS
 *
 * Tests ALGORITHMS response from NEGOTIATE_ALGORITHMS request.
 */

#include "rmt_spdmresponder.h"

#define SPDM_ALL_ASYM_ALGO                (SpdmBaseAsymAlgoTpmAlgRsaSsa2048       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaPss2048       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaSsa3072       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaPss3072       | \
                                           SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP256 | \
                                           SpdmBaseAsymAlgoTpmAlgRsaSsa4096       | \
                                           SpdmBaseAsymAlgoTpmAlgRsaPss4096       | \
                                           SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP384 | \
                                           SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP521)

#define SPDM_ALL_HASH_ALGO                (SpdmBaseHashAlgoTpmAlgSha256   | \
                                           SpdmBaseHashAlgoTpmAlgSha384   | \
                                           SpdmBaseHashAlgoTpmAlgSha512   | \
                                           SpdmBaseHashAlgoTpmAlgSha3_256 | \
                                           SpdmBaseHashAlgoTpmAlgSha3_384 | \
                                           SpdmBaseHashAlgoTpmAlgSha3_512)

#define SPDM_ALL_DHE_ALGO                 (SpdmDheAlgoTypeFfdhe2048 | \
                                           SpdmDheAlgoTypeFfdhe3072 | \
                                           SpdmDheAlgoTypeFfdhe4096 | \
                                           SpdmDheAlgoTypeSecp256r1 | \
                                           SpdmDheAlgoTypeSecp384r1 | \
                                           SpdmDheAlgoTypeSecp521r1)

#define SPDM_ALL_AEAD_ALGO                (SpdmAeadAlgoAes128Gcm           | \
                                           SpdmAeadAlgoAes256Gcm           | \
                                           SpdmAeadAlgoChaCha20Poly1305    | \
                                           SpdmAeadAlgoAes128CtrHmacSha256)

// TODO: Class implementing tests should define these values.
#define SPDM_EXPECTED_NUM_ALG_TABLES      (0x04)
#define SPDM_EXPECTED_ALG_TABLE_ALG_COUNT (0x20)

//------------------------------------------------------------------------
/*!
 * ALGORITHMS positive test case. Ensures successful ALGORITHMS response from
 * valid NEGOTIATE_ALGORITHMS request, with expected supported algorithms selected.
 */
RC SpdmResponderTest::ExpectedAlgorithms()
{
    SPDM_MESSAGE_NEGOTIATE_ALGORITHMS reqNegotiateAlgorithms;
    SPDM_MESSAGE_ALGORITHMS           rspAlgorithms;
    LwU32                             reqSize = sizeof(reqNegotiateAlgorithms);
    LwU32                             rspSize = sizeof(rspAlgorithms);
    RC                                rc      = OK;

    memset(&reqNegotiateAlgorithms, 0, reqSize);
    memset(&rspAlgorithms,          0, rspSize);

    //
    // NEGOTIATE_ALGORITHMS request, send supported algorithms, Responder will choose for session.
    // Advertise support for all algorithms to ensure Responder selects expected ones.
    //
    reqNegotiateAlgorithms.NegotiateAlgorithms.SpdmVersionId             = SpdmVersionId_1_1;
    reqNegotiateAlgorithms.NegotiateAlgorithms.RequestResponseCode       = SpdmRequestIdNegotiateAlgorithms;
    reqNegotiateAlgorithms.NegotiateAlgorithms.NumAlgTables              = SPDM_EXPECTED_NUM_ALG_TABLES;
    reqNegotiateAlgorithms.NegotiateAlgorithms.MessageLength             = reqSize;
    reqNegotiateAlgorithms.NegotiateAlgorithms.MeasurementSpecification  = SpdmMeasurementSpecificationDmtf;
    reqNegotiateAlgorithms.NegotiateAlgorithms.BaseAsymAlgo              = SPDM_ALL_ASYM_ALGO;
    reqNegotiateAlgorithms.NegotiateAlgorithms.BaseHashAlgo              = SPDM_ALL_HASH_ALGO;
    reqNegotiateAlgorithms.NegotiateAlgorithms.ExtAsymCount              = 0;
    reqNegotiateAlgorithms.NegotiateAlgorithms.ExtHashCount              = 0;

    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[0].AlgType      = SpdmAlgTableTypeDhe;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[0].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[0].AlgSupported = SPDM_ALL_DHE_ALGO;

    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[1].AlgType      = SpdmAlgTableTypeAeadCipherSuite;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[1].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[1].AlgSupported = SPDM_ALL_AEAD_ALGO;

    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[2].AlgType      = SpdmAlgTableTypeReqBaseAsymAlg;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[2].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[2].AlgSupported = SPDM_ALL_ASYM_ALGO;

    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[3].AlgType      = SpdmAlgTableTypeKeySchedule;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[3].AlgCount     = SPDM_EXPECTED_ALG_TABLE_ALG_COUNT;
    reqNegotiateAlgorithms.NegotiateAlgorithms.AlgTables[3].AlgSupported = SpdmKeyScheduleAlgoSpdm;

    CHECK_RC(SendSpdmRequest((LwU8 *)&reqNegotiateAlgorithms, reqSize, (LwU8 *)&rspAlgorithms, &rspSize));

    MASSERT(rspSize                                              == sizeof(SPDM_MESSAGE_ALGORITHMS));
    MASSERT(rspAlgorithms.Algorithms.SpdmVersionId               == SpdmVersionId_1_1);
    MASSERT(rspAlgorithms.Algorithms.RequestResponseCode         == SpdmResponseIdAlgorithms);
    MASSERT(rspAlgorithms.Algorithms.NumAlgTables                == SPDM_EXPECTED_NUM_ALG_TABLES);
    MASSERT(rspAlgorithms.Algorithms.MessageLength               == sizeof(SPDM_MESSAGE_ALGORITHMS));
    MASSERT(rspAlgorithms.Algorithms.MeasurementSpecificationSel == SpdmMeasurementSpecificationDmtf);
    MASSERT(rspAlgorithms.Algorithms.MeasurementHashAlgo         == SpdmMeasurementHashAlgoTpmAlgSha256);
    MASSERT(rspAlgorithms.Algorithms.BaseAsymSel                 == SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP256);
    MASSERT(rspAlgorithms.Algorithms.BaseHashSel                 == SpdmBaseHashAlgoTpmAlgSha256);
    MASSERT(rspAlgorithms.Algorithms.ExtAsymSelCount             == 0);
    MASSERT(rspAlgorithms.Algorithms.ExtHashSelCount             == 0);

    // TODO: Class implementing tests should define the expected algorithms.
    MASSERT(rspAlgorithms.Algorithms.AlgTables[0].AlgType        == SpdmAlgTableTypeDhe);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[0].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[0].AlgSupported   == SpdmDheAlgoTypeSecp256r1);

    MASSERT(rspAlgorithms.Algorithms.AlgTables[1].AlgType        == SpdmAlgTableTypeAeadCipherSuite);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[1].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[1].AlgSupported   == SpdmAeadAlgoAes128CtrHmacSha256);

    MASSERT(rspAlgorithms.Algorithms.AlgTables[2].AlgType        == SpdmAlgTableTypeReqBaseAsymAlg);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[2].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[2].AlgSupported   == SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP256);

    MASSERT(rspAlgorithms.Algorithms.AlgTables[3].AlgType        == SpdmAlgTableTypeKeySchedule);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[3].AlgCount       == SPDM_EXPECTED_ALG_TABLE_ALG_COUNT);
    MASSERT(rspAlgorithms.Algorithms.AlgTables[3].AlgSupported   == SpdmKeyScheduleAlgoSpdm);

    return rc;
}

//------------------------------------------------------------------------
RC SpdmResponderTest::RunAlgorithmsTests()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "--------------------------------------------------\n");
    Printf(Tee::PriHigh, "SpdmResponderTest: Running ALGORITHMS tests.\n");
    Printf(Tee::PriHigh, "--------------------------------------------------\n");

    // Initialize positive test case separately, as test case used in SpdmSetState().
    CHECK_RC(SpdmSetState(SpdmRequestIdNegotiateAlgorithms));
    CHECK_RC(ExpectedAlgorithms());

    return rc;
}
