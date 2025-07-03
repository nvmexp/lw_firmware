FMC-package for PMU GA10B, contains SK and RTOS springboard.

# Total fmc-package steady-state size (counting 0x1000 SK imem and 0x1000 SK dmem):
# IMEM - 0x1000
# DMEM - 0x1000
#
# IMEM layout (LW_RISCV_AMAP_IMEM_START = 0x100000):
# 0x100000:0x101000 - SK code
# 0x101000:0x12A000 - RTOS code
#
# DMEM layout (LW_RISCV_AMAP_DMEM_START = 0x180000):
# 0x180000:0x181000 - SK data
# 0x181000:0x1AC800 - RTOS data (includes print buffer)
# 0x1AC400:0x1AC800 - RTOS print buffer


RTOS PMP settings (non-inst):

rv pmp
Build Dec  8 2021 15:56:08 on PMU RISC-V. Args: pmp
Core PMP registers:
 0 0x0000000000100000-0x0000000000100fff  unlocked mode:napot ---
 1 0x0000000000180000-0x0000000000180fff  unlocked mode:napot ---
 2 0x0000000001400000-0x00000000015fffff    locked mode:napot rw-
 3 0x2000000000000000-0x2000000003ffffff    locked mode:napot rw-
 4 0x0000000000100000-0x000000000017ffff    locked mode:napot --x
 5 0x0000000000180000-0x00000000001fffff    locked mode:napot rw-
 6 0x7400000000000000-0x77ffffffffffffff    locked mode:napot rwx
 7 0x9400000000000000-0x97ffffffffffffff    locked mode:napot r--
 8 0x0000000000000000                     unlocked mode:off   ---
 9 0x0000000000000000                     unlocked mode:off   ---
10 0x0000000000000000                     unlocked mode:off   ---
11 0x0000000000000000                     unlocked mode:off   ---
12 0x0000000000000000                     unlocked mode:off   ---
13 0x0000000000000000                     unlocked mode:off   ---
14 0x0000000000000000                     unlocked mode:off   ---
15 0x0000000000000000                     unlocked mode:off   ---
16 0x0000000000000000                     unlocked mode:off   ---
17 0x0000000000000000                     unlocked mode:off   ---
18 0x0000000000000000                     unlocked mode:off   ---
19 0x0000000000000000                     unlocked mode:off   ---
20 0x0000000000000000                     unlocked mode:off   ---
21 0x0000000000000000                     unlocked mode:off   ---
22 0x0000000000000000                     unlocked mode:off   ---
23 0x0000000000000000                     unlocked mode:off   ---
24 0x0000000000000000                     unlocked mode:off   ---
25 0x0000000000000000                     unlocked mode:off   ---
26 0x0000000000000000                     unlocked mode:off   ---
27 0x0000000000000000                     unlocked mode:off   ---
28 0x0000000000000000                     unlocked mode:off   ---
29 0x0000000000080000                     unlocked mode:off   ---
30 0x0000000000080000-0x000000000008ffff  unlocked mode:tor   ---
31 0x0000000000000000-0xffffffffffffffff    locked mode:napot ---

rv iopmp
Build Dec  8 2021 15:56:08 on PMU RISC-V. Args: iopmp
IO-PMP registers:
 0 0x0000000000100000-0x0000000000100fff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
 1 0x0000000000180000-0x0000000000180fff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
 2 0x0000000000100000-0x000000000017ffff    locked mode:napot w- sources: FBDMA_IMEM
 3 0x0000000000180000-0x00000000001fffff    locked mode:napot wr sources: FBDMA_DMEM CPDMA
 4 0x00000000001ac400-0x00000000001ac7ff    locked mode:napot -r sources: PMB:0x0f
 5 0x00000000000001fc                     unlocked mode:off   -- sources: none
 6 0x00000000000001fc                     unlocked mode:off   -- sources: none
 7 0x00000000000001fc                     unlocked mode:off   -- sources: none
 8 0x00000000000001fc                     unlocked mode:off   -- sources: none
 9 0x00000000000001fc                     unlocked mode:off   -- sources: none
10 0x00000000000001fc                     unlocked mode:off   -- sources: none
11 0x00000000000001fc                     unlocked mode:off   -- sources: none
12 0x00000000000001fc                     unlocked mode:off   -- sources: none
13 0x00000000000001fc                     unlocked mode:off   -- sources: none
14 0x00000000000001fc                     unlocked mode:off   -- sources: none
15 0x00000000000001fc                     unlocked mode:off   -- sources: none
16 0x00000000000001fc                     unlocked mode:off   -- sources: none
17 0x00000000000001fc                     unlocked mode:off   -- sources: none
18 0x00000000000001fc                     unlocked mode:off   -- sources: none
19 0x00000000000001fc                     unlocked mode:off   -- sources: none
20 0x00000000000001fc                     unlocked mode:off   -- sources: none
21 0x00000000000001fc                     unlocked mode:off   -- sources: none
22 0x00000000000001fc                     unlocked mode:off   -- sources: none
23 0x00000000000001fc                     unlocked mode:off   -- sources: none
24 0x00000000000001fc                     unlocked mode:off   -- sources: none
25 0x00000000000001fc                     unlocked mode:off   -- sources: none
26 0x00000000000001fc                     unlocked mode:off   -- sources: none
27 0x00000000000001fc                     unlocked mode:off   -- sources: none
28 0x00000000000001fc                     unlocked mode:off   -- sources: none
29 0x00000000000001fc                     unlocked mode:off   -- sources: none
30 0x0000000000100000-0x000000000017ffff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
31 0x0000000000100000-0x000000000017ffff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff


RTOS PMP settings (inst/PL0):

rv pmp
Build Dec  8 2021 15:56:08 on PMU RISC-V. Args: pmp
Core PMP registers:
 0 0x0000000000100000-0x0000000000100fff  unlocked mode:napot ---
 1 0x0000000000180000-0x0000000000180fff  unlocked mode:napot ---
 2 0x0000000001400000-0x00000000015fffff    locked mode:napot rw-
 3 0x2000000000000000-0x2000000003ffffff    locked mode:napot rw-
 4 0x0000000000100000-0x000000000017ffff    locked mode:napot --x
 5 0x0000000000180000-0x00000000001fffff    locked mode:napot rw-
 6 0x7400000000000000-0x77ffffffffffffff    locked mode:napot rwx
 7 0x9400000000000000-0x97ffffffffffffff    locked mode:napot rwx
 8 0x0000000000000000                     unlocked mode:off   ---
 9 0x0000000000000000                     unlocked mode:off   ---
10 0x0000000000000000                     unlocked mode:off   ---
11 0x0000000000000000                     unlocked mode:off   ---
12 0x0000000000000000                     unlocked mode:off   ---
13 0x0000000000000000                     unlocked mode:off   ---
14 0x0000000000000000                     unlocked mode:off   ---
15 0x0000000000000000                     unlocked mode:off   ---
16 0x0000000000000000                     unlocked mode:off   ---
17 0x0000000000000000                     unlocked mode:off   ---
18 0x0000000000000000                     unlocked mode:off   ---
19 0x0000000000000000                     unlocked mode:off   ---
20 0x0000000000000000                     unlocked mode:off   ---
21 0x0000000000000000                     unlocked mode:off   ---
22 0x0000000000000000                     unlocked mode:off   ---
23 0x0000000000000000                     unlocked mode:off   ---
24 0x0000000000000000                     unlocked mode:off   ---
25 0x0000000000000000                     unlocked mode:off   ---
26 0x0000000000000000                     unlocked mode:off   ---
27 0x0000000000000000                     unlocked mode:off   ---
28 0x0000000000000000                     unlocked mode:off   ---
29 0x0000000000080000                     unlocked mode:off   ---
30 0x0000000000080000-0x000000000008ffff  unlocked mode:tor   ---
31 0x0000000000000000-0xffffffffffffffff    locked mode:napot ---

rv iopmp
Build Dec  8 2021 15:56:08 on PMU RISC-V. Args: iopmp
IO-PMP registers:
 0 0x0000000000100000-0x0000000000100fff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
 1 0x0000000000180000-0x0000000000180fff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
 2 0x0000000000100000-0x000000000017ffff    locked mode:napot w- sources: FBDMA_IMEM
 3 0x0000000000180000-0x00000000001fffff    locked mode:napot wr sources: FBDMA_DMEM CPDMA
 4 0x00000000001ac400-0x00000000001ac7ff    locked mode:napot -r sources: PMB:0x0f
 5 0x00000000000001fc                     unlocked mode:off   -- sources: none
 6 0x00000000000001fc                     unlocked mode:off   -- sources: none
 7 0x00000000000001fc                     unlocked mode:off   -- sources: none
 8 0x00000000000001fc                     unlocked mode:off   -- sources: none
 9 0x00000000000001fc                     unlocked mode:off   -- sources: none
10 0x00000000000001fc                     unlocked mode:off   -- sources: none
11 0x00000000000001fc                     unlocked mode:off   -- sources: none
12 0x00000000000001fc                     unlocked mode:off   -- sources: none
13 0x00000000000001fc                     unlocked mode:off   -- sources: none
14 0x00000000000001fc                     unlocked mode:off   -- sources: none
15 0x00000000000001fc                     unlocked mode:off   -- sources: none
16 0x00000000000001fc                     unlocked mode:off   -- sources: none
17 0x00000000000001fc                     unlocked mode:off   -- sources: none
18 0x00000000000001fc                     unlocked mode:off   -- sources: none
19 0x00000000000001fc                     unlocked mode:off   -- sources: none
20 0x00000000000001fc                     unlocked mode:off   -- sources: none
21 0x00000000000001fc                     unlocked mode:off   -- sources: none
22 0x00000000000001fc                     unlocked mode:off   -- sources: none
23 0x00000000000001fc                     unlocked mode:off   -- sources: none
24 0x00000000000001fc                     unlocked mode:off   -- sources: none
25 0x00000000000001fc                     unlocked mode:off   -- sources: none
26 0x00000000000001fc                     unlocked mode:off   -- sources: none
27 0x00000000000001fc                     unlocked mode:off   -- sources: none
28 0x00000000000001fc                     unlocked mode:off   -- sources: none
29 0x00000000000001fc                     unlocked mode:off   -- sources: none
30 0x0000000000100000-0x000000000017ffff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
31 0x0000000000100000-0x000000000017ffff    locked mode:napot -- sources: FBDMA_IMEM FBDMA_DMEM CPDMA SHA PMB:0x7fff
