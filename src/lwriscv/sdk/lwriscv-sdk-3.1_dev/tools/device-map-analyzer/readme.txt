This scripts are used to analyze RISC-V objectdumps for device map usage.
For help poke mkulikowski.

Supported chips: GA10x, T234, GH100
Supported engines: PMU, GSP, LWDEC, SEC/TSEC

Requirements:
- Bash
- Python 3

Limitations:
- Won't work if you have fancy VA mapping for PRI or LocalIO
- It assumes engine accesses go via LocalIO, PRI detection is very limited

Usage:
1. You need to have P4ROOT set to tree with //sw/resman/manuals
2. You need to build database (at least once, and when manuals change):
     - Run generate-device-map-db.sh, this will create database in P4ROOT/sw/lwriscv/main/tools/device-map-analyzer/db/
3. Generate (source annotated) objdump of your image: riscv64-linux-gnu-objdump -Sd <elf file name> > <objdump file name>
4. Then you can run analyzer on .objdumps, remember to keep the same P4ROOT set:
./analyze-device-map.py <chip> <engine> <objdump file>

Where
- chip: ampere-ga102, hopper-gh100, t23x-t234
- engine: pmu, gsp, lwdec, sec
- objdump: objdump file

5. Read output
WARNING: this tool may not catch all usages. That said it should not cause false-positives (if says register is written, it most likely is)

- "Checking source for outliers..." are priv registers that match your engine (tool doesn't track those, so one must check them)
- "Accessed registers.. per group" is collection of registers where access was found, grouped by device map groups.
For each register, functions that use it are printed.

There are 3 possible access patterns:
MMODE / LW_PRGNLCL_RISCV_TRACECTL
    Tainted in acrTraceInit
MMODE / LW_PRGNLCL_FBIF_REGIONCFG
    Load  in fbifConfigureAperture
    Store in fbifConfigureAperture

    - Tainted means register is mentioned in source, but script was unable to find assembly accessing it - it may be that that register access
        was optimized out.
    - Load means register was read
    - Store means register was written

- At the end, summary is given with group usage:
Unused groups: (SCRATCH_GROUP3:5),(HOSTIF:16),(ROM_PATCH:4),(SCRATCH_GROUP2:5),(FBIF:25),(KEY:7),(SCRATCH_GROUP1:5),(NOACCESS:0),(BROM:33),(IOPMP:11),(RISCV_CTL:0),(DIO:10),(FALCON_ONLY:67),(KMEM:13),(HUB_DIO:9),(SCRATCH_GROUP0:5),(PRGN_CTL:11)
Read-only groups: (TIMER:7),(SCP:46)
Write-only groups: (PIC:33),(PMB:18)

Keep in mind, that tainted registers are tainting groups they belong to (as used, because script was unable to figure out access type)


6. Improvements TODO:
- Automatic group detection per engine/chip