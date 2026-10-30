# Design decisions and open items

Notes written from the design/ordering discussion (Oct 2026). Each item says whether it was checked against a datasheet or is still a guess. Check the datasheets and the KiCad files before relying on anything marked "unverified".

## Overview

Environmental sensor, Modbus RTU over RS-485. STM32G031F8P6 (TSSOP-20) plus a snap-off sensor tab with SHT45 (temperature/humidity) and ENS160 (air quality). Input 12-30 V. Plan: build 5 boards by hand.

## Sensors

- **ENS160** was chosen over the SGP41. Swapping to the SGP41 would be a rev 2 change (it gives relative VOC/NOx indices and needs a host-run algorithm).
- ENS160 outputs (datasheet v0.95):
  - TVOC in ppb (calibrated against ethanol).
  - eCO2 is an estimate, not a real CO2 measurement. Do not present it as CO2.
  - AQI-UBA index 1-5.
  - Accuracy is only specified as "<12 % typical for hydrogen" in clean chamber air.
  - Warm-up about 1 minute, initial start-up about 1 hour (once), 48 h settling.
  - DATA_STATUS flags (0-3) must be read and exposed so users can tell whether values are valid.
- When exposing values over Modbus, expose the status flag next to the values.

## Power

- 12-30 V input, MCP16301 buck, then 5 V, 3.3 V and 1.8 V (for ENS160, MCP1700-1802) rails. Check the BOM/schematic for the exact parts.
- **C3 (buck input capacitor)**: the BOM says 10 uF, >= 50 V. No such 0805 part was in stock at Mouser, so 4.7 uF / 50 V X5R (Murata GRM21BR61H475KE51L) is used for the prototype.
  - MCP16301 datasheet recommends 4.7-10 uF input capacitance (minimum 2.2 uF). X7R is preferred over X5R.
  - Ceramic capacitance drops under DC bias. At 24-30 V the real value may be only about 1.5-2 uF (this is an engineering estimate, not from the datasheet; check the manufacturer's DC-bias tool).
  - Test: measure VIN ripple at U5 with a scope at 12 V and 30 V input. Fix if needed by swapping in a 10 uF part or adding a capacitor at the input terminals.
- Other 10 uF capacitors (C5, C13, C14) are 16 V X7R (GRM21BZ71C106KE15K). 1 uF capacitors (C1, C6, C7, C15) are 25 V X7R 0805.
- F2 is a PTC resettable fuse (Bourns MF-R050 type: hold 0.5 A, trip 1 A, 60 V). Trade-off versus a conventional fuse: resets by itself, but trips slower and its hold current drops at high temperature.
- Inductor L2: 22 uH, saturation current >= 1.7 A (SLF7055T-220M1R7).

## RS-485

- MAX13487E, AutoDirection (no DE pin to drive from the MCU), on USART1.
- ESD/surge: SM712-type asymmetric TVS (Bourns CDSOT23-SM712), pin 3 = ground, pins 1 and 2 = A/B. Datasheet checked: 7 V / 12 V working voltage, 400 W, 90 pF.
- 120 ohm termination (R18) on board. Only the two ends of the bus should be terminated.
- **Re-check before power-up (unverified):** from the PCB, MAX13487 pin 8 is on the 5V net and pins 2/3 on 3V3, and the RO net is separate from the MCU's USART1_RX net. Confirm supply, level shifting and pin functions against the MAX13487E datasheet.

## Clock

- Y1: Kyocera KC5032Z8.00000C1JX oscillator (footprint SeikoEpson SG8002LB 5.0x3.2 mm). Pad 1 INH to 3V3, pad 2 GND, pad 3 output (OSC net), pad 4 3V3 (wiring checked on the PCB).
- Output goes to PC14 (HSE bypass / external clock). Frequency is probably 8 MHz (from the part code, unverified).
- `docs/skeem.md` still says PC14/PC15 are unused and uses older connector designators. It needs updating.

## Connectors

- Würth WR-TBL: 382-series headers (e.g. 691382010008, 8 pin), mating push-in plugs from the 381 series (691381000008, 8 pin). The 4-pin plug code 691381000004 is inferred and was not verified. Use wire ferrules.
- J7: 4-pin 2.54 mm header.

## Manufacturing and ordering

- PCBs from JLCPCB (planned): 2 layers, 74 x 76 mm panel including the snap-off tab, qty 5, 1.6 mm, HASL with lead, flying probe test. Stencil 60 x 50 mm, top only, for the sensor tab. PCB assembly off. DFM check was clean.
- JLCPCB assembly was not an option: SHT45-AD1F-R2 is not in the LCSC catalogue and the ENS160 was out of stock.
- Costs: imports into the EU carry VAT, and from 1 July 2026 a flat 3 EUR customs duty per item for small consignments (details on the EMTA site). JLCPCB shipping with customs came to about 28 EUR total.
- Components from Mouser (Farnell Estonia required a KMKR VAT number). Many parts were backordered or out of stock. Always filter on "In Stock" and check the cart for "Backordered" lines.
- Check the real order against the BOM. The Excel BOM in the project was stale in places; the KiCad-exported `manufacturing/BOM.csv` is the reference.

## Assembly and test

- Sensor tab: SHT45 (DFN) and ENS160 (LGA) are leadless. Use stencil + solder paste (Chip Quik TS391AX, Sn63/Pb37, no-clean, T4) and hot air or a hotplate, following the sensors' reflow profiles. Do this last. Do not wash the sensor area: solvents and flux residue can disturb the ENS160 and SHT45.
- Everything else: iron, leaded flux-core solder, gel flux for the TSSOP-20.
- Programming: ST-LINK over SWD. A NUCLEO-F411RE (ST-LINK/V2-1) can program an external board by removing the two CN2 jumpers and wiring CN4. Pinout must be checked in the Nucleo-64 user manual (UM1724). The F411RE is a Cortex-M4, so it is not a development target for the G031 code.
- RS-485 testing: any USB-RS485 adapter (borrowed from work). If there is no reply, swap A and B.

## Firmware notes

- 64 KB flash, 8 KB RAM. Avoid dynamic allocation.
- Slave address from the four DIP switches (SW1-SW4).
- Keep the Modbus parser, CRC and register map independent of hardware so they can be unit-tested on the PC.
- Modbus register map: not designed yet.

## Open items

- Update `docs/skeem.md` (PC14/PC15, connector designators, Y1, U6).
- Verify: RGB LED D3 common cathode / pin order against the Kingbright datasheet; SW2 DIP switch row spacing 7.62 mm; KC5032K footprint pad match; MAX13487 connections (see above).
- Fill in the STM32 port names in the pin table in `CLAUDE.md` using CubeMX.
- Possible rev 2: SGP41 option, X7R / 10 uF for C3.
