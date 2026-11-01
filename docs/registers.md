# Modbus register map

Keskkonnasensor, firmware 0.1. Modbus RTU slave over RS-485.

Source of truth: `firmware/keskkonnasensor/App/regmap.c` (tested in
`tests/test_regmap.c`). If this table and the code disagree, the code wins
and this file needs fixing.

## Conventions

- **Addresses are 0-based, as sent on the wire.** In the old 1-based
  notation, input register 0 is 30001 and holding register 0 is 40001.
  Check which one your master tool expects (Modbus Poll: "PLC addresses
  (Base 1)"; pymodbus: 0-based).
- **16-bit registers, high byte first.**
- **32-bit values use two registers, high word first.** Example: uptime
  0x00012345 s reads as register 13 = 0x0001, register 14 = 0x2345.
- **Scaled values:** "× 100" means the register holds the value times 100.
  23.45 °C is sent as 2345.
- **Signed values** (marked *s16*) are two's complement: −5.20 °C is −520,
  sent as 0xFDF8. Read them as `int16`.
- **No valid value:** a measurement that is not available is sent as
  **0x8000** (signed registers) or **0xFFFF** (unsigned registers), never
  as an error reply. Register 10 (status) says why.
- **Reserved** addresses inside a block read as 0. Reading past the last
  register of a block gives exception 02.

Supported function codes: 03, 04, 06, 16 (0x10). Exceptions: 01 illegal
function, 02 illegal address, 03 illegal value.

## Input registers (read-only, function 04)

| Addr | Old style | Value | Unit / encoding | Source |
|---:|---:|---|---|---|
| 0 | 30001 | Temperature (offset applied) | °C × 100, s16 | SHT45 |
| 1 | 30002 | Relative humidity (offset applied) | %RH × 100, 0…10000 | SHT45 |
| 2 | 30003 | Dew point | °C × 100, s16 | calculated |
| 3 | 30004 | Absolute humidity | g/m³ × 100 | calculated |
| 4 | 30005 | Air quality index AQI-UBA | 1…5 | ENS160 |
| 5 | 30006 | TVOC | ppb, 0…65000 | ENS160 |
| 6 | 30007 | eCO2 | ppm, 400…65000 | ENS160 |
| 7 | 30008 | ENS160 validity | 0 ok, 1 warm-up, 2 initial start-up, 3 invalid | ENS160 |
| 8–9 | | *reserved* | | |
| 10 | 30011 | Status bits | see below | |
| 11 | 30012 | Age of the last good SHT45 reading | s | |
| 12 | 30013 | Age of the last good ENS160 reading | s | |
| 13–14 | 30014–15 | Uptime | s, 32-bit | |
| 15 | 30016 | Modbus requests answered | count, wraps at 65535 | |
| 16 | 30017 | Frames dropped for a bad CRC | count | |
| 17 | 30018 | Failed SHT45 transfers | count | |
| 18 | 30019 | Failed ENS160 transfers | count | |
| 19 | | *reserved* | | |
| 20 | 30021 | Firmware version | major × 256 + minor | |
| 21 | 30022 | Slave address | 1…16, from the DIP switches | |
| 22–23 | 30023–24 | SHT45 serial number | 32-bit | SHT45 |
| 24 | 30025 | ENS160 part ID | 0x0160 | ENS160 |
| 25 | 30026 | ENS160 firmware version | major × 256 + minor | ENS160 |
| 26 | 30027 | ENS160 firmware release | | ENS160 |
| 27–29 | | *reserved* | | |
| 30–33 | 30031–34 | ENS160 raw sensor values R1…R4 | arbitrary log units | ENS160 |

### Status bits (register 10)

| Bit | Meaning |
|---:|---|
| 0 | SHT45 values are valid |
| 1 | ENS160 values are valid |
| 2 | ENS160 warming up (first 3 minutes after power-on) |
| 3 | ENS160 initial start-up (first hour of the sensor's life) |
| 4 | SHT45 does not answer |
| 5 | ENS160 does not answer |
| 6 | ENS160 is getting temperature/humidity compensation from the SHT45 |

The ENS160 times come from its datasheet (v1.3, section 10). During
warm-up and start-up the air quality values are less accurate.

## Holding registers (settings, functions 03 / 06 / 16)

A value outside the range is refused with exception 03 and nothing is
changed. A multi-register write is all or nothing.

| Addr | Old style | Setting | Range | Default |
|---:|---:|---|---|---:|
| 0 | 40001 | Measurement interval | 1…3600 s | 10 |
| 1 | 40002 | Temperature offset | −1000…1000 (°C × 100, s16) | 0 |
| 2 | 40003 | Humidity offset | −1000…1000 (%RH × 100, s16) | 0 |
| 3 | 40004 | LED mode | 0 off, 1 status only, 2 air quality | 2 |
| 4 | 40005 | LED brightness | 0…100 % | 50 |
| 5 | 40006 | eCO2 level for yellow LED | 400…65000 ppm | 1000 |
| 6 | 40007 | eCO2 level for red LED | 400…65000 ppm | 1500 |
| 7 | 40008 | Blue LED flash on each answered request | 0 / 1 | 1 |
| 100 | 40101 | Command register (write only, reads 0) | see below | |

Settings are kept in RAM: they return to the defaults after a reset or
power cut. Saving them in flash is planned.

The eCO2 defaults follow the ENS160 datasheet's rating table: 1000–1500 ppm
"poor, ventilation recommended", above 1500 "bad, ventilation required".

### Commands (holding register 100)

Write one register with function 06. Any other value gives exception 03.

| Value | Command |
|---:|---|
| 1 | One SHT45 heater pulse (removes condensation; T/RH not valid for a moment) |
| 2 | Reset the counters in registers 15–18 to 0 |

## Status

| Part | State |
|---|---|
| Register map, settings, commands | done, PC-tested |
| Uptime, slave address, request counter | done |
| SHT45 values (0–3, 11, 17, 22–23) | step 6 |
| ENS160 values (4–7, 12, 18, 24–26, 30–33) | step 7 |
| CRC error counter (16) | step 4 |
| LED settings (3–7) take effect | step 5 |
| Slave address from the DIP switches | step 5 (until then always 1) |
