# Guition JC3636K718C_I_Y1 hardware reference

1.8in 360x360 round knob display. ESP32-S3R8 (8 MB octal PSRAM in package), 16 MB external flash, on a base board with USB-C, Li-ion charger, power switch, LED ring, audio and mic.

Everything below was checked against the manufacturer schematics. Where example code disagrees, the schematic wins and the conflict is noted. Items marked **UNVERIFIED** are inference or general datasheet knowledge, not something shown in the download.

## Source key

| Tag | File (under `mfr-download/`) |
|---|---|
| M-pN | `6-Schematic diagram/JC3636K718.pdf`, page N (main board, 5 pages: p1 haptic/encoders/board FPC, p2 3V3 buck, p3 SD, p4 LCD FPC/backlight, p5 ESP32-S3 + flash) |
| B-pN | `6-Schematic diagram/JC3636K718_P.pdf`, page N (base board, 6 pages: p1 connector, p2 power/USB-C, p3 ETA6003 charger, p4 LED ring, p5 audio/mic, p6 partial BOM) |
| pincfg.h | `2-Program Example/Demo_arduino/ST77916_LVGL_DEMO/pincfg.h` (identical copy in ST77916_LVGL_IMG_DEMO) |
| scr.h | `2-Program Example/Demo_arduino/ST77916_LVGL_DEMO/scr_st77916.h` |
| idf/ | `2-Program Example/Demo_idf/demo/main/` (msc_demo has an identical `device/pinconfig.h`) |
| xz/ | `2-Program Example/Demo_idf/xiaozhi/xiaozhi-esp32/main/boards/guition-jc3636k718/` |
| INI | `4-Chip manual/ST77916_T 1.8'' 360x360 initial code_V1.0_20241015.INI` |
| burn.txt | `9-Burn/Burn operation instructions/` burn instructions text file (Chinese file name) |

## GPIO table

Every ESP32-S3 GPIO that is bonded out is used. There are no spare pins on the main board.

| GPIO | Net | Function | Dir | Notes | Source |
|---|---|---|---|---|---|
| 0 | GPIO0 / RGB_DATA | BOOT button **and** LED ring data | I/O | Strapping pin (low at reset = download mode). 10k pull-up R11 on main. BOOT button SW1 (GT-TC018B) to GND on base. Same net drives WS2812 DIN through 100R (base R54). Pressing BOOT while LEDs run corrupts LED data, harmless. | M-p5, B-p1, B-p4; idf/device/pinconfig.h:44; pincfg.h:13 |
| 1 | EC1_B | Encoder B | In | 10k pull-up R53 to 3V3 | M-p1, M-p5; pincfg.h:24 |
| 2 | EC1_A | Encoder A | In | 10k pull-up R52 to 3V3 | M-p1, M-p5; pincfg.h:23 |
| 3 | GPIO3 / I2S_BCK | I2S bit clock to PCM5100A BCK | Out | 22R series (R48). Strapping pin (JTAG source select), only matters if that eFuse is set. | B-p1, B-p5; pincfg.h:34 |
| 4 | PDM_MIC_DATA | PDM mic data | In | | B-p1, B-p5; pincfg.h:40 |
| 5 | PDM_MIC_SCK | PDM mic clock | Out | | B-p1, B-p5; pincfg.h:41 |
| 6 | BAT_ADC | Supply voltage sense (ADC1_CH5) | Analog | Divider 10k/10k from VCC_OUT, see Battery | B-p1, B-p2; idf/device/pinconfig.h:42 (named PIN_BAT_DAC) |
| 7 | TP_INT | Touch interrupt | In | Via LCD FPC pin 1 | M-p4, M-p5; pincfg.h:20 |
| 8 | TP_RST | Touch reset | Out | Separate from LCD reset (idf/device/lcd_init.c:301 comment "Shared with LCD reset" is wrong) | M-p4, M-p5; pincfg.h:21 |
| 9 | TP_SDA = HAPTIC_SDA | I2C SDA | I/O | 2.2k pull-up R56 to 3V3. Also on board FPC to base, unused there. | M-p1, M-p5; pincfg.h:19 |
| 10 | TP_SCL = HAPTIC_SCL | I2C SCL | Out | 2.2k pull-up R57 to 3V3. Also on board FPC to base, unused there. | M-p1, M-p5; pincfg.h:18 |
| 11 | LCD_QSPI_SCL | LCD QSPI clock | Out | | M-p4, M-p5; pincfg.h:8 |
| 12 | LCD_QSPI_CS | LCD chip select | Out | | M-p5; pincfg.h:7 |
| 13 | LCD_QSPI_D0 | LCD data 0 | I/O | | M-p5; pincfg.h:9 |
| 14 | LCD_QSPI_D1 | LCD data 1 | I/O | | M-p5; pincfg.h:10 |
| 15 | LCD_QSPI_D2 | LCD data 2 | I/O | XTAL_32K_P pin, so no 32 kHz crystal | M-p5; pincfg.h:11 |
| 16 | LCD_QSPI_D3 | LCD data 3 | I/O | XTAL_32K_N pin | M-p5; pincfg.h:12 |
| 17 | LCD_RST | LCD reset | Out | | M-p4, M-p5; pincfg.h:6 |
| 18 | LCD_TE | LCD tearing effect | In | Not in pincfg.h. In idf/device/pinconfig.h:30 and xz/config.h:43, but TE sync is commented out in idf/device/lcd_init.c | M-p4, M-p5 |
| 19 | USB_DN | Native USB D- | I/O | Straight to USB-C, no bridge | M-p5, M-p1, B-p1, B-p2 |
| 20 | USB_DP | Native USB D+ | I/O | Straight to USB-C, no bridge | M-p5, M-p1, B-p1, B-p2 |
| 21 | LCD_BLK | Backlight enable / PWM | Out | High = on. Gate of AO3400A low-side switch, 10k gate pull-down (main R54) so off at reset | M-p4, M-p5; pincfg.h:4 |
| 26-32 | SPI flash bus | W25Q128JVPIQ | | Not available | M-p5 |
| 33-37 | (no connect) | Octal PSRAM inside ESP32-S3R8 | | Not available, drawn NC | M-p5 |
| 38 | SDMMC_CMD | SD CMD | I/O | 10k pull-up R48 (main) | M-p3, M-p5; pincfg.h:31 |
| 39 | SDMMC_SCK | SD CLK | Out | | M-p3, M-p5; pincfg.h:30 |
| 40 | SDMMC_D0 | SD D0 | I/O | 10k pull-up | M-p3, M-p5; pincfg.h:26 |
| 41 | SDMMC_D1 | SD D1 | I/O | 10k pull-up | M-p3, M-p5; pincfg.h:27 |
| 42 | I2S_DO | I2S data to PCM5100A DIN | Out | 22R series (R47). Also MTMS (JTAG) pin. | B-p1, B-p5; pincfg.h:36 |
| 43 | U0TX | UART0 TX (console) | Out | 470R series R65 on main. Goes to board FPC, then **nowhere** on the base. | M-p5, M-p1, B-p1 |
| 44 | U0RX | UART0 RX | In | Goes to board FPC, then nowhere on the base | M-p5, M-p1, B-p1 |
| 45 | I2S_WS | I2S LRCK to PCM5100A | Out | 22R series (R46). **Strapping pin: VDD_SPI voltage.** Must be low at reset for the 3.3 V flash; no pull-up anywhere. | B-p1, B-p5; pincfg.h:35 |
| 46 | PA_CTRL | Speaker amp enable (NS4150B CTRL) | Out | High = amp on. 10k pull-down R32 on base. **Strapping pin: must not be pulled up** (schematic note "45 46 default pull-down, especially 46 must not be pulled up", B-p1). | B-p1, B-p5; idf/device/pinconfig.h:34 |
| 47 | SDMMC_D3 | SD D3 | I/O | 10k pull-up | M-p3, M-p5; pincfg.h:29 |
| 48 | SDMMC_D2 | SD D2 | I/O | 10k pull-up | M-p3, M-p5; pincfg.h:28 |

Notes:
- CHIP_PU (EN) has a 10k pull-up and 1 uF to GND (R55, C8, M-p5). There is **no reset button** on either board.
- Encoder direction conflict: idf/knob/knob.c:27-28 uses A=1, B=2; pincfg.h, idf/device/pinconfig.h:46-47 and the schematic use A=2, B=1. Only the sense of rotation differs.

## Board-to-board connector

20-pin 0.5 mm FPC (AFC42-S20FMA-1H, FPC6 on both boards). Main pin N = base pin 21-N (the cable flips it).

| Main pin | Net | Main pin | Net |
|---|---|---|---|
| 1 | 5V (VCC_OUT) | 11 | GPIO5 |
| 2 | GND | 12 | GPIO6 |
| 3 | U0TX | 13 | USB_DN |
| 4 | U0RX | 14 | USB_DP |
| 5 | GPIO0 | 15 | TP_SDA |
| 6 | GPIO46 | 16 | TP_SCL |
| 7 | GPIO45 | 17 | EC2_B |
| 8 | GPIO42 | 18 | EC2_A |
| 9 | GPIO3 | 19 | GND |
| 10 | GPIO4 | 20 | 5V |

Source: M-p1, B-p1. On the base, U0TX, U0RX, TP_SDA, TP_SCL, EC2_A and EC2_B arrive but connect to nothing (B-p1). They are the only expansion points if you are prepared to solder.

## I2C bus

One bus, GPIO9 SDA / GPIO10 SCL, 2.2k external pull-ups (M-p5). Touch and haptic **share it**: the HAPTIC_SDA/SCL nets are tied to TP_SDA/SCL (M-p1, M-p5).

| Device | 7-bit address | Where | Source |
|---|---|---|---|
| CST816 (code uses CST816S driver) | 0x15 | On the LCD panel flex, via LCD FPC pins 1-4 | xz/taiji_pi_s3.cc:296; spec in 1-Instructions PDF |
| DRV2605L haptic driver (U13) | 0x5A | Main board | M-p1. Address is **UNVERIFIED** from this download (fixed 0x5A per TI datasheet); no code in the download talks to it. |

The demos also enable the ESP32 internal pull-ups (idf/device/lcd_init.c:293). xiaozhi uses I2C port 1, the IDF demo port 0; either works.

## Display

| Item | Value | Source |
|---|---|---|
| Controller | ST77916, QSPI, RGB565 | M-p4; idf/device/lcd_init.c |
| Resolution / offsets | 360x360, no column or row offset | xz/config.h:45-46; scr.h:11-12 |
| QSPI clock in demos | 50 MHz | idf/device/lcd_init.c:259; scr.h:229 |
| Orientation | IDF demo and xiaozhi mirror both X and Y (panel and touch) | idf/device/lcd_init.c:280, 309-310; xz/config.h:26-27 |
| Colour inversion | INI sends 0x21 (INVON); Arduino also calls invertColor(true) | INI; scr.h:465 |
| Backlight | LEDA to 3V3 directly, LEDK through 10R (R30) and AO3400A to GND. GPIO21 high = on, PWM-able (demo: LEDC 20 kHz, 10-bit) | M-p4; idf/device/backlight.c:25-27 |
| LCD FPC | 20-pin FPC-05F-20PH20: 1 TP_INT, 2 TP_RST, 3 TP_SDA, 4 TP_SCL, 5 3V3, 6-7 GND, 8 QSPI_SCL, 9 CS, 10-13 D0-D3, 14 LCD_RST, 15 LCD_TE, 16 3V3, 17 GND, 18 LEDA, 19 LEDK, 20 GND | M-p4 |

**Init sequence.** The vendor original is the INI file (185 writes, ending 0x21, 0x11, 120 ms delay, 0x29). The same sequence is in idf/device/lcd_init.c:56-240, scr.h:42 onwards and xz/taiji_pi_s3.cc:18 onwards. I compared all three against the INI with a script: every command and parameter matches; the code just sends a dummy 0x00 parameter with 0x21, 0x11 and 0x29.

## Rotary encoder and buttons

| Item | Detail | Source |
|---|---|---|
| Encoder EC1 | Footprint SW2, part SSCM110100. A to GPIO2, B to GPIO1, common to GND, 10k pull-ups. Read by the `knob` component (quadrature in software). | M-p1, M-p5 |
| Encoder EC2 | Second footprint SW1, same part. A/B go only to the board FPC and end unconnected on the base. Probably an alternative placement. **UNVERIFIED** which footprint is fitted. | M-p1, B-p1 |
| Knob push | None. The encoder has no switch contact on the schematic. | M-p1 |
| BOOT button | SW1 on base, GPIO0 to GND | B-p1 |
| Reset button | None | M-p5, B-p1 |

## LED ring

| Item | Detail | Source |
|---|---|---|
| Part | WS2812E-1313 | B-p4 |
| Count | **12** (LED2 to LED13; chain starts at LED13). Code uses 13, harmless. | B-p4; idf/led_strip/led_strip_init.c:18,22 |
| Data | GPIO0 through 100R (base R54) | B-p1, B-p4 |
| Colour order | GRB, model WS2812, RMT 10 MHz | idf/led_strip/led_strip_init.c:27,39-40 |
| Supply | VDD_RGB = VCC_OUT (battery/system rail, not regulated 5 V). 3.3 V data into LEDs at up to ~4.4 V supply is within WS2812 spec only marginally: **UNVERIFIED**. | B-p4 |

The white LED1 on the base is the charge status LED (see Power), not part of the ring.

## Haptic

DRV2605L (U13, M-p1). EN tied to 3V3 (always enabled), IN/TRIG tied to GND, so it can only be driven over I2C (internal trigger or real-time playback mode). Outputs go to LRA_P / LRA_N on two spring contacts PP1/PP2, annotated "7.5 mm high, 2 mm diameter" (M-p1), so the LRA motor is off-board in the case and touches the board through pogo pins (**UNVERIFIED** interpretation). Net names say LRA; actuator type not otherwise confirmed. No haptic code exists anywhere in the download.

## Audio out

```
GPIO3 BCK, GPIO45 WS, GPIO42 DIN --22R--> PCM5100A (U25)
  OUTL --470R--+--2.2nF-- GND --> jack pin 2 (LINE_OUT_L)
  OUTR --470R--+--2.2nF-- GND --> jack pin 3 (LINE_OUT_R)
  jack NC contacts 5/6 (LINE_L/LINE_R) --10k each, summed--> NS4150B (U11) --> speaker U24
  GPIO46 PA_CTRL --> NS4150B CTRL (10k pull-down)
```

| Item | Detail | Source |
|---|---|---|
| DAC | PCM5100A, I2S (FMT=GND), no MCLK (SCK pin grounded, so it clocks from BCK), FLT=GND (normal latency), DEMP=GND (off) | B-p5 |
| DAC soft mute | XSMT tied to CODEC_3V3: never muted. There is no DAC mute GPIO. | B-p5 |
| DAC supply | CODEC_3V3 from RT9013-33 LDO (U26) off VCC_OUT; also feeds the mic | B-p5 |
| Amp | NS4150B mono class D, powered from VCC_OUT. CTRL = PA_CTRL = GPIO46, high = on. | B-p5 |
| GPIO46 conflict | pincfg.h:37 and xz/config.h:18 call it AUDIO_MUTE_PIN "low = mute". It is really the speaker amp enable; it does not affect the jack output. idf/music/music_paly.c:152-157 sets it high. | B-p5 |
| Speaker | 2-pin 1.25 mm connector U24 (pin 2 PA_OUTL+, pin 1 PA_OUTL-), 1 nF to GND on each leg | B-p5 |
| Demo I2S setup | Philips, 16-bit, mono default 22050 Hz; xiaozhi uses 24 kHz, both slots | idf/music/music_paly.c:28-31; xz/config.h:9-10 |

**The 3.5 mm-looking socket** is a real 3.5 mm stereo audio jack, CN3, part PJ-342 (schematic annotation C707223, an LCSC part number). It is a **line/headphone output** straight from the DAC through 470R and 2.2 nF filters. Sleeve pins 1 and 4 are GND. Its two normally-closed switch contacts (pins 5, 6) feed the speaker amp, so inserting a plug should disconnect the speaker path (standard PJ-342 behaviour, **UNVERIFIED** on hardware). It is not a mic input, not UART, not a charger input. Which of pins 2/3 is tip versus ring is not stated. Drive is a line-level DAC through 470R, so headphone volume will be modest.

## Microphone

MSM261D4030H1CPM PDM MEMS mic (MIC1). CLK = GPIO5, DATA = GPIO4, L/R pin tied to GND, powered from CODEC_3V3 (B-p5). Code: pincfg.h:40-41, xz/taiji_pi_s3.cc:393-401 (I2S PDM RX).

## Battery and power

```
USB-C VBUS --> ETA6003 charger (U1) --SYS--> POWER_SYS --[SW2 slide switch]--> VCC_OUT ("5V" on main)
                   |                                                           |-> TLV62569 buck -> 3V3 (ESP32, flash, LCD, touch, haptic, SD)
                BAT+ (2-pin 1.25 mm, U23)                                      |-> RT9013-33 LDO -> CODEC_3V3 (DAC, mic)
                                                                               |-> NS4150B amp, WS2812 ring
                                                                               |-> 10k/10k -> BAT_ADC (GPIO6)
```

| Item | Detail | Source |
|---|---|---|
| Charger | ETA6003Q3Q switching charger with power path. ENB, NTC, ENPPB tied to ground (enabled, no thermistor). Charge current set by ISET1 = 2.2k (R40); current value **UNVERIFIED** (no ETA6003 datasheet in the download). | B-p3 |
| Charge LED | STAT drives white LED1 (via 5.1k from POWER_SYS). STAT is **not** connected to the ESP32. | B-p3 |
| Battery connector | U23, HX 1.25 mm 2-pin: pin 1 BAT+, pin 2 GND | B-p2 |
| Power switch | SW2, SSSS810701 slide switch, between POWER_SYS and VCC_OUT. Off cuts everything downstream including the ESP32, so USB does nothing with the switch off. The charger sits before the switch, so it should still charge when off (**UNVERIFIED** on hardware). Left = on, per burn.txt, which also says the switch must be on to flash. | B-p2; burn.txt |
| "5V" rail | Main board's 5V is VCC_OUT, i.e. the charger SYS rail at battery-ish voltage, not a regulated 5 V. The spec sheet "working voltage 5V" refers to USB input. | B-p1 (5V = VCC_OUT), B-p2 |
| 3V3 | TLV62569 buck, 100k/22.1k feedback, 3.31 V | M-p2 |
| BAT_ADC | GPIO6 = VCC_OUT x 10k/(10k+10k) = **half of VCC_OUT**. It measures the system rail after the switch, not BAT+ directly. On battery that is roughly the cell voltage; on USB it is whatever the charger regulates SYS to (**UNVERIFIED**). Use 12 dB attenuation (about 2.1 V at a full cell). The divider draws about 0.2 mA while the switch is on. No battery-reading code exists in the download. | B-p2 |
| USB presence | No GPIO senses VBUS or charge state | B-p1..p3 |

## USB

USB-C receptacle (USB3, TYPEC-304-BCP16) with 5.1k CC pull-downs (device only), D+/D- of both orientations joined, H5VL10B TVS diodes on D+, D- and VBUS (B-p2). D+/D- run over the board FPC straight to GPIO20/GPIO19 (M-p5). **There is no CH340 or other UART bridge on either board**; the CH340 driver in `8-Tool/` is a generic Guition bundle item. UART0 (GPIO43/44) is not reachable from outside.

So all flashing and console traffic goes over the ESP32-S3 native USB. With USB-Serial/JTAG it is a normal serial port; the factory firmware instead runs a USB HID device (1-Instructions PDF; the IDF demo does the same with TinyUSB in idf/knob/knob.c), which is why the manuals say to turn off HID in settings or hold BOOT while powering on to flash. The IDF demo's sdkconfig sets the console to UART0 (`CONFIG_ESP_CONSOLE_UART_NUM=0`), which goes nowhere, so its logs are invisible unless you switch the console to USB-Serial/JTAG.

## Storage and RF

| Item | Detail | Source |
|---|---|---|
| Flash | W25Q128JVPIQ, 16 MB, 3.3 V, external on main board | M-p5 |
| PSRAM | 8 MB octal, inside ESP32-S3R8; demo runs it OCT 80 MHz | M-p5; Demo_idf/demo/sdkconfig |
| microSD | TF-018 socket, 4-bit SDMMC, 10k pull-ups on CMD/D0-D3, 3V3, card-detect pin not connected | M-p3 |
| Crystal | 40 MHz | M-p5 |
| Antenna | On-board chip antenna ANT1 (CA-C03) with pi match | M-p5 |

## Not on any example pin map

- LCD_TE on GPIO18 (only in the IDF pinconfig, unused).
- DRV2605L haptic driver: no pin map entry or code anywhere.
- BAT_ADC on GPIO6: only in the IDF pinconfig, no code.
- Charger STAT: hardware LED only.
- EC2_A/EC2_B: second encoder footprint, not on any GPIO.
- U0TX/U0RX and the I2C bus on the base-side FPC: unused there.
- Mic L/R strap (GND) and the jack-switch speaker cut-off.

## Open questions

- Which encoder footprint (SW1 or SW2) is populated, and whether the part has detents matching one quadrature step.
- LRA motor location and resonant frequency (needed for DRV2605L auto-calibration).
- ETA6003 charge current and SYS voltage on USB, which affects what BAT_ADC reads while charging.
- Whether the PJ-342 switch really cuts the speaker on plug insertion.
