/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mods.js"
g_SpecArgs = "-power_cap_max";
g_TestMode = 16;

// Frequencies of musical notes
ARG_STATE_VARIABLE(g_C,  [16.35160]);
ARG_STATE_VARIABLE(g_Cs, [17.32391]);
ARG_STATE_VARIABLE(g_Db, [17.32391]);
ARG_STATE_VARIABLE(g_D,  [18.35405]);
ARG_STATE_VARIABLE(g_Ds, [19.44544]);
ARG_STATE_VARIABLE(g_Eb, [19.44544]);
ARG_STATE_VARIABLE(g_E,  [20.60172]);
ARG_STATE_VARIABLE(g_F,  [21.82676]);
ARG_STATE_VARIABLE(g_Fs, [23.12465]);
ARG_STATE_VARIABLE(g_Gb, [23.12465]);
ARG_STATE_VARIABLE(g_G,  [24.49971]);
ARG_STATE_VARIABLE(g_Gs, [25.95654]);
ARG_STATE_VARIABLE(g_Ab, [25.95654]);
ARG_STATE_VARIABLE(g_A,  [27.50000]);
ARG_STATE_VARIABLE(g_As, [29.13524]);
ARG_STATE_VARIABLE(g_Bb, [29.13524]);
ARG_STATE_VARIABLE(g_B,  [30.86771]);

// Fake a rest by hitting an inaudibly high frequency
ARG_STATE_VARIABLE(g_Rest, 25000);

// Note durations. Whole note = g_1, half note = g_2, quarter = g_4, etc
ARG_STATE_VARIABLE(g_1, []);
ARG_STATE_VARIABLE(g_2, []);
ARG_STATE_VARIABLE(g_4, []);
ARG_STATE_VARIABLE(g_8, []);
ARG_STATE_VARIABLE(g_12, []); // triplet
ARG_STATE_VARIABLE(g_16, []);
ARG_STATE_VARIABLE(g_32, []);

function InitFreqs()
{
    for (let octave = 1; octave <= 9; octave++)
    {
        g_C[octave] = 2 * g_C[octave - 1];
        g_Cs[octave] = 2 * g_Cs[octave - 1];
        g_Db[octave] = 2 * g_Db[octave - 1];
        g_D[octave] = 2 * g_D[octave - 1];
        g_Ds[octave] = 2 * g_Ds[octave - 1];
        g_Eb[octave] = 2 * g_Eb[octave - 1];
        g_E[octave] = 2 * g_E[octave - 1];
        g_F[octave] = 2 * g_F[octave - 1];
        g_Fs[octave] = 2 * g_Fs[octave - 1];
        g_Gb[octave] = 2 * g_Gb[octave - 1];
        g_G[octave] = 2 * g_G[octave - 1];
        g_Gs[octave] = 2 * g_Gs[octave - 1];
        g_Ab[octave] = 2 * g_Ab[octave - 1];
        g_A[octave] = 2 * g_A[octave - 1];
        g_As[octave] = 2 * g_As[octave - 1];
        g_Bb[octave] = 2 * g_Bb[octave - 1];
        g_B[octave] = 2 * g_B[octave - 1];
    }
}

function SetTempo(bpm)
{
    let beatMs = 60000 / bpm;
    g_1 = beatMs * 4;
    g_2 = beatMs * 2;
    g_4 = beatMs;
    g_8 = beatMs / 2;
    g_12 = beatMs / 3;
    g_16 = beatMs / 4;
    g_32 = beatMs / 8;
}

function scale(testList, unused)
{
    InitFreqs();
    SetTempo(120);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1, // So it exits after the first iteration
        Notes:
        [
            { FreqHz: g_C[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_E[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 }
        ]
    });
}

function doodle(testList, unused)
{
    InitFreqs();
    SetTempo(128);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1, // So it exits after the first iteration
        Notes:
        [
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_E[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_E[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },

            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Eb[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_Ab[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_Ab[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Eb[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_16 },
            { FreqHz: g_B[5], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_16 },

            { FreqHz: g_C[6], LengthMs: g_8 },
            { FreqHz: g_D[6], LengthMs: g_16 },
            { FreqHz: g_Eb[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_Bb[6], LengthMs: g_16 },
            { FreqHz: g_Ab[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_Eb[6], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_16 },

            { FreqHz: g_C[6], LengthMs: g_8 },
            { FreqHz: g_D[6], LengthMs: g_16 },
            { FreqHz: g_Eb[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16 },
            { FreqHz: g_E[7], LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 }
        ]
    });
}

function failSound(testList, unused)
{
    InitFreqs();
    SetTempo(128);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1, // So it exits after the first iteration
        Notes:
        [
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_Gs[6], LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_16 },
            { FreqHz: g_B[5], LengthMs: g_4 },
        ]
    });
}

function passSound(testList, unused)
{
    InitFreqs();
    SetTempo(108);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1, // So it exits after the first iteration
        Notes:
        [
            { FreqHz: g_C[6], LengthMs: g_16 },
            { FreqHz: g_E[6], LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_4 },
        ]
    });
}

function usa(testList, unused)
{
    InitFreqs();
    SetTempo(92);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1, // So it exits after the first iteration
        Notes:
        [
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_E[6], LengthMs: g_16 },
            { FreqHz: g_C[6], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_C[7], LengthMs: g_2 },
            { FreqHz: g_E[7], LengthMs: g_8 },
            { FreqHz: g_E[7], LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_Fs[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_E[7], LengthMs: g_4 },
            { FreqHz: g_E[7], LengthMs: g_8 },
            { FreqHz: g_D[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_4 },
            { FreqHz: g_B[6], LengthMs: g_2 },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_C[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_E[6], LengthMs: g_16 },
            { FreqHz: g_C[6], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_C[7], LengthMs: g_2 },
            { FreqHz: g_E[7], LengthMs: g_8 },
            { FreqHz: g_E[7], LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_Fs[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_E[7], LengthMs: g_4 },
            { FreqHz: g_E[7], LengthMs: g_8 },
            { FreqHz: g_D[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_4 },
            { FreqHz: g_B[6], LengthMs: g_2 },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_C[6], LengthMs: g_4 },
            { FreqHz: g_E[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_E[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_E[7], LengthMs: g_4 },
            { FreqHz: g_F[7], LengthMs: g_4 },
            { FreqHz: g_G[7], LengthMs: g_8 },
            { FreqHz: g_G[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[7], LengthMs: g_2 },
            { FreqHz: g_F[7], LengthMs: g_8 },
            { FreqHz: g_E[7], LengthMs: g_8 },
            { FreqHz: g_D[7], LengthMs: g_4 },
            { FreqHz: g_E[7], LengthMs: g_4 },
            { FreqHz: g_F[7], LengthMs: g_8 },
            { FreqHz: g_F[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_F[7], LengthMs: g_4 },
            { FreqHz: g_F[7], LengthMs: g_8 },
            { FreqHz: g_F[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_F[7], LengthMs: g_4 },
            { FreqHz: g_E[7], LengthMs: g_4 },
            { FreqHz: g_E[7], LengthMs: g_8 },
            { FreqHz: g_D[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_4 },
            { FreqHz: g_B[6], LengthMs: g_2 },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_4 },
            { FreqHz: g_E[6], LengthMs: g_4 },
            { FreqHz: g_Fs[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_B[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_4 / 0.9 },
            { FreqHz: g_F[7], LengthMs: g_8 / 0.85 },
            { FreqHz: g_E[7], LengthMs: g_8 / 0.85 },
            { FreqHz: g_D[7], LengthMs: g_8 / 0.85 },
            { FreqHz: g_C[7], LengthMs: g_16 / 0.80 },
            { FreqHz: g_Rest, LengthMs: g_16 / 0.80 },
            { FreqHz: g_C[7], LengthMs: g_4 / 0.75 },
            { FreqHz: g_B[6], LengthMs: g_4 / 0.50 },
            { FreqHz: g_Rest, LengthMs: g_16 / 0.75 },
            { FreqHz: g_G[6], LengthMs: g_16 / 0.75 },
            { FreqHz: g_Rest, LengthMs: g_16 / 0.75 },
            { FreqHz: g_G[6], LengthMs: g_16 / 0.75 },
            { FreqHz: g_Rest, LengthMs: g_16 / 0.75 },
            { FreqHz: g_C[7], LengthMs: g_4 / 0.75 },
            { FreqHz: g_C[7], LengthMs: g_8 / 0.75 },
            { FreqHz: g_D[7], LengthMs: g_8 / 0.75 },
            { FreqHz: g_E[7], LengthMs: g_8 / 0.75 },
            { FreqHz: g_F[7], LengthMs: g_8 / 0.75 },
            { FreqHz: g_G[7], LengthMs: g_2 / 0.75 },
            { FreqHz: g_C[7], LengthMs: g_8 / 0.67 },
            { FreqHz: g_D[7], LengthMs: g_8 / 0.67 },
            { FreqHz: g_E[7], LengthMs: g_4 / 0.67 },
            { FreqHz: g_E[7], LengthMs: g_8 / 0.60 },
            { FreqHz: g_F[7], LengthMs: g_8 / 0.60 },
            { FreqHz: g_D[7], LengthMs: g_4 / 0.55 },
            { FreqHz: g_C[7], LengthMs: g_2 / 0.50 },
            { FreqHz: g_C[7], LengthMs: g_4 / 0.50 }
        ]
    });
}

function happyBirthday(testList)
{
    InitFreqs();
    SetTempo(84);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1, // So it exits after the first iteration
        Notes:
        [
            { FreqHz: g_C[6], LengthMs: g_8, Lyric: "Happy" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[6], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_4, Lyric: "Birthday" },
            { FreqHz: g_C[6], LengthMs: g_4 },
            { FreqHz: g_F[6], LengthMs: g_4, Lyric: "to" },
            { FreqHz: g_E[6], LengthMs: g_2, Lyric: "you" },

            { FreqHz: g_C[6], LengthMs: g_8, Lyric: "Happy" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[6], LengthMs: g_16 },
            { FreqHz: g_D[6], LengthMs: g_4, Lyric: "Birthday" },
            { FreqHz: g_C[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_4, Lyric: "to" },
            { FreqHz: g_F[6], LengthMs: g_2, Lyric: "you" },

            { FreqHz: g_C[6], LengthMs: g_8, Lyric: "Happy" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[6], LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_4, Lyric: "Birthday" },
            { FreqHz: g_A[6], LengthMs: g_4 },
            { FreqHz: g_F[6], LengthMs: g_4 / 0.9, Lyric: "dear" },
            { FreqHz: g_E[6], LengthMs: g_4 / 0.9, Lyric: "MODS" },
            { FreqHz: g_D[6], LengthMs: g_2 / 0.9, Lyric: "user" },

            { FreqHz: g_Bb[6], LengthMs: g_8 / 0.8, Lyric: "HAPPY" },
            { FreqHz: g_Rest, LengthMs: g_16 / 0.8 },
            { FreqHz: g_Bb[6], LengthMs: g_16 / 0.8 },
            { FreqHz: g_A[6], LengthMs: g_4 / 0.75, Lyric: "BIRTHDAY" },
            { FreqHz: g_F[6], LengthMs: g_4 / 0.75 },
            { FreqHz: g_G[6], LengthMs: g_4 / 0.75, Lyric: "TO" },
            { FreqHz: g_F[6], LengthMs: g_2 / 0.70, Lyric: "YOU!!! :)" },
            { FreqHz: g_F[6], LengthMs: g_4 / 0.70 }
        ]
    });
}

function jingleBells(testList, unused)
{
    InitFreqs();
    SetTempo(100);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1,
        Notes:
        [
            { FreqHz: g_D[6], LengthMs: g_8, Lyric: "Dashing" },
            { FreqHz: g_B[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "through" },
            { FreqHz: g_G[6], LengthMs: g_8, Lyric: "the" },
            { FreqHz: g_D[6], LengthMs: g_4, Lyric: "snow" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_D[6], LengthMs: g_32, Lyric: "In" },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_D[6], LengthMs: g_32, Lyric: "a" },
            { FreqHz: g_Rest, LengthMs: g_32 },

            { FreqHz: g_D[6], LengthMs: g_8, Lyric: "one" },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "horse" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "open" },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_E[6], LengthMs: g_4, Lyric: "sleigh" },
            { FreqHz: g_E[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_E[6], LengthMs: g_8, Lyric: "Over" },
            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "the" },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "fields" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "we" },
            { FreqHz: g_Fs[6], LengthMs: g_4, Lyric: "go" },
            { FreqHz: g_Fs[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "Laughing" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "all" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "the" },
            { FreqHz: g_B[6], LengthMs: g_4, Lyric: "way" },
            { FreqHz: g_B[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_D[6], LengthMs: g_8, Lyric: "Bells" },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "on" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "bobtail" },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_D[6], LengthMs: g_4, Lyric: "ring" },
            { FreqHz: g_D[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_D[6], LengthMs: g_8, Lyric: "Making" },
            { FreqHz: g_B[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "spirits" },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_E[6], LengthMs: g_4, Lyric: "bright" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_E[6], LengthMs: g_8, Lyric: "What" },

            { FreqHz: g_E[6], LengthMs: g_8, Lyric: "fun" },
            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "it" },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "is" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "to" },
            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "ride" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "and" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "sing" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "a" },

            { FreqHz: g_E[7], LengthMs: g_8, Lyric: "sleighing" },
            { FreqHz: g_D[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "song" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "tonight" },
            { FreqHz: g_G[6], LengthMs: g_4 },
            { FreqHz: g_G[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "Jingle" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "bells" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "Jingle" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "bells" },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "Jingle" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_8, Lyric: "all" },
            { FreqHz: g_G[6], LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_16, Lyric: "the" },
            { FreqHz: g_B[6], LengthMs: g_4, Lyric: "way" },
            { FreqHz: g_B[6], LengthMs: g_8 },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "Oh" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "what" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "fun" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "it" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "is" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "to" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "ride" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_32, Lyric: "in" },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_B[6], LengthMs: g_32, Lyric: "a" },
            { FreqHz: g_Rest, LengthMs: g_32 },

            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "one" },
            { FreqHz: g_A[6], LengthMs: g_16, Lyric: "horse" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "open" },
            { FreqHz: g_B[6], LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_4, Lyric: "sleigh" },
            { FreqHz: g_D[7], LengthMs: g_4, Lyric: "HEY!" },

            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "Jingle" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "bells" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "Jingle" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_8, Lyric: "bells" },
            { FreqHz: g_Rest, LengthMs: g_8 },

            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "Oh" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "what" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "fun" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "it" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "is" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "to" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_16, Lyric: "ride" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_B[6], LengthMs: g_32, Lyric: "in" },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_B[6], LengthMs: g_32, Lyric: "a" },
            { FreqHz: g_Rest, LengthMs: g_32 },

            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "one" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_D[7], LengthMs: g_16, Lyric: "horse" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "open" },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_G[6], LengthMs: g_2, Lyric: "sleigh" }
        ]
    });
}

function christmasTree(testList, unused)
{
    InitFreqs();
    SetTempo(84);
    testList.AddTest("SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" }});
    testList.AddTest("VkStressPulse",
    {
        PulseMode: VkStressConst.MUSIC,
        RuntimeMs: 1,
        Notes:
        [
            { FreqHz: g_C[6], LengthMs: g_4,  Lyric: "O" },

            { FreqHz: g_F[6], LengthMs: g_8,  Lyric: "Christmas" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_32 },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_F[6], LengthMs: g_4,  Lyric: "tree" },
            { FreqHz: g_G[6], LengthMs: g_4,  Lyric: "O" },

            { FreqHz: g_A[6], LengthMs: g_8,  Lyric: "Christmas" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_32 },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_A[6], LengthMs: g_4,  Lyric: "tree" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "How" },

            { FreqHz: g_G[6], LengthMs: g_8, Lyric: "steadfast" },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_Bb[6], LengthMs: g_4, Lyric: "are" },
            { FreqHz: g_E[6], LengthMs: g_4, Lyric: "your" },

            { FreqHz: g_G[6], LengthMs: g_4, Lyric: "branches" },
            { FreqHz: g_F[6], LengthMs: g_4 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_C[6], LengthMs: g_4,  Lyric: "O" },

            { FreqHz: g_F[6], LengthMs: g_8,  Lyric: "Christmas" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_F[6], LengthMs: g_32 },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_F[6], LengthMs: g_4,  Lyric: "tree" },
            { FreqHz: g_G[6], LengthMs: g_4,  Lyric: "O" },

            { FreqHz: g_A[6], LengthMs: g_8,  Lyric: "Christmas" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_32 },
            { FreqHz: g_Rest, LengthMs: g_32 },
            { FreqHz: g_A[6], LengthMs: g_4,  Lyric: "tree" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "How" },

            { FreqHz: g_G[6], LengthMs: g_8, Lyric: "steadfast" },
            { FreqHz: g_A[6], LengthMs: g_8 },
            { FreqHz: g_Bb[6], LengthMs: g_4, Lyric: "are" },
            { FreqHz: g_E[6], LengthMs: g_4, Lyric: "your" },

            { FreqHz: g_G[6], LengthMs: g_4, Lyric: "branches" },
            { FreqHz: g_F[6], LengthMs: g_4 },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "Your" },
            { FreqHz: g_Rest, LengthMs: g_16 },

            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "boughs" },
            { FreqHz: g_A[6], LengthMs: g_8, Lyric: "are" },
            { FreqHz: g_D[7], LengthMs: g_4, Lyric: "green" },
            { FreqHz: g_D[7], LengthMs: g_8 },
            { FreqHz: g_C[7], LengthMs: g_16, Lyric: "in" },
            { FreqHz: g_Rest, LengthMs: g_16 },

            { FreqHz: g_C[7], LengthMs: g_8, Lyric: "summer's" },
            { FreqHz: g_Bb[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_Bb[6], LengthMs: g_4, Lyric: "clime" },
            { FreqHz: g_Rest, LengthMs: g_8 },
            { FreqHz: g_Bb[6], LengthMs: g_16, Lyric: "and" },
            { FreqHz: g_Rest, LengthMs: g_16 },

            { FreqHz: g_Bb[6], LengthMs: g_16, Lyric: "through" },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_G[6], LengthMs: g_8, Lyric: "the" },
            { FreqHz: g_C[7], LengthMs: g_4, Lyric: "snows" },
            { FreqHz: g_C[7], LengthMs: g_8 },
            { FreqHz: g_Bb[6], LengthMs: g_16, Lyric: "of" },
            { FreqHz: g_Rest, LengthMs: g_16 },

            { FreqHz: g_Bb[6], LengthMs: g_8, Lyric: "winter" },
            { FreqHz: g_A[6], LengthMs: g_16 },
            { FreqHz: g_Rest, LengthMs: g_16 },
            { FreqHz: g_A[6], LengthMs: g_4, Lyric: "time" },

            { FreqHz: g_Rest, LengthMs: g_4 },
            { FreqHz: g_C[6], LengthMs: g_4 / .90,  Lyric: "O" },

            { FreqHz: g_F[6], LengthMs: g_8 / .90,  Lyric: "Christmas" },
            { FreqHz: g_Rest, LengthMs: g_16 / .90 },
            { FreqHz: g_F[6], LengthMs: g_32 / .90 },
            { FreqHz: g_Rest, LengthMs: g_32 / .90 },
            { FreqHz: g_F[6], LengthMs: g_4 / .90,  Lyric: "tree" },
            { FreqHz: g_G[6], LengthMs: g_4 / .90,  Lyric: "O" },

            { FreqHz: g_A[6], LengthMs: g_8 / .90,  Lyric: "Christmas" },
            { FreqHz: g_Rest, LengthMs: g_16 / .90 },
            { FreqHz: g_A[6], LengthMs: g_32 / .90 },
            { FreqHz: g_Rest, LengthMs: g_32 / .90 },
            { FreqHz: g_A[6], LengthMs: g_4 / .90,  Lyric: "tree" },
            { FreqHz: g_Rest, LengthMs: g_8 / .90 },
            { FreqHz: g_A[6], LengthMs: g_8 / .80, Lyric: "How" },

            { FreqHz: g_G[6], LengthMs: g_8 / .80, Lyric: "steadfast" },
            { FreqHz: g_A[6], LengthMs: g_8 / .80 },
            { FreqHz: g_Bb[6], LengthMs: g_4 / .80, Lyric: "are" },
            { FreqHz: g_E[6], LengthMs: g_4 / .75, Lyric: "your" },

            { FreqHz: g_G[6], LengthMs: g_4 / .70, Lyric: "branches" },
            { FreqHz: g_F[6], LengthMs: g_4 / .70 },
        ]
    });
}

function userSpec(testList)
{
    failSound(testList);
    passSound(testList);
    scale(testList);
    doodle(testList);
    happyBirthday(testList);
    usa(testList);
    jingleBells(testList);
    christmasTree(testList);
}

