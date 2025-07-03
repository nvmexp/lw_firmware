
my $imemOverlaysTaskPrHs =
[
    PRSBHSDECRYPTMPK =>
    {
        NAME           => 'prSbHsDecryptMPK',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'PR sandbox HS Decrypt MPK for GDK',
        INPUT_SECTIONS =>
        [
            ['*.o'       , '.text.prSbHsDecryptMPK' ],
            ['*.o'       , '.text.prDecryptMpk_*' ],
            ['*.o'       , '.text.prCheckDependentBinMilwersion_*' ],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_prSbHsDecryptMPK._start'],
        ],

        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    PR_HS_COMMON =>
    {
        FLAGS          => ':HS_OVERLAY',
        LS_SIG_GROUP   => 1,
        ALIGNMENT      => 256,
        DESCRIPTION    => 'Common overlay for PR HS codes',
        INPUT_SECTIONS =>
        [
            ['*.o'       , '.text.OEM_TEE_HsCommonEntry' ],
            ['*.o'       , '.text.ComputeDmhash_HS' ],
            ['*.o'       , '.text.Oem_Aes_EncryptOneBlockWithKeyForKDFMode_LWIDIA_HS' ],
            ['*.o'       , '.text._Oem_Aes_EncryptOneBlockWithKeyDerivedFromSalt_LWIDIA_HS' ],
            ['*.o'       , '.text.OEM_TEE_FlcnStatusToDrmStatus_HS' ],
            ['*.o'       , '.text._OEM_TEE_ProgramDispBlankingPolicyToHw_HS' ],
            ['*.o'       , '.text._OEM_TEE_UpdateDispBlankingPolicy_HS' ],
            ['*.o'       , '.text.prHsPreCheck_*' ],
            ['*.o'       , '.text._OEM_TEE_AllowVprScanoutToInternal_HS' ],
            ['*.o'       , '.text.prInitializeSharedStructHS_*' ],
            ['*.o'       , '.text.prGetSharedDataRegionDetailsHS_*' ],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_prHsCommon.OEM_TEE_HsCommonEntry'],
        ],

        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    PRSBHSENTRY =>
    {
        NAME           => 'prSbHsEntry',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        LS_SIG_GROUP   => 1,
        ALIGNMENT      => 256,
        DESCRIPTION    => 'PR sandbox HS Entry for GDK',
        INPUT_SECTIONS =>
        [
            ['*.o'       , '.text.prSbHsEntry' ],
            ['*.o'       , '.text.prSaveStackValues' ],
            ['*.o'       , '.text.prBlockDmaAndPrivHs_*' ],
            ['*.o'       , '.text.prUpdateStackBottom_*' ],
            ['*.o'       , '.text.prIsImemBlockPartOfLassahsOverlays' ],
            ['*.o'       , '.text.prIlwalidateBlocksOutsideLassahs' ],
            ['*.o'       , '.text.prMarkDataBlocksAsSelwre' ],
            ['*.o'       , '.text.prGetImemBlockCount_*' ],
            ['*.o'       , '.text.prVerifyLsSigGrp' ],
            ['*.o'       , '.text.prInsertInOrder' ],
            ['*.o'       , '.text.prCallwlateDmHashForOverlay' ],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_prSbHsEntry._start'],
        ],

        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },

    PRSBHSEXIT =>
    {
        NAME           => 'prSbHsExit',
        FLAGS          => ':HS_OVERLAY' .
                          ':HS_LIB',
        ALIGNMENT      => 256,
        DESCRIPTION    => 'PR sandbox HS Exit for GDK',
        INPUT_SECTIONS =>
        [
            ['*.o'       , '.text.prSbHsExit' ],
            ['*.o'       , '.text.prStkScrub' ],
            ['*.o'       , '.text.prUnblockDmaAndPrivHs_*' ],
            ['*.o'       , '.text.prUnmarkDataBlocksAsSelwre' ],
        ],
        KEEP_SECTIONS  =>
        [
            ['.imem_prSbHsExit._start'],
        ],

        ENABLED_ON     => [ GP10X_and_later, ],
        PROFILES       => [ 'sec2-*', '-sec2-tu10a', '-sec2-*_nouveau*', ],
    },
];

# return the data of PR HS Overlays definition
return $imemOverlaysTaskPrHs;

