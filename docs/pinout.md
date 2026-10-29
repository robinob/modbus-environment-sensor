# STM32G031F8P6 (TSSOP20) signaalide jaotus

**NB:** MCU pakend muudeti LQFP32 → **TSSOP20** (20 jalga), et pakend oleks
lihtsamini käsitsi joodetav (vt `skeem.md` ptk 2 põhjendust).

**Jalanumbrid allpool on KONTROLLITUD otse ST ametlikust andmelehest**
(`STM32G031F8P6_STMicroelectronics.pdf`, Figure 8 "STM32G031Fx TSSOP20
pinout" + Table 12 "Pin assignment and description") — mitte enam
esialgne funktsionaalne/kontrollimata jaotus.

## KRIITILINE LEID: PA9/PA10 ei ole TSSOP20-l eraldi jalad

Andmelehe Table 12-s on PA9 ja PA10 ridade TSSOP20-veerg **tühi ("-")**.
Nende asemel näitab Figure 8 pinout-joonis:

- **Jalg 16 = "PA11[PA9]"**
- **Jalg 17 = "PA12[PA10]"**

Joonealune märkus (Table 12, märkus 3): *"Pins PA9 and PA10 can be
remapped in place of pins PA11 and PA12 (default mapping), using
SYSCFG_CFGR1 register."*

See tähendab: füüsiline jalg 16 on vaikimisi PA11 (AF: USART1_CTS,
TIM1_CH4, ADC_IN15) ja jalg 17 vaikimisi PA12 (AF: SPI1_MOSI,
USART1_RTS_DE_CK, TIM1_ETR, I2C2_SDA, ADC_IN16). Et neilt jalgadelt
saada **PA9 vastavalt PA10 alternatiivfunktsioone (sh USART1_TX/RX,
mida see projekt vajab)**, tuleb firmware's (STM32CubeMX genereeritud
kood või käsitsi) seada **SYSCFG_CFGR1 registri remap-bitid**
(PA9_PA11_RMP ja PA10_PA12_RMP) enne USART1 initsialiseerimist.
Skeemis/silkskreenil tähista neid jalgu **"PA11[PA9]"** ja
**"PA12[PA10]"**, mitte lihtsalt "PA9"/"PA10".

## Signaalide nimekiri (jalg, funktsioon)

| Jalg | Signaal (silkskreen) | Funktsioon projektis | Märkus |
|---|---|---|---|
| 4 | VDD/VDDA | 3.3V toide | bypass C1/C2 lähedal |
| 5 | VSS/VSSA | maandus | |
| 6 | PF2-NRST | Reset | pull-up sisemine, C7 (100nF, valikuline) väliselt |
| 19 | PA14-BOOT0 | Bootloader valik / SWCLK | pull-down R10 normaalrežiimiks; SWD jaoks kasutatakse sama jalga SWCLK-na (J2) |
| **16** | **PA11[PA9]** | USART1_TX (REMAP vajalik!) → U2 (MAX13487) DI | vt ptk "Kriitiline leid" ülal |
| **17** | **PA12[PA10]** | USART1_RX (REMAP vajalik!) → R11/R12 pingejaotur → U2 RO | jaotur kaitseb 3.3V sisendit U2 5V-loogika eest, vt `skeem.md` ptk 4 |
| 20 | PB6 | I2C1_SCL → J3 (anduriplokk) | pull-up R5/R7 4.7k; otse olemas, remap'i vaja EI ole |
| 1 | PB7 | I2C1_SDA → J3 (anduriplokk) | pull-up R6/R8 4.7k, sama märkus |
| 18 | PA13 | SWDIO | J2 programmeerimispesa, ST-Link |
| 19 | PA14-BOOT0 | SWCLK | jagab jala BOOT0-ga (vt ülal), tavapärane STM32 komboots |
| 12 | PA5 | TIM3_CH1 (PWM) | LED1 punane (R15) |
| 13 | PA6 | TIM3_CH2 (PWM) | LED1 roheline (R16) |
| 14 | PA7 | TIM3_CH3 (PWM) | LED1 sinine (R17) |
| 7 | PA0 | GPIO sisend | Modbus aadress bit 0, SW1, pull-up |
| 8 | PA1 | GPIO sisend | Modbus aadress bit 1, SW1, pull-up |
| 9 | PA2 | GPIO sisend | Modbus aadress bit 2, SW1, pull-up |
| 10 | PA3 | GPIO sisend | Modbus aadress bit 3, SW1, pull-up |
| 2 | PB9/PC14-OSC32_IN | kasutamata | sisemine RC, kristalli pole |
| 3 | PC15-OSC32_OUT | kasutamata | sisemine RC, kristalli pole |
| 11 | PA4 | kasutamata (varu) | |
| 15 | PB0/PB1/PB2/PA8 | kasutamata (varu) | |

**16 jalga 20-st kasutusel**, 4 vaba (2, 3, 11, 15) — VDD/VSS/NRST/BOOT0
arvestamata on kasutuses ka need. MAX13487 (U2) RE ja SHDN pinnid on
püsivalt kõvasti juhitud (RE→GND, SHDN→5V, vt `skeem.md` ptk 4) — need
ei vaja MCU GPIO-d.

## Kui soovid rohkem vaba I/O-d (soovituslik lihtsustus)

Kuna TSSOP20 pinnieelarve on tihe, kaalu ühte (või mõlemat) järgmistest,
kui tulevikus tahad lisada mõne sisendi/väljundi ilma plaati ümber
disainimata:

- **RGB LED → üksik 2-värviline või 1-värviline LED** (3 PWM pinni asemel
  1) — säästab 2 pinni. Olekuinfo (ok/viga/init) on ka ühe LED-i
  vilkumismustriga edastatav.
- **4-bit Modbus aadress → 2-bit** (max 4 seadet bussil, mitte 16) —
  säästab 2 pinni. Kui projektis on plaanis ainult üks-kaks seadet ühes
  majapidamises, on 4 seadet piisav varu.

Mõlemat rakendades jääks vabaks ~4 pinni tuleviku laienduseks (nt lisa-
andur või teine digitaalne sisend/väljund).

## Kokkuvõte perifeeriast

- **USART1**: Modbus RTU side RS-485 transiiveri (MAX13487, autodirection)
  kaudu — ei kasuta TX/RX kõrval lisa-GPIO-sid, kuna suunavahetus on
  riistvaraline. TX/RX on füüsiliselt jalgadel 16/17 ("PA11[PA9]" /
  "PA12[PA10]") ja vajavad SYSCFG_CFGR1 remap't (vt ülal). U2 saab 5V
  otse U5 buck-muunduri väljundilt (vt `skeem.md` ptk 1) ja RO signaal
  läbib enne jalga 17 jõudmist R11/R12 kaitsejaoturi (vt `skeem.md`
  ptk 4).
- **I2C1**: jagatud buss kaugpaigaldatud anduriplokini (J3 kaabel) —
  SHT45 (temp/niiskus) ja ENS160 (eCO2/TVOC), mõlemad füüsiliselt eemal
  põhiplaadist, korpuse ventileeritud osas.
- **TIM3 PWM kanalid**: RGB olekuindikaator (vt lihtsustusvõimalust ülal).
- **4x GPIO sisend**: Modbus slave aadressi seadistamine DIP-lülitiga
  (loetakse boot ajal, aadress = 1 + binaarväärtus, vahemik 1-16).
- **SWD**: programmeerimine/silumine ST-Link kaudu.
