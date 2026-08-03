"""Generuje test/AccuracyCoinData.h z README.md AccuracyCoin.

Opisy kodow bledow i wariantow sa kopiowane 1:1 z README.
Uzycie: python test/gen_accuracycoin_data.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(ROOT, "nes-test-roms-master", "AccuracyCoin-main", "README.md")
OUT = os.path.join(ROOT, "test", "AccuracyCoinData.h")

# Strony 3-11 (Unofficial Instructions) maja w README jedna wspolna sekcje;
# nazwy testow per strona pochodza z menu ROM-u (TestPages w AccuracyCoin.asm).
UNOFFICIAL_PAGES = [
    ("Unofficial Instructions: SLO", [
        ("$03 SLO indirect,X", False), ("$07 SLO zeropage", False),
        ("$0F SLO absolute", False), ("$13 SLO indirect,Y", False),
        ("$17 SLO zeropage,X", False), ("$1B SLO absolute,Y", False),
        ("$1F SLO absolute,X", False)]),
    ("Unofficial Instructions: RLA", [
        ("$23 RLA indirect,X", False), ("$27 RLA zeropage", False),
        ("$2F RLA absolute", False), ("$33 RLA indirect,Y", False),
        ("$37 RLA zeropage,X", False), ("$3B RLA absolute,Y", False),
        ("$3F RLA absolute,X", False)]),
    ("Unofficial Instructions: SRE", [
        ("$43 SRE indirect,X", False), ("$47 SRE zeropage", False),
        ("$4F SRE absolute", False), ("$53 SRE indirect,Y", False),
        ("$57 SRE zeropage,X", False), ("$5B SRE absolute,Y", False),
        ("$5F SRE absolute,X", False)]),
    ("Unofficial Instructions: RRA", [
        ("$63 RRA indirect,X", False), ("$67 RRA zeropage", False),
        ("$6F RRA absolute", False), ("$73 RRA indirect,Y", False),
        ("$77 RRA zeropage,X", False), ("$7B RRA absolute,Y", False),
        ("$7F RRA absolute,X", False)]),
    ("Unofficial Instructions: *AX", [
        ("$83 SAX indirect,X", False), ("$87 SAX zeropage", False),
        ("$8F SAX absolute", False), ("$97 SAX zeropage,Y", False),
        ("$A3 LAX indirect,X", False), ("$A7 LAX zeropage", False),
        ("$AF LAX absolute", False), ("$B3 LAX indirect,Y", False),
        ("$B7 LAX zeropage,Y", False), ("$BF LAX absolute,Y", False)]),
    ("Unofficial Instructions: DCP", [
        ("$C3 DCP indirect,X", False), ("$C7 DCP zeropage", False),
        ("$CF DCP absolute", False), ("$D3 DCP indirect,Y", False),
        ("$D7 DCP zeropage,X", False), ("$DB DCP absolute,Y", False),
        ("$DF DCP absolute,X", False)]),
    ("Unofficial Instructions: ISC", [
        ("$E3 ISC indirect,X", False), ("$E7 ISC zeropage", False),
        ("$EF ISC absolute", False), ("$F3 ISC indirect,Y", False),
        ("$F7 ISC zeropage,X", False), ("$FB ISC absolute,Y", False),
        ("$FF ISC absolute,X", False)]),
    ("Unofficial Instructions: SH*", [
        ("$93 SHA indirect,Y", True), ("$9F SHA absolute,Y", True),
        ("$9B SHS absolute,Y", True), ("$9C SHY absolute,X", False),
        ("$9E SHX absolute,Y", False), ("$BB LAE absolute,Y", False)]),
    ("Unofficial Immediates", [
        ("$0B ANC Immediate", False), ("$2B ANC Immediate", False),
        ("$4B ASR Immediate", False), ("$6B ARR Immediate", False),
        ("$8B ANE Immediate", False), ("$AB LXA Immediate", False),
        ("$CB AXS Immediate", False), ("$EB SBC Immediate", False)]),
]

CODE_RE = re.compile(r"^  ([0-9A-Z]): (.*?)\s*$")
PAGE_RE = re.compile(r"^## Pages? ([\d, and]+): (.*?)\s*$")


def parse_readme(path):
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()

    section = None
    pages = []          # lista (numery_str, nazwa, [testy]); test = (nazwa, draw, [kody])
    success = {}        # nazwa testu (lowercase) -> [kody]
    success_names = {}  # lowercase -> oryginalna nazwa
    cur_page = None
    cur_test = None

    for line in lines:
        if line.startswith("# "):
            title = line[2:].strip()
            if title == "Error Codes":
                section = "err"
            elif title == "Success Codes":
                section = "ok"
            elif section in ("err", "ok"):
                section = None
            cur_page = cur_test = None if section != "err" else cur_page
            continue

        if section == "err":
            m = PAGE_RE.match(line)
            if m:
                cur_page = (m.group(1), m.group(2), [])
                pages.append(cur_page)
                cur_test = None
                continue
            if line.startswith("### "):
                name = line[4:].strip()
                draw = name.startswith("DRAW ")
                if draw:
                    name = name[len("DRAW "):]
                cur_test = (name, draw, [])
                cur_page[2].append(cur_test)
                continue
            m = CODE_RE.match(line)
            if m and cur_test is not None:
                cur_test[2].append((m.group(1), m.group(2)))

        elif section == "ok":
            if line.startswith("### "):
                cur_test = line[4:].strip()
                success[cur_test.lower()] = []
                success_names[cur_test.lower()] = cur_test
                continue
            m = CODE_RE.match(line)
            if m and cur_test is not None:
                success[cur_test.lower()].append((m.group(1), m.group(2)))

    return pages, success


def merge_dupes(codes):
    merged = []
    for c, t in codes:
        if merged and merged[-1][0] == c:
            merged[-1] = (c, merged[-1][1] + " / " + t)
        else:
            merged.append((c, t))
    return merged


def c_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def slug(name):
    s = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_")
    return s


def emit_codes(out, var, codes):
    out.append("static const AcCode %s[] = {" % var)
    for c, t in merge_dupes(codes):
        out.append("    {'%s',\"%s\"}," % (c, c_escape(t)))
    out.append("};")


def main():
    pages, success = parse_readme(README)

    page_map = {}   # numer strony -> (nazwa, [testy])
    unofficial_err = None
    for nums, name, tests in pages:
        if "," in nums:
            assert len(tests) == 1 and not tests[0][1], "nieoczekiwana sekcja multi-page"
            unofficial_err = tests[0][2]
            for n in [int(x) for x in re.findall(r"\d+", nums)]:
                page_map[n] = None
        else:
            page_map[int(nums)] = (name, tests)

    assert unofficial_err is not None, "brak sekcji Unofficial Instructions"
    assert sorted(page_map) == list(range(1, 21)), "README nie pokrywa stron 1-20"

    out = [
        "#pragma once",
        "// WYGENEROWANO przez test/gen_accuracycoin_data.py - nie edytowac recznie.",
        "// Opisy kodow skopiowane 1:1 z nes-test-roms-master/AccuracyCoin-main/README.md",
        "// Kody czerwone = kody bledow, niebieskie = akceptowalne warianty (success codes).",
        "",
        "struct AcCode { char code; const char* text; };",
        "struct AcTest { const char* name; const AcCode* err; int errN; const AcCode* ok; int okN; };",
        "struct AcPage { const char* name; const AcTest* tests; int testN; };",
        "",
        "#define AC_TST(name, err)        { name, err, (int)(sizeof(err)/sizeof((err)[0])), nullptr, 0 }",
        "#define AC_TSTV(name, err, ok)   { name, err, (int)(sizeof(err)/sizeof((err)[0])), ok, (int)(sizeof(ok)/sizeof((ok)[0])) }",
        "#define AC_DRAW(name)            { name, nullptr, 0, nullptr, 0 }",
        "",
    ]

    page_entries = []   # (nazwa strony, var testow)

    for pnum in range(1, 21):
        out.append("// ===================== Page %d =====================" % pnum)

        if 3 <= pnum <= 11:
            pname, tests = UNOFFICIAL_PAGES[pnum - 3]
            if pnum == 3:
                emit_codes(out, "e_Unofficial", unofficial_err)
                emit_codes(out, "ok_ShaShs", success["unofficial instructions: sha, shs"])
            var = "p%d" % pnum
            out.append("static const AcTest %s[] = {" % var)
            for tname, has_ok in tests:
                if has_ok:
                    out.append("    AC_TSTV(\"%s\", e_Unofficial, ok_ShaShs)," % c_escape(tname))
                else:
                    out.append("    AC_TST(\"%s\", e_Unofficial)," % c_escape(tname))
            out.append("};")
            out.append("")
            page_entries.append((pname, var))
            continue

        pname, tests = page_map[pnum]
        test_vars = []
        for i, (tname, draw, codes) in enumerate(tests):
            if draw:
                test_vars.append((tname, None, None))
                continue
            ev = "e_p%d_%s" % (pnum, slug(tname))
            emit_codes(out, ev, codes)
            ov = None
            if tname.lower() in success:
                ov = "ok_p%d_%s" % (pnum, slug(tname))
                emit_codes(out, ov, success[tname.lower()])
            test_vars.append((tname, ev, ov))

        var = "p%d" % pnum
        out.append("static const AcTest %s[] = {" % var)
        for tname, ev, ov in test_vars:
            if ev is None:
                out.append("    AC_DRAW(\"%s\")," % c_escape(tname))
            elif ov:
                out.append("    AC_TSTV(\"%s\", %s, %s)," % (c_escape(tname), ev, ov))
            else:
                out.append("    AC_TST(\"%s\", %s)," % (c_escape(tname), ev))
        out.append("};")
        out.append("")
        page_entries.append((pname, var))

    used_ok = {"unofficial instructions: sha, shs"}
    for pnum in range(1, 21):
        if 3 <= pnum <= 11:
            continue
        for tname, draw, _ in page_map[pnum][1]:
            if not draw and tname.lower() in success:
                used_ok.add(tname.lower())
    leftover = set(success) - used_ok
    if leftover:
        print("UWAGA: success codes bez testu: %s" % sorted(leftover), file=sys.stderr)

    out.append("// ===================== Page table =====================")
    out.append("static const AcPage AC_PAGES[20] = {")
    for pname, var in page_entries:
        out.append("    {\"%s\", %s, (int)(sizeof(%s)/sizeof(%s[0]))}," % (c_escape(pname), var, var, var))
    out.append("};")
    out.append("")
    out.append("inline const char* acLookup(const AcCode* table, int n, char code) {")
    out.append("    for (int i = 0; i < n; ++i)")
    out.append("        if (table[i].code == code) return table[i].text;")
    out.append("    return nullptr;")
    out.append("}")
    out.append("")

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))

    total = 0
    for pnum in range(1, 21):
        if 3 <= pnum <= 11:
            n = len(UNOFFICIAL_PAGES[pnum - 3][1])
        else:
            n = len(page_map[pnum][1])
        total += n
        print("page %2d: %d testow" % (pnum, n))
    print("razem: %d -> %s" % (total, OUT))


if __name__ == "__main__":
    main()
