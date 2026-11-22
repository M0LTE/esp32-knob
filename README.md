# esp32-knob

Board bring-up for the Guition JC3636K718C_I_Y1: a 1.8 inch 360x360 round touch display on an ESP32-S3R8 (16 MB flash, 8 MB PSRAM), with an aluminium rotary bezel, a 12-LED ring, a haptic motor, a speaker plus 3.5 mm line out, a PDM microphone, a TF card slot and a Li-ion charger in the base.

## What's here

| Path | What |
|------|------|
| `docs/hardware.md` | Pin map, I2C devices, audio, power and USB, each fact traced to the schematic or vendor code |
| `docs/vendor-firmware.md` | Audit of the firmware the board shipped with: what it talks to, what it listens on, what was left in flash |
| `hello/` | ESP-IDF 5.5 + LVGL 9 bring-up app: knob drives an arc, the LED ring and haptic ticks; tapping the glass gives a ripple, a chime and a click; the mic pulses the centre |
| `dumps/` (not in git) | Factory flash backup and eFuse summary; kept privately, not published (the vendor firmware is not ours to redistribute) |

The manufacturer's download is kept locally in `mfr-download/` and is not committed (448 MB, third-party).

## Factory state and the wipe

- eFuses were clean: no secure boot, no flash encryption, no keys, download mode not disabled. Nothing was burned.
- The board ran `PROJECT_N7_IDF` built 13 Jan 2026, a slightly older build than the 29 Jan image in the download. The differences are minor Wi-Fi reconnect changes; no new hosts or services.
- Full 16 MB flash was read, verified on-chip (MD5), then the whole chip was erased and read back as all 0xFF before flashing our app.
- The factory backup (sha256 `bc344f3a91884c2340b7387c2324fc72cf1e899c0a0ea4aed704ff09ab96bbe3`) can be restored with `esptool write-flash 0 factory-flash.bin`.

## Build and flash

```
. ~/esp/esp-idf-v5.5.1/export.sh
cd hello && idf.py build
esptool --port /dev/ttyACM0 write-flash --flash-mode keep 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/knob_hello.bin
```

The USB-C port goes straight to the ESP32-S3's built-in USB Serial/JTAG, so no driver is needed. The base's slide switch must be on (left). To force download mode hold BOOT while switching on.

Don't pass `--flash-mode qio` to esptool: it rewrites the bootloader header, the ROM then can't read this board's flash and the chip resets every second before our code runs. Leave the header as built (`keep`); the bootloader switches to QIO itself.

The app waits 5 s after power-on before touching any hardware, and halts on a crash instead of rebooting, so USB always comes up for esptool and the crash log.


## Built on this

[esp32-knob-pages](https://github.com/M0LTE/esp32-knob-pages) is a desk status display for this board: hand-drawn LVGL pages (network, packet station, modem, energy, rooms, aircraft, live waterfall with audio, a 2.4 GHz scope that uses the board's own Wi-Fi radio as a receiver), alerts, data sources that run on the knob, a settings page served by the knob, over-the-air updates with rollback, and a desktop simulator. Its [docs/memory.md](https://github.com/M0LTE/esp32-knob-pages/blob/main/docs/memory.md) records where this board's internal RAM goes with all of that running.

## Licence

AGPL-3.0-or-later. See [LICENSE](LICENSE).
