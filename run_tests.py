#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Przeglad stanu emulatora NES: build + accuracy_coin + wszystkie ROM-y
testowe z jednoznacznym wynikiem tekstowym (bez testow wizualnych i
niejednoznacznych - nie da sie ich zweryfikowac automatycznie). Wypisuje
aktualny stan: wszystkie faile z powodami. Nic nie porownuje - raport to
po prostu stan. Uzycie:
  python run_tests.py              # build + wszystkie testy (~4 min)
  python run_tests.py --no-build   # bez przebudowy
"""
import argparse
import os
import re
import subprocess
import sys
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
        args_rom = RESET_ARGS.get(rel)
        if args_rom is None:
            if rel.startswith("apu_reset/"):
                args_rom = ["frames:120", "reset", "frames:900", "ascii"]
            else:
                args_rom = ["frames:%d" % frames_for(rel), "ascii"]
        text, err = run_rom(rel, args_rom)
        if err != "ok":
            unverified.append("%s: MISSING" % rel)
            print("[%3d/%d] %-9s %s" % (i, len(TESTS), "MISSING", rel))
            continue
        status = classify(text)
        if note.startswith("info:"):
            tag = "INFO"
        elif status == "pass":
            tag = "PASS"
            pass_count += 1
        elif status == "fail":
            tag = "FAIL"
            fails.append((rel, note))
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
