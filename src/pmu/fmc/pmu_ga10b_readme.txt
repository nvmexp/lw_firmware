FMC-package for PMU GA10B, contains SK, RTOS springboard and PMU CheetAh-ACR partition.

# Total fmc-package steady-state size (counting 0x1000 SK imem and 0x1000 SK dmem):
# IMEM - 0x4000
# DMEM - 0x2000
#
# IMEM layout (LW_RISCV_AMAP_IMEM_START = 0x100000):
# 0x100000:0x101000 - SK code
# 0x101000:0x102C00 - CheetAh-ACR code
# 0x102C00:0x104000 - Shared LIBLWRISCV code
# 0x104000:0x110000 - RTOS code
#
# DMEM layout (LW_RISCV_AMAP_DMEM_START = 0x180000):
# 0x180000:0x181000 - SK data
# 0x181000:0x181400 - CheetAh-ACR RO data
# 0x181400:0x182000 - CheetAh-ACR RW data
# 0x182000:0x182000 - Shared RO data + RTOS RO data (lwrrently empty)
# 0x182000:0x190000 - RTOS data (includes print buffer)
# 0x18fc00:0x190000 - RTOS print buffer


Actual RTOS PMP settings (non-inst):

rv pmp
Build Nov 25 2021 16:23:53 on PMU RISC-V. Args: pmp
Core PMP registers:
 0 0x0000000000100000-0x0000000000100fff  unlocked mode:napot ---
 1 0x0000000000180000-0x0000000000180fff  unlocked mode:napot ---
 2 0x0000000001400000-0x00000000015fffff    locked mode:napot rw-
 3 0x2000000000000000-0x2000000003ffffff    locked mode:napot rw-
 4 0x0000000000102c00                       locked mode:off   ---
 5 0x0000000000102c00-0x0000000000103fff    locked mode:tor   --x
 6 0x0000000000000000                     unlocked mode:off   ---
 7 0x0000000000000000                     unlocked mode:off   ---
 8 0x0000000000104000                     unlocked mode:off   ---
 9 0x0000000000104000-0x000000000017ffff  unlocked mode:tor   --x
10 0x0000000000182000                     unlocked mode:off   ---
11 0x0000000000182000-0x00000000001fffff  unlocked mode:tor   rw-
12 0x8060000000000000                     unlocked mode:off   ---
13 0x8060000000000000-0x807fffffffffffff  unlocked mode:tor   rwx
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
29 0x0000000000000000                     unlocked mode:off   ---
30 0x0000000000080000-0x00000000000bffff  unlocked mode:napot ---
31 0x0000000000000000-0xffffffffffffffff    locked mode:napot ---

rv iopmp
Build Nov 25 2021 16:23:53 on PMU RISC-V. Args: iopmp
IO-PMP registers:
 0 0x0000000000100000-0x0000000000103fff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
 1 0x0000000000180000-0x0000000000180fff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
 2 0x00000000000001fc                     unlocked mode:off   -- sources: none
 3 0x00000000000001fc                     unlocked mode:off   -- sources: none
 4 0x00000000000001fc                     unlocked mode:off   -- sources: none
 5 0x00000000000001fc                     unlocked mode:off   -- sources: none
 6 0x00000000000001fc                     unlocked mode:off   -- sources: none
 7 0x00000000000001fc                     unlocked mode:off   -- sources: none
 8 0x0000000000181000-0x0000000000181fff  unlocked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
 9 0x0000000000100000-0x000000000010ffff  unlocked mode:napot w- sources: FBDMA
10 0x0000000000180000-0x000000000018ffff  unlocked mode:napot wr sources: FBDMA CPDMA
11 0x000000000018fc00-0x000000000018ffff  unlocked mode:napot -r sources: PMB:0xff
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
30 0x0000000000100000-0x000000000017ffff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
31 0x0000000000180000-0x00000000001fffff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff


Actual CheetAh-ACR PMP settings (non-inst):

rv pmp
Build Nov 25 2021 16:23:53 on PMU RISC-V. Args: pmp
Core PMP registers:
 0 0x0000000000100000-0x0000000000100fff  unlocked mode:napot ---
 1 0x0000000000180000-0x0000000000180fff  unlocked mode:napot ---
 2 0x0000000001400000-0x00000000015fffff    locked mode:napot rw-
 3 0x2000000000000000-0x2000000003ffffff    locked mode:napot rw-
 4 0x0000000000102c00                       locked mode:off   ---
 5 0x0000000000102c00-0x0000000000103fff    locked mode:tor   --x
 6 0x0000000000000000                     unlocked mode:off   ---
 7 0x0000000000000000                     unlocked mode:off   ---
 8 0x0000000000101000                     unlocked mode:off   ---
 9 0x0000000000101000-0x0000000000102bff  unlocked mode:tor   --x
10 0x0000000000181000                     unlocked mode:off   ---
11 0x0000000000181000-0x00000000001813ff  unlocked mode:tor   r--
12 0x0000000000181400                     unlocked mode:off   ---
13 0x0000000000181400-0x0000000000181fff  unlocked mode:tor   rw-
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
29 0x0000000000000000                     unlocked mode:off   ---
30 0x0000000000080000-0x00000000000bffff  unlocked mode:napot ---
31 0x0000000000000000-0xffffffffffffffff    locked mode:napot ---

rv iopmp
Build Nov 25 2021 16:23:53 on PMU RISC-V. Args: iopmp
IO-PMP registers:
 0 0x0000000000100000-0x0000000000103fff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
 1 0x0000000000180000-0x0000000000180fff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
 2 0x00000000000001fc                     unlocked mode:off   -- sources: none
 3 0x00000000000001fc                     unlocked mode:off   -- sources: none
 4 0x00000000000001fc                     unlocked mode:off   -- sources: none
 5 0x00000000000001fc                     unlocked mode:off   -- sources: none
 6 0x00000000000001fc                     unlocked mode:off   -- sources: none
 7 0x00000000000001fc                     unlocked mode:off   -- sources: none
 8 0x0000000000181000-0x00000000001813ff  unlocked mode:napot -r sources: FBDMA CPDMA
 9 0x0000000000181000-0x0000000000181fff  unlocked mode:napot wr sources: FBDMA CPDMA
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
30 0x0000000000100000-0x000000000017ffff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
31 0x0000000000180000-0x00000000001fffff    locked mode:napot -- sources: FBDMA CPDMA SHA PMB:0xff
