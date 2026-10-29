# Skeemikirjeldus — Keskkonnaandur (Modbus RTU slave)

Funktsionaalsete blokkide tasemel kirjeldus, mida kasutada trükkplaadi
skemaatika (KiCad vms) koostamisel. Viited komponentidele vastavalt
`BOM.csv` failile.

## 1. Toiteahel

Sisend: bussikaablis kaasatulev alalisvool, tüüpiliselt **12–24V DC**
(vastavalt masterkontrolleri/paneeli toitele).

**ARHITEKTUUR LIHTSUSTATUD (kahest kõrgepinge-regulaatorist üheks):**
varasemalt oli plaadil kolm toitekomponenti — buck (24V→3.3V), pluss
eraldi 5V LDO ainult U2 jaoks (24V→5V), pluss "varu" 3.3V LDO (U7) —
mis oli segadusttekitav ja millel oli ka reaalne viga: U7 (AMS1117)
polnuks kunagi ohutult otse 24V bussilt toidetav (AMS1117 talub
sisendina vaid ~15V). **Uus lahendus: üksainus buck teeb raske töö
(kõrge sisendpinge → 5V), ja odav/lihtne LDO teeb kerge töö (5V → 3.3V):**

```
Bussikaabli toitejuhe (12-24V, kuni 30V) --> F1 (PTC kaitse, vt allpool) -->
   D1 (SS34, polaarsuskaitse) -->
   C3 (10uF bulk) --> U5 (MCP16301, SOT-23-6 buck-muundur) --> 5.0V liin (PEAMINE)
                       |  |                                  |    +--> C4 (10uF, sisend @ VIN)
                       |  |                                  |    +--> C5 (22uF, väljund @ VOUT)
                       |  |                                  |
                       |  |                                  +--> U2 (MAX13487) VCC otse (+ C6 bypass)
                       |  |                                  |
                       |  |                                  +--> C8 (1uF, sisend) --> U7 (TLV70433,
                       |  |                                       3.3V fikseeritud LDO) --> 3.3V liin
                       |  |                                       (MCU + andurid) + C9 (1uF, väljund)
                       |  |
                       |  +-- SW jalg (pin6) --> L1 (22uH) --> 5.0V liin (VOUT)
                       |  |                  --> D2 (SS34 Schottky, katood SW poole,
                       |  |                       anood GND) -- vabajooksu dioot
                       |  +-- BOOST jalg (pin1) <-- C10 (100nF) --> SW jalg (pin6)
                       |  +-- BOOST jalg (pin1) <-- D3 (1N4148, katood BOOST poole,
                       |                            anood 5.0V liini/VOUT poole) --
                       |                            boost-blokeeriv dioot (vt allpool)
                       |
                       +-- VIN jalg (pin5) -- EN jalg (pin4) otse kokku (alati "sees")
                       +-- VFB jalg (pin3) <-- R13 (52.3k, VOUT poolt) / R14 (10k, GND poolt)
                       +-- GND jalg (pin2) --> maandus
```

**NB — see on MITTE-sünkroonne buck, mitte sünkroonne** (varasem
kirjeldus selles dokumendis oli ebatäpne). MCP16301 andmelehe järgi
(ptk 3.0 "Pin Descriptions" ja 5.7 "Freewheeling Diode") on SW jalale
**kohustuslik väline Schottky vabajooksu-dioot (D2)** ja BOOST jala
juurde **kohustuslik bootstrap-kondensaator (C10, 100nF, ≥6.3V)** —
ilma nendeta buck ei tööta üldse, sõltumata FB-jaoturi õigsusest.
Varasemas BOM-i versioonis L1, D2 ja C10 üldse puudusid ning U5 pakend
oli valesti märgitud (SOT-23-5, tegelik on **SOT-23-6**: 1=BOOST,
2=GND, 3=VFB, 4=EN, 5=VIN, 6=SW) — see on nüüd parandatud nii BOM-is
kui ülal olevas ahelas. L1=22uH tuleb otse MCP16301 andmelehe Table
5-1-st (VOUT=5.0V → K=0.23 → 22uH) ja täpselt kattub andmelehe enda
"Typical Application" näitega 5.0V@600mA väljundi jaoks.

**★ PARANDUS — D3 (1N4148, boost-blokeeriv dioot) puudus täielikult.**
See viga avastati alles kasutaja enda KiCad-skeemi (kus vastav dioot
juba korrektselt olemas oli, tähistatud "D4") võrdlemisel MCP16301
andmelehe välis­komponentide nõuetega. Andmelehe ptk 4.1.3 ("External
Components") loetleb bootstrap-kondensaatori (C10) **kõrval eraldi ka
"boost blocking diode"** — komponendi, mida see dokument varem üldse ei
maininud. Ilma C10-ta ei tooda buck väljundpinget üldse (see osa oli
juba varem parandatud); D3 puudumisel C10 küll laeb end esimesel
lülitustsüklil, aga ei saa hiljem korralikult uuesti laadida läbi SW
solme (mis liigub pidevalt 0V ja VOUT vahel) — praktikas võib see
väljenduda ebastabiilse või liiga madala väljundpingena kõrgema
koormuse juures.

**Polaarsus on kriitiline:** anood peab tulema **väljundist (5.0V
liin/VOUT), mitte VIN-ist**. Andmelehe ptk 4.2.5 ("Alternative Boost
Bias"): *"for 3.0V to 5.0V output voltage applications, the boost
supply is typically the output voltage"*. Kui kasutada selle asemel
VIN-i (meie skeemis kuni 30V), rikuks see BOOST-SW jala absoluutse
maksimumpinge piiri (**−0.5V…+6.0V**, andmeleht ptk 1.0) ja võiks kiibi
püsivalt rikkuda. Andmelehe enda lk 3 "Typical Applications" 5.0V@600mA
näiteskeem kasutab täpselt sama osa — 1N4148 — täpselt selle
ühendusega, mis kinnitab valikut.

**Buck-muunduri väljundpinge jaoturi arvutus (MCP16301, FB = 0.8V):**

Vout = 0.8V x (1 + R_top/R_bottom)

Kui valida R_bottom = 10 kohm ja soovitud Vout = 5.0V:

R_top = R_bottom x (Vout/0.8 - 1) = 10k x (5.0/0.8 - 1) = 10k x 5.25 = 52.5 kohm

Praktikas kasutada standardväärtust **R13 = 52.3 kohm** (E96 seeria, ülemine, VOUT poolt) koos **R14 = 10 kohm** (alumine, GND poolt),
mis annab Vout = 0.8 x (1 + 52.3/10) = 4.984V — sobib nii U2 (MAX13487
VCC = 5V ±5%, s.o. 4.75–5.25V) kui ka U7 (TLV70433) sisendiks.

Miks buck (mitte lihtne LDO) just siin: sisendpinge 12-30V ja LDO korral
läheks (Vin-Vout)×Iload kaotsi soojusena — buck on efektiivsem ja väldib
jahutusprobleeme väikeses suletud karbis. See kehtib just 24V→5V astme
kohta, kus pingevahe on suur.

**Teine aste, U7 (TLV70433DBVR): 5.0V → 3.3V, toidab MCU-d ja andureid.**
Kuna sisend on nüüd ainult 5V (mitte 24-30V), on siin pingevahe väike
(~1,7V) — lihtne LDO piisab, kaotsiminev soojus on tühine ka pideva
koormuse korral. TLV70433 on sama TLV704 pere mis eelmises versioonis
kasutatud U8 (TLV70450) — **ceraamiliste kondensaatoritega stabiilne**,
ei vaja tantaal-väljundkondensaatorit (erinevalt AMS1117 klassikalisest
versioonist, mis eelmises BOM-i versioonis U7 kohal oli ja mille
minimaalse-ESR/tantaali nõue oleks sama probleem mis LM2937-l, vt
varasem LDO valiku arutelu). Ceraamilised C8/C9 (≥0.47uF datasheet
nõue, kasutame 1uF) piisavad.

**U2 (MAX13487) VCC tuleb nüüd otse U5 5V liinilt** (mitte enam eraldi
U8-lt) — eraldi kõrgepinge-taluvat 5V-LDO-t (TLV70450/TPS7B6950) **ei
ole enam vaja**, sest buck ise juba toodab 5V otse 24-30V sisendist.
Kuna U2 tõmbab voolu peamiselt bussi ajamise pursete ajal (Modbus RTU
on madala mahulaadungiga protokoll), ei koorma see 5V liini oluliselt.

## 2. Mikrokontroller (U1, STM32G031F8P6, TSSOP20)

- **Pakendi valik:** LQFP32 (K8T6) asemel valiti **TSSOP20 (F8P6)** — sama
  silikoon, sama 64KB flash ja perifeeriakomplekt (I2C, USART, PWM-taimerid),
  aga 20 jalga 32 asemel. TSSOP on gull-wing jalgadega (nähtaval kahel
  küljel, mitte kiibi alla peidetud nagu QFN) — käsitsi joodetav flux'i ja
  peenikese jooteotsikuga, hoolimata tihedamast 0,65mm sammust. **Väldi**
  sama pere QFN-varianti (nt UFQFPN28) — selle kontaktid on kiibi all
  nähtamatud ja see ei ole reaalselt käsitsi joodetav.
- VDD/VDDA jalad: bypass kondensaatorid C1, C2 (100nF) võimalikult lähedal
  jalgadele.
- NRST: pull-up sisemiselt, lisada väline 100nF kondensaator müra
  summutamiseks (valikuline).
- BOOT0: tõmmata GND külge üle takisti (normaalne töörežiim; jumper
  programmeerimisrežiimi jaoks, kui vaja UART bootloaderit).
- **TSSOP20 jalanumbrid on nüüd kontrollitud otse andmelehelt
  (`STM32G031F8P6_STMicroelectronics.pdf`, Figure 8 + Table 12) ja on
  fikseeritud allpool ning `pinout.md`-s.**
- **OLULINE LEID — PA9/PA10 remap:** TSSOP20 pakendil **EI OLE PA9 ega
  PA10 eraldi füüsilist jalga**. Andmelehe joonisel (Figure 8) on jalg
  16 märgitud **"PA11[PA9]"** ja jalg 17 **"PA12[PA10]"**, ning
  joonealune märkus 3 ütleb otsesõnu: *"Pins PA9 and PA10 can be
  remapped in place of pins PA11 and PA12 (default mapping), using
  SYSCFG_CFGR1 register."* See tähendab:
  - **Jalg 16** (silkskreenil/skeemis "PA11[PA9]") kannab USART1_TX
    (PA9) funktsiooni ainult siis, kui tarkvaras on **SYSCFG_CFGR1
    remap-bit** sisse lülitatud selle jala jaoks (vaikimisi on jalg
    PA11).
  - **Jalg 17** (silkskreenil/skeemis "PA12[PA10]") kannab USART1_RX
    (PA10) funktsiooni samamoodi ainult remap-bitiga.
  - Kuna meie disain kasutab USART1_TX/RX otse U2 (MAX13487) DI/RO
    jaoks (vt ptk 4), **peab firmware STM32CubeMX/HAL-is aktiveerima
    selle remap'i** (SYSCFG->CFGR1, PA9_PA11_RMP ja PA10_PA12_RMP
    bitid) — muidu USART1 TX/RX signaalid nendele jalgadele ei jõua.
  - **Skeemis/silkskreenil tuleb neid jalgu tähistada kui
    "PA11[PA9]" (jalg 16) ja "PA12[PA10]" (jalg 17)**, mitte lihtsalt
    "PA9"/"PA10" — need nimed viitavad füüsiliselt olemasolevatele
    jalgadele, mille vaikefunktsioon on PA11/PA12 ja remap-funktsioon
    PA9/PA10.
  - See ei ole tarkvara-viga ega meie disainiviga — see on ST enda
    pin-reduction skeem väikeste pakendite (TSSOP20, WLCSP18, SO8N)
    jaoks, dokumenteeritud ja tavapärane. Sama mehhanism kehtib ka
    I2C1 SDA/SCL kohta suuremates pakendites, aga PB6/PB7 (mida meie
    kasutame I2C jaoks) on TSSOP20-l olemas otse, eraldi jalgadena
    (jalg 20 ja jalg 1) — nende jaoks remap'i vaja EI ole.
- Täielik TSSOP20 jalajaotus meie kasutatud signaalide jaoks (kõik
  kontrollitud otse andmelehe Table 12-st):

  | Jalg | Silkskreen/skeem | Funktsioon projektis |
  |---|---|---|
  | 4 | VDD/VDDA | toide 3.3V |
  | 5 | VSS/VSSA | maandus |
  | 6 | PF2-NRST | Reset |
  | 7 | PA0 | SW1 aadress bit0 |
  | 8 | PA1 | SW1 aadress bit1 |
  | 9 | PA2 | SW1 aadress bit2 |
  | 10 | PA3 | SW1 aadress bit3 |
  | 12 | PA5 | LED1 PWM (punane, TIM3_CH1) |
  | 13 | PA6 | LED1 PWM (roheline, TIM3_CH2) |
  | 14 | PA7 | LED1 PWM (sinine, TIM3_CH3) |
  | 16 | **PA11[PA9]** | USART1_TX (remap!) → U2 DI |
  | 17 | **PA12[PA10]** | USART1_RX (remap!) → R11/R12 → U2 RO |
  | 18 | PA13 | SWDIO (J2) |
  | 19 | PA14-BOOT0 | SWCLK (J2) / BOOT0 (R10 pull-down) |
  | 20 | PB6 | I2C1_SCL → J3 |
  | 1 | PB7 | I2C1_SDA → J3 |
  | 2, 3 | PC14-OSC32_IN, PC15-OSC32_OUT | kasutamata (sisemine RC) |
  | 11, 15 | PA4, PB0/PB1/PB2/PA8 | kasutamata (varu) |

## 3. Andurid (I2C1 buss, KAUGPAIGALDATUD anduriplokk)

**Miks kaugpaigaldus:** kui temp/niiskuse andur istub samal plaadil
MCU/buck-muunduriga, mõõdab ta elektroonika lokaalset (soojemat, kuivemat)
mikrokliimat, mitte tegelikku toa õhku — see teeb küttereguleerimise
sisendi ebatäpseks. Seetõttu **ei ole SHT45/ENS160 põhiplaadil**, vaid
moodustavad eraldi väikese **anduriploki**, mis paigaldatakse korpuse
ventileeritud ossa, füüsiliselt eemal MCU/buck-muunduri soojusallikatest.
Anduriplokk ühendub põhiplaadiga läbi **kahe** identse pistikupesa —
**J3 põhiplaadil** (MCU I2C1 juures) ja **J4 anduriploki plaadil** —
ning nende vahele jääva lühikese (~10-30cm) 4-soonelise I2C kaabli
(VCC/GND/SDA/SCL), mille mõlemas otsas on push-in pistik (vt allpool).
*(Varem kandsid mõlemad pistikupesad eksitavalt sama viidet "J3" — vt
skeemid.html paneel 4 fixBox — nüüd on need eristatud: J3 = põhiplaat,
J4 = anduriplokk.)*

**Käsitsi jootmine kuumaõhuga (ei ole reflow-ahju, ainult kuumaõhk +
jootekolb):** nii SHT45 (DFN-4) kui ENS160 (LGA-9) on **jalgadeta
("leadless") pakendid** — kontaktid on ainult kiibi ALL, mitte küljel
nähtavad. See ei tähenda, et kuumaõhuga jootmine ei toimi (see on
tavapärane ja levinud viis hobiseppadel neid pakendeid käsitsi jootmiseks),
aga kontrolli-loogika on teine:
- **Kasuta joodepastat pastale, mitte jooteotsiku tinutamist** — jalgadeta
  pinnale ei saa "vedada" nagu SOIC/SOT jalgadele. Väikese (üheksa jala,
  peene sammuga) kiibi jaoks on odav laserlõigatud teraspasta-šabloon
  (~10-20€ nt JLCPCB/OSH Stencils) oluliselt usaldusväärsem kui pasta
  käsitsi süstlaga peale kandmine.
- Aseta kiip pintsettidega suurendusklaasi all, rohke no-clean flux,
  seejärel kuumaõhk ülevalt, otsik suunatud vaid selle kiibi peale
  (mitte naaberkomponente kuumutamata). Korralik jootmine on näha
  kiibi "vajumisest"/isejoondumisest reflow hetkel — see on ainus
  visuaalne vihje, sest jooteliited ise jäävad kiibi alla nähtamatuks.
- **ENS160 andmelehe enda reflow-profiil** (ptk 18 "Soldering
  information"): tippemperatuur **260°C**, no-clean pasta, ja
  **EI TOHI plaati pärast jootmist vedelikuga pesta** — see on
  otsene nõue, mitte soovitus, sest pesuvedelik võib kahjustada
  avatud gaasianduri tundlikku ala.
- **Järjekord:** joota ENS160 ja SHT45 (mõlemad jalgadeta) kuumaõhuga
  KÕIGEPEALT, alles seejärel **J4** (anduriploki pistikupesa, THT)
  jootekolviga — nii ei kuumuta sa juba tehtud nähtamatuid liiteid
  uuesti üle.
- **Kontroll on elektriline, mitte visuaalne:** multimeetriga
  läbihelistus (VDD/VDDIO/GND/SDA/SCL) või lihtsalt plaadi toite alla
  panek ja I2C bussi kontroll aadressidel 0x44 (SHT45) ja 0x52/0x53
  (ENS160). ENS160 INTn (jalg 6) ei ole meie disainis kasutuses —
  seda ei pea kontrollima.

**Bare kiibid, mitte valmismoodulid:** kuna kinnitasid, et sul on ligipääs
jootejaamale (kolb + kuumaõhk) ja/või JLCPCB SMT-assamblee teenusele,
minnakse tagasi **bare kiipide** juurde — **SHT45-AD1F (DFN-8, PTFE
membraaniga)** ja **ENS160-BGLM (LGA-10)**. See lihtsustab BOM-i (üks
Farnelli tellimus valmismoodulite asemel), on odavam ja vastab üheselt
"trükkplaadi valmistamine" nõudele. Mõlemad kiibid joodetakse **anduriploki
plaadile** — väikesele eraldi trükkplaadile (mitte põhiplaadile), mis
tellitakse koos põhiplaadiga samas PCB-tehase tellimuses. Kokkupanekul:
joota need kaks kiipi kuumaõhuga ise, VÕI telli PCB-tehaselt (nt JLCPCB)
osaline SMT-assamblee ainult nende kahe raske komponendi jaoks — ülejäänud
komponendid (MCU, konnektorid, passiivid) joodad ise kolviga.

**Miks eraldi plaat, mitte põhiplaadil:** vt eespool — enda-soojenemise
vältimine kehtib sõltumata sellest, kas kiibid on valmismoodulil või bare
kujul. Anduriploki plaat paigaldub korpuse ventileeritud ossa, ühendatuna
põhiplaadiga J3↔J4 kaabli kaudu.

**J3/J4 osanumbrid (PARANDATUD, vt ptk 4 lõppu täieliku selgituse jaoks):**
J3 (põhiplaat) ja J4 (anduriplokk) on täpselt sama füüsiline osa, ainult
eri asukohas — seega üks osanumbrite komplekt kehtib mõlemale:
lõplikult valitud **Würth Elektronik WR-TBL, seeria 382/381** (2,5mm
samm) — sama seeria mis J1 (vt ptk 4), Farnellis otseselt laos,
kontrollitud tootja enda andmelehtedelt ja Farnelli tootelehelt (mitte
otsingu-väljavõtetest):

- **Pistikupesa (PCB-le joodetav, PÄRIS pluggable header, 90°/horisontaalne):**
  Würth Elektronik **691382010004** (WR-TBL 382, 4-pos, 2,5mm, 12A/300V) —
  Farnelli tellimisnr **3767027**, laos 800 tk (kontrollitud)
- **Pistik (kaabli otsa, push-in, mitte kruvi):** Würth Elektronik
  **691381000004** (WR-TBL 381, 4-pos, push-in) —
  Farnelli tellimisnr **3767009**, laos 1541 tk (kontrollitud)

Vaja on 2x pistikupesa (**J3** põhiplaadil, **J4** anduriploki plaadil)
+ 2x pistik (J3-P, kaabli mõlemas otsas). Mõlemad on otse Farnellis
laos, ei ole vaja teist turustajat kasutada.

```
Põhiplaat:                              Anduriploki plaat (eraldi PCB):
STM32 PB6 (SCL) ---+--- R5 (4.7k) ---+                        |
                    |                 |  J3 ↔ J4 kaabel (~10-30cm)  |
STM32 PB7 (SDA) ---+--- R7 (4.7k) ---+---------------------- + --- SHT45 SCL/SDA
                                                               +--- ENS160 SCL/SDA
                                          (jagatud I2C buss anduriploki plaadil)
```

- **PARANDATUD PAKENDID (kontrollitud andmelehtedelt):** SHT45 (U3) on
  **DFN-4** (mitte DFN-8): 1=SDA, 2=SCL, 3=VDD, 4=VSS. ENS160 (U4) on
  **LGA-9** (mitte LGA-10), 3x3mm: 1=MOSI/SDA, 2=SCLK/SCL, 3=MISO/ADDR,
  4=VDD, 5=VDDIO, 6=INTn, 7=CSn, 8+9=VSS.
- **KRIITILINE — ENS160 CSn:** andmelehe joonis 14 ("Recommended
  application circuit, I2C operation") nõuab, et **CSn (jalg 7) on
  otse (mitte takisti kaudu) joodetud VDDIO külge** — kui CSn jääb
  ujuvaks või on GND-s, valib kiip vaikimisi **SPI-liidese**, mitte
  I2C-d, ja kiip ei vasta bussil üldse. See EI OLE hetkel üheski
  dokumendis kirjas olnud ja tuleb anduriploki skeemi kindlasti lisada.
- SHT45 I2C aadress: 0x44 (fikseeritud).
- ENS160 I2C aadress: **MISO/ADDR (jalg 3) otse GND külge = 0x52**,
  või otse VDDIO külge = 0x53 — sama nõue (otse, mitte takisti kaudu)
  kehtib ka siin. Konflikti SHT45 aadressiga (0x44) pole.
- ENS160 vajab oma kohalikke bypass-kondensaatoreid **100nF (VDDIO) ja
  10µF (VDD)** jalgade 4/5 juures (andmelehe joonis 14) — lisa
  anduriploki plaadi komponendinimekirja, need pole hetkel BOM-is
  eraldi real (U3/U4 read katavad praegu vaid kiipide endi hinna).
- Pull-up takistid R5-R8 (4.7 kohm) asuvad **põhiplaadil** ja on nüüd
  **kohustuslikud** (mitte valikulised) — bare kiipidel (erinevalt
  valmismoodulitest) ei ole omaenda pull-up'e. Kaabli pikkuse (~10-30cm)
  tõttu kaalu vajadusel madalamat pull-up väärtust (nt 2.2 kohm) suurema
  bussikapatsitiivsuse kompenseerimiseks.
- I2C kiirus: 100kHz (standard mode). Kaabli pikkusega (mitte enam
  ainult plaadisisesed lühikesed rajad) on see piir, mille juures
  standardsed pull-up väärtused veel usaldusväärselt töötavad —
  pikema kaabli korral (>30cm) kaaluda I2C puhvrit/extenderit.
- **Andurite koostöö:** ENS160 vajab täpsemaks eCO2/TVOC arvutuseks
  temperatuuri ja niiskuse kompensatsiooni. Tarkvaraloogika loeb esmalt
  SHT45 väärtused ja kirjutab need ENS160 vastavatesse
  kompensatsiooniregistritesse (TEMP_IN 0x13, RH_IN 0x15) enne ENS160
  mõõtetulemuse lugemist — sensorid ei tööta üksteisest sõltumatult,
  ja mõlemad peavad seetõttu füüsiliselt samas (toaõhu käes olevas)
  asukohas olema.
- Valgustugevuse andurit (nt BH1750) ei ole seadmes vaja: küttereguleerimise
  jaoks on valgustase nõrk ja kaudne signaal (päikesekoormuse/kohaloleku
  kaudne vihje), samas kui õhukvaliteet (eCO2) on otsene ventilatsiooni- ja
  kohaloleku-signaal, mis on küttesüsteemi (eriti soojatagastusega
  ventilatsiooni) reguleerimisel oluliselt asjakohasem.

## 4. RS-485 / Modbus RTU liides (U2, MAX13487) — daisy-chain topoloogia

**Üks 8-positsiooniline konnektor, mitte kaks:** iga seade bussil peab
olema võimalik ühendada ahelasse ("daisy chain") — vool/side siseneb
ühest kaablist ja jätkub teise kaablisse järgmisesse seadmesse, ilma
eraldi hargnemiskarbita. Selleks kasutatakse **ühte J1 konnektorit**,
kus iga neti jaoks on **kaks kõrvutist pinni** (üks sissetuleva, üks
väljamineva kaabli jaoks) — tüüpiline lahendus tööstuslike RS-485
loop-through klemmide juures:

```
Pin 1: V+ (sisse)     Pin 2: V+ (välja)
Pin 3: GND (sisse)    Pin 4: GND (välja)
Pin 5: A (sisse)      Pin 6: A (välja)
Pin 7: B (sisse)      Pin 8: B (välja)
```

Kõik "sisse" ja "välja" pinnid on **PCB-l rööbiti ühendatud samadele
netidele** (V+-V+, GND-GND, A-A, B-B) — kõrvutine paigutus vähendab
juhtmestamisvigu (õige klemmi leidmine kaabli otsale on visuaalselt
lihtsam kui plokk-jaotusega 1-4/5-8).

- **Pistikupesa (PCB-le joodetav, PÄRIS pluggable header, 90°/horisontaalne):**
  Würth Elektronik **691382010008** (WR-TBL seeria 382, 2,5mm samm,
  8-pos, 12A/300V, Through Hole Right Angle) — Farnelli tellimisnr
  **3767031**, laos 396 tk (kontrollitud otse Farnelli tootelehelt)
- **Pistik (juhtmete pool, kaabel pushitakse sisse):** Würth Elektronik
  **691381000008** (WR-TBL seeria 381, 2,5mm samm, 8-pos, push-in/
  screwless) — Farnelli tellimisnr **3767014**, laos 381 tk

**LÕPLIK OTSUS (varasem Phoenix Contact ja "Würth ei sobi" eksitus
parandatud):** Phoenix Contact PTSM 0,5/8-HH-2,5-THR R44 (1778683) ja
selle J3 vaste jäid Farnellis laost otsa — see oli reaalne probleem.
Selle peale otsisin esialgu Würthi *asemele* muid tootjaid (WAGO 734
seeria oli hea varuvariant, vt allpool), aga tegelik probleem ei olnud
Würth ise, vaid see, et algne uurimine ei leidnud Würthi 90°-kraadist
(horisontaalset) varianti — **Würth WR-TBL 382/381 seeriatel on 90°
variandid täiesti olemas**, kuni 8 (isegi 12) positsioonini, 2,5mm
sammuga, täpselt samas mõõdus mis algne Phoenix PTSM 0,5-seeria.
Otsest tootja andmelehte (we-online.com) ja Farnelli tootelehte
(mitte ainult otsingutulemuste pealkirju) kontrollides:
- Header perekond **691382010XXX** = "WR-TBL 382, 2.50mm Horizontal
  PCB Header" (X = positsioonide arv: 2,3,4,5,6,7,8,12) — päris
  pluggable header, MALE pinnid, THT jootmiseks.
- Plug perekond **691381000XXX** = "WR-TBL 381, 2.50mm Vertical CAB
  Entry Plug Screwless" — see on kaabli-poolne FEMALE pistik, push-in
  (screwless) klambritega; WE tootelehel on see otseselt "compatible
  mating part" mõlemale header-perekonnale (vertikaalne 691382000XXX
  ja horisontaalne 691382010XXX).

**Male/female:** samad põhimõtted kehtivad nagu varem: **pistikupesa
(header) on PCB küljes, MALE pinnid**, **pistik on FEMALE**, push-in
klambritega juhtmete jaoks, pistetakse headerisse.

**Tellimine:** mõlemad (header + pistik, nii J1 kui J3) on otse
Farnellis laos praegu (koguste kohta vt ülal) — ei ole vaja teise
turustaja juurde minna. Push-in klambrid lubavad juhet tühja pessa
lisada ilma juba paigaldatud juhtmeid häirimata — ainult terve plugi
PCB-lt eemaldamine (nt seadme asendamisel) lahutab mõlemad kaablid
korraga, mis on järkjärgulisel paigaldamisel harva probleem.

**J3 ja J4 kasutavad nüüd sama Würth WR-TBL 382/381 perekonda** (vt
ptk 3 parandatud osanumbreid) — kogu plaadil (mõlemal PCB-l) taas üks
konnektoriperekond, sama 2,5mm samm mis algselt Phoenix PTSM-iga
plaanitud, seega footprint'i mõõtudes muudatust pole.

**Varuvariant (kui Würthi laoseis peaks muutuma):** WAGO 734 seeria
("MCS MINI", 3,5mm samm) — header 734-168 (8-pos, 90°, Farnell
2667104, oli kontrollimise ajal 102 tk laos) + pistik 734-108 (8-pos,
kruviga) J1-le; header 734-164 (4-pos, 90°, Farnell 2667101, 163 tk)
+ pistik 734-104 J3-le. Erinev samm (3,5mm vs 2,5mm) tähendaks
footprint'i muutust, kui seda kunagi vaja läheb.

```
J1 (sisse: pin 1,3,5,7; välja: pin 2,4,6,8) -- [rööbiti PCB-l] --
                                    |
                                    +--> U6 (SM712, SOT-23) --> U2 (MAX13487) A/B --> STM32 USART1
                                    +--> F1 --> D1 --> C3 --> U5 (buck) --> 5.0V liin --+--> U2 VCC
                                                                                        +--> U7 (TLV70433) --> 3.3V liin (MCU+andurid)
```

**U6 osa parandus #1 (pakend):** varem BOM-is oli pakend märgitud
"SOD-323" kogusega 2 (üks liin per kiip) — vale. Õige pakend on
**SOT-23/SOT-323, 3 jalga**, ja **üks kiip kaitseb mõlemat liini (A ja
B) korraga** — vaja on ainult 1 tk, mitte 2.

**U6 osa parandus #2 (VALE OSA — leitud sinu enda küsimuse peale):**
BOM-is olnud **PESD1CAN-U (Nexperia)** on andmelehe enda järgi
(ptk 7 "Application information") **konkreetselt CAN-bussi jaoks
mõeldud** — "protection of two automotive CAN bus lines" — ja selle
standby-pinge (VRWM) on **sümmeetriline 24V mõlemas suunas**. RS-485
(EIA-485) standard lubab aga **asümmeetrilist** ühisrežiimi vahemikku
**-7V kuni +12V**, ja TI oma rakendusnoot TVS-dioodi valikuks
RS-232/RS-485/CAN jaoks ütleb otsesõnu: ära kasuta CAN-jaoks mõeldud
dioodi RS-485 peal, vali transiiveri vahemikuga sobitatud asümmeetriline
osa. PESD1CAN-U poleks olnud ohtlik (24V > 12V, ei rakendu normaaltöös
valesti), aga see on üledimensioneeritud/vale tööriist — ei anna
RS-485-le optimaalset lähedast kaitset.

**Uus osa: Semtech SM712** — sama SOT-23, 3-jalga jalajälg (pin1/pin2 =
liinid, pin3 = ühine GND, sama pin-roll), aga sisaldab **2× 12V + 2× 7V
dioodi**, mis annab täpselt RS-485 vajaliku **-7V/+12V asümmeetrilise**
piirangu. VBR: 13.3V (positiivne suund) / 7.5V (negatiivne suund) @1mA.

SM712 pinout (U6, 3 jalga):

```
1 (12V/7V dioodid)  --- A liin (J1 pin 5+6 / U2 pin 6)
2 (12V/7V dioodid)  --- B liin (J1 pin 7+8 / U2 pin 7)
3 ühine (GND)        --- GND
```

Paiguta U6 J1 konnektori ja U2 vahele, võimalikult lähedal J1-le, et
kaitsta liini juba enne selle plaadile sisenemist.

MAX13487 8-pin SO pinout (U2):

```
1 RO   --[R11 3.3k]--+--> STM32 U1 jalg 17 ("PA12[PA10]", USART1_RX remap)   (pingejagaja, vt allpool)
                      +--[R12 4.7k]--> GND
2 RE   --> U5 5V liin (püsivalt KÕRGE — laseb AutoDirection'il RO-d ise juhtida; vt PARANDUS allpool)
3 SHDN --> U5 5V liin (püsivalt "sees")
4 DI   <-- STM32 U1 jalg 16 ("PA11[PA9]", USART1_TX remap)
5 GND  --> GND
6 A    --- U6 pin 1 --- J1 pin 5+6 (A, rööbiti)
7 B    --- U6 pin 2 --- J1 pin 7+8 (B, rööbiti)
8 VCC  <-- U5 5V liin otse (vt ptk 1, ei ole enam eraldi U8 LDO-d), bypass C6 (100nF) VCC juures
```

**NB:** jalgade "PA11[PA9]" ja "PA12[PA10]" remap'i vajadus on
selgitatud ptk 2-s — need EI OLE lihtsalt PA9/PA10, need on TSSOP20
pakendi füüsilised jalad 16 ja 17, mis vajavad SYSCFG_CFGR1 remap-bitti
firmware's, et kanda USART1 TX/RX funktsiooni.

MAX13487 valiti spetsiaalselt, kuna sellel on **automaatne
saatmis/vastuvõtu suuna lülitus** (autodirection) — DI-l ja A/B
diferentsiaalpingel põhinev riistvaraline olekumasin lülitab saatja ise
välja, mitte tarkvara ajastuse põhjal. See ei vaja MCU-lt ühtegi
DE/RE-tüüpi GPIO-d: **RE on püsivalt 5V liini külge** (vt PARANDUS
allpool — see EI ole GND, nagu varem siin kirjas oli) ja **SHDN on
püsivalt 5V liini külge** (kiip pole kunagi shutdown-režiimis).

**★ PARANDUS (RE oli varem valesti GND-l):** algselt kirjutasin siia,
et RE→GND tähendab "vastuvõtja alati kuulamas". See on vale — kontrollisin
uuesti otse andmelehe Pin Description tabelist ja "AutoDirection
Circuitry" peatükist:
- **RE̅ MADAL (GND)** = sunnib RO-d ALATI sisse lülitatuks, ka omaenda
  saatmise ajal — st sa kuuleksid oma enda saadetud andmeid tagasi
  (echo), sest vastuvõtja ei lülitu saatmise ajaks kunagi välja.
- **RE̅ KÕRGE (5V)** = laseb kiibi sisemisel olekumasinal (mis hoiab
  sisemist signaali `RECEIVER_ENABLE` = `DRIVER_ENABLE` inversioon)
  RO-d ISE juhtida — vastuvõtja lülitub saatmise ajaks automaatselt
  välja (väldib echo't) ja lülitub kohe pärast tagasi sisse.

Kuna meie seade on Modbus RTU **slave**, mis peab nii küsimusi vastu
võtma KUI KA vastuseid saatma, on õige (ja ainus mõistlik) valik
**RE̅ = PÜSIVALT KÕRGE (5V)** — täpselt see, mida "AutoDirection"
funktsioon ette näeb. Sama peatükk kinnitab ka, et A/B liinide
pullup/pulldown-takistid (R3/R4, vt allpool) **ei ole valikuline
mugavus, vaid otseselt vajalikud** selle sama autodirection-mehhanismi
toimimiseks: pärast saatmise lõppu lülitab kiip draiveri ise välja ja
usaldab, et need takistid hoiavad bussi kehtivas "idle" olekus
(A−B > 200mV).

**RO signaali kaitsejaotur (R11/R12):** U2 VCC = 5V, aga STM32 GPIO-d on
3.3V domeenis — RO väljund (loogiline "1" ~VCC lähedal) ühendatuna otse
PA10-le võib ületada MCU sisendi lubatud pinge (enamik pole 5V-taluvad
"FT" jalad; kontrollimata jätmine riskib GPIO kahjustusega). Jaotur toob
signaali ohutusse vahemikku ka halvimal juhul (VCC = 5.25V):

Vout = VCC x R12/(R11+R12)

R11 = 3.3 kohm, R12 = 4.7 kohm:
Vout(nom, VCC=5V) = 5V x 4.7/8 = 2.94V
Vout(halvim, VCC=5.25V) = 5.25V x 4.7/8 = 3.08V

Mõlemad jäävad selgelt alla 3.6V (tüüpiline non-FT GPIO absoluutne
maksimum) ja piisavalt üle loogilise "1" läve (~2V) usaldusväärseks
lugemiseks. R11+R12 = 8 kohm koormus RO väljundile on tühine (MAX13487
push-pull väljund veab seda kergesti) ja RC-ajakonstant (koos ~10-20pF
trassi/sisendmahtuvusega) jääb ~100-200ns kanti — palju kiirem kui
ükski Modbus RTU bitiaeg, seega servad ei moondu.

**DI liini ühilduvus (3,3V MCU → 5V kiip, otseühendus on OK):** kontrollitud
MAX13487E andmelehe elektriliste karakteristikute tabelist — DI/SHDN/RE
sisendite lävid on **fikseeritud absoluutväärtused** (mitte VCC-suhtelised):
V_IH min = 2,0V, V_IL max = 0,8V, sõltumata VCC=5V-st. STM32 3,3V GPIO
väljundi "1" (~3,0V+) on selgelt üle 2,0V läve, seega **PA9 → DI otseühendus
ilma taseme-muundamiseta on ohutu ja korrektne** — ei ole vaja optokoplerit
ega pingejagurit selles suunas. (RO vastupidises suunas *vajab* kaitset,
vt R11/R12 jagurit allpool — seal on probleem, et 5V-lähedane väljund läheb
3,3V-domeeni sisendisse, mis pole 5V-taluv; DI-l seda probleemi pole, kuna
suund on vastupidine ja lävi on fikseeritud madal.)

**Optokopler/isolatsioon (Maxim'i "Isolated RS-485 Interface" näidisskeem,
VISO/VSYS) EI OLE kasutusel:** MAX13487E andmelehe esilehel näidatud
rakendusskeem sisaldab valikulist galvaanilist isolatsiooni (kaks
optokopleri RO/DI liinidel + eraldi isoleeritud toitepool VISO). See on
mõeldud juhtudeks, kus buss ja MCU vajavad eraldi maandusi (nt seadmed
eri hoonetes, maandustsilmuse risk). Meie disainis on **üks jagatud
maandus** kogu plaadil (U2 VCC tuleb otse kohalikult U5 buck'ilt, sama
maandus mis U1-l) — isolatsiooni pole vaja ja seda ei ole lisatud.

**Bussi kaitse ja termineerimine:**

- U6 (Semtech SM712, üks kiip, SOT-23) kaitseb mõlemat liini (A, B)
  korraga ESD/liinihäirete eest, RS-485-le sobiva asümmeetrilise
  -7V/+12V piiranguga (vt pinout ptk 4 alguses).
- R1-R2 (120 ohm) termineerimistakistid — paigaldada **ainult siis**, kui
  see konkreetne seade on kogu ahela **füüsilises otspunktis** (jumper
  valikuline, vaikimisi eemaldatud). Daisy-chain ahelas on termineeritud
  ainult kaks seadet — ahela mõlemad füüsilised otsad — kõik vahepealsed
  seadmed jätavad R1/R2 aktiveerimata.
- R3-R4 (560 ohm) fail-safe biasing takistid, mis tagavad, et buss on
  loetav kindla olekuna ("1"), kui ükski seade parasjagu ei saada — see
  väldib valesid kaadreid, kui buss on hetkeks jõude. **Täpsustus:**
  MAX13487 andmelehe järgi pole need lihtsalt "kena lisada" tüüpi
  fail-safe, vaid **otseselt vajalikud kiibi enda AutoDirection-mehhanismi
  jaoks** — pärast saatmise lõppu lülitab kiip oma draiveri ise välja
  ja usaldab, et R3 (A pullup) / R4 (B pulldown) hoiavad bussi kehtivas
  "idle" olekus (A−B > 200mV). Ilma nendeta on nii bussi vaikimisi lugemine
  KUI autodirection'i enda üleminekud ebausaldusväärsed.
- **F1 (polyfuse)** paigutub J1 **sisenevate pinnide (V+ pin 1)**
  toiteliinile, **kõige esimesena, enne D1-te** (nii ptk 1 diagrammis
  kui allpool) — kaitseb kogu allavoolu ahelat (sh D1, C3, U5 jne),
  kui mõni järgmine (allavoolu) ahela lüli lühistab. Rööbitiühendus
  väljuva V+ pinniga (pin 2) toimub F1-st JÄRGNEVALT (F1 kaitseb
  ainult seda seadet ennast, mitte järgmisi ahelas — igal seadmel on
  oma F1).

**Modbus aadress:** SW1 (4-bit DIP-lüliti) ühendatud vabade GPIO-de külge
(nt PA0-PA3), mis loetakse boot ajal ja määrab slave aadressi (1-16).
See lubab sama trükkplaati kasutada mitmes toas ilma tarkvara muutmata.

## 5. Olekuindikaator (LED1)

**OSA VALITUD (parandab varasema andmelehe vastuolu):** kausta
`datasheet` laetud `LTST-C170GKT_Lite-On_Inc.pdf` kirjeldas tegelikult
**ühevärvilist ROHELIST LED-i** (GaP roheline, 2 jalga) — see EI OLNUD
4-jalaga RGB LED, mida see projekt vajab.

**OSA ASENDATUD 22.09.2026:** esimene valik, Kingbright
WP154A4SUREQBFZGC, on Farnellist otsas/lõpetatud. Praegune BOM-i osa:
**Kingbright L-154A4SUREQBFZGEW** — sama tootja, sama 5mm T-1¾,
4-jalaga THT, **common cathode** vorm, Farnellis otse laos (13500+tk,
uk.farnell.com tellimisnr 2290375, mitte ainult CPC/Farnelli tütarpoes).
Andmelehe (farnell.com/datasheets/3360919.pdf) elektrilised näitajad
@ If=20mA:

| Kanal | Vf tüüpiline | Vf max | Iv (heledus) |
|---|---|---|---|
| Punane | 1.9V | 2.5V | 400-1000mcd |
| Roheline | 3.2V | 4.0V | 900-1700mcd |
| Sinine | 3.3V | 4.0V | 300-500mcd |

Vf-väärtused on peaaegu identsed algse osaga, nii et asendus ei muuda
allpool toodud takistiloogikat sisuliselt. Heledus on võrdne või parem
algsest osast — kaaluti ka odavamaid Pro Signal PSG91925 (THT) ja
Multicomp Pro 703-1039 (SMD PLCC-6) alternatiive, aga need jäid kas
avaldamata heleduse (Pro Signal) või selgelt madalama mcd (Multicomp,
punane 450mcd/sinine 430mcd) tõttu kõrvale.

**Jalgade järjekorda (kumb jalg on ühine katood, R/G/B järjekord)
polnud võimalik andmelehe graafilisest diagrammist automaatselt
kätte saada — see graafik pole tekstina väljaloetav.** Enne joodist
**kontrolli jalgade järjekord multimeetri dioodi-testiga** (või vaata
andmelehe PDF-i pinout-joonist otse), ära eelda järjekorda.

RGB LED ühendatud **kolme sõltumatu PWM-võimelise GPIO külge** (TIM3
kanalid, vt ptk 2 tabel: PA5=punane, PA6=roheline, PA7=sinine):
- Roheline: normaalne töö, side olemas.
- Punane: andurite või side viga.
- Kollane (R+G): käivitus/init.

**Kolm eraldi piirtakistit, mitte üks** — R15 (punane), R16
(roheline), R17 (sinine), kuna kolm sõltumatult PWM-juhitavat kanalit
vajavad igaüks oma takistit.

### 5V-lülitatud draiverahel (lisatud 22.09.2026)

**GPIO otse ei jõua rohelist/sinist täie heledusega süüdata**, sest
nende Vf (3.2V/3.3V tüüp.) on 3.3V MCU toitepinge lähedal — otse
GPIO-lt juhtides jääks vooluseadja-takistile peaaegu olematu varu
(vt vana arvutus: R16/R17=47Ω andis hinnanguliselt vaid 1-3mA, mis on
olekutulena piisav, aga mitte "hele").

**Varem siin pakutud lihtne idee — "juhi PNP otse GPIO baasist" —
EI TÖÖTA korrektselt** ja on nüüd asendatud: PNP-transistori emitter
oleks 5V liinil, aga MCU GPIO loogiline "1" on vaid 3.3V, mitte 5V.
See tähendab Vbe = 5V − 3.3V = 1.7V, mis on ikka üle ~0.7V
läviväärtuse — transistor ei lülitu GPIO "1" korral täielikult välja,
vaid jääb osaliselt juhtima (hõõgub/lekib).

**Korrektne lahendus: 2-transistoriline kõrgpoolne lüliti iga kanali
kohta** (vt skeemid.html ptk 5, diagrammil Q1A/Q1B jne):

1. GPIO → **Rb (10kΩ)** → NPN (**BC847**, Nexperia, SOT-23) baas.
2. NPN emitter → GND. NPN kollektor = "sõlm A".
3. **Rpu (10kΩ)** sõlmest A üles 5V liinile — hoiab sõlme A vaikimisi
   5V-l (PNP vaikimisi väljas), kui GPIO on madal.
4. Sõlm A → PNP (**BC856**, Nexperia, SOT-23) baas.
5. PNP emitter 5V liinil, kollektor → **Rlimit** (R15/R16/R17,
   uued väärtused allpool) → LED anood.

**Töö:** GPIO "1" (3.3V) küllastab NPN-i (Ib=(3.3V−0.7V)/10kΩ≈260µA,
BC847 hFE min 125 @2mA → Ic saadaval ~32mA, palju rohkem kui vaja).
NPN tõmbab sõlme A ~0V-ni (Vce,sat tüüp. 90mV @10mA). See annab
PNP-le Vbe≈−4.8V — sügavalt küllastuses, olenemata sellest, et GPIO
ise ulatub vaid 3.3V-ni. Sõlm A saab seega täie 0V/5V vahetuse, mitte
GPIO piiratud 3.3V vahetuse.

**Uued Rlimit väärtused (5V liinilt, ~15mA sihtvool, kõik 0805,
piisav varu kuni 30mA/kanal absoluutse maksimumini):**
- R15 (punane): R=(5V−0.2V−1.9V)/15mA=193Ω → **200Ω**, ~14.5mA.
- R16 (roheline): R=(5V−0.2V−3.2V)/15mA=107Ω → **110Ω**, ~13.6mA.
- R17 (sinine): R=(5V−0.2V−3.3V)/15mA=100Ω → **100Ω**, ~15mA.

**Lisandunud BOM-i read:** Q1A-Q3A (BC847, NPN, 3×), Q1B-Q3B (BC856,
PNP, 3×), Rb1-Rb3 (10kΩ, 3×), Rpu1-Rpu3 (10kΩ, 3×) — kokku 6
transistorit + 6 takistit, kõik SOT-23/0805, käsitsi joodetavad.

**Miks BC856, mitte BC857:** sama Nexperia PNP pere (BC856/857/858),
erinevad ainult Vceo pingeklassi poolest (856=65V, 857=45V, 858=30V) —
meie 5V ahelas pole see vahe oluline, kumbki sobib võrdselt hästi.
hFE min 125 @2mA on baasosal (ilma A/B/C sufiksita) sama mis eelpool
kasutatud Rb/Rpu arvutuses, seega väärtused kehtivad muutumata. Valik
tehti kasutaja Farnellist leitud konkreetse osa järgi.

**LED1 peab olema THT (läbi plaadi), mitte SMD.** Põhjus on lihtne
loogika: LED1 näitab seadme olekut kasutajale, mis tähendab, et see peab
lõplikus paigalduses olema korpuse pealt/august nähtav. SMD LED, mis on
joodetud lapiku pinnaga vastu trükkplaati, jääks korpuse sisse peidetuks —
seda ei ole võimalik väljastpoolt näha, kui plaat on suletud korpuses.
Seetõttu:
- LED1 on **5mm 4-jalaga THT RGB LED** (mitte 3mm — vt allpool
  põhjendust ega SMD).
- **Paiguta LED1 plaadi servale**, samas suunas kuhu korpuse
  vaateava/valgusjuhe jääb (vt `manuaal.md` 3D-korpuse peatükki).
- Kui LED peab paistma risti plaadi tasapinnaga (nt korpuse esiküljelt,
  plaat ise horisontaalne) — kas painuta LED1 jalgu pärast joodist 90°
  nurga alla (tavaline praktika 5mm THT LED-idega, jalad on selleks
  piisavalt jäigad ja pikad), või kasuta valgusjuhet (light pipe), mis
  kannab valguse plaadilt otse LED-ilt korpuse pinnale. Kui vahemaa
  plaadi ja korpuse seina vahel on väike ja LED-i saab paigutada otse
  au taha, piisab tavalisest püstisest (vertikaalsest) paigaldusest.
- KiCad jalajälg: `LED_THT:LED_D5.0mm-4_RGB_Staggered_Pins` (KiCadi
  vaikimisi LED_THT teegis pole 3mm 4-jalaga RGB varianti olemas, ainult
  5mm — see oli ka põhjus, miks BOM-is 3mm → 5mm parandati).

## 6. Programmeerimine (J2, SWD)

4-pinniline header: VDD, SWCLK, SWDIO, GND — ühilduv ST-Link
programmaatoriga. Paigutada plaadi serva lähedale, ligipääsetavaks ka
pärast korpusesse paigaldamist (või jätta ajutiseks arendusfaasi
konnektoriks, mida lõplikus paigalduses ei kasutata).

## 7. PCB paigutuse soovitused

- Digitaal- (MCU) ja analoog-/anduri osa hoida eraldi, ühine maandustasand.
- RS-485 liinid (A/B) paigutada paarina, vältida teravaid nurki, hoida
  eemal kõrgsageduslikest rööbastest.
- Buck-muunduri induktiivpool ja lülituslüliti tagasisideahelast eemale,
  et vähendada müra.
- **U7 (5V→3.3V LDO, uus roll)** paigutada U5 (buck) väljundi lähedale,
  C8/C9 vahetult U7 jalgade juures; kuna sisendpinge vahe on nüüd väike
  (5V→3.3V), ei ole erilist soojushajumisvajadust (vt ptk 1 —
  arhitektuur lihtsustatud, eraldi kõrgepinge-LDO-d U8 enam pole).
- **J1 (8-pin bussikonnektor)** on üks ühine konnektor sisse- ja
  väljumineva kaabli jaoks (paaris pinnid V+/V+, GND/GND, A/A, B/B —
  vt ptk 4) — paiguta plaadi servale, kust mõlemad bussikaablid
  (eelmine ja järgmine seade) mugavalt karpi/seina suunas jooksevad.
- **J3 pistikupesa** (põhiplaadil, MCU I2C1 juures) paigutada plaadi
  servale, kust kaabel mugavalt anduriploki plaadi (SHT45+ENS160)
  juurde jookseb. Anduriploki plaat ise ei ole põhiplaadiga sama PCB —
  vt ptk 3 põhjendust ja `manuaal.md` 3D-korpuse ptk-i, kus anduriploki
  plaat paigutub korpuse ventileeritud ossa, füüsiliselt eemal
  MCU/buck-muunduri soojusallikatest.
- **Anduriploki plaadi enda layout:** väike plaat, mis kannab ainult U3
  (SHT45), U4 (ENS160) ja **J4** (anduripoolne pistikupesa, sama osa
  mis J3, vt ptk 3) — pull-up takistid R5-R8 jäävad põhiplaadile (vt
  BOM/ptk 3), anduriploki plaat ise on lihtne ja väike. Peab mahtuma
  korpuse ventilatsiooniava taha.
- **LED1 (olekuindikaator)** on THT ja peab jääma korpuse pealt/august
  nähtavaks — paiguta plaadi servale, korpuse vaateava suunas (vt ptk 5
  põhjendust: SMD LED jääks korpuse sees peidetuks, mistõttu see valik
  ei ole seadme jaoks kasutatav).

### 7.1 U5 (buck) "kuum ahel" — kriitilisim paigutus plaadil

Lülituva muunduri kiire impulssvool (mitte alalisvool) käib ahelas
**C3/C4 (sisend) → U5 sisemine ülemine lüliti → L1/SW-sõlm → D2
(vabajooksu dioot) → tagasi C3/C4-sse**. See on "kuum ahel" — mida
suurem selle ahela füüsiline silmus, seda rohkem induktiivsust see
kannab ja seda rohkem müra/üleminguvõnkeid tekib SW-sõlmes.

- **C3/C4 (sisendkondensaator) on tähtsam paigutada lähedale kui C5
  (väljund)** — sisendvool on ruuthüppeline (lülitub nanosekundites),
  väljundvool on induktori tõttu juba silutud kolmnurklaine. C3/C4
  jalad vahetult U5 VIN/GND jalgade kõrvale, lühim võimalik rada.
- **D2 (vabajooksu Schottky)** samasse kuuma ahelasse, samale kihile
  mis C3/C4 — see lülitub sünkroonselt ülemise lülitiga.
- **L1** võib olla mõni mm eemal (induktori vool on silutud), aga
  SW-sõlme rada (U5 pin6 → L1) hoia lühikesena ja **eemal
  tagasisidetakistite R13/R14 rajast** — SW-sõlm on ise müraallikas,
  tagasiside (VFB) rada ei tohi sellest läbi ega selle kõrvalt joosta.
- **C5 (väljundkondensaator)** paiguta koormuse (U2/U7 sisendite)
  lähedale, mitte tingimata U5 kõrvale.
- Jäta U5 aluse/GND-tasandi alla **terve, katkematu vasktasand** —
  ära lõika ega vii sealt läbi signaaliviase; maanduse tagasitee peab
  olema sama lühike ja otsene kui edasitee.

### 7.2 Lokaalne bypass — iga kiibi juures, mitte "kuskil real"

Reegel: iga IC/regulaatori toitepinn saab oma kondensaatori
**vahetult selle jala juures**, sõltumata sellest, kas samal real on
juba mõni teine kondensaator. Konkreetsed asukohad selles projektis:

- **U1 (MCU) C1/C2** — otse VDD-jalgade juures, lühim rada VDD-lt
  kondensaatorini ja sealt VSS-ni (mitte pikk rada läbi plaadi).
- **U2 (MAX13487) C6** — otse VCC-jala juures.
- **U5 → U7 vaheline C8** — otse U7 VIN-jala juures (vt ptk 1);
  **C9** otse U7 VOUT-jala juures.
- **ENS160 uus 1.8V regulaator (MCP1700T-1802E/TT)** — sisendkondik
  otse VI-jala juures (sinu enda skeemis see hetkel puudus, vt
  varasem märkus), väljundkondikud (100nF + 10µF) jäävad otse VDD
  juurde nagu praegu joonistatud — kuna regulaator ja ENS160 on samal
  väikesel anduriploki plaadil, on need füüsiliselt niigi lähestikku.
- **Kristall Y1 (kui kasutusel)** — võimalikult lähedal MCU
  OSC-jalgadele, lühikesed sümmeetrilised rajad, ümber väike
  maandustasandi "kaitseala" (guard ring), teised signaalid sellest
  mitte läbi ei tohi joosta.

### 7.3 Maandus

- Kasuta **ühte tervet vasktasandit** (ground pour) kogu plaadi
  alumisel/vabal kihil, mitte eraldi "digitaal" ja "analoog" saari —
  see plaat on liiga väike ja madala sagedusega, et jagatud maandus
  end ära tasuks; jagatud maandus ilma hoolika ühendussilla
  (single-point) planeerimiseta pigem tekitab probleeme kui lahendab.
- Ainuke koht, kus tõesti tasub maandust "eraldi hoida", on U5 kuum
  ahel (vt 7.1) — selle tagasitee peaks jõudma põhi-GND-tasandile läbi
  lühikese, otsese raja, mitte läbi pika ümbersõidu.
- Lisa **maandusläbiviigud (stitching vias)** iga paari cm tagant
  mööda plaadi serva ja konnektorite (J1/J2/J3) ümber — hoiab
  maandustasandi ühtlasena ja annab RS-485/ESD kaitsele lühima tee.
- **U6 (SM712 TVS)** paiguta **J1 ja U2 vahele, võimalikult lähedal
  J1-le** — kaitse töötab ainult siis, kui müra/ESD impulss jõuab
  TVS-ini enne transiiverini; kui TVS on kaugemal J1-st kui U2, kaotab
  see suure osa oma mõttest.

## 8. Komponentide valiku juhised (pakend, võimsus, pinge, vool)

See peatükk koondab üldised reeglid, mida kasutati kogu selle projekti
takistite/kondensaatorite/induktorite valikul — kasulik ka enda KiCad
sümboliteegi loomisel ("mis formfaktorit/väärtust kasutada?").

**Pakend (formfaktor):** projekt on valdavalt käsitsi joodetav (TSSOP20
MCU 0.65mm sammuga, DFN/LGA andurid kuumaõhuga) — seepärast:
- **Takistid: 0805** — kõige lihtsam käsitsi joota (nähtavad
  jooteväljad, pintsettidega hõlbus), takistite arv/suurus ei ole
  siin plaadi kitsaskoht.
- **Kondensaatorid: 0603** enamikule (bypass/decoupling), **0805/1206**
  suurematele bulk/väljundkondensaatoritele (nt C3/C4 10µF, C5 22µF) —
  suurem pakend talub paremini kõrgemat pingetaluvust ilma mahtuvuse
  languseta (vt allpool).
- **0402 pole soovitatav** käsitsi jootmiseks ilma šabloon+reflow'ta
  või oskusliku kuumaõhuta — kui plaan on lasta JLCPCB SMT-teenusel
  komponendid paigaldada, muutub 0402 mõistlikuks ja vähendab plaadi
  pindala.
- **Induktor (L1) ei ole EIA-koodiga generic passiivne** — tema
  jalajälg tuleb võtta konkreetse valitud osa enda andmelehelt/3D
  mudelilt, mitte oletada üldist suurust.

**Takistite võimsustaluvus (P = I²R):** kontrolli alati tegelikku voolu
läbi takisti selle nimivõimsuse vastu (tüüpiline 0603 = 1/10W (100mW),
0805 = 1/8W (125mW), aga see erineb tootjati — kontrolli konkreetse
osa andmelehelt):
- **R3/R4 (560Ω fail-safe bias):** ~4mA jooksul → **~10mW** — tühine
  koormus, iga 0603/0805 (100-125mW nimi) annab suure varu.
  RS-485 bussid, mis idle-olekus enam voolu ei tarbi, on siin turvalises
  tsoonis.
- **R1/R2 (120Ω bussi termineerimine):** idle-olekus tühine, aga
  aktiivse saatmise ajal näeb see takisti suurt osa draiveri
  väljundpingest (mitu volti) — see võib 120Ω juures 0805 (1/8W)
  nimivõimsusele lähedale jõuda. Modbus RTU liiklus on lühikeste
  pursetega (madal keskmine soojenemine), aga kui soovid täit varu,
  kontrolli MAX13487 andmelehe VOD (draiveri väljundpinge koormuse all)
  vastu või vali 1/4W-nimega takisti — odav kindlustus.
- **Täpsusnõudega kohad (R12/R13, U5 FB-jagur):** kasuta **1%
  tolerantsi**, sest väljundpinge täpsus sõltub otse selle jaguri
  suhtest — mujal (bias, pull-up, LED-piirtakistid) piisab 5%-st.

**Kondensaatorite pingetaluvus ja dielektrik — kriitiline, kergesti
unustatav väärtus:**
- Vali nimipinge, mis on **selgelt üle** tegeliku tööpinge, mitte
  ainult "veidi üle". Selles projektis: sisendi bulk-kondensaatorid
  (C3/C4) istuvad otse V+ liinil (kuni 30V) — nõutav **≥50V**
  nimipinge, mitte nt 35V.
- **Keraamilise X7R kondensaatori TEGELIK mahtuvus langeb DC-eelpinge
  kasvades** — tüüpiliselt kaotab 10µF/50V X7R kondensaator kõrge
  (24-30V) eelpinge juures suure osa oma nimimahtuvusest (see on
  füüsikaline nähtus, mitte tootja viga). Reegel: vali nimipinge
  ~2× tegelikust tööpingest, et jääda tootja DC-bias kõvera lamedale
  osale, ja kontrolli konkreetse valitud osa enda DC-bias kõverat
  andmelehelt — ära usalda ainult sildil olevat nimimahtuvust.
- **Dielektrik:** kasuta läbivalt **X7R** (stabiilne temperatuuri ja
  pinge suhtes) — väldi Y5V/Z5U tüüpi, mis võivad kõrge pinge/temp
  all kaotada 50-80% oma mahtuvusest.
- Väljundpoolel (C5, 5V liin) piisab madalamast nimipingest
  (**≥10V** annab mõistliku varu 5V töö kohta ilma üledimensioonimata).

**Induktori (L1) kaks erinevat voolunäitajat — mõlemat tuleb
kontrollida:**
- **Küllastusvool (Isat):** peab ületama tegeliku tipuvoolu varuga
  (siin ~0.7-0.8A tipp, valitud osa Isat=1.7A) — vastasel juhul
  südamik küllastub ja kasutegur/soojenemine halveneb järsult.
- **Nimivool/Irms:** eraldi termiline piirang (kui palju voolu enne
  mähise ülekuumenemist) — andmelehed annavad need TAVALISELT eraldi
  väärtustena, mõlemad tuleb kontrollida, mitte ainult üht.
- **DCR (alalisvoolu takistus):** madalam = parem kasutegur, siin
  valitud osal (TDK SLF7055T-220M1R7-3PF) 64.3mΩ.
- Näide sellest, kuidas seda praktikas rakendada: vt L1 valiku
  põhjendust `BOM.csv`-s — 22µH induktorite parameetrilises otsingus
  oli enamik "22µH" tulemustest tegelikult signaali-/RF-induktorid
  (voolutaluvus 2-250mA), mitte võimsusinduktorid, hoolimata samast
  induktiivsuse väärtusest. Induktiivsuse väärtus üksi EI PIISA
  komponendi sobivuse hindamiseks.

## 9. Anduriploki paneeli-lahendus (mouse bites) — üks JLCPCB tellimus

Ptk 3/7 kirjeldatud anduriplokk (U3+U4+J4) on eraldi elektriline
plaat, mis ühendub põhiplaadiga J3↔J4 kaabli kaudu (vt ptk 3/4) ja
istub lõplikus korpuses füüsiliselt mujal (ventilatsiooniava taga,
eemal MCU/buck soojusallikatest). See EI MUUDA seda otsust — aga
selle asemel, et tellida **kaks eraldi PCB-d kahe eraldi JLCPCB
tellimusena**, on odavam ja lihtsam joonistada mõlemad **ühele
paneelile ja murda need käsitsi lahti enne kaabli ühendamist.**

**Mouse bites (mitte V-cut) on õige valik siin, sest:**
- JLCPCB V-cut nõuab **sirget joont üle terve ristkülikukujulise
  paneeli** ja min paneeli mõõtu 70×70mm — anduriploki plaat on
  tõenäoliselt liiga väike ja/või ebaregulaarse kujuga (väljaulatuv
  tükk, mitte täisristkülik), mis V-cut nõudeid ei täida.
- Mouse bites sobivad just ebaregulaarsete/väikeste väljaulatuvate
  osade jaoks — annab konarliku serva (lihvi vajadusel viiliga), aga
  töötab suvalise kujuga.

**Konkreetsed JLCPCB reeglid mouse bites jaoks:**
- Auguava läbimõõt: **0.6mm** iga auk.
- Aukude vaheline (jääv) materjal: **0.3-0.4mm** serva-servani.
- **5-8 auku** ühes "perforatsiooniribas"; alla 30mm laiuse ühenduse
  jaoks piisab kahest sümmeetrilisest ribast (nt kaks nurka), pikema
  ühenduse jaoks lisa üks riba iga 50-60mm kohta.
- Hoia perforatsiooniribad **vabas alas** — mitte üle padjade, radade,
  kinnitusavade ega kõrgete komponentide.
- Pärast tellimuse saabumist: murra anduriploki plaat põhiplaadi
  küljest lahti käsitsi (vajadusel toeta murdmisjoont mõlemalt poolt
  näppudega), lihvi vajadusel ots viiliga siledaks enne J4 kasutamist.

## 10. Kinnitusavad, silkscreen ja JLCPCB tootmiseks ettevalmistus

**Kinnitusavad (korpuse kruvid):**
- Tavaliselt 4 nurgaava, M2/M2.5/M3 kruvi jaoks (vali oma 3D-prinditud
  korpuse kruviaugu läbimõõdu järgi — vt `manuaal.md`).
- Jäta **vähemalt ~1mm vaskvaba (keep-out) ala** augu ümber — ära vii
  radu ega pinnavälju otse kinnitusaugu serva lähedale, kruvi/seib
  võib lühistada.
- Paiguta nurgaavad nii, et need ei jää U5 kuuma ahela (ptk 7.1) ega
  RS-485/TVS ala (ptk 7.3) kohale — vibratsioon/pinge kruvimisel ei
  tohi mõjutada tundlikke alasid.

**Silkscreen — mida lisada enne tellimist:**
- **Iga komponendi viide** (R1, C3, U5 jne) nähtavana, mitte
  komponendi enda alla peidetuna — kontrolli iga viide käsitsi läbi
  (KiCad "Silkscreen check" / DRC ei leia seda automaatselt).
- **Polaarsusmärgid:** dioodid (D1-D3, katoodi triip), LED1 (anoodi/
  katoodi märge, vt ptk 5 pinout), elektrolüüt-kondensaatorid kui
  selliseid lisandub (+ märk) — need JÄÄVAD sinu enda risk kui
  puuduvad, käsitsi jootmisel on lihtne LED/dioot valesti pidi panna.
- **Pin-1 märge** igal IC-l (U1 MCU, U2, U5, U7, uus 1.8V regulaator)
  — enamik KiCad footprinte lisab selle automaatselt, aga kontrolli.
- **Konnektorite pinout selgelt lahti kirjutatud** J1 (V+/GND/A/B
  kummalgi poolel) ja J3/J4 (SDA/SCL/VDD/GND) juures — need on
  välise kaabelduse jaoks kriitilised, vale ühendus võib komponendi
  rikkuda (vt ptk 4 RS-485 hoiatused).
- **Plaadi nimi + versioon + kuupäev** kuskile vabale kohale (vt ka
  KiCad kirjanurga väljad) — kasulik kui plaadist tuleb mitu revisiooni.
- Jäta üks **väike tühi ala silkscreenil** (nt alumisel küljel nurgas)
  — JLCPCB trükib sinna vaikimisi oma tellimuse numbri, kui vaba kohta
  ei jäta, võib see kattuda sinu enda teksti/komponentidega.

**JLCPCB tootmisreeglid (2-kihiline, standardvõimekus) — kontrolli
DRC nende vastu enne gerberite eksportimist:**
- Min raja laius/vahe: **0.10mm / 0.10mm** (0.15-0.2mm annab
  käsitsi-jootmise plaadil turvalise varu, pole vaja miinimumi taga
  ajada).
- Min läbiviigu ava: **0.15mm**, min välisläbimõõt **0.25mm**, min
  soovituslik "annular ring" **0.25mm** (absoluutne miinimum 0.18mm).
- Min mitte-metalliseeritud ava (nt mehaaniline kinnitusava): **0.5mm**.
- Min vahe vasest plaadi servani (ruuditud serv): **0.2mm**.
- Silkscreen: min joonelaius **0.15mm**, min vahe padjast **0.15mm**.
- Plaadi paksuse valikud: 0.4/0.6/0.8/1.0/1.2/1.6/2.0mm — **1.6mm on
  tavavalik**, kui korpus/konnektorid ei nõua teisiti.
- Kui plaanid lasta JLCPCB-l ka SMT-komponendid (bare-kiibid U3/U4,
  DFN/LGA pakendid, vt ptk 8) ise paigaldada: lisa **vähemalt 3
  fiduciaalmärki** (ümmargune vaskpind, soldermask-vaba, tüüpiliselt
  1mm) paneeli/plaadi nurkadesse — need on masinale visuaalseks
  joondamiseks kohustuslikud, mitte valikulised.

## 11. Testpunktid (debug/mõõtmine)

**Otsus (arutletud vestluses):** kuna see on üks käsitsi kokku pandav
plaat, mida testib disainer ise (mitte tootmisliini bed-of-nails
fixture), ei ole vaja iga huvipakkuvat netti varustada päris
komponendiga. Enamiku punktide jaoks piisab **paljast, soldermask-vabast
vasepadjast** (KiCad `TestPoint` teek, sümbol `TestPoint:TestPoint`,
jalajälg `TestPoint_Pad_D1.5mm` enamikule kohtadele, `TestPoint_Pad_D2.0mm`
avaramates kohtades nagu toiteliinid, `TestPoint_Pad_D1.0mm` tihedas
naabruses nagu SW1 või SOT-23 jalgade lähedal) — see annab suurema,
turvalisema sihtmärgi multimeetri/oskoprobe otsikule kui pelgalt
komponendi enda SMD-pada, ilma lisakuluta.

**Kui soovid päris komponenti (nt 2-3 kõige rohkem kasutatavale
toiteliini-punktile):** Keystone Electronics **5015** (Micro-Mini SMT
test point, fosforpronks/hõbetatud, KiCad'i sisseehitatud jalajälg
`TestPoint_Keystone_5015_Micro_Mini`) — kinnitatud päris osana:
- [DigiKey 5015](https://www.digikey.com/en/products/detail/keystone-electronics/5015/278885)
- [Tootja (Key Electronics) tooteleht](https://www.keyelco.com/product.cfm/Micro-Miniature/5015/product_id/1353)

**Asukohad (kasutades selle dokumendi läbivat viitesüsteemi):**

| Signaal | Asukoht | Põhjendus |
|---|---|---|
| VIN (toores sisend) | D1 järel, enne C3/U5 | kinnitab, mis bussikaablist tegelikult sisse tuleb, enne buck'i |
| 5V | U5 väljundil / U7 VIN jalal (sama net, vt ptk 1) | kinnitab buck'i regulatsiooni JA et miski trassil ei kaota pinget enne U7-te |
| 3V3 | U7 VOUT jalal | peamine loogikaliin (MCU + andurid) |
| GND | 2 kohta: toiteosas (U5/U7 lähedal) ja digitaalosas (U1/U2 lähedal) | kaks referentspunkti mugavam kui üks, eriti oskoprobe maandusjuhtme jaoks |
| RO | U1 jalg 17 (PA12[PA10]) pool, R11/R12 jaguri JÄREL | mis MCU tegelikult loeb — mitte U2 pool (enne jagurit) |
| DI | U1 jalg 16 (PA11[PA9]) / U2 pin 4 | otseühendus, jagurit siin pole |
| RS485 A/B | U2 pin 6/7 | valikuline — J1 pinnid on juba ligipääsetavad, aga see väldib konnektori enda koormamist probega |
| I2C SDA/SCL/GND | J3 (kaabli väljumiskoht anduriploki poole) | topeltotstarve: bussi debug JA anduriploki eraldi kontroll pärast mouse-bite lahtimurdmist |
| BOOT0 | U1 jalg 19 (PA14-BOOT0) / R10 pulldown-sõlm | lubab bootloader-režiimi sundida jumper-traadiga ilma programmaatorita |
| SW1 aadressibitid | U1 jalad 7-10 (PA0-PA3) | kuni 4 punkti — kinnitab riistvaralist bitti sõltumatult firmware loetust |
| LED-draiveri GPIO | Rb1-Rb3 (baasitakisti) GPIO-poolne ots | eristab "MCU ei ajanud GPIO-d" vs "draiver/LED ise vigane" — vt ptk 5 |

**Mida MITTE testpunktiga varustada:**
- **SW-sõlm** (U5 pin6 → L1, enne induktorit) — kiire lülitusmüraga
  sõlm, vt ptk 7.1. Proovi otse oskoprobiga, kui vaja, ära jäta
  püsivat padja-otsa.
- **RS-485/RO-Ahela DE/RE-tüüpi punkt EI KEHTI selles disainis** —
  ★ parandus varasema jutu suhtes: see plaat kasutab MAX13487
  AutoDirection režiimi, kus RE̅ ja SHDN on **püsivalt 5V liini
  küljes** (vt ptk 4), mitte MCU GPIO poolt aktiivselt juhitud. Siin
  ei ole muutuvat suunasignaali, mida testida — RO/DI (ülal) katavad
  kogu diagnostilise väärtuse, mida "DE/RE" tüüpi disainis annaks.

**Lahtine punkt — Y1 (aktiivne ostsillaator) ja MCP1700 (ENS160 1.8V
regulaator) testpunktid:** need komponendid otsustati vestluse hilisemas
osas (Y1 = Kyocera KC5032Z8.00000C1JX, HSE bypass PC14-le; MCP1700T-1802E/TT
ENS160 VDD jaoks), aga **pole veel selles dokumendis (ptk 1-10) kirjas** —
praegune §2 pin-tabel näitab endiselt "PC14/PC15 kasutamata (sisemine RC)",
mis on aegunud. Kui soovid, uuendan enne testpunktide lisamist kõigepealt
need osad dokumenti, muidu viitaksin siin komponentidele, mida dokumendi
muus osas veel ei eksisteeri.
