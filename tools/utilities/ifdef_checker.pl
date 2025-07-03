#!/usr/bin/elw perl

#
# Purpose: Used to verify if defines used under #if checks are valid or not
#          Lwrrently supports ACR, SEC2, UPROC_LIBS, SCRUBBER, AUTO_ACR, and RVBL
#
# Usage  : 1) perl ifdef_checker.pl -dir=acr                    # searches in //uproc/acr
#          2) perl ifdef_checker.pl -dir=sec2                   # searches in //uproc/sec2
#          3) perl ifdef_checker.pl -dir=libs                   # searches in //uproc/libs
#          4) perl ifdef_checker.pl -dir=selwrescrub            # searches in //uproc/selwrescrub
#          5) perl ifdef_checker.pl -dir=auto_acr               # searches in //uproc/auto_acr
#          6) perl ifdef_checker.pl -dir=lwriscv/bootloader     # searches in //uproc/lwriscv/bootloader
#

use Getopt::Long;
use File::Find;


#
# Get directory to be parsed
#
my $dir;
my $LWUPROC;
GetOptions ('dir=s' => \$dir, 'LWUPROC=s' => \$LWUPROC);
my $directory = "$LWUPROC/$dir";
my %ifdef_list;

#
# List of supported #defines in ACR
#
my %ifdef_list_acr = (

    # profile specific defines
    'ACR_BSI_BOOT_PATH'               => 1,
    'BSI_LOCK'                        => 1,
    'ACR_BSI_PHASE'                   => 1,
    'ACR_BUILD_ONLY'                  => 1,
    'ACR_FALCON_PMU'                  => 1,
    'ACR_FALCON_SEC2'                 => 1,
    'ACR_FIXED_TRNG'                  => 1,
    'ACR_FMODEL_BUILD'                => 1,
    'ACR_RISCV_LS'                    => 1,
    'ACR_LOAD_PATH'                   => 1,
    'ACR_LSF_LWRRENT_BOOTSTRAP_OWNER' => 1,
    'ACR_ONLY_BO'                     => 1,
    'ACR_SIG_VERIF'                   => 1,
    'ACR_SKIP_MEMRANGE'               => 1,
    'ACR_SKIP_SCRUB'                  => 1,
    'ACR_UNLOAD'                      => 1,
    'ACRCFG_ENGINE_SETUP'             => 1,
    'ACRDBG_PRINTF'                   => 1,
    'DMEM_APERT_ENABLED'              => 1,
    'DMA_XFER_SIZE_BYTES'             => 1,
    'DPU_PRESENT'                     => 1,
    'LSF_BOOTSTRAP_OWNER_PMU'         => 1,
    'LSF_BOOTSTRAP_OWNER_SEC2'        => 1,
    'LWDEC_PRESENT'                   => 1,
    'LWENC_PRESENT'                   => 1,
    'GSP_PRESENT'                     => 1,
    'FBFALCON_PRESENT'                => 1,
    'SEC2_PRESENT'                    => 1,
    'NOUVEAU_ACR'                     => 1,
    'UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE' => 1,
    'IS_SSP_ENABLED'                  => 1,
    'ASB'                             => 1,
    'AHESASC'                         => 1,
    'AMPERE_PROTECTED_MEMORY'         => 1,
    'NOT_HALTING_IN_HS'               => 1,
    'ACR_UNLOAD_ON_SEC2'              => 1,
    'PKC_ENABLED'                     => 1,
    'PRI_SOURCE_ISOLATION_PLM'        => 1,
    'IS_ENHANCED_AES'                 => 1,
    'PKC_LS_RSA3K_KEYS_GA10X'         => 1,
    'NEW_WPR_BLOBS'                   => 1,
    'BOOT_FROM_HS_BUILD'              => 1,
    'DISABLE_SE_ACCESSES'             => 1,
    'LWDEC1_PRESENT'                  => 1,
    'LWDEC_RISCV_PRESENT'             => 1,
    'LWDEC_RISCV_EB_PRESENT'          => 1,
    'LWJPG_PRESENT'                   => 1,
    'OFA_PRESENT'                     => 1,
    'ACR_ENFORCE_LS_UCODE_ENCRYPTION' => 1,
    'LIB_CCC_PRESENT'                 => 1,
    'ACR_MEMSETBYTE_ENABLED'          => 1,
    'ACR_GSP_RM_BUILD'                => 1,

    # HW manual defines
    'LW_CPWR_FALCON_DMEM_PRIV_RANGE0'               => 1,
    'LW_FALCON_LWENC2_BASE'                         => 1,
    'LW_FUSE_SPARE_BIT_3'                           => 1,
    'LW_PMC_BOOT_42_CHIP_ID_GP108'                  => 1,
    'LW_CPWR_FALCON_DIC_D0'                         => 1,
    'LW_CSEC_FALCON_DIC_D0'                         => 1,
    'LW_PMC_DEVICE_ENABLE'                          => 1,
    'LW_CSEC_BAR0_TMOUT_CYCLES_PROD'                => 1,
    'LW_PRISCV_RISCV_CPUCTL_ALIAS_EN'               => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2fbfalcon_pri'     => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2fbfalcon_pri'    => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2fbfalcon_pri'    => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri0'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri0'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri0'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri1'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri1'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri1'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri2'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri2'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri2'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri3'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri3'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri3'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri4'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri4'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri4'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri5'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri5'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri5'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri6'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri6'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri6'      => 1,
    'LW_PPRIV_SYS_PRI_MASTER_fecs2lwdec_pri7'       => 1,
    'LW_PPRIV_SYSB_PRI_MASTER_fecs2lwdec_pri7'      => 1,
    'LW_PPRIV_SYSC_PRI_MASTER_fecs2lwdec_pri7'      => 1,
    'LW_SCAL_LITTER_NUM_SYSB'                       => 1,
    'LW_SCAL_LITTER_NUM_SYSC'                       => 1,

    # others
    'NONE'                            => 1,
    'NULL'                            => 1,
    'LW_SIZEOF32'                     => 1,
);

#
# List of supported #defines in Auto ACR
#
my %ifdef_list_autoacr = (

    # profile specific defines
    'ACR_BSI_BOOT_PATH'               => 1,
    'BSI_LOCK'                        => 1,
    'ACR_BSI_PHASE'                   => 1,
    'ACR_BUILD_ONLY'                  => 1,
    'ACR_FALCON_PMU'                  => 1,
    'ACR_FALCON_SEC2'                 => 1,
    'ACR_FIXED_TRNG'                  => 1,
    'ACR_FMODEL_BUILD'                => 1,
    'ACR_LOAD_PATH'                   => 1,
    'ACR_LSF_LWRRENT_BOOTSTRAP_OWNER' => 1,
    'ACR_ONLY_BO'                     => 1,
    'ACR_SIG_VERIF'                   => 1,
    'ACR_SKIP_MEMRANGE'               => 1,
    'ACR_SKIP_SCRUB'                  => 1,
    'ACR_UNLOAD'                      => 1,
    'ACRCFG_ENGINE_SETUP'             => 1,
    'ACRDBG_PRINTF'                   => 1,
    'DMEM_APERT_ENABLED'              => 1,
    'DMA_XFER_SIZE_BYTES'             => 1,
    'DPU_PRESENT'                     => 1,
    'LSF_BOOTSTRAP_OWNER_PMU'         => 1,
    'LSF_BOOTSTRAP_OWNER_SEC2'        => 1,
    'LWDEC_PRESENT'                   => 1,
    'LWENC_PRESENT'                   => 1,
    'GSP_PRESENT'                     => 1,
    'SEC2_PRESENT'                    => 1,
    'NOUVEAU_ACR'                     => 1,
    'UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE' => 1,
    'IS_SSP_ENABLED'                  => 1,
    'ASB'                             => 1,
    'AHESASC'                         => 1,
    'NOT_HALTING_IN_HS'               => 1,
    'ACR_UNLOAD_ON_SEC2'              => 1,

    # HW manual defines
    'LW_CPWR_FALCON_DMEM_PRIV_RANGE0' => 1,
    'LW_FALCON_LWENC2_BASE'           => 1,
    'LW_FUSE_SPARE_BIT_3'             => 1,
    'LW_PMC_BOOT_42_CHIP_ID_GP108'    => 1,
    'LW_CPWR_FALCON_DIC_D0'           => 1,
    'LW_CSEC_FALCON_DIC_D0'           => 1,
    'LW_PMC_DEVICE_ENABLE'            => 1,
    'LW_CSEC_BAR0_TMOUT_CYCLES_PROD'  => 1,

    # others
    'NONE'                            => 1,
    'NULL'                            => 1,
    'LW_SIZEOF32'                     => 1,
);


#
# List of supported #defines in SEC2
#
my %ifdef_list_sec2 = (

    # sec2 common defines
    'DBG_PRINTF_ENABLED_CHNMGMT'               => 1,
    'DBG_PRINTF_ENABLED_CMDMGMT'               => 1,
    'DMEM_VA_SUPPORTED'                        => 1,
    'EMEM_SUPPORTED'                           => 1,
    'GFE_GEN_MODE'                             => 1,
    'HDCP_DEBUG_DEMO_SESSION'                  => 1,
    'HDCP_DEBUG_MODE'                          => 1,
    'HDCP_MAX_SESSIONS'                        => 1,
    'HDCP_POR_NUM_RECEIVERS'                   => 1,
    'HDCP_SESSION_FEATURE_ENCRYPT'             => 1,
    'HDCP_SESSION_FEATURE_ON'                  => 1,
    'HDCP_SESSION_FEATURE_SIGNATURE'           => 1,
    'HDCP_TARGET_TSEC'                         => 1,
    'HDCP_DISABLE_BUG_1778504'                 => 1,
    'HS_OVERLAYS_ENABLED'                      => 1,
    'OSTASK_ATTRIBUTE_WKRTHD_COUNT'            => 1,
    'RSA'                                      => 1,
    'SEC2CFG_ENGINE_SETUP'                     => 1,
    'USE_RSA_1K'                               => 1,
    'NOUVEAU_SUPPORTED'                        => 1,
    'ACR_RISCV_LS'                             => 1,

    # Playready defines
    'PR_SL3000_ENABLED'                        => 1,
    '__GNUC__'                                 => 1,
    'TRUE'                                     => 1,
    'FALSE'                                    => 1,
    '__arm'                                    => 1,
    '__BUILDMACHINE__'                         => 1,
    '__cplusplus'                              => 1,
    '__cplusplus_cli'                          => 1,
    'DRM_METERCERT_STORE'                      => 1,
    'DRM_STACK_ALLOCATOR'                      => 1,
    'DRM_STACK_ALLOCATORTYPES'                 => 1,
    '__GNUC_MINOR__'                           => 1,
    '__MACINTOSH__'                            => 1,
    '__PRAGMA_WARNING_MACROS__'                => 1,
    '__STDC__'                                 => 1,
    '__WARNING_ANSI_APICALL'                   => 1,
    '__WCHAR_MAX__'                            => 1,
    '_ADDLICENSE_WRITE_THRU'                   => 1,
    '_DATASTORE_WRITE_THRU'                    => 1,
    '_DATASTORE_WRITE_THRU'                    => 1,
    '_M_AMD64'                                 => 1,
    '_M_ARM'                                   => 1,
    '_M_ARM64'                                 => 1,
    '_M_IA64'                                  => 1,
    '_M_IX86'                                  => 1,
    '_M_PPC'                                   => 1,
    '_MANAGED'                                 => 1,
    '_MSC_VER'                                 => 1,
    '_PREFAST_'                                => 1,
    '_TEST_LPROV_CERT_GEN_'                    => 1,
    '_WIN32_WCE'                               => 1,
    '_WIN64'                                   => 1,
    '_XBOX'                                    => 1,
    'ARM'                                      => 1,
    'ARM64'                                    => 1,
    'BIGENDIAN'                                => 1,
    'BIGNUM_EXPENSIVE_DEBUGGING'               => 1,
    'BUILD_NO_METHODS'                         => 1,
    'C'                                        => 1,
    'DBG'                                      => 1,
    'DBLINT_BUILTIN'                           => 1,
    'DRM_64BIT_TARGET'                         => 1,
    'DRM_ACTIVATION_PLATFORM'                  => 1,
    'DRM_API'                                  => 1,
    'DRM_API_DEFAULT'                          => 1,
    'DRM_BITS_PER_BYTE'                        => 1,
    'DRM_BUILD_PLATFORM'                       => 1,
    'DRM_BUILD_PLATFORM_MAC'                   => 1,
    'DRM_BUILD_PROFILE'                        => 1,
    'DRM_BUILD_PROFILE_ANDROID'                => 1,
    'DRM_BUILD_PROFILE_IOS'                    => 1,
    'DRM_BUILD_PROFILE_LINUX'                  => 1,
    'DRM_BUILD_PROFILE_MAC'                    => 1,
    'DRM_BUILD_PROFILE_MPR'                    => 1,
    'DRM_BUILD_PROFILE_OEM'                    => 1,
    'DRM_BUILD_PROFILE_OEMGATES'               => 1,
    'DRM_BUILD_PROFILE_OEMPLAYONLY'            => 1,
    'DRM_BUILD_PROFILE_OEMTEE'                 => 1,
    'DRM_BUILD_PROFILE_OEMTEEPLAYONLY'         => 1,
    'DRM_BUILD_PROFILE_PC'                     => 1,
    'DRM_BUILD_PROFILE_PK'                     => 1,
    'DRM_BUILD_PROFILE_PK_TEST_MAX'            => 1,
    'DRM_BUILD_PROFILE_PK_TEST_MIN'            => 1,
    'DRM_BUILD_PROFILE_PRND_TX_1'              => 1,
    'DRM_BUILD_PROFILE_RMSDK'                  => 1,
    'DRM_BUILD_PROFILE_WP8_1'                  => 1,
    'DRM_BUILDING_CRTIMPLREAL_C'               => 1,
    'DRM_CHAR_BIT'                             => 1,
    'DRM_DBG'                                  => 1,
    'DRM_DWORDS_PER_DIGIT'                     => 1,
    'DRM_ENABLE_TEE_CRITSEC'                   => 1,
    'DRM_EXPORT_APIS_TO_DLL'                   => 1,
    'DRM_EXPORT_VAR'                           => 1,
    'DRM_HDS_COPY_BUFFER_SIZE'                 => 1,
    'DRM_INCLUDE_PK_NAMESPACE_USING_STATEMENT' => 1,
    'DRM_INIT_ALL_STRINGS'                     => 1,
    'DRM_INIT_CHAR_OBFUS'                      => 1,
    'DRM_INIT_WCHAR_OBFUS'                     => 1,
    'DRM_INLINING_SUPPORTED'                   => 1,
    'DRM_ISODD'                                => 1,
    'DRM_KEYFILE_VERSION'                      => 1,
    'DRM_MAX_LICENSE_CHAIN_DEPTH'              => 1,
    'DRM_MSC_VER'                              => 1,
    'DRM_NO_OBFUS'                             => 1,
    'DRM_NO_OF'                                => 1,
    'DRM_NO_OPT'                               => 1,
    'DRM_OBFUS_NEED_PADDING'                   => 1,
    'DRM_PC_TEST_CAPTURE_TEE_HEAP_USAGE'       => 1,
    'DRM_PC_USE_TEE_HEAP_USAGE_DIAGNOSITCS'    => 1,
    'DRM_RADIX_BITS'                           => 1,
    'DRM_RESULT_DEFINED'                       => 1,
    'DRM_SHA1_DIGEST_LEN'                      => 1,
    'DRM_STR_CONST'                            => 1,
    'DRM_SUPPORT_ASSEMBLY'                     => 1,
    'DRM_SUPPORT_DATASTORE_PREALLOC'           => 1,
    'DRM_SUPPORT_DEVICE_SIGNING_KEY'           => 1,
    'DRM_SUPPORT_ECCPROFILING'                 => 1,
    'DRM_SUPPORT_FILE_LOCKING'                 => 1,
    'DRM_SUPPORT_FORCE_ALIGN'                  => 1,
    'DRM_SUPPORT_INLINEDWORDCPY'               => 1,
    'DRM_SUPPORT_MULTI_THREADING'              => 1,
    'DRM_SUPPORT_NATIVE_64BIT_TYPES'           => 1,
    'DRM_SUPPORT_NOLWAULTSIGNING'              => 1,
    'DRM_SUPPORT_PRECOMPUTE_GTABLE'            => 1,
    'DRM_SUPPORT_PRIVATE_KEY_CACHE'            => 1,
    'DRM_SUPPORT_PROFILING'                    => 1,
    'DRM_SUPPORT_REACTIVATION'                 => 1,
    'DRM_SUPPORT_SELWREOEMHAL'                 => 1,
    'DRM_SUPPORT_SELWREOEMHAL_PLAY_ONLY'       => 1,
    'DRM_SUPPORT_SST_REDUNANCY'                => 1,
    'DRM_SUPPORT_TEE'                          => 1,
    'DRM_SUPPORT_TOOLS_NET_IO'                 => 1,
    'DRM_SUPPORT_TRACING'                      => 1,
    'DRM_TEST_LINK_TO_DRMAPI_DLL'              => 1,
    'DRM_TEST_SUPPORT_ACTIVATION'              => 1,
    'DRM_TEST_USE_OFFSET_CLOCK'                => 1,
    'DRM_USE_IOCTL_HAL_GET_DEVICE_INFO'        => 1,
    'DRM_USE_OBFUS_STRUCT_ALIGN'               => 1,
    'DRMASSERT'                                => 1,
    'DRMCASSERT'                               => 1,
    'DRMCRT_memcmp'                            => 1,
    'DRMCRT_memcpy'                            => 1,
    'DRMCRT_memset'                            => 1,
    'DRMMATHSAFEUSEINTRINSICS'                 => 1,
    'ECC_P256_INTEGER_SIZE_IN_DIGITS'          => 1,
    'KMODE_RSA32'                              => 1,
    'MAX_ECTEMPS'                              => 1,
    'mp_mul22s_asm'                            => 1,
    'mp_mul22s_c'                              => 1,
    'mp_mul22u_asm'                            => 1,
    'mp_mul22u_c'                              => 1,
    'MS'                                       => 1,
    'NONE'                                     => 1,
    'NULL'                                     => 1,
    'OEM'                                      => 1,
    'OEM_SELWRE_DIGITTCPY'                     => 1,
    'OEM_SELWRE_DIGITZEROMEMORY'               => 1,
    'OEM_SELWRE_DWORDCPY'                      => 1,
    'OEM_SELWRE_MEMCPY'                        => 1,
    'OEM_SELWRE_MEMCPY_IDX'                    => 1,
    'OEM_SELWRE_ZERO_MEMORY'                   => 1,
    'PROFILE_STACK_SIZE'                       => 1,
    'PROFILE_USE_AGG_SCOPE'                    => 1,
    'PROFILE_USE_SCOPE'                        => 1,
    'PROFILE_USER_DATA'                        => 1,
    'SEC_COMPILE'                              => 1,
    'SEC_USE_CERT_TEMPLATE'                    => 1,
    'SINGLE_METHOD_ID_REPLAY'                  => 1,
    'TARGET_LITTLE_ENDIAN'                     => 1,
    'TARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS' => 1,
    'TRACE'                                    => 1,
    'TRACE_IF_FAILED'                          => 1,
    'UNREFERENCED_PARAMETER'                   => 1,
    'USE_CLAW'                                 => 1,
    'USE_FN_PTRS'                              => 1,
    'USE_MSFT_SW_AES_IMPLEMENTATION'           => 1,
    'USE_PK_NAMESPACES'                        => 1,
    'WINNT'                                    => 1,
    'XBOX'                                     => 1,
    'XBOX/PPC'                                 => 1,

    # playready header filename defines
    '__DRM_DOMAIN_STORE_TYPES_H'               => 1,
    '__DRM_DOMAIN_STORE_TYPES_H'               => 1,
    '_HDS_IMPL_H'                              => 1,
    '_HDS_IMPL_H'                              => 1,
    '__DRM_NONCE_STORE_H'                      => 1,
    '__DRM_NONCE_STORE_H'                      => 1,
    '__DRM_UTF_H'                              => 1,
    '__DRM_UTF_H'                              => 1,
    '__DRM_UTFTYPES_H'                         => 1,
    '__DRM_UTFTYPES_H'                         => 1,
    'BIGDECLS_H'                               => 1,
    'BIGDECLS_H'                               => 1,
    'BIGDEFS_H'                                => 1,
    'BIGDEFS_H'                                => 1,
    'BIGNUM_H'                                 => 1,
    'BIGNUM_H'                                 => 1,
    '__BIGPRIV_H'                              => 1,
    '__BIGPRIV_H'                              => 1,
    'DBLINT_H'                                 => 1,
    'DBLINT_H'                                 => 1,
    'ELWRVE_H'                                 => 1,
    'ELWRVE_H'                                 => 1,
    '__FIELD_H'                                => 1,
    '__FIELD_H'                                => 1,
    '__FIELDPRIV_H'                            => 1,
    '__FIELDPRIV_H'                            => 1,
    '_MPRAND_H'                                => 1,
    '_MPRAND_H'                                => 1,
);

#
# List of supported #defines in selwrescrub
#
my %ifdef_list_selwrescrub = (
    'SELWRESCRUBCFG_ENGINE_SETUP'              => 1,
    'IS_ENHANCED_AES'                          => 1,
    'PKC_ENABLED'                              => 1,
    'IS_SSP_ENABLED'                           => 1,
    'SELWRESCRUB_FIXED_TRNG'                   => 1,
);

#
# List of supported #defines in fub
#
my %ifdef_list_fub = (
    'FUBCFG_ENGINE_SETUP'                       => 1,
);

#
# List of supported #defines in libs
#
my %ifdef_list_libs = (
    'COMPILE_SWI2C'                             => 1,
    'DEBUG_CPU_HDCP22'                          => 1,
    'DMA_NACK_SUPPORTED'                        => 1,
    'DMA_REGION_CHECK'                          => 1,
    'DMA_XFER_ESIZE_MIN_READ_4'                 => 1,
    'DMEM_VA_SUPPORTED'                         => 1,
    'DMREAD_WAR_200142015'                      => 1,
    'DPAUXCFG_ENGINE_SETUP'                     => 1,
    'DPU_COMPILE'                               => 1,
    'DPU_RTOS'                                  => 1,
    'HDCP_TARGET_MSVLD_FIXSTACK_BIGINT_COMPARE' => 1,
    'HDCP22WIREDCFG_ENGINE_SETUP'               => 1,
    'HDCPCFG_ENGINE_SETUP'                      => 1,
    'HS_OVERLAYS_ENABLED'                       => 1,
    'I2CCFG_ENGINE_SETUP'                       => 1,
    'MAX_PKA_SIZE_U32'                          => 1,
    'MRU_OVERLAYS'                              => 1,
    'NULL'                                      => 1,
    'LWDEC_COMPILE'                             => 1,
    'OS_CALLBACKS_WSTATS'                       => 1,
    'OS_CALLBACKS_SINGLE_MODE'                  => 1,
    'OS_DEBUG_STACK_CHECK_ENABLED'              => 1,
    'ON_DEMAND_PAGING_BLK'                      => 1,
    'ON_DEMAND_PAGING_OVL_IMEM'                 => 1,
    'OS_TASK_DBG_STATS'                         => 1,
    'OVL_INDEX_ILWALID'                         => 1,
    'PMU_COMPILE'                               => 1,
    'PMU_RTOS'                                  => 1,
    'RM_FALC_STACK_FILL'                        => 1,
    'SEC2_COMPILE'                              => 1,
    'SEC2_CSB_ACCESS'                           => 1,
    'SEC2_RTOS'                                 => 1,
    'SECFG_ENGINE_SETUP'                        => 1,
    'SIM_BUILD'                                 => 1,
    'TASK_QUEUE_MAP'                            => 1,
    'TASK_RESTART'                              => 1,
    'UNITTEST'                                  => 1,
    'UNUSED_FOR_EC'                             => 1,
    'CSB_DRF_DEF'                               => 1,
    'CSB_DRF_VAL'                               => 1,
    'CSB_FIELD'                                 => 1,
    'CSB_FLD_TEST_DRF_NUM'                      => 1,
    'CSB_REG'                                   => 1,
    'DMA_SUSPENSION'                            => 1,
    'DMTAG_WAR_1845883'                         => 1,
    'DYNAMIC_FLCN_PRIV_LEVEL'                   => 1,
    'FB'                                        => 1,
    'FB_CMD_QUEUES'                             => 1,
    'FBFLCN_BUILD'                              => 1,
    'FPGA_BUILD'                                => 1,
    'GSPLITE_CSB_ACCESS'                        => 1,
    'GSPLITE_RTOS'                              => 1,
    'HDCP22_DEBUG_MODE'                         => 1,
    'HDCP22WIRED_OS_CALLBACK_CENTRALISED'       => 1,
    'HDCP22WIREDCFG_FEATURE_ENABLED'            => 1,
    'HDCPAUTH_HS_BUILD'                         => 1,
    'HW_STACK_LIMIT_MONITORING'                 => 1,
    'IS_SSP_ENABLED'                            => 1,
    'LWDEC_RTOS'                                => 1,
    'USE_CBB'                                   => 1,
    'USE_CSB'                                   => 1,
);

#
# List of supported #defines in RISC-V bootloader
#
my %ifdef_list_rvbl = (
    # Header guards.
    'BOOTLOADER_DEBUG_H'                        => 1,
    'BOOTLOADER_H'                              => 1,
    'BOOTLOADER_MPU_H'                          => 1,
    'DMEM_ADDRS_H'                              => 1,
    'ELF_H'                                     => 1,
    'SSP_H'                                     => 1,
    'STATE_H'                                   => 1,
    'UTIL_H'                                    => 1,

    # Configurations.
    'ELF_IN_PLACE'                              => 1,
    'ELF_IN_PLACE_FULL_IF_NOWPR'                => 1,
    'ELF_IN_PLACE_FULL_ODP_COW'                 => 1,
    'ELF_LOAD_USE_DMA'                          => 1,
    'IS_FUSE_CHECK_ENABLED'                     => 1,
    'IS_PRESILICON'                             => 1,
    'IS_SSP_ENABLED'                            => 1,
    'LWRISCV_DEBUG_PRINT_ENABLED'               => 1,
    'USE_CBB'                                   => 1,
    'USE_CSB'                                   => 1,
    'USE_GSCID'                                 => 1,
    'UTF_TEST'                                  => 1,
    'RUN_ON_SEC'                                => 1,

    # Intrinsics.
    '__riscv_xlen'                              => 1,
    'DMEM_BASE'                                 => 1,
    'DMEM_SIZE'                                 => 1,
    'DMEM_DMA_BUFFER_BASE'                      => 1,
    'DMEM_DMA_BUFFER_SIZE'                      => 1,
    'DMEM_DMA_ZERO_BUFFER_BASE'                 => 1,
    'DMEM_DMA_ZERO_BUFFER_SIZE'                 => 1,
    'DMESG_BUFFER_BASE'                         => 1,
    'DMESG_BUFFER_SIZE'                         => 1,
    'NULL'                                      => 1,
    'LW_PFALCON_FALCON_DMATRFBASE1'             => 1, # MMINTS-TODO: remove once we switch to LIBLWRISCV DMA!
    'LW_RISCV_AMAP_DMEM_START'                  => 1,
    'LW_RISCV_AMAP_SYSGPA_START'                => 1,
    'LW_RISCV_AMAP_FBGPA_START'                 => 1,
    'LW_RISCV_CSR_MSTATUS_VM'                   => 1,
);

#
# List of supported #defines in RISC-V bootstub
#
my %ifdef_list_rvbs = (
    # Header guards.
    'BOOTSTUB_DEBUG_H'                          => 1,
    'BOOTSTUB_DMEM_ADDRS_H'                     => 1,
    'BOOTSTUB_H'                                => 1,
    'BOOTSTUB_LOAD_H'                           => 1,
    'BOOTSTUB_MPU_H'                            => 1,
    'BOOTSTUB_SSP_H'                            => 1,
    'BOOTSTUB_UTIL_H'                           => 1,

    # Configurations.
    'DMEM_BASE'                                 => 1,
    'DMEM_CONTENTS_END'                         => 1,
    'DMEM_CONTENTS_SIZE'                        => 1,
    'DMEM_END_CARVEOUT_SIZE'                    => 1,
    'DMEM_SIZE'                                 => 1,
    'DMEM_STACK_BASE'                           => 1,
    'DMEM_STACK_CANARY_BASE'                    => 1,
    'IS_FUSE_CHECK_ENABLED'                     => 1,
    'IS_SSP_ENABLED'                            => 1,
    'LWRISCV_DEBUG_PRINT_ENABLED'               => 1,
    'RETURN_FLAG_BASE'                          => 1,
    'RETURN_FLAG_SIZE'                          => 1,
    'STACK_CANARY_SIZE'                         => 1,
    'STACK_SIZE'                                => 1,

    # Intrinsics.
    '__riscv_xlen'                              => 1,
    'NULL'                                      => 1,
    'LW_RISCV_AMAP_DMEM_START'                  => 1,
);

#
# List of supported #defines in SEC2_RISCV
#
my %ifdef_list_sec2_riscv = (

    # sec2 common defines
    'SCHEDULER_ENABLED'                        => 1,
    'DO_TASK_RM_CMDMGMT'                       => 1,
    'DO_TASK_RM_MSGMGMT'                       => 1,
    'DO_TASK_TEST'                             => 1,
    'DO_TASK_RM_CHNLMGMT'                      => 1,
    'DO_TASK_WORKLAUNCH'                       => 1,
    'DO_TASK_SCHEDULER'                        => 1,
    'STAGING_TEMPORARY_CODE'                   => 1,
    'LW_RISCV_AMAP_SYSGPA_START'               => 1,
    'LW_RISCV_AMAP_FBGPA_START'                => 1,
    'ODP_ENABLED'                              => 1,
    'LW_RISCV_CSR_MUCOUNTEREN'                 => 1,
    'USE_CBB'                                  => 1,
    'EXCLUDE_LWOSDEBUG'                        => 1,
    '__ASSEMBLER__'                            => 1,
     COT_ENABLED                               => 1,

);

#
# List of supported #defines in SOE_RISCV
#
my %ifdef_list_soe_riscv = (

    # soe common defines
    'SCHEDULER_ENABLED'                        => 1,
    'DO_TASK_RM_CMDMGMT'                       => 1,
    'DO_TASK_RM_MSGMGMT'                       => 1,
    'DO_TASK_TEST'                             => 1,
    'DO_TASK_RM_CHNLMGMT'                      => 1,
    'DO_TASK_WORKLAUNCH'                       => 1,
    'DO_TASK_SCHEDULER'                        => 1,
    'HAS_BOOT_ARGUMENTS'                       => 1,
    'STAGING_TEMPORARY_CODE'                   => 1,
    'LW_RISCV_AMAP_SYSGPA_START'               => 1,
    'LW_RISCV_AMAP_FBGPA_START'                => 1,
    'ODP_ENABLED'                              => 1,
    'LW_RISCV_CSR_MUCOUNTEREN'                 => 1,
    'USE_CBB'                                  => 1,
    'EXCLUDE_LWOSDEBUG'                        => 1,
    '__ASSEMBLER__'                            => 1,
    'DO_TASK_RM_THERM'                         => 1,
    'DO_TASK_RM_CORE'                          => 1,
    'DO_TASK_RM_SPI'                           => 1,
    'DO_TASK_RM_BIF'                           => 1,
    'DO_TASK_RM_SMBPBI'                        => 1,
    'DMA_SUSPENSION'                           => 1,

);

my %chip_list = (

    # Chips in ACR/SEC2
    'GM20X' => 1,
    'GP10X' => 1,
    'GV10X' => 1,
    'TU10X' => 1,
    'GA100' => 1,

    # Chips in ACR/SEC2
    'GM20B' => 1,
    'GP100' => 1,
    'GP10B' => 1,
    'T18X'  => 1,
    'T21X'  => 1,

    # Features in ACR
    'ACR_USING_SEC2_MUTEX' => 1,

    # Features in SEC2
    'SEC2_EMEM_APERTURE_BAR0_ACCESS' => 1,
    'SEC2_HDCPMC_VERIF'              => 1,
    'BUG_200272812_HALT_INTR_MASK_WAR' => 1,
    'DBG_PRINTF_FALCDEBUG' => 1,
    'DBG_PRINTF_FALCTRACE' => 1,
    'SEC2TASK_GFE' => 1,
    'SEC2TASK_LWSR' => 1,
    'SEC2TASK_HWV' => 1,
    'SEC2TASK_PR' => 1,
    'SEC2TASK_VPR' => 1,
    'SEC2TASK_HDCP22WIRED' => 1,
    'SEC2TASK_HDCP1X' => 1,
    'SEC2TASK_ACR' => 1,
    'SEC2TASK_HDCPMC' => 1,
    'SEC2TASK_APM' => 1,
    'SEC2TASK_SPDM' => 1,
);


#
# Check if the chip/feature parsed is a valid(known)
#
sub checkIfChipOrFeatureIsValid
{
    my $define = shift;
    my $fileIndex = shift;
    my $lineNumber = shift;

    if ($chip_list{"$define"} != 1)
    {
        print "Wrong chip/feature used $define in file $files[$fileIndex] +$lineNumber\n";
    }
}


#
# Check if the define parsed is a valid(known)
#
sub checkIfDefineIsValid
{
    my $define = shift;
    my $fileIndex = shift;
    my $lineNumber = shift;

    if ($ifdef_list{"$define"} != 1 and index($files[$fileIndex], 'src/_out') == -1)
    {
        die "Error: Wrong define used $define in file $files[$fileIndex] +$lineNumber\n";
    }
}


#
# Check which type of files are required to parse
#
sub wanted
{
    if (($File::Find::name =~ /\.h$/) || ($File::Find::name =~ /\.c$/))
    {
        if (($File::Find::name =~ 'g_') || ($File::Find::name =~ 'gt_'))
        {
            next;
        }
        push (@files, $File::Find::name);
    }
}


#
# Get the files in the directory
#
find(\&wanted, $directory);

#
# Get the list of supported defines
#
if ($dir eq 'acr')
{
    %ifdef_list = %ifdef_list_acr;
}
elsif ($dir eq 'auto_acr')
{
    %ifdef_list = %ifdef_list_autoacr;
}
elsif ($dir eq 'sec2')
{
    %ifdef_list = %ifdef_list_sec2;
}
elsif ($dir eq 'sec2_riscv')
{
    %ifdef_list = %ifdef_list_sec2_riscv;
}
elsif ($dir eq 'soe_riscv')
{
    %ifdef_list = %ifdef_list_soe_riscv;
}
elsif ($dir eq 'selwrescrub')
{
    %ifdef_list = %ifdef_list_selwrescrub;
}
elsif ($dir eq 'fub')
{
    %ifdef_list = %ifdef_list_fub;
}
elsif ($dir eq 'libs')
{
    %ifdef_list = %ifdef_list_libs;
}
elsif ($dir eq 'lwriscv/bootloader')
{
    %ifdef_list = %ifdef_list_rvbl;
}
elsif ($dir eq 'lwriscv/bootstub')
{
    %ifdef_list = %ifdef_list_rvbs;
}
else
{
    print "Unsupported directory\n";
}


#
# Parse through each file in given directory and check for defines
#
for (my $i=0; $i < @files; $i++)
{
    open (my $suData, '<', $files[$i]) or die "Could not open '$files[$i]' $!\n";

    my $lineNumber=0;

    # Parsing line by line
    while (my $line = <$suData>)
    {
        $lineNumber++;
        if (($line =~ '#if') || ($line =~ '#elif') || ($line =~ '#else') || ($line =~ '#endif'))
        {
            if ($line =~ 'def ')                            # normal ifdef pattern
            {
                my @field1 = split "def ", $line;
                my @field2 = split " ", $field1[1];

                if ($field2[0] =~ '_H')
                {
                    next;
                }

                checkIfDefineIsValid($field2[0], $i, $lineNumber);
            }
            elsif ($line =~ 'defined')                      # patter line #if defined
            {
                my @field3 = split(/\(/, $line);

                for (my $j=1; $j < @field3; $j++)
                {
                    my @field4 = split(/\)/, $field3[$j]);

                    for (my $p=0; $p < @field4; $p++)
                    {
                        my @field5 = split " ", $field4[$p];

                        for (my $n=0; $n < @field5; $n++)
                        {
                            if (($field5[$n] eq '==') || ($field5[$n] eq '!=') || ($field5[$n] eq '<=') || ($field5[$n] eq '>=') || ($field5[$n] eq '&&') ||
                                ($field5[$n] eq '||') || ($field5[$n] eq '>')  || ($field5[$n] eq '<')  || ($field5[$n] eq '-')  || ($field5[$n] eq '(')  ||
                                ($field5[$n] eq ')' ) || ($field5[$n] eq "'"))
                            {
                                next;
                            }

                            if ($field5[$n] =~ '[0-9a-z]+')
                            {
                                if ($field5[$n] !~ '_')
                                {
                                    next;
                                }
                            }

                            if (($field5[$n] =~ '_H') || ($field5[$n] =~ '\/\*') || ($field5[$n] =~ '\*\/') || ($field5[$n] =~ '\/\/'))
                            {
                                next;
                            }

                            if ($field5[$n] =~ /\\/)
                            {
                                next;
                            }

                            checkIfDefineIsValid($field5[$n], $i, $lineNumber);
                        }
                    }
                }
            }
            elsif ($line =~ 'ENABLED\(')                    # pattern like ***CFG_CHIP_ENABLED
            {
                my @field6  = split "ENABLED", $line;
                my @field7  = split(/\(/, $field6[1]);
                my @field8  = split(/\)/, $field7[1]);

                checkIfChipOrFeatureIsValid($field8[0], $i, $lineNumber);
            }
            else                                            # defines inside comments
            {
                my @field9  = split " ", $line;

                for (my $k=1; $k < @field9; $k++)
                {
                    if (($field9[$k] eq '#if') || ($field9[$k] eq '#ifndef') || ($field9[$k] eq '#else') || ($field9[$k] =~ '#elif') || ($field9[$k] eq '#endif'))
                    {
                        next;
                    }

                    if (($field9[$k] eq '==') || ($field9[$k] eq '!=') || ($field9[$k] eq '<=') || ($field9[$k] eq '>=') || ($field9[$k] eq '&&') ||
                        ($field9[$k] eq '||') || ($field9[$k] eq '>')  || ($field9[$k] eq '<')  || ($field9[$k] eq '-')  || ($field9[$k] eq '+')  ||
                        ($field9[$k] eq '(')  || ($field9[$k] eq ')' ) || ($field9[$k] eq '&')  || ($field9[$k] eq '%'))
                    {
                        next;
                    }

                    if (($field9[$k] =~ '_H') || ($field9[$k] =~ '\/\*') || ($field9[$k] =~ '\*\/') || ($field9[$k] =~ '\/\/'))
                    {
                        next;
                    }

                    if ($field9[$k] =~ '[0-9a-z]+')
                    {
                        if ($field15[$n] !~ '_')
                        {
                            next;
                        }
                    }

                    if ($field9[$k] =~ '\(')
                    {
                        my @field10  = split(/\(/, $field9[$k]);

                        for (my $m=1; $m < @field10; $m++)
                        {
                            if ($field10[$m] eq '')
                            {
                                next;
                            }

                            my @field11  = split(/\)/, $field10[$m]);

                            checkIfDefineIsValid($field11[0], $i, $lineNumber);
                        }
                    }
                    elsif ($field9[$k] =~ '\)')
                    {
                        my @field11 = split(/\)/, $field9[$k]);

                        if ($field11[0] eq '')
                        {
                            print "Error: Syntax not proper in file $files[$i]\t\t\t\t$line\n";
                        }
                        else
                        {
                            checkIfDefineIsValid($field11[0], $i, $lineNumber);
                        }
                    }
                    else
                    {
                        my $temp;
                        if ($field9[$k] =~ '!')
                        {
                            my @tempArray = split "!", $field9[$k];
                            $temp = $tempArray[1];
                        }
                        else
                        {
                            $temp = $field9[$k];
                        }

                        checkIfDefineIsValid($temp, $i, $lineNumber);
                    }
                }
            }
        }
    }
    close ($files[$i]);
}
