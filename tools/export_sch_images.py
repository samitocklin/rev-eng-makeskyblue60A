#!/usr/bin/env python3
"""Regenerate the README schematic images in pictures/.
Usage: python3 tools/export_sch_images.py   (run from the project root; needs kicad-cli, rsvg-convert, Pillow)
Crop boxes are in a 1000-px-wide coordinate frame of the whole sheet; adjust if blocks move."""
import glob, os, subprocess, tempfile
from PIL import Image

KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
CROPS = {
    "sch_overview": None,
    "sch_power_stage": (85, 262, 590, 462),   # TLP350 drivers, HY1920W, STTH3003, L1/L2
    "sch_aux_supply": (495, 40, 945, 265),    # MIC5205, UC3845 flyback, TR1
    "sch_pv_input": (30, 480, 600, 675),      # isolated PV switch drive, shunt, terminals, load switch
    "sch_mcu": (60, 25, 385, 250),            # NUC029LAN, buttons, LCD header
}

tmp = tempfile.mkdtemp()
subprocess.run([KICAD_CLI, "sch", "export", "svg", "--exclude-drawing-sheet", "-o", tmp,
                "MakeSkyBlue-60A.kicad_sch"], check=True, stderr=subprocess.DEVNULL)
svg = glob.glob(os.path.join(tmp, "*.svg"))[0]
big = os.path.join(tmp, "big.png")
subprocess.run(["rsvg-convert", "-w", "6000", "-b", "white", svg, "-o", big], check=True)

im = Image.open(big)
k = im.width / 1000
for name, box in CROPS.items():
    c = im if box is None else im.crop(tuple(int(v * k) for v in box))
    if c.width > 2400:
        c = c.resize((2400, round(2400 * c.height / c.width)), Image.LANCZOS)
    c.convert("RGB").save(f"pictures/{name}.png", optimize=True)
    print(f"pictures/{name}.png", c.size)
