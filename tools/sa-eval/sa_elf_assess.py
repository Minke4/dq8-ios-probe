#!/usr/bin/env python3
"""Static PS2 ELF assessment for the GTA:San Andreas port question.

Reads a PS2 executable and reports how heavy it is for PS2Recomp and for an iOS
port: size, code volume, function-entry estimate, vector-unit use, and a coarse
comparison against the DQ8-Recompiled baseline we already verified on device.

    python3 sa_elf_assess.py SLUS_209.46 [--json out.json]

Only statistics are emitted; no game code is dumped.
"""
import argparse
import json
import struct
from collections import Counter

DQ8_BASELINE = {"functions": 12441, "vu1_images": 82, "vu1_runs": 1129}

PT_LOAD = 1
SHT_PROGBITS = 1
SHF_EXECINSTR = 0x4

OP_NAMES = {
    0x00: "SPECIAL", 0x01: "REGIMM", 0x02: "J", 0x03: "JAL",
    0x04: "BEQ", 0x05: "BNE", 0x06: "BLEZ", 0x07: "BGTZ",
    0x08: "ADDI", 0x09: "ADDIU", 0x0A: "SLTI", 0x0B: "SLTIU",
    0x0C: "ANDI", 0x0D: "ORI", 0x0E: "XORI", 0x0F: "LUI",
    0x10: "COP0", 0x11: "COP1", 0x12: "COP2", 0x13: "COP1X",
    0x14: "BEQL", 0x15: "BNEL", 0x16: "BLEZL", 0x17: "BGTZL",
    0x18: "DADDI", 0x19: "DADDIU", 0x1A: "LDL", 0x1B: "LDR",
    0x1C: "MMI", 0x1E: "LQ", 0x1F: "SQ", 0x20: "LB", 0x21: "LH",
    0x23: "LW", 0x25: "LHU", 0x27: "LWU", 0x28: "SB", 0x29: "SH",
    0x2B: "SW", 0x2D: "SH", 0x2F: "CACHE", 0x31: "LWC1", 0x36: "LQC2",
    0x39: "SWC1", 0x3A: "SWC2", 0x3E: "SQC2",
}


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def read_elf(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    if data[4] != 1 or data[5] != 1:
        raise SystemExit("expected 32-bit little-endian ELF (PS2 executables are)")
    entry = u32(data, 24)
    phoff, shoff = u32(data, 28), u32(data, 32)
    phentsize, phnum = u16(data, 42), u16(data, 44)
    shentsize, shnum = u16(data, 46), u16(data, 48)
    shstrndx = u16(data, 50)

    segments = []
    for i in range(phnum):
        o = phoff + i * phentsize
        if u32(data, o) != PT_LOAD:
            continue
        flags = u32(data, o + 24)
        segments.append({
            "offset": u32(data, o + 4), "vaddr": u32(data, o + 8),
            "filesz": u32(data, o + 16), "memsz": u32(data, o + 20),
            "flags": flags, "exec": bool(flags & 1),
        })

    sections = []
    names_base = u32(data, shoff + shstrndx * shentsize + 16) if shnum else 0
    for i in range(shnum):
        o = shoff + i * shentsize
        name_off = u32(data, o)
        end = data.find(b"\0", names_base + name_off)
        name = data[names_base + name_off:end].decode("utf-8", "replace")
        sections.append({
            "name": name, "type": u32(data, o + 4), "flags": u32(data, o + 8),
            "addr": u32(data, o + 12), "offset": u32(data, o + 16),
            "size": u32(data, o + 20),
        })

    return {"data": data, "machine": u16(data, 18), "entry": entry,
            "segments": segments, "sections": sections}


def executable_ranges(elf):
    rng = [(s["offset"], s["offset"] + s["filesz"], s["vaddr"])
           for s in elf["segments"] if s["exec"] and s["filesz"]]
    if rng:
        return rng
    return [(s["offset"], s["offset"] + s["size"], s["addr"])
            for s in elf["sections"]
            if s["type"] == SHT_PROGBITS and (s["flags"] & SHF_EXECINSTR)]


def scan(elf):
    data = elf["data"]
    counts = Counter()
    targets = set()
    dyn = sysc = words = 0

    for seg_start, seg_end, _v in executable_ranges(elf):
        # A PT_LOAD segment can start at any file offset. Decode the segment as
        # the guest sees it: word i sits at seg_start + 4*i, exactly like the
        # PS2 loader maps it, so no realignment is needed or wanted.
        off = seg_start
        while off + 4 <= seg_end:
            insn = u32(data, off)
            op = insn >> 26
            words += 1
            name = OP_NAMES.get(op)
            if name:
                counts[name] += 1
            if op == 0x03:  # JAL
                t = (0x80000000 | ((insn & 0x03FFFFFF) << 2)) & 0xFFFFFFFF
                targets.add(t)
                targets.add(t & 0x1FFFFFFF)
            elif op == 0x00:
                fn = insn & 0x3F
                if fn == 0x0C:
                    sysc += 1
                elif fn == 0x08 and ((insn >> 21) & 0x1F) == 31:
                    dyn += 1
            off += 4

    return {"words": words, "counts": dict(counts), "targets": targets,
            "syscalls": sysc, "dynamic_jumps": dyn}


def vu(elf, counts):
    dense = 0
    for s in elf["sections"]:
        if s["type"] != SHT_PROGBITS or (s["flags"] & SHF_EXECINSTR):
            continue
        a, b, step = s["offset"], s["offset"] + s["size"], 0x1000
        for off in range(a, max(a, b - step), step):
            chunk = elf["data"][off:off + step]
            if len(chunk) < step:
                break
            if chunk.count(0) < step * 0.15:
                dense += 1
    return {
        "cop2_instructions": counts.get("COP2", 0),
        "vu_memory_ops": counts.get("LQC2", 0) + counts.get("SQC2", 0) + counts.get("SWC2", 0),
        "dense_data_blocks_4k": dense,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf")
    ap.add_argument("--json")
    args = ap.parse_args()

    elf = read_elf(args.elf)
    st = scan(elf)
    text = sum(e - s for s, e, _ in executable_ranges(elf))
    funcs = len(st["targets"])
    ratio = funcs / DQ8_BASELINE["functions"]
    if ratio < 1.5:
        weight = "similar to DQ8"
    elif ratio < 3.0:
        weight = "moderately heavier"
    else:
        weight = "heavy"

    out = {
        "file": args.elf,
        "size_bytes": len(elf["data"]),
        "machine": elf["machine"],
        "entry": "0x%08X" % elf["entry"],
        "code_bytes": text,
        "code_words": st["words"],
        "estimated_functions": funcs,
        "syscalls": st["syscalls"],
        "dynamic_jumps": st["dynamic_jumps"],
        "instruction_counts": st["counts"],
        "vu": vu(elf, st["counts"]),
        "baseline": DQ8_BASELINE,
        "verdict": {"function_ratio_vs_dq8": round(ratio, 2), "weight": weight},
    }

    print("file            %s" % args.elf)
    print("size            {:,} bytes".format(out["size_bytes"]))
    print("entry           %s   (machine %d)" % (out["entry"], out["machine"]))
    print("code bytes      {:,}  ({:,} instructions)".format(text, st["words"]))
    print("JAL targets     {:,}  function-entry estimate".format(funcs))
    print("syscalls        {:,}".format(st["syscalls"]))
    print("dynamic jumps   {:,}".format(st["dynamic_jumps"]))
    print()
    print("instruction mix (top 12)")
    for n, c in Counter(st["counts"]).most_common(12):
        print("  %-8s %12s %6.1f%%" % (n, "{:,}".format(c),
                                       100.0 * c / max(1, st["words"])))
    print()
    print("vector unit evidence")
    print("  COP2 (VU macro) insns   {:,}".format(out["vu"]["cop2_instructions"]))
    print("  LQC2/SQC2/SWC2 ops      {:,}".format(out["vu"]["vu_memory_ops"]))
    print("  dense 4K data blocks    {:,}".format(out["vu"]["dense_data_blocks_4k"]))
    print()
    print("verdict: %s (%sx DQ8 functions)" % (weight, round(ratio, 2)))

    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(out, fh, indent=2)
        print("\njson written to %s" % args.json)


if __name__ == "__main__":
    main()
