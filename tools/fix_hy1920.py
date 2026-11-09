#!/usr/bin/env python3
"""1) Build HY1920.kicad_sym (HUAYI HY1920W, TO-247, G=1 D=2 S=3) from stock Transistor_FET:BUZ11 graphics.
2) Re-point Q10/Q12/Q14/Q16 (Device:Q_NMOS "N-FET HY1920") to HY1920:HY1920W with TO-247 footprint.

Pin geometry of BUZ11 == Device:Q_NMOS, so existing wires stay connected."""
import re, sys
PROJ = sys.argv[1]
STOCK = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/Transistor_FET.kicad_sym"
SCH = f"{PROJ}/MakeSkyBlue-60A.kicad_sch"

def block_at(s, i):
    d = 0
    for j in range(i, len(s)):
        if s[j] == '(':
            d += 1
        elif s[j] == ')':
            d -= 1
            if d == 0:
                return s[i:j + 1]

def setprop(b, key, val):
    return re.sub(r'(\(property "%s" )"[^"]*"' % key, lambda m: m.group(1) + '"%s"' % val, b, count=1)

# ---------- 1) library symbol ----------
stock = open(STOCK).read()
buz = block_at(stock, stock.find('(symbol "BUZ11"'))
sym = buz.replace('"BUZ11"', '"HY1920W"').replace('"BUZ11_', '"HY1920W_')
sym = setprop(sym, "Value", "HY1920W")
sym = setprop(sym, "Footprint", "Package_TO_SOT_THT:TO-247-3_Vertical")
sym = setprop(sym, "Datasheet", "${KIPRJMOD}/Datasheets/HY1920W.pdf")
sym = setprop(sym, "Description",
              "HUAYI HY1920W N-channel MOSFET, 200 V, 90 A, RDS(on) 23 mOhm typ @ VGS 10 V, "
              "VGS +/-20 V, TO-247A-3L (1=G, 2=D/tab, 3=S)")
sym = setprop(sym, "ki_keywords", "HY1920 HY1920W HUAYI N-Channel MOSFET 200V 90A TO-247")
sym = setprop(sym, "ki_fp_filters", "TO?247*")
open(f"{PROJ}/HY1920.kicad_sym", "w").write(
    '(kicad_symbol_lib\n\t(version 20251024)\n\t(generator "hermes_fix_hy1920")\n\t(generator_version "10.0")\n\t'
    + sym + '\n)\n')

# ---------- 2) schematic ----------
s = open(SCH).read()

# embed lib symbol (lib_symbols cache) once
cache = sym.replace('(symbol "HY1920W"', '(symbol "HY1920:HY1920W"', 1)
if '(symbol "HY1920:HY1920W"' not in s:
    i = s.find('(lib_symbols')
    j = s.find('\n', i)
    s = s[:j + 1] + '\t\t' + cache + '\n' + s[j + 1:]

PINMAP = {"G": "1", "D": "2", "S": "3"}
changed = []
out, pos = [], 0
for m in re.finditer(r'\n\t\(symbol\n\t\t\(lib_id "([^"]+)"\)', s):
    st = m.start() + 1
    b = block_at(s, st)
    ref = re.search(r'\(property "Reference" "([^"]*)"', b).group(1)
    nb = b
    if m.group(1) == "Device:Q_NMOS" and "HY1920" in b:
        nb = nb.replace('(lib_id "Device:Q_NMOS")', '(lib_id "HY1920:HY1920W")', 1)
        nb = setprop(nb, "Value", "HY1920W")
        nb = setprop(nb, "Footprint", "Package_TO_SOT_THT:TO-247-3_Vertical")
        nb = setprop(nb, "Datasheet", "${KIPRJMOD}/Datasheets/HY1920W.pdf")
        nb = setprop(nb, "Description", "HUAYI HY1920W N-MOSFET 200 V 90 A, TO-247")
        nb = re.sub(r'\(pin "([GDS])"', lambda p: '(pin "%s"' % PINMAP[p.group(1)], nb)
        changed.append(ref)
    if nb != b:
        out.append(s[pos:st]); out.append(nb); pos = st + len(b)
out.append(s[pos:])
s = "".join(out)
open(SCH, "w").write(s)
print("changed:", changed)
