#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Przeglad stanu emulatora NES: build + accuracy_coin + wszystkie ROM-y
testowe z jednoznacznym wynikiem tekstowym (bez testow wizualnych i
niejednoznacznych - nie da sie ich zweryfikowac automatycznie). Wypisuje
aktualny stan: wszystkie faile z powodami. Nic nie porownuje - raport to
po prostu stan. Uzycie:
  python run_tests.py              # build + wszystkie testy (~2 min)
  python run_tests.py --no-build   # bez przebudowy
"""
import argparse
import os
import queue
import re
import subprocess
import sys
import threading
import time
import xml.etree.ElementTree as ET

BASE = os.path.dirname(os.path.abspath(__file__))
ROMSDIR = os.path.join(BASE, "nes-test-roms-master")
BUILD = os.path.join(BASE, "build", "release")
NES_TEST = os.path.join(BUILD, "nes_test.exe")
ACCURACY = os.path.join(BUILD, "accuracy_coin.exe")

# (rom, note) - wszystkie oczekuja przejscia; "info:" = tylko raport tekstu
TESTS = [
    # ---------------- PPU ----------------
    ("blargg_ppu_tests_2005.09.15b/palette_ram.nes", ""),
    ("blargg_ppu_tests_2005.09.15b/sprite_ram.nes", ""),
    ("blargg_ppu_tests_2005.09.15b/vbl_clear_time.nes", ""),
    ("blargg_ppu_tests_2005.09.15b/vram_access.nes", ""),
    ("ppu_open_bus/ppu_open_bus.nes", ""),
    ("ppu_vbl_nmi/ppu_vbl_nmi.nes", ""),
    ("ppu_vbl_nmi/rom_singles/01-vbl_basics.nes", ""),
    ("ppu_vbl_nmi/rom_singles/02-vbl_set_time.nes", ""),
    ("ppu_vbl_nmi/rom_singles/03-vbl_clear_time.nes", ""),
    ("ppu_vbl_nmi/rom_singles/04-nmi_control.nes", ""),
    ("ppu_vbl_nmi/rom_singles/05-nmi_timing.nes", ""),
    ("ppu_vbl_nmi/rom_singles/06-suppression.nes", ""),
    ("ppu_vbl_nmi/rom_singles/07-nmi_on_timing.nes", ""),
    ("ppu_vbl_nmi/rom_singles/08-nmi_off_timing.nes", ""),
    ("ppu_vbl_nmi/rom_singles/09-even_odd_frames.nes", ""),
    ("ppu_vbl_nmi/rom_singles/10-even_odd_timing.nes", ""),
    ("vbl_nmi_timing/1.frame_basics.nes", ""),
    ("vbl_nmi_timing/2.vbl_timing.nes", ""),
    ("vbl_nmi_timing/3.even_odd_frames.nes", ""),
    ("vbl_nmi_timing/4.vbl_clear_timing.nes", ""),
    ("vbl_nmi_timing/5.nmi_suppression.nes", ""),
    ("vbl_nmi_timing/6.nmi_disable.nes", ""),
    ("vbl_nmi_timing/7.nmi_timing.nes", ""),
    ("sprite_hit_tests_2005.10.05/01.basics.nes", ""),
    ("sprite_hit_tests_2005.10.05/02.alignment.nes", ""),
    ("sprite_hit_tests_2005.10.05/03.corners.nes", ""),
    ("sprite_hit_tests_2005.10.05/04.flip.nes", ""),
    ("sprite_hit_tests_2005.10.05/05.left_clip.nes", ""),
    ("sprite_hit_tests_2005.10.05/06.right_edge.nes", ""),
    ("sprite_hit_tests_2005.10.05/07.screen_bottom.nes", ""),
    ("sprite_hit_tests_2005.10.05/08.double_height.nes", ""),
    ("sprite_hit_tests_2005.10.05/09.timing_basics.nes", ""),
    ("sprite_hit_tests_2005.10.05/10.timing_order.nes", ""),
    ("sprite_hit_tests_2005.10.05/11.edge_timing.nes", ""),
    ("sprite_overflow_tests/1.Basics.nes", ""),
    ("sprite_overflow_tests/2.Details.nes", ""),
    ("sprite_overflow_tests/3.Timing.nes", ""),
    ("sprite_overflow_tests/4.Obscure.nes", ""),
    ("sprite_overflow_tests/5.Emulator.nes", ""),
    ("sprdma_and_dmc_dma/sprdma_and_dmc_dma.nes", ""),
    ("sprdma_and_dmc_dma/sprdma_and_dmc_dma_512.nes", ""),
    # ---------------- CPU ----------------
    ("blargg_nes_cpu_test5/cpu.nes", ""),
    ("blargg_nes_cpu_test5/official.nes", ""),
    ("cpu_interrupts_v2/cpu_interrupts.nes", ""),
    ("cpu_interrupts_v2/rom_singles/1-cli_latency.nes", ""),
    ("cpu_interrupts_v2/rom_singles/2-nmi_and_brk.nes", ""),
    ("cpu_interrupts_v2/rom_singles/3-nmi_and_irq.nes", ""),
    ("cpu_interrupts_v2/rom_singles/4-irq_and_dma.nes", ""),
    ("cpu_interrupts_v2/rom_singles/5-branch_delays_irq.nes", ""),
    ("instr_test-v3/all_instrs.nes", ""),
    ("instr_test-v3/official_only.nes", ""),
    ("instr_misc/instr_misc.nes", ""),
    ("instr_timing/instr_timing.nes", ""),
    ("cpu_timing_test6/cpu_timing_test.nes", ""),
    ("branch_timing_tests/1.Branch_Basics.nes", ""),
    ("branch_timing_tests/2.Backward_Branch.nes", ""),
    ("branch_timing_tests/3.Forward_Branch.nes", ""),
    ("cpu_dummy_reads/cpu_dummy_reads.nes", ""),
    ("cpu_dummy_writes/cpu_dummy_writes_oam.nes", ""),
    ("cpu_dummy_writes/cpu_dummy_writes_ppumem.nes", ""),
    ("cpu_reset/ram_after_reset.nes", ""),
    ("cpu_reset/registers.nes", ""),
    ("cpu_exec_space/test_cpu_exec_space_apu.nes", ""),
    ("cpu_exec_space/test_cpu_exec_space_ppuio.nes", ""),
    # ---------------- APU ----------------
    ("apu_test/apu_test.nes", ""),
    ("blargg_apu_2005.07.30/01.len_ctr.nes", ""),
    ("blargg_apu_2005.07.30/02.len_table.nes", ""),
    ("blargg_apu_2005.07.30/03.irq_flag.nes", ""),
    ("blargg_apu_2005.07.30/04.clock_jitter.nes", ""),
    ("blargg_apu_2005.07.30/05.len_timing_mode0.nes", ""),
    ("blargg_apu_2005.07.30/06.len_timing_mode1.nes", ""),
    ("blargg_apu_2005.07.30/07.irq_flag_timing.nes", ""),
    ("blargg_apu_2005.07.30/08.irq_timing.nes", ""),
    ("blargg_apu_2005.07.30/09.reset_timing.nes", ""),
    ("blargg_apu_2005.07.30/10.len_halt_timing.nes", ""),
    ("blargg_apu_2005.07.30/11.len_reload_timing.nes", ""),
    ("apu_reset/4015_cleared.nes", ""),
    ("apu_reset/4017_timing.nes", ""),
    ("apu_reset/4017_written.nes", ""),
    ("apu_reset/irq_flag_cleared.nes", ""),
    ("apu_reset/len_ctrs_enabled.nes", ""),
    ("apu_reset/works_immediately.nes", ""),
    # ---------------- Mappery ----------------
    ("mmc3_test/1-clocking.nes", ""),
    ("mmc3_test/2-details.nes", ""),
    ("mmc3_test/3-A12_clocking.nes", ""),
    ("mmc3_test/4-scanline_timing.nes", ""),
    ("mmc3_test/5-MMC3.nes", "test rewizji B (Sharp); emulujemy rewizje A/MMC6 - rewizje sie wykluczaja"),
    ("mmc3_test/6-MMC6.nes", ""),
    ("mmc3_irq_tests/1.Clocking.nes", ""),
    ("mmc3_irq_tests/2.Details.nes", ""),
    ("mmc3_irq_tests/3.A12_clocking.nes", ""),
    ("mmc3_irq_tests/4.Scanline_timing.nes", ""),
    ("mmc3_irq_tests/5.MMC3_rev_A.nes", ""),
    ("mmc3_irq_tests/6.MMC3_rev_B.nes", "test rewizji B (Sharp); emulujemy rewizje A/MMC6 - rewizje sie wykluczaja"),
    # ---------------- Joypad ----------------
    ("read_joy3/count_errors.nes", "info: szum sprzetu - Nestopia tez ma >0"),
    ("read_joy3/count_errors_fast.nes", "info: szum sprzetu - Nestopia tez ma >0"),
    ("read_joy3/test_buttons.nes", "info: interaktywny - czeka na przyciski; zweryfikowany recznie"),
    # ---------------- Holy Mapperel (pinobatch/holy-mapperel) ----------------
    # Test plytki PCB: detekcja mappera, bankowanie PRG/CHR, WRAM, mirroring, IRQ.
    # Wynik: ekran (DETAILED TEST RESULT: XXXX, 0=OK) lub kod morsa przy twardej
    # awarii. Klasyfikacja: hm_classify() + detekcja kodu morsa ze stosu CPU.
    # Mappery generowane przez make_roms.py: 0,1,2,3,4,7,9,10,11,28,34,66,78.3,118,180.
    ("holy-mapperel/testroms/M0_P32K_C8K_V.nes", ""),
    ("holy-mapperel/testroms/M0_P32K_CR8K_V.nes", ""),
    ("holy-mapperel/testroms/M0_P32K_CR32K_V.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_C128K.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_C128K_S8K.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_C128K_W8K.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_C32K.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_C32K_S8K.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_C32K_W8K.nes", ""),
    ("holy-mapperel/testroms/M1_P128K_CR8K.nes", ""),
    ("holy-mapperel/testroms/M1_P512K_CR8K_S32K.nes", ""),
    ("holy-mapperel/testroms/M1_P512K_CR8K_S8K.nes", ""),
    ("holy-mapperel/testroms/M2_P128K_CR8K_V.nes", ""),
    ("holy-mapperel/testroms/M3_P32K_C32K_H.nes", ""),
    ("holy-mapperel/testroms/M4_P128K_CR8K.nes", ""),
    ("holy-mapperel/testroms/M4_P128K_CR32K.nes", ""),
    ("holy-mapperel/testroms/M4_P1M_CR32K.nes", ""),
    ("holy-mapperel/testroms/M4_P256K_C256K.nes", ""),
    ("holy-mapperel/testroms/M4_P256K_CR32K.nes", ""),
    ("holy-mapperel/testroms/M7_P128K_CR8K.nes", ""),
    ("holy-mapperel/testroms/M9_P128K_C64K.nes", ""),
    ("holy-mapperel/testroms/M10_P128K_C64K_S8K.nes", ""),
    ("holy-mapperel/testroms/M10_P128K_C64K_W8K.nes", ""),
    ("holy-mapperel/testroms/M11_P64K_C64K_V.nes", ""),
    ("holy-mapperel/testroms/M11_P64K_CR32K_V.nes", ""),
    ("holy-mapperel/testroms/M28_P512K_CR32K.nes", ""),
    ("holy-mapperel/testroms/M28_P1M_CR32K.nes", ""),
    ("holy-mapperel/testroms/M34_P128K_CR8K_H.nes", ""),
    ("holy-mapperel/testroms/M66_P64K_C16K_V.nes", ""),
    ("holy-mapperel/testroms/M69_P128K_C64K_S8K.nes", ""),
    ("holy-mapperel/testroms/M69_P128K_C64K_W8K.nes", ""),
    ("holy-mapperel/testroms/M78.3_P128K_C64K.nes", ""),
    ("holy-mapperel/testroms/M78.3_P128K_CR32K.nes", ""),
    ("holy-mapperel/testroms/M118_P128K_C64K.nes", ""),
    ("holy-mapperel/testroms/M180_P128K_CR8K_H.nes", ""),
]

# testy wymagajace resetu (timing resetu jest czescia testu)
RESET_ARGS = {
    "cpu_reset/ram_after_reset.nes": ["frames:900", "reset", "frames:900", "ascii"],
    "cpu_reset/registers.nes": ["frames:900", "reset", "frames:900", "ascii"],
    "apu_reset/4017_written.nes": ["frames:120", "reset", "frames:600", "reset", "frames:600", "ascii"],
}

# testy, ktore potrzebuja wiecej klatek niz test_roms.xml x3
FRAMES_OVERRIDE = {
    "cpu_interrupts_v2/cpu_interrupts.nes": 1200,
    "mmc3_test/4-scanline_timing.nes": 600,
}

# liczba klatek: z test_roms.xml (x3 zapas), cpu.nes specjalnie dlugo
xml_frames = {}
_tree = ET.parse(os.path.join(ROMSDIR, "test_roms.xml"))
for _t in _tree.getroot().findall("test"):
    xml_frames[_t.get("filename")] = int(_t.get("runframes"))


def frames_for(rel):
    if rel.startswith("holy-mapperel/"):
        return 2400
    if rel in FRAMES_OVERRIDE:
        return FRAMES_OVERRIDE[rel]
    if rel in xml_frames:
        return min(max(xml_frames[rel] * 3, 180), 3600)
    if rel.endswith("blargg_nes_cpu_test5/cpu.nes"):
        return 7200
    return 900


def classify(text_lines):
    t = " ".join(text_lines).lower()
    if "fail" in t:
        return "fail"
    if "$01" in t:  # blargg APU
        return "pass"
    if "assed" in t or "all tests" in t:
        return "pass"
    return "unknown"


# ---------------- Holy Mapperel ----------------
# Font 8x5: litery na kafelkach $01-$1A (put stosuje and #$3F), reszta jak ASCII.
HM_MAP = " ABCDEFGHIJKLMNOPQRSTUVWXYZ      !\"#$%&'()*+,-./0123456789:;<=>?"

# Kody morsa (wartosc A przy JSR morsebeep -> litera), morse.inc
HM_MORSE_SYM = {
    0b10001000: "B", 0b10101000: "C", 0b10010000: "D", 0b00101000: "F",
    0b00100000: "I", 0b01001000: "L", 0b11100000: "M", 0b10100000: "N",
    0b11110000: "O", 0b01010000: "R", 0b00010000: "S", 0b11000000: "T",
    0b00110000: "U", 0b00011000: "V", 0b01110000: "W",
}
HM_MORSE_DESC = {
    "WB": "wrong bank przy starcie - zly bank PRG w $F000-$FFFF",
    "MIR": "mirroring nie pasuje do zadnego wspieranego mappera (detekcja nieudana)",
    "SU": "SUROM: przelaczenie na druga polowe (4M MMC1) nieudane",
    "LB": "detekcja mappera nie zostawila ostatnich 16K PRG w oknie",
    "RB": "powrot do ostatniego banku po nieudanym tescie nieudany",
    "CBT": "CHR bank tags niespojne - zle bankowanie CHR (RAM)",
    "FON": "font w CHR nie zgadza sie z kopia w PRG (zly odczyt CHR)",
    "DRV": "brak sterownika testow dla wykrytego mappera",
    "SMS": "kod SMS (zart z README)",
}
# Detal 4 cyfry: WRAM, PRG ROM, IRQ, CHR. 0 = OK, reszta wg README.
HM_POS = ["WRAM", "PRG", "IRQ", "CHR"]
HM_DETAILED = {
    (1, 0, "1"): "$E000 bit 4 nie wylacza WRAM",
    (1, 0, "4"): "$A000 bit 4 nie wylacza WRAM (SNROM) lub wylacza WRAM (poza SNROM)",
    (4, 0, "2"): "brak trybu read-only WRAM ($A001)",
}
# MISSING na ekranie jest oczekiwane, gdy naglowek NES 2.0 nie deklaruje PRG RAM


def hm_header_prgram(rel):
    """Deklarowany PRG RAM z naglowka (bajt 10, 0 = brak)."""
    try:
        with open(os.path.join(ROMSDIR, rel), "rb") as f:
            data = f.read(16)
    except OSError:
        return None
    return data[10] if len(data) >= 16 else None


def hm_mapper_of(rel):
    m = re.match(r"M(\d+)", os.path.basename(rel))
    return int(m.group(1)) if m else 0


def hm_detail_desc(rel, digits):
    mapper = hm_mapper_of(rel)
    out = []
    for i, d in enumerate(digits):
        if d == "0":
            continue
        known = HM_DETAILED.get((mapper, i, d))
        out.append("%s=%s%s" % (HM_POS[i], d, " (" + known + ")" if known else ""))
    return "; ".join(out)


def hm_classify(rel, text_lines):
    t = " ".join(text_lines)
    if "unsupported iNES mapper" in t:
        return "fail", "emulator nie implementuje tego mappera (Cartridge odrzuca ROM)"
    m = re.search(r"DETAILED TEST RESULT:\s*([0-9A-Fa-f]{4})", t)
    if not m:
        return None, None  # brak wyniku -> detekcja kodu morsa
    digits = m.group(1).upper()
    bad = [ln.strip() for ln in text_lines
           if "PROBLEM" in ln or "MISSING" in ln]
    if digits != "0000":
        return "fail", "detal %s: %s%s" % (
            digits, hm_detail_desc(rel, digits),
            ("; ekran: " + "; ".join(bad)) if bad else "")
    if bad:
        problems = [b for b in bad if "PROBLEM" in b]
        missing = [b for b in bad if "MISSING" in b]
        # PROBLEM = zawsze fail; MISSING = fail tylko gdy naglowek deklaruje PRG RAM
        if problems or (missing and hm_header_prgram(rel)):
            return "fail", "ekran: " + "; ".join(bad)
    return "pass", ""


# Kody morsa czytane z pamieci zamiast trace CPU:
# - litery to stale w kodzie ROM: wzorzec A9 xx 20 B0 FF = LDA #lit; JSR morsebeep
# - adres powrotu z JSR lezy na stosie CPU ($01F0-$01FF, zwykly RAM) -> identyfikuje
#   aktywna petle, a litera stoi w kodzie pod adresem (jsr_addr - 1)
# - sciezka MIR (unknown_mapper/morsebeep_axy) trzyma litery w zeropage $01-$03
HM_MORSE_RET = {}  # adres powrotu (CPU) -> litera


def hm_morse_sites():
    if HM_MORSE_RET:
        return
    path = None
    for rel, _ in TESTS:
        if rel.startswith("holy-mapperel/"):
            p = os.path.join(ROMSDIR, rel)
            if os.path.exists(p):
                path = p
                break
    if not path:
        return
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return
    if len(data) < 16:
        return
    # NES 2.0: bajt 4 = PRG low, bajt 8 bity 0-3 = PRG high; ostatnie 32K = primary
    prg = (data[4] | ((data[8] & 0x0F) << 8)) * 16384
    prim = data[16 + prg - 0x8000:16 + prg]
    i = 0
    while True:
        j = prim.find(b"\x20\xB0\xFF", i)
        if j < 0:
            break
        if j >= 2 and prim[j - 2] == 0xA9:
            sym = HM_MORSE_SYM.get(prim[j - 1])
            if sym:
                # 6502: JSR wpycha (adres_powrotu - 1), RTS dodaje 1
                HM_MORSE_RET[0x8000 + j + 2] = sym
        i = j + 1


def hm_morse_desc_from_letters(letters):
    seen = list(dict.fromkeys(letters))
    sc = set(seen)
    exact = [c for c in HM_MORSE_DESC if set(c) == sc]
    if exact:
        return "brak ekranu wyniku - kod morsa %s (%s)" % (
            exact[0], HM_MORSE_DESC[exact[0]])
    partial = [c for c in HM_MORSE_DESC if sc <= set(c)]
    if partial:
        return "brak ekranu wyniku - litery morsa %s (fragment kodu; mozliwe: %s)" % (
            "".join(seen), ", ".join(partial))
    return "brak ekranu wyniku - litery morsa %s (nieznany kod)" % "".join(seen)


def run_rom(rel, args):
    path = os.path.join(ROMSDIR, rel)
    if not os.path.exists(path):
        return None, "MISSING"
    p = subprocess.run([NES_TEST, path] + args, capture_output=True,
                       text=True, timeout=240)
    out = (p.stdout or "") + (p.stderr or "")
    lines = out.splitlines()
    screen = []
    for j, ln in enumerate(lines):
        if "screen" in ln and "ascii" in ln:
            screen = lines[j + 1:]
            break
    text = [ln.rstrip() for ln in screen if ln.strip()]
    if not text:
        text = ["[brak tekstu ekranu; exit=%d] %s" % (p.returncode, out.strip()[:100])]
    return text, "ok"


def poll_status(rel, text):
    if rel.startswith("holy-mapperel/"):
        return hm_classify(rel, text)
    return classify(text), ""


def hm_morse_complete(letters):
    exact = [c for c in HM_MORSE_DESC if set(c) == set(letters)]
    return exact[0] if exact else None


def run_rom_poll(rel, budget, step=30):
    """Spawn nes_test in stdin mode and drive it interactively: clock `step`
    frames, dump ascii, classify, stop as soon as a final result appears
    (HM: single poll; others: two consecutive polls with the same status to
    dodge transient screens). For HM ROMs the CPU stack ($01F0) and zero page
    are sampled too: the JSR-morsebeep return addresses on the stack identify
    the active morse beep loop immediately (no trace needed).
    Returns (text, err, hm_morse) where hm_morse is a description string or
    empty. Fallback for HM stuck ROMs: caller classifies the empty screen."""
    path = os.path.join(ROMSDIR, rel)
    if not os.path.exists(path):
        return None, "MISSING", ""
    hm = rel.startswith("holy-mapperel/")
    if hm:
        hm_morse_sites()
    char_map = HM_MAP if hm else ""
    stderr_tail = []
    try:
        p = subprocess.Popen([NES_TEST, path], stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             text=True, bufsize=1)
    except OSError:
        return [], "ok", ""

    q = queue.Queue()
    done = threading.Event()

    def reader():
        for line in p.stdout:
            q.put(line)
        done.set()
        q.put(None)

    def err_reader():
        for line in p.stderr:
            stderr_tail.append(line.rstrip())
            if len(stderr_tail) > 200:
                stderr_tail.pop(0)

    t_out = threading.Thread(target=reader, daemon=True)
    t_err = threading.Thread(target=err_reader, daemon=True)
    t_out.start()
    t_err.start()

    def send(cmd):
        p.stdin.write(cmd + "\n")
        p.stdin.flush()

    def read_output(nlines):
        got = []
        for _ in range(nlines):
            try:
                item = q.get(timeout=30)
            except queue.Empty:
                break
            if item is None:
                break
            got.append(item)
        return got

    def parse_mem(got, prefix):
        for ln in got:
            m = re.match(prefix + r": (.*)", ln)
            if m:
                return [int(x, 16) for x in m.group(1).split()]
        return []

    frames = 0
    last_text = []
    prev = None  # poprzedni status pass/fail (dla potwierdzenia 2 polli)
    mir_prev = False  # wzorzec MIR w zeropage w poprzednim pollu
    morse_letters = []  # akumulacja liter morsa z kolejnych probeek stosu
    nlines = 31 + (4 if hm else 0)  # ascii (31) + mem:0x01F0:16 + mem:0x00:8
    try:
        while frames < budget:
            try:
                send("frames:%d" % step)
                send("ascii" if not char_map else "ascii:%s" % char_map)
                if hm:
                    send("mem:0x01F0:16")
                    send("mem:0x00:8")
            except OSError:
                break  # dziecko padlo (np. nieobslugiwany mapper)
            got = read_output(nlines)
            text = [ln.rstrip() for ln in got[1:31] if ln.strip()]
            if hm:
                stack = parse_mem(got, r"01F0")
                for i in range(len(stack) - 1):
                    pushed = stack[i] | (stack[i + 1] << 8)
                    sym = HM_MORSE_RET.get(pushed)
                    if sym and sym not in morse_letters:
                        morse_letters.append(sym)
                if morse_letters:
                    # gesta faza potwierdzenia: probki co 10 klatek, az zbior
                    # liter dopasuje sie do jednego kodu z README
                    for _ in range(15):
                        if hm_morse_complete(morse_letters):
                            return text, "ok", hm_morse_desc_from_letters(morse_letters)
                        try:
                            send("frames:10")
                            send("mem:0x01F0:16")
                        except OSError:
                            break
                        got2 = read_output(2)
                        stack2 = parse_mem(got2, r"01F0")
                        for i in range(len(stack2) - 1):
                            pushed = stack2[i] | (stack2[i + 1] << 8)
                            sym = HM_MORSE_RET.get(pushed)
                            if sym and sym not in morse_letters:
                                morse_letters.append(sym)
                    return text, "ok", hm_morse_desc_from_letters(morse_letters)
                zp = parse_mem(got, r"0000")
                if len(zp) >= 4:
                    z = [HM_MORSE_SYM.get(b) for b in zp[1:4]]
                    if z == ["M", "I", "R"] and mir_prev:
                        return text, "ok", hm_morse_desc_from_letters(z)
                    mir_prev = (z == ["M", "I", "R"])
                else:
                    mir_prev = False
            status, _ = poll_status(rel, text)
            if status in ("pass", "fail"):
                if hm or status == prev:
                    return text, "ok", ""
                prev = status
            else:
                prev = None
            last_text = text
            frames += step
            if done.is_set():
                break
        if done.is_set():
            t_err.join(timeout=1)  # daj watkowi stderr dojsc do konca
            return (last_text or []) + stderr_tail[-10:], "ok", ""
        return last_text, "ok", ""
    finally:
        try:
            p.kill()
        except OSError:
            pass
        p.wait()
        t_out.join(timeout=1)
        t_err.join(timeout=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()

    t0 = time.time()

    if not args.no_build:
        print("=== BUILD ===")
        r = subprocess.run(["ninja", "-C", BUILD])
        if r.returncode != 0:
            print("BUILD FAILED")
            return 2
        print("build OK")

    fails = []          # (nazwa, powod) - wszystkie faile
    unverified = []     # ekran bez markerow pass/fail
    pass_count = 0

    # ---- accuracy_coin ----
    print("\n=== accuracy_coin ===")
    r = subprocess.run([ACCURACY], capture_output=True, text=True, timeout=900)
    out = (r.stdout or "") + (r.stderr or "")
    m = re.search(r"TESTS PASSED:\s*(\d+)\s*/\s*(\d+)\s*\(failed:\s*(\d+)", out)
    if m:
        print("TESTS PASSED: %s / %s (failed: %s)" % m.groups())
    else:
        print("nie znaleziono linii podsumowania")
        print(out[-500:])
    for l in out.splitlines():
        if " - FAIL" in l:
            name = re.sub(r" - FAIL.*", "", l.strip())
            name = re.sub(r"^\[test \d+\]\s*", "test: ", name)
            fails.append(("accuracy_coin: %s" % name, "timing cyklowy PPU (szczegoly w sekcji accuracy_coin)"))
            print("  FAIL:", l.strip())

    # ---- testy ROM ----
    print("\n=== TESTY ROM (%d) ===" % len(TESTS))
    for i, (rel, note) in enumerate(TESTS, 1):
        hm_morse = ""
        if rel in RESET_ARGS or rel.startswith("apu_reset/"):
            args_rom = RESET_ARGS.get(rel)
            if args_rom is None:
                args_rom = ["frames:120", "reset", "frames:900", "ascii"]
            text, err = run_rom(rel, args_rom)
        else:
            text, err, hm_morse = run_rom_poll(rel, frames_for(rel))
        if err != "ok":
            unverified.append("%s: MISSING" % rel)
            print("[%3d/%d] %-9s %s" % (i, len(TESTS), "MISSING", rel))
            continue
        status = classify(text)
        reason = note
        if rel.startswith("holy-mapperel/"):
            if hm_morse:
                status, reason = "fail", hm_morse
            else:
                status, reason = hm_classify(rel, text)
                if status is None:
                    status = "fail"
                    reason = ("brak ekranu wyniku i brak kodu morsa - zawieszony "
                              "(zly bank PRG przy starcie / czeka na NMI)")
        if note.startswith("info:"):
            tag = "INFO"
        elif status == "pass":
            tag = "PASS"
            pass_count += 1
        elif status == "fail":
            tag = "FAIL"
            fails.append((rel, reason))
        else:  # unknown
            tag = "NIEZWERYFIKOWANY"
            unverified.append(rel)
        print("[%3d/%d] %-17s %s" % (i, len(TESTS), tag, rel))

    # ---- podsumowanie ----
    print("\n" + "=" * 60)
    print("PODSUMOWANIE (%.0fs)" % (time.time() - t0))
    print("=" * 60)
    print("PASS: %d    FAILI: %d" % (pass_count, len(fails)))
    for name, reason in fails:
        print("  - %s" % name)
        if reason:
            print("      %s" % reason)
    if unverified:
        print("\nNIEZWERYFIKOWANE (ekran bez markerow, sprawdz recznie):")
        for rel in unverified:
            print("  - %s" % rel)
    print("=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(main())
