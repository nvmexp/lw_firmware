This is skeleton for Confidential Compute.

TOC:
1. Introduction
2. Boot flow
3. How to run
4. Build Requirements
5. Build Instructions && Output(s)
6. Integrating custom libraries, exports etc.

--------------------------------------------------------------------------------
1. Introduction
--------------------------------------------------------------------------------
It has lwrrently 6 partition stubs:
- RM Proxy
- RM (for full GSP-RM, mutually exlusive with RM Proxy)
- SPDM
- ACR
- Init
- Attestation

Partition stub structure:
├── entry.S
^ Entry point to partition, should be single function (partition_<name>_return).
This function should setup stack and exception vector and possibly restore context.
Then jump either to restored context or to C entry point.

├── main.c
^ Partition C entry point, and any code
├── rules.mk
^ Extra build rules. For example if partition has submakes, they should be added
here. Or extra cflags. See ACR for example (it builds external library)

└── sources.mk
^ List of sources in partition (that are in partition folder, not external libraries
that are linked with partition).
It is possible to add/remove files conditionally, for example:
ifeq ($(LWRISCV_IS_CHIP_FAMILY),ampere)
  objs-y += code-ga10x.o
endif

--------------------------------------------------------------------------------
2. Boot flow
--------------------------------------------------------------------------------
Init -> ACR -> RM

--------------------------------------------------------------------------------
3. How to run
--------------------------------------------------------------------------------
Use lwwatch command "rv brb" to side load images to GSP on ga10x or GH100.
As of 2020-12-02, GH100 emulation is not supported.

Expected output (gh100 fmodel):
(gdb) rv brb c/g_cc_gsp_gh100.text.encrypt.bin c/g_cc_gsp_gh100.data.encrypt.bin c/g_cc_gsp_gh100.manifest.encrypt.bin.out.bin
WARNING: [207871] Build Dec  1 2020 16:29:44 on GSP RISC-V. Args: brb c/g_cc_gsp_gh100.text.encrypt.bin c/g_cc_gsp_gh100.data.encrypt.bin c/g_cc_gsp_gh100.manifest.encrypt.bin.out.bin
WARNING: [207871] Resetting core *AND* SE...
WARNING: [207871] Issuing reset to GSP... Done.
WARNING: [207906] Issuing reset to SE...  Done.
WARNING: [207913] Configuring bcr...
WARNING: [207914] Loading images...
WARNING: [207914] .text image: c/g_cc_gsp_gh100.text.encrypt.bin
WARNING: [207922] Reading 28672 bytes from file.
WARNING: [207928] Writing 28672 bytes at 0x0 using PMB.
WARNING: [229436] .data image: c/g_cc_gsp_gh100.data.encrypt.bin
WARNING: [229444] Reading 8704 bytes from file.
WARNING: [229450] Writing 8704 bytes at 0x0 using PMB.
WARNING: [235982] manifest image: c/g_cc_gsp_gh100.manifest.encrypt.bin.out.bin
WARNING: [235990] Reading 2560 bytes from file.
WARNING: [235996] Writing 2560 bytes at 0xf600 using PMB.
WARNING: [237920] Starting core...
WARNING: [237935] Starting RISC-V in BROM mode...
Warning: [ lwclk=237937 falcon . riscv_core.cc 846 ] Write RISCV_BCR_CTRL is dropped for RISCV_BCR_CTRL.VALID==1
WARNING: [237938] Waiting for BROM to finish...
len_b_l:3800 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:1a0 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:340 len_b_h:0 pm_len: 00000080 block_nb:1
 len_b_l:49400 len_b_h:0 pm_len: 00000080 block_nb:1
 Warning: [ lwclk=238569 falcon . processor.cc 680 ] Exception trap_instruction_access_fault, epc 0x000000000008edc8, cause2 n/a(0x0)
Warning: [ lwclk=238569 falcon . processor.cc 682 ]           tval 0x000000000008edc8
Warning: [ lwclk=238569 falcon . prgn_dev_map.cc 132 ] sec_pegerine_device_map device map check fail: reg = 00005c00 is_read=0 msu=1 dmap_group=6 error_info=0xbadf5611!
WARNING: [238579] BROM finished with result: PASS
WARNING: [238579] Core started... status:
WARNING: [238583] Bootrom Configuration: 0x00000011 (VALID) core: RISC-V brfetch: DISABLED
WARNING: [238587] Bootrom DMA configuration: 0x00000000 target: 0x0(localFB) UNLOCKED
WARNING: [238591] RISCV priv lockdown is DISABLED
WARNING: [238595] Bootrom DMA SEC configuration: 0x00000000 wprid: 0x0
WARNING: [238599] RETCODE: 0x0003ff37 Result: PASS Phase: CONFIGURE_FMC_ELW Syndrome: OK Info:INIT
WARNING: [238607] FMC Code addr: 0x0000000000100000
WARNING: [238615] FMC Data addr: 0x0000000000000000
WARNING: [238623] PKC addr     : 0x0000000000000000
WARNING: [238631] PUBKEY addr  : 0x0000000000000000
(gdb) rv dmesg
WARNING: [238631] Build Dec  1 2020 16:29:44 on GSP RISC-V. Args: dmesg
WARNING: [238653] Found debug buffer at 0xf000, size 0xff0, w 722, r 0, magic f007ba11
WARNING: [239379] _riscvReadTcm_TU10X: doing unaligned tail reads (2 bytes).
WARNING: [239393] DMESG buffer
--------
Hello from init partition.. Doing very important stuff..
My SSPM is 200f1 ucode id 0
Important stuff done. Switching to ACR...
Hello from ACR partition.
Here I will initialize stuff and then call into RM.
My args are 0 0 0 0
My SSPM is 200f1 ucode id 0
Switching to RM partition.
Welcome to RM partition. Called from 1
My args are 0 0 0 0
My SSPM is 11 ucode id 0
And now some test calling ACR...
Switching 0
ACR called from partition 4.
My args are 10 4 5 6
Returning back.
Returned 0: 10
Switching 1
ACR called from partition 4.
My args are 11 4 5 6
Returning back.
Returned 1: 11
Switching 2
ACR called from partition 4.
My args are 12 4 5 6
Returning back.
Returned 2: 12
Test seems completed. I think...
Entering ICD


--------------------------------------------------------------------------------
4. Build Requirements
--------------------------------------------------------------------------------
There are two ways to build ucode. One is using prebuilt libraries, other
building everything from scratch.

a) Prebuilts
- Only build on Linux is supported (for now)
- LW_SOURCE, LW_TOOLS and P4ROOT need to be set correctly

b) Everything
- chips_a tree must have //sw/lwriscv/main included ($P4ROOT/sw/lwriscv/...)
- ADA community toolchain is required ($LW_TOOLS/riscv/adacore/...)

--------------------------------------------------------------------------------
5. Build Instructions && Output(s)
--------------------------------------------------------------------------------
By default, prebuilt RISCV SDK and SK is used. There is no need to do full build
unless manifest or partition policies were changed (or RISC-V SDK).

1. make or lwmake in top level directory builds both ga10x and gh100 profiles
2. "Important" output files are placed in drivers/resman/kernel/inc/gspcc/bin:
bin
├── g_cc_gsp_ga10x.data.encrypt.bin
├── g_cc_gsp_ga10x.manifest.encrypt.bin.out.bin
├── g_cc_gsp_ga10x.text.encrypt.bin
├── g_cc_gsp_gh100.data.encrypt.bin
├── g_cc_gsp_gh100.manifest.encrypt.bin.out.bin
├── g_cc_gsp_gh100.text.encrypt.bin
^ SK + Partitions images, can be side-loaded by lwwatch

├── g_cc_gsp_ga10x_desc.bin
├── g_cc_gsp_ga10x_image.bin
├── g_cc_gsp_gh100_desc.bin
├── g_cc_gsp_gh100_image.bin
^ RM-compatible images (to be loaded via bindata)

├── g_manifest_gsp_ga10x_riscv.hex
├── g_manifest_gsp_gh100_riscv.hex
^ Binary hex of both manifests

├── g_partitions_gsp_ga10x_riscv.elf
├── g_partitions_gsp_ga10x_riscv.objdump
├── g_partitions_gsp_ga10x_riscv.readelf
├── g_partitions_gsp_gh100_riscv.elf
├── g_partitions_gsp_gh100_riscv.objdump
├── g_partitions_gsp_gh100_riscv.readelf
^ Linked partitions code. Useful for debugging if crash happens while in partitions

├── g_sepkern_gsp_ga10x_riscv.elf
├── g_sepkern_gsp_ga10x_riscv.objdump
├── g_sepkern_gsp_ga10x_riscv.readelf
├── g_sepkern_gsp_gh100_riscv.elf
├── g_sepkern_gsp_gh100_riscv.objdump
└── g_sepkern_gsp_gh100_riscv.readelf
^ SK code, useful for debugging if crash happens in SK

3. Build is done in _out/ga10x|gh100_gsp/ folders:
_out/ga10x_gsp/
├── partitions
│   ├── acr
│   ├── attestation
│   ├── init
│   ├── rm_proxy
│   ├── shared
│   └── spdm
^ Compiled partition code (every partition is compiled into libpart_<name>.a,
then everything is linked together).

├── sdk
│   └── src
│       └── libc
^ risc-v sdk build directory (only for full build)

├── sk
^ SK build directory (only for full build)

└── staging
    └── sdk (only for full build)
        ├── inc
        │   └── liblwriscv
        └── lib
^ "Staging" directory where build is finalized, risc-v sdk is installed etc.

--------------------------------------------------------------------------------
6. Integrating custom libraries, exports etc.
--------------------------------------------------------------------------------
1. Adding code to partitions
Add new files to partition folder and then update sources.mk
For now subdirectories will likely not work.

2. HAL
At CC skeleton level, there is no HAL's or chip_config.
Coarse control is possible at file level by changing partitions source.mk file.
