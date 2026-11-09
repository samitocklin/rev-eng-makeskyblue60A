#!/usr/bin/env python3
"""Generate TLP350.kicad_sym (Toshiba TLP350 / TLP350F gate-drive photocoupler)
with the internal circuit drawn per datasheet section 6 (Rev.2.0, 2025-10-23)."""
import sys

out = sys.argv[1]
G = []  # graphics for unit 0 style 1

def stroke(w=0.254, t="default"):
    return f'(stroke (width {w}) (type {t}))'

def pl(*pts, w=0.254, fill="none", t="default"):
    p = " ".join(f"(xy {x:g} {y:g})" for x, y in pts)
    G.append(f'(polyline (pts {p}) {stroke(w, t)} (fill (type {fill})))')

def rect(x1, y1, x2, y2, w=0.254, fill="none"):
    G.append(f'(rectangle (start {x1:g} {y1:g}) (end {x2:g} {y2:g}) {stroke(w)} (fill (type {fill})))')

def dot(x, y, r=0.3):
    G.append(f'(circle (center {x:g} {y:g}) (radius {r:g}) {stroke(0)} (fill (type outline)))')

def text(s, x, y, size=1.0, just=""):
    j = f" (justify {just})" if just else ""
    G.append(f'(text "{s}" (at {x:g} {y:g} 0) (effects (font (size {size:g} {size:g})){j}))')

# Body
rect(-15.24, 10.16, 15.24, -10.16, w=0.254, fill="background")

# --- Input LED (pin 2 anode y=+2.54, pin 3 cathode y=-2.54) ---
lx = -11.43
pl((-15.24, 2.54), (lx, 2.54), (lx, 0.9525))
pl((lx - 0.9525, 0.9525), (lx + 0.9525, 0.9525), (lx, -0.9525), (lx - 0.9525, 0.9525), fill="outline")
pl((lx - 0.9525, -0.9525), (lx + 0.9525, -0.9525))
pl((lx, -0.9525), (lx, -2.54), (-15.24, -2.54))
text("A", -14.5, 3.4, 1.0, "left")
text("K", -14.5, -3.4, 1.0, "left")

# Light arrows LED -> photodiode
for y in (0.635, -0.635):
    pl((-9.525, y), (-6.6, y))
    pl((-6.35, y), (-7.112, y + 0.381), (-7.112, y - 0.381), (-6.35, y), fill="outline")

# --- Faraday shield (dashed), tied to GND rail ---
pl((-5.08, 7.62), (-5.08, -7.62), t="dash")
text("SHIELD", -5.08, -8.9, 0.8)

# --- Photodiode (reverse biased, cathode up) ---
px = -3.175
pl((px - 0.9525, -0.9525), (px + 0.9525, -0.9525), (px, 0.9525), (px - 0.9525, -0.9525), fill="outline")
pl((px - 0.9525, 0.9525), (px + 0.9525, 0.9525))
pl((px, 0.9525), (px, 1.905), (-0.635, 1.905))
pl((px, -0.9525), (px, -1.905), (-0.635, -1.905))

# --- Amplifier ---
pl((-0.635, 3.175), (4.445, 0), (-0.635, -3.175), (-0.635, 3.175), fill="background")
pl((1.905, 1.5875), (1.905, 7.62))
pl((1.905, -1.5875), (1.905, -7.62))
pl((4.445, 0), (5.715, 0))

# --- Driver / level-shift block feeding the two gates ---
rect(5.715, 5.08, 6.985, -5.08)
pl((6.35, 5.08), (6.35, 7.62))
pl((6.35, -5.08), (6.35, -7.62))

# --- Supply rails ---
pl((1.905, 7.62), (15.24, 7.62))            # VCC (pin 8)
pl((-5.08, -7.62), (15.24, -7.62))          # GND (pin 5)

def nfet(yc, drain_up=True):
    """N-channel enhancement MOSFET, gate on left at x=8.89, channel x=9.525,
    drain/source returns at x=11.43, body arrow pointing into channel."""
    gx, cx, rx = 8.89, 9.525, 11.43
    pl((6.985, yc), (gx, yc))
    pl((gx, yc - 1.27), (gx, yc + 1.27))
    for a, b in ((0.762, 1.27), (-0.254, 0.254), (-1.27, -0.762)):
        pl((cx, yc + a), (cx, yc + b), w=0.3)
    top, bot = yc + 1.016, yc - 1.016
    pl((cx, top), (rx, top))
    pl((cx, bot), (rx, bot))
    pl((cx, yc), (rx, yc))
    pl((cx, yc), (cx + 0.762, yc + 0.381), (cx + 0.762, yc - 0.381), (cx, yc), fill="outline")

# M1: high side, drain -> VCC, source+body -> VO
nfet(3.81)
pl((11.43, 4.826), (11.43, 7.62))
pl((11.43, 3.81), (11.43, -3.81 + 1.016))   # source/body of M1 to drain of M2 (VO node)
# M2: low side, drain -> VO, source+body -> GND
nfet(-3.81)
pl((11.43, -3.81), (11.43, -7.62))
pl((11.43, 0), (15.24, 0))                  # VO -> pin 6
text("M1", 8.4, 6.0, 1.0)
text("M2", 8.4, -6.0, 1.0)

for x, y in ((6.35, 7.62), (11.43, 7.62), (1.905, -7.62), (6.35, -7.62), (11.43, -7.62), (11.43, 0)):
    dot(x, y)

text("VCC", 14.6, 8.6, 1.0, "right")
text("VO", 14.6, 0.9, 1.0, "right")
text("GND", 14.6, -8.6, 1.0, "right")
text("NC", -14.5, 7.62, 1.0, "left")
text("NC", -14.5, -7.62, 1.0, "left")
text("NC", 14.6, 5.08, 1.0, "right")

def pin(etype, num, name, x, y, ang, hide=False):
    h = " (hide yes)" if hide else ""
    return (f'(pin {etype} line (at {x:g} {y:g} {ang}) (length 2.54){h} '
            f'(name "{name}" (effects (font (size 1.27 1.27)))) '
            f'(number "{num}" (effects (font (size 1.27 1.27)))))')

pins = [
    pin("no_connect", 1, "NC", -17.78, 7.62, 0),
    pin("passive", 2, "A", -17.78, 2.54, 0),
    pin("passive", 3, "K", -17.78, -2.54, 0),
    pin("no_connect", 4, "NC", -17.78, -7.62, 0),
    pin("power_in", 5, "GND", 17.78, -7.62, 180),
    pin("output", 6, "VO", 17.78, 0, 180),
    pin("no_connect", 7, "NC", 17.78, 5.08, 180),
    pin("power_in", 8, "VCC", 17.78, 7.62, 180),
]

def prop(k, v, x=0, y=0, hide=True, just=""):
    h = " (hide yes)" if hide else ""
    j = f" (justify {just})" if just else ""
    return f'(property "{k}" "{v}" (at {x:g} {y:g} 0) (effects (font (size 1.27 1.27)){j}){h})'

desc = ("Toshiba gate-drive photocoupler, IR LED + Faraday-shielded photo IC, totem-pole N-MOS output "
        "(M1 high side, M2 low side), +/-2.5 A peak, VCC 15-30 V, IFLH 5 mA max, tpd 500 ns max, "
        "CMTI 15 kV/us, 3750 Vrms, DIP-8 7.62 mm. Needs 0.1 uF between pin 8 and pin 5. "
        "LED on -> M1 on -> VO high.")

lib = ['(kicad_symbol_lib (version 20241209) (generator "hermes_gen_tlp350") (generator_version "9.0")']
lib.append(
    '(symbol "TLP350" (pin_names (offset 0) (hide yes)) (exclude_from_sim no) (in_bom yes) (on_board yes) '
    + prop("Reference", "U", 0, 11.43, hide=False)
    + prop("Value", "TLP350", 0, -11.43, hide=False)
    + prop("Footprint", "Package_DIP:DIP-8_W7.62mm")
    + prop("Datasheet", "${KIPRJMOD}/Datasheets/TLP350_datasheet_en_20251023.pdf")
    + prop("Description", desc)
    + prop("ki_keywords", "TLP350 Toshiba photocoupler optocoupler gate driver IGBT MOSFET isolated totem pole")
    + prop("ki_fp_filters", "DIP*W7.62mm* SMDIP*W9.53mm*")
    + '(symbol "TLP350_0_1" ' + " ".join(G) + ')'
    + '(symbol "TLP350_1_1" ' + " ".join(pins) + ')'
    + ')'
)
lib.append(
    '(symbol "TLP350F" (extends "TLP350") '
    + prop("Reference", "U", 0, 11.43, hide=False)
    + prop("Value", "TLP350F", 0, -11.43, hide=False)
    + prop("Footprint", "Package_DIP:DIP-8_W10.16mm")
    + prop("Datasheet", "${KIPRJMOD}/Datasheets/TLP350_datasheet_en_20251023.pdf")
    + prop("Description", desc.replace("DIP-8 7.62 mm", "DIP-8 10.16 mm wide lead pitch (8 mm creepage)"))
    + prop("ki_keywords", "TLP350F Toshiba photocoupler optocoupler gate driver IGBT MOSFET isolated totem pole")
    + prop("ki_fp_filters", "DIP*W10.16mm* SMDIP*W11.48mm*")
    + ')'
)
lib.append(')')
open(out, "w").write("\n".join(lib) + "\n")
print("wrote", out)
