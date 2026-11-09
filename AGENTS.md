# AGENTS.md

Notes for coding agents working in this repo. Read `README.md` first. For firmware work, `replacement_esp32_firmware/README.md` holds the pin map and status.

## KiCad rules
- **The user usually has the project open in KiCad.** Before editing `.kicad_sch` from a script, back it up. Afterwards, tell the user to reload the file, or their next save in KiCad overwrites your edit. Re-read the file before each edit; the user saves often and re-annotates references (e.g. the HY1920Ws are now Q10/Q12/Q13/Q15).
- **Don't change parts the user didn't name.** If a footprint looks wrong, ask first (Q2/Q3 TO-220 are intentional).
- **Verify with kicad-cli, not by eye:** `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`
  - `sch erc` (baseline: 82 violations, with annotation warnings)
  - `sch export netlist --format kicadxml`: diff before and after to prove connectivity didn't change.
  - `sym export svg` + `rsvg-convert` to check that symbols render properly.
- **Pin numbers must match the footprint.** `Device:Q_NMOS`/`Q_PMOS` use the letters D/G/S as pin numbers, which don't map to TO-220/TO-247 pads. Use a numbered symbol (G=1, D=2, S=3; geometry same as `Transistor_FET:BUZ11`).
- **Project libraries:** register new `.kicad_sym` files in `sym-lib-table` with `${KIPRJMOD}`. Point the Datasheet fields at `${KIPRJMOD}/Datasheets/...`.
- **Generator scripts:** put them in `tools/` so symbols can be regenerated.
- **README images:** after changing the schematic, regenerate them with `python3 tools/export_sch_images.py` (adjust the crop boxes if blocks move).
- **Datasheets:** verify every part against its datasheet in `Datasheets/`. The PDFs there are git-ignored (vendor copyright); add each new download link to `Datasheets/SOURCES.md`. Read them with `pdftotext -layout`, and render pages with `pdftoppm` to read figures. LCSC PDFs: `https://www.lcsc.com/datasheet/<Cxxxx>.pdf` returns HTML; take the real `datasheet.lcsc.com/...pdf` link from inside it.

## Board facts
- The original MCU is a Nuvoton NUC029LAN (LQFP-48); it's dead on this unit. The NUC120 files are an earlier wrong guess.
- All TO-220/TO-247 parts are mounted on the back, lying flat with the tab away from the PCB. Use `Package_TO_SOT_THT:*_Horizontal_TabUp` footprints and flip them to B.Cu.
- TR1 `MP-1248-3MF1554` is a custom flyback transformer with no public datasheet. Its current symbol and footprint are a Breve TEZ placeholder. Don't invent pinouts; ask for measurements.

## Firmware
- ESP-IDF v5.5. Set `IDF_PYTHON_ENV_PATH=~/.espressif/python_env/idf5.5_py3.13_env`, then `. ~/esp/esp-idf/export.sh` (the default Python 3.14 breaks it).
- The power stage and load output are locked off by Kconfig. Never enable them by default.

## Git
- The remote is `git@github.com:samitocklin/rev-eng-makeskyblue60A.git`. KiCad backups, locks, `.kicad_prl`, firmware `build/`, `managed_components/` and `sdkconfig` are git-ignored.
