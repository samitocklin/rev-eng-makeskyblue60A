#!/usr/bin/env python3
"""Set every TO-220 / TO-247 part to the *_Horizontal_TabUp footprint.
On this board the power parts lie flat on the BACK side with the metal tab facing
away from the PCB (towards the heatsink): place them in PCB and press F (flip to B.Cu).
Pad numbering is unchanged (Vertical/TabDown -> TabUp keep pads 1..3)."""
import re, sys
SCH = sys.argv[1]
s = open(SCH).read()

def end_of(i):
    d = 0
    for j in range(i, len(s)):
        d += s[j] == '('
        d -= s[j] == ')'
        if d == 0:
            return j + 1

FP = re.compile(r'(\(property "Footprint" ")Package_TO_SOT_THT:(TO-(?:220|247)-\d)_(?:Vertical|Horizontal_TabDown)(")')
out, pos, changed = [], 0, []
for m in re.finditer(r'\n\t\(symbol\n\t\t\(lib_id "[^"]+"\)', s):
    st = m.start() + 2
    en = end_of(st)
    b = s[st:en]
    nb, n = FP.subn(lambda x: f'{x.group(1)}Package_TO_SOT_THT:{x.group(2)}_Horizontal_TabUp{x.group(3)}', b, count=1)
    if n:
        ref = re.search(r'\(property "Reference" "([^"]*)"', b).group(1)
        changed.append((ref, FP.search(b).group(2)))
        out += [s[pos:st], nb]
        pos = en
out.append(s[pos:])
open(SCH, "w").write("".join(out))
for r, p in sorted(changed):
    print(r, "->", p + "_Horizontal_TabUp")
