#!/usr/bin/env python3
"""
re_progress.py — reverse-engineering progress dashboard for oDFs's QVM work.

For each of the three DeFRaG QVMs (qagame/cgame/ui), this reports:

  - total functions              — count of entries in the .dis file's own
                                    function hash-table header (one row per
                                    compiled function, address+size+hash).
  - hash-matched to vanilla       — how many of those hashes also appear in
                                    the corresponding baseq3-*.hmap file.
                                    This means the function's opcode SHAPE
                                    matches vanilla Q3 and its name is known
                                    "for free" — it does NOT mean DeFRaG's
                                    behavior there is independently verified,
                                    since the hash is shape-only and not
                                    constants-sensitive (a modified constant
                                    with the same opcode shape still matches).
  - referenced in disasm/ docs    — how many of those function addresses
                                    show up (as `0x...` or func_0x...) inside
                                    this project's own disasm/ markdown. This
                                    is the closest proxy to "a human actually
                                    looked at this function," but it is a
                                    heuristic, not a precise metric:
                                      * it can OVER-count: a function's
                                        address can appear in a doc merely as
                                        a struct-offset or unrelated constant
                                        that happens to collide with a real
                                        function's entry address.
                                      * it can UNDER-count: a doc might
                                        describe a function by name only
                                        (no address) in surrounding prose.

Run with no arguments for the default paths (this repo + the sibling q3vm
disassembly checkout); pass --q3vm-dir / --disasm-dir to point elsewhere.
"""

import argparse
import io
import pathlib
import re
import sys
from collections import defaultdict

# Force UTF-8 stdout regardless of the host console's codepage (avoids mangled
# output on Windows terminals that default to cp1252/cp437).
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

HEADER_LINE_RE = re.compile(r'^0x([0-9a-fA-F]{8})\s+0x([0-9a-fA-F]+)\s+(-?[0-9a-fA-F]+)\s*$')
HMAP_LINE_RE = re.compile(r'^0x([0-9a-fA-F]+)\s+(\S+)\s+(-?[0-9a-fA-F]+)\s*$')
# func_0x... identifiers are always a reliable function-address reference.
FUNC_IDENT_RE = re.compile(r'\bfunc_0x([0-9a-fA-F]+)\b')
# Backtick-wrapped hex (`0x1234`) is ambiguous on its own — this project also
# backtick-wraps struct offsets, bitmasks, etc. Only trust it as a FUNCTION
# address on a line that also contains the word "address" (as in this
# project's consistent "**Address:** `0x...`" / "vanilla `0x...`" convention),
# to avoid matching e.g. "`ent->flags` (offset `0x218`)" and coincidentally
# hitting some unrelated real function that happens to start at 0x218.
BACKTICK_HEX_RE = re.compile(r'`0x([0-9a-fA-F]+)`')
# Strict "this doc's main subject" declaration — this project uses two
# conventions for "here is THE function this writeup is about" (as opposed to
# an incidental mention inside a "Key addresses"/"Likely function" call-graph
# table a few lines further down): a bold "**Address:**" line (most modules),
# or a title-line "# `Name` — DeFRaG/DeFraG address `0x...`" (BG_MISC).
PRIMARY_SUBJECT_RES = [
    re.compile(r'^\*\*(?:[A-Za-z]+\s+)?[Aa]ddress:\*\*\s*`0x([0-9a-fA-F]+)`'),
    re.compile(r'^#.*\bDe[Ff][Rr]a[Gg] address\b.*?`0x([0-9a-fA-F]+)`'),
]


def parse_dis_header(dis_path):
    """Returns {address:int -> (size:int, hash:str)} from the .dis file's own hash-table header."""
    funcs = {}
    with open(dis_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = HEADER_LINE_RE.match(line)
            if m:
                addr = int(m.group(1), 16)
                size = int(m.group(2), 16)
                h = m.group(3).lower()
                funcs[addr] = (size, h)
    return funcs


def parse_hmap(hmap_path):
    """Returns {hash:str -> name:str} from a baseq3-*.hmap file."""
    names_by_hash = {}
    with open(hmap_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = HMAP_LINE_RE.match(line)
            if m:
                names_by_hash[m.group(3).lower()] = m.group(2)
    return names_by_hash


def referenced_addresses(md_files):
    """Returns (touched, primary_subject): two sets of integer addresses.

    `touched`: any address mentioned anywhere (func_0x..., or `0x..` on a
    line containing "address") — an UPPER BOUND including incidental
    call-graph/"likely function" mentions, not necessarily a real writeup.

    `primary_subject`: only addresses declared via this project's
    "**Address:**"/"**Function address:**" convention — i.e. "this doc's
    main subject is this function." Much closer to "actually written up,"
    though a single doc can legitimately cover 2+ functions this way (e.g.
    a function and its ExtraPolate sibling) and some genuinely-analyzed
    functions might predate this convention and be missed (undercounting).

    See the module docstring and the regex comments above for why both are
    heuristics, not exact counts.
    """
    touched = set()
    primary_subject = set()
    for path in md_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in FUNC_IDENT_RE.finditer(text):
            touched.add(int(m.group(1), 16))
        for line in text.splitlines():
            lower = line.lower()
            for ps_re in PRIMARY_SUBJECT_RES:
                ps_m = ps_re.match(line)
                if ps_m:
                    primary_subject.add(int(ps_m.group(1), 16))
                    touched.add(int(ps_m.group(1), 16))
            if "address" not in lower:
                continue
            for m in BACKTICK_HEX_RE.finditer(line):
                # Skip "vanilla `0x...`" cross-references on an "**Address:**"
                # line — that's the UNRELATED vanilla-Q3 address space, not a
                # DeFRaG function address, and would otherwise register as a
                # false hit whenever it coincidentally equals some other real
                # DeFRaG function's entry point.
                preceding = lower[max(0, m.start() - 20):m.start()]
                if "vanilla" in preceding:
                    continue
                touched.add(int(m.group(1), 16))
    return touched, primary_subject


def pct(n, d):
    return f"{n} ({100.0 * n / d:.1f}%)" if d else f"{n} (n/a)"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--q3vm-dir", type=pathlib.Path,
                     default=pathlib.Path(r"D:/Other/Reverse Engineering/q3vm"),
                     help="directory containing qagame.dis/cgame.dis/ui.dis and the .hmap files")
    ap.add_argument("--disasm-dir", type=pathlib.Path,
                     default=pathlib.Path(__file__).resolve().parent.parent,
                     help="this project's disasm/ directory (default: parent of this script)")
    ap.add_argument("--by-module", action="store_true",
                     help="also break qagame down by disasm/ subfolder")
    args = ap.parse_args()

    # Which disasm/ subfolders belong to which QVM. CGAME/ is cgame.dis;
    # a future UI/ folder (none exists yet) would be ui.dis; everything else
    # under disasm/ documents qagame.dis.
    all_subdirs = [d for d in args.disasm_dir.iterdir() if d.is_dir() and d.name != "tools"]
    folder_for_qvm = {
        "qagame": [d for d in all_subdirs if d.name not in ("CGAME", "UI")],
        "cgame": [d for d in all_subdirs if d.name == "CGAME"],
        "ui": [d for d in all_subdirs if d.name == "UI"],
    }

    qvms = {
        "qagame": {"dis": "qagame.dis", "hmap": "baseq3-game-functions.hmap"},
        "cgame": {"dis": "cgame.dis", "hmap": "baseq3-cgame-functions.hmap"},
        "ui": {"dis": "ui.dis", "hmap": "baseq3-ui-functions.hmap"},
    }

    print("=== oDFs QVM reverse-engineering progress ===")
    print("(heuristic — see this script's module docstring for methodology & caveats)\n")

    header = (f"{'QVM':8} {'total fns':>10}  {'hash-matched (vanilla-shape)':>30}  "
               f"{'dedicated writeup':>20}  {'touched (any mention)':>24}")
    print(header)
    print("-" * len(header))

    module_rows = []  # collected if --by-module

    for qvm, paths in qvms.items():
        dis_path = args.q3vm_dir / paths["dis"]
        hmap_path = args.q3vm_dir / paths["hmap"]
        if not dis_path.exists() or not hmap_path.exists():
            print(f"{qvm:8} -- missing {dis_path} or {hmap_path}, skipped --")
            continue

        funcs = parse_dis_header(dis_path)
        names_by_hash = parse_hmap(hmap_path)
        total = len(funcs)
        hash_matched = sum(1 for (_size, h) in funcs.values() if h in names_by_hash)

        md_files = []
        for d in folder_for_qvm[qvm]:
            md_files.extend(d.rglob("*.md"))
        # top-level README.md (not inside a module folder) only applies to qagame by default
        if qvm == "qagame":
            top_readme = args.disasm_dir / "README.md"
            if top_readme.exists():
                md_files.append(top_readme)
            intro = args.disasm_dir / "INTRO.md"
            if intro.exists():
                md_files.append(intro)

        mentioned, primary = referenced_addresses(md_files)
        touched = sum(1 for addr in funcs if addr in mentioned)
        written_up = sum(1 for addr in funcs if addr in primary)

        print(f"{qvm:8} {total:>10}  {pct(hash_matched, total):>30}  "
              f"{pct(written_up, total):>20}  {pct(touched, total):>24}")

        if args.by_module and qvm == "qagame":
            for d in sorted(folder_for_qvm[qvm], key=lambda d: d.name):
                sub_mentioned, sub_primary = referenced_addresses(list(d.rglob("*.md")))
                sub_touched = sum(1 for addr in funcs if addr in sub_mentioned)
                sub_written_up = sum(1 for addr in funcs if addr in sub_primary)
                module_rows.append((d.name, sub_written_up, sub_touched))

    if args.by_module and module_rows:
        print("\nPer-module address hits (qagame; a function can be counted in more than one module's docs):")
        print(f"  {'module':16} {'dedicated writeup':>18}  {'touched (any mention)':>22}")
        for name, written_up, count in module_rows:
            print(f"  {name:16} {written_up:>18}  {count:>22}")

    print(
        "\n\"dedicated writeup\"     = this project's own \"**Address:**\"-style declaration —\n"
        "                          the closest thing to a real, confident writeup.\n"
        "\"touched (any mention)\" = includes incidental call-graph/\"likely function\"\n"
        "                          mentions too — an UPPER BOUND, not a completion metric.\n"
        "Neither number means the function's DeFRaG-specific behavior deltas (if any) are\n"
        "confirmed beyond what that doc actually says — read the doc, don't just trust the count."
    )


if __name__ == "__main__":
    sys.exit(main())
