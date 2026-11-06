# Static PS2 ELF assessment

`sa_elf_assess.py <elf> [--json out.json]` reports how heavy a PS2 game is for
PS2Recomp and for the iOS port:

- ELF shape: segments, sections, entry point, code/data split
- R5900 instruction mix, JAL-derived function-entry estimate, SYSCALL count,
  unresolved dynamic jumps
- vector-unit evidence: COP2 macro instructions, LQC2/SQC2/SWC2 traffic
- comparison against the DQ8 baseline verified on device
  (12,441 functions, 82 VU1 images, 1,129 runs)

It reads the ELF only and prints statistics; no game code is emitted.
