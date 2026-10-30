# keskkonnasensor-modbus

Environmental sensor with Modbus RTU over RS-485. Hardware: KiCad board in `kicad/`, docs in `docs/` (see `docs/skeem.md`), JLCPCB outputs in `manufacturing/`, firmware in `firmware/`.

The owner writes mostly Estonian, sometimes English. Reply in the language they use. Code, comments and commit messages in English.

## Hardware (from the KiCad PCB, board rev as ordered Oct 2026)

- MCU: STM32G031F8P6, TSSOP-20 (64 KB flash, 8 KB RAM). Keep code size small.
- Clock: Y1 = Kyocera KC5032Z8.00000C1JX oscillator (likely 8 MHz, confirm) into PC14 (HSE bypass, not a crystal).
- RS-485: MAX13487E with AutoDirection (no DE pin to drive), on USART1.
- Sensors on one I²C bus: SHT45 (temperature / humidity) and ENS160 (TVOC, eCO2, AQI-UBA) on a snap-off sensor tab.
- 4 DIP switch inputs (slave address), RGB LED, SWD header.

U4 (STM32) pin numbers to nets. Ports checked against the CubeMX-generated code (`firmware/keskkonnasensor/Core/Src/`).

| Pin | Net | Port |
|---|---|---|
| 1 | I2C_SDA | PB7, I2C1_SDA, AF6 |
| 2 | OSC (Y1) | PC14, HSE bypass |
| 3 | unconnected | PC15 |
| 4 / 5 | 3V3 / GND | |
| 6 | RST | NRST |
| 7-10 | SW1..SW4 | PA0..PA3, input |
| 11 | unconnected | PA4 |
| 12-14 | LED_R / LED_G / LED_B | PA5 / PA6 / PA7, push-pull output |
| 15 | unconnected | |
| 16 / 17 | USART1_TX / USART1_RX | PA9 / PA10, AF1 (remapped onto PA11/PA12 pads) |
| 18 / 19 | SWDIO / SWCLK | PA13 / PA14 |
| 20 | I2C_CLK | PB6, I2C1_SCL, AF6 |

- Pins 16/17 only work with `SYSCFG_REMAP_PA11` / `SYSCFG_REMAP_PA12` enabled (done in `stm32g0xx_hal_msp.c`). Do not remove the remap in CubeMX.
- Pins 1, 2 and 20 are shared with other pads (PB8, PB9, PB3-PB5). Leave those unconfigured.
- Clock: 8 MHz HSE × 16 ÷ 2 = 64 MHz SYSCLK, flash latency 2.
- Placeholder defaults, not final: USART1 at 115200 8N1, I²C timing `0x10B17DB5` (CubeMX value), LEDs as plain GPIO (`docs/skeem.md` plans TIM3 PWM, which is not configured).
- DIP switch SW2 (in `headers.kicad_sch`) connects PA0-PA3 to GND, and the board has no external pull-ups. PA0-PA3 need internal pull-ups, and a switch turned ON reads 0. The CubeMX config still has them as no-pull and must be changed.
- LED1 is common cathode per `docs/skeem.md`, so a high output should turn the LED on. Not tested on hardware.

## Firmware rules

- Toolchain: STM32CubeMX (standalone) generates the CMake project. Build with CMake presets (Debug / Release). VS Code with ST's STM32 extension.
- Write code only between `USER CODE BEGIN/END` markers in generated files, or put it in separate files. CubeMX regeneration overwrites everything else.
- Keep protocol logic (Modbus RTU parser, CRC16, register map) in hardware-independent files so it can be unit-tested on the PC. Hardware access (I²C, UART) goes behind small interfaces.
- No dynamic memory allocation. Mind the 8 KB RAM.
- Check claims against datasheets. Say what was verified and what was not. Do not guess register addresses or timings.

## Do not, without asking

- Edit KiCad files (`kicad/`), `docs/skeem.md` or `manufacturing/`.
- Flash or erase hardware, or run commands that delete files.
- Commit or push.
