# Keskkonnasensor — Modbus RTU

STM32G031F8P6-based environmental sensor with RS-485/Modbus RTU slave interface. Measures temperature, humidity, and air quality (eCO2/TVOC) via a snap-off remote sensor tab, reporting over an RS-485 daisy-chain bus to a Modbus RTU master.

## Key specs
- MCU: STM32G031F8P6 (TSSOP20)
- Sensors: SHT45 (temp/humidity), ENS160 (air quality), on a separate snap-off tab, panelized with the main board via mouse-bite
- Interface: RS-485, MAX13487 transceiver, AutoDirection
- Power: 12-30V DC input, MCP16301 buck → 5V, TLV70433 LDO → 3.3V (tab: MCP1700 → 1.8V for ENS160)
- 2-layer PCB, JLCPCB manufacturable

## Repo structure
- `*.kicad_*` (root) — KiCad project files
- `manufacturing/` — Gerbers, drill files, and BOM for ordering
- `docs/` — schematic design documentation and pinout reference

## Status
PCB layout complete, DRC clean (0 unconnected items), ready for fabrication. Tab board's SHT45/ENS160 to be hand-soldered via hot air + stencil.
