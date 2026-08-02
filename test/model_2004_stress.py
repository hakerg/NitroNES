import re, sys

ASM = "nes-test-roms-master/AccuracyCoin-main/AccuracyCoin.asm"

def parse_key(name):
    lines = open(ASM, encoding="utf-8", errors="replace").read().splitlines()
    out = []
    start = None
    for i, l in enumerate(lines):
        if l.startswith(name + ":"):
            start = i + 1
            break
    assert start is not None
    for l in lines[start:]:
        m = re.match(r"\s*\.byte\s+(.*)", l)
        if not m:
            break
        for tok in m.group(1).split(","):
            tok = tok.strip()
            if tok.startswith("$"):
                out.append(int(tok[1:], 16))
            elif tok:
                out.append(int(tok))
    return out

key1 = parse_key("Test_2004_Stress_AnswerKey1")
key2 = parse_key("Test_2004_Stress_AnswerKey2")
print("key1 len", len(key1), "key2 len", len(key2))

oam1 = [(0xFF - i) & 0xFF for i in range(256)]
oam2 = [0x80, 0x00, 0x00, 0xFF,
        0x7F, 0x01, 0x20, 0xEE,
        0x7E, 0x02, 0x40, 0xDD,
        0x7D, 0x03, 0x60, 0xCC,
        0x7C, 0x04, 0x80, 0xBB,
        0x7B, 0x05, 0xA0, 0xAA,
        0x7A, 0x06, 0xC0, 0x99,
        0x79, 0x07, 0xE0, 0x88] + [(i - 32) & 0xFF for i in range(32, 256)]

SCANLINE = 128

def simulate(OAM):
    sec = [0xFF] * 32
    res = {}
    buffer = 0
    oamAddr = 0
    secAddr = 0
    copying = 0
    oamAddrOv = False
    secOv = False
    spriteInRange = False
    ovBugCounter = 0
    overflowFlagDot = -1

    # dots 1-64: clear
    for dot in range(1, 65):
        if dot & 1:
            buffer = 0xFF
        else:
            sec[secAddr & 0x1F] = buffer
            secAddr = (secAddr + 1) & 0x1F
        res[dot] = buffer

    # eval 65-256
    secAddr = 0
    for dot in range(65, 257):
        if dot & 1:
            buffer = OAM[oamAddr]
            if (oamAddr & 3) == 2:
                buffer &= 0xE3
        else:
            orig = buffer
            if not (oamAddrOv or secOv):
                sec[secAddr & 0x1F] = buffer
            else:
                buffer = sec[secAddr & 0x1F]
            if copying > 0:
                copying -= 1
                oamAddr = (oamAddr + 1) & 0xFF
                secAddr = (secAddr + 1) & 0x1F
                if secAddr == 0:
                    secOv = True
                if oamAddr == 0:
                    oamAddrOv = True
            else:
                in_range = 0 <= SCANLINE - orig < 8
                if not secOv:
                    if in_range and not oamAddrOv:
                        copying = 3
                        oamAddr = (oamAddr + 1) & 0xFF
                        secAddr = (secAddr + 1) & 0x1F
                        if secAddr == 0:
                            secOv = True
                        if oamAddr == 0:
                            oamAddrOv = True
                    else:
                        oamAddr = (oamAddr + 4) & 0xFC
                        if oamAddr == 0:
                            oamAddrOv = True
                else:
                    if oamAddrOv:
                        oamAddr = (oamAddr + 4) & 0xFC
                    elif in_range or spriteInRange:
                        if not spriteInRange:
                            spriteInRange = True
                            if overflowFlagDot < 0:
                                overflowFlagDot = dot
                        addrL = (oamAddr & 3) + 1
                        if addrL == 4:
                            oamAddr = ((oamAddr + 4) & 0xFC)
                            addrL = 0
                        oamAddr = (oamAddr & 0xFC) | addrL
                        if ovBugCounter == 0:
                            ovBugCounter = 3
                        else:
                            ovBugCounter -= 1
                            if ovBugCounter == 0:
                                oamAddrOv = True
                                oamAddr &= 0xFC
                    else:
                        oamAddr = ((oamAddr + 4) & 0xFC) | ((oamAddr + 1) & 3)
                        if (oamAddr & 0xFC) == 0:
                            oamAddrOv = True
        res[dot] = buffer

    # sprite loading 257-320
    secAddr = 0
    for dot in range(257, 321):
        step = (dot - 257) & 7
        if step in (0, 1, 2, 3) and dot != 257:
            secAddr = (secAddr + 1) & 0x1F
        buffer = sec[secAddr & 0x1F]
        res[dot] = buffer

    # hazard increment after dot 320
    secAddr = (secAddr + 1) & 0x1F

    # dots 321-340 and dot 0
    for dot in range(321, 341):
        buffer = sec[secAddr & 0x1F]
        res[dot] = buffer
    res[0] = sec[secAddr & 0x1F]
    return res, sec, overflowFlagDot

def compare(name, key, res):
    mism = []
    for dot in range(341):
        if key[dot] != res[dot]:
            mism.append((dot, key[dot], res[dot]))
    print(f"{name}: {len(mism)} mismatches")
    for dot, k, r in mism[:40]:
        print(f"  dot {dot:3d}: key={k:02X} model={r:02X}")
    return mism

r1, sec1, ov1 = simulate(oam1)
print("sec OAM after eval (key1):", " ".join(f"{b:02X}" for b in sec1), "overflowDot", ov1)
compare("key1", key1, r1)
print()
r2, sec2, ov2 = simulate(oam2)
print("sec OAM after eval (key2):", " ".join(f"{b:02X}" for b in sec2), "overflowDot", ov2)
compare("key2", key2, r2)
