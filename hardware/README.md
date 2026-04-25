# SprinKlr-8 Custom PCB Hardware

Custom board design for SprinKlr-8. Replaces the ACEIRMC off-shelf module.

## Directory Structure

```
hardware/
├── schematics/   — EasyEDA schematic exports (.json, .pdf)
├── pcb/          — EasyEDA PCB layout exports (.json, .pdf)
├── bom/          — Bill of materials (JLCPCB BOM CSV, master BOM)
├── gerbers/      — Gerber ZIP files exported from EasyEDA for JLCPCB order
└── README.md
```

## Design Reference

See `SprinKlr8_PCB_Design_Guide.docx` in the project root for the full
schematic entry guide, component netlist, layout rules, and JLCPCB order process.

## Key ICs

| Ref | Part | LCSC |
|-----|------|------|
| U1 | AMS1117-3.3 SOT-223 | C6186 |
| U2 | CH340C SOP-16 | C84681 |
| U3 | ULN2803A SOP-18 | C8652 |
| U4 | ESP32-WROOM-32E | C701341 |
| K1–K8 | HF46F-G/5-HS1 5V SPST-NO | C165255 |

## Board Specs

- 2-layer, 80×90mm target
- JLCPCB Standard PCBA, top side assembly
- USB-C programming (CH340C, auto-reset)
- 8-zone SPST relay outputs, 24VAC valve support
