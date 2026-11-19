# Vendor firmware audit: JC3636K718_V1.1.bin

What the shipped Guition JC3636K718 firmware does, checked statically before we wipe it. Nothing from `mfr-download` was executed.

## Verdict

Low risk. The firmware is a local gadget, not a cloud device. Its only network traffic is NTP time sync and, if the user sets it up, plain HTTP to an AIDA64 PC on the LAN. It has no cloud service, no telemetry, no network firmware update, no Bluetooth stack, no keys or certificates, and the image ships with blank NVS (no factory Wi-Fi credentials or IDs).

The weak points are local only: a setup access point with a published password (`My-Ap` / `12345678`) and an unauthenticated config page while in that mode, Wi-Fi passwords logged in clear on the serial console, and an NTP client that accepts a time reply from anyone on the LAN. None of these matter once we replace the firmware.

The device may be running a different build from this file. Compare against a dump of the actual device before relying on this.

## The image

| Item | Value |
|---|---|
| File | `JC3636K718_V1.1.bin`, 12,324,864 bytes (0xbc1000), flash image from offset 0 |
| Bootloader | ESP-IDF v5.5.1-dirty, built Jan 27 2026, 16 MB DIO 80 MHz, checksum and SHA-256 valid |
| App (app0) | `PROJECT_N7_IDF`, app version "1", built Jan 29 2026 15:41:48, ESP-IDF v5.5.1-dirty, secure version 0, ELF SHA-256 a47f9075...ce91 |
| Branding strings | "N7 Workshop", "Night7 Workshop", "N7 Dial", "N7 Disk", "TAIJI KNOB MODULE" |

The file stops at 0xbc1000, partway into the `assets` partition. Everything after that (rest of assets, coredump) is not in the file.

| Partition | Offset / size | Content in the file |
|---|---|---|
| bootloader | 0x0 | plain ESP-IDF bootloader, ends 0x53a0 |
| partition table | 0x8000 | 7 entries, MD5 present, no "encrypted" flags |
| nvs | 0x9000 / 0x5000 | all 0xFF (blank) |
| otadata | 0xe000 / 0x2000 | all 0xFF (so the bootloader runs app0) |
| app0 | 0x10000 / 0x300000 | the app, about 2 MB used |
| app1 | 0x310000 / 0x300000 | all 0xFF (empty) |
| spiffs | 0x610000 / 0x400000 | all 0xFF (empty, never formatted) |
| assets | 0xa10000 / 0x5e0000 | custom "ASST" container, 3 entries, data only |
| coredump | 0xff0000 / 0x10000 | not in the file; core dump support is not compiled into the app |

## 1. Network endpoints

Every hostname, URL and IP literal in the app, plus how it is used. String references were traced in the disassembly to confirm callers.

| Endpoint | Protocol | When | Notes |
|---|---|---|---|
| time.windows.com, pool.ntp.org, ntp.aliyun.com, cn.pool.ntp.org | NTP, UDP 123 out, local UDP port 1234 bound on all interfaces | In Wi-Fi station mode | A table of four servers. Hand-rolled client, not ESP-IDF SNTP. |
| `http://<Host IP>[:port]/sse` | Plain HTTP GET, Server-Sent Events | Only when the AIDA64 app is used and the user has set "Host IP" | Reads PC sensor data from AIDA64's remote sensor feature. No headers beyond the defaults (User-Agent "ESP32 HTTP Client/1.0"), no auth, no body. |
| `http://<AP IP>` | DHCP option 114 (captive portal URI) | AP setup mode only | Points phones at the local config page. |
| 0.0.0.0, 255.255.255.255, 127.0.0.1 | n/a | n/a | lwIP and settings defaults, not destinations. |

That is the complete list. There are no other domains, no IP literals, and no `http://%s`-style format strings beyond the AIDA64 one. `esp_http_client_init` has exactly one caller in the app: the AIDA64 task.

### OTA

There is no network firmware update in this build.

- The UI string table has "Check update", "Check slave update", "Found new ver:%d.%d.%d", "Update server not found", "No download link" and similar. These sit in a shared, indexed message table used across the vendor's product family.
- The code behind them is absent: no `esp_https_ota`, no `esp_ota_begin` or `esp_ota_write` (none of their log or function-name strings are present), no update URL, and the only HTTP client user is AIDA64. `ESP_ERR_HTTPS_OTA_*` appear only in ESP-IDF's built-in error-name table, which every app carries.
- The only OTA code linked is the rollback check at boot ("CURRENT FIRMWARE: PENDING_VERIFY" then "FIRMWARE SET VALID!").
- HTTPS is possible in principle (mbedTLS and esp-tls are linked as dependencies of the HTTP client), but no CA certificate or certificate bundle is present, so no HTTPS connection could verify a server.
- Images are not signed (see section 5). Updates are by USB flashing.

## 2. Listening services and remote control

| Service | When active | Auth | Detail |
|---|---|---|---|
| Soft AP "My-Ap" | When no Wi-Fi is configured, or the user picks "Reboot to AP mode" | WPA/WPA2-PSK, password `12345678` (hardcoded) | Channel 8, max 4 clients |
| HTTP server (esp_http_server) | AP mode only | None | Three GET routes, plus a 404 handler that redirects to `/` |
| Captive DNS server, UDP 53 | AP mode only | n/a | Answers every query with the AP address |
| NTP reply socket, UDP 1234 | Station mode | None | Accepts any packet of at least 48 bytes as the time. A LAN host could set the clock. Nothing else is parsed. |

HTTP routes (all `HTTP_GET`):

| URI | Does |
|---|---|
| `/` | Serves the built-in "Configure WiFi" page (inline HTML and JS) |
| `/wifilist` | Scans and returns nearby networks as `ssid\|rssi\|security,...` |
| `/configwifi?ssid=..&pwd=..` | Joins that network, saves it to NVS, logs the password to the console |

There is no file upload, no firmware upload, no command or shell endpoint, no telnet, no other TCP listener, no mDNS, no SmartConfig or provisioning service. The `http_auth_basic` and Digest code seen in strings is the HTTP client library's, not server-side auth, and is unused.

Other inputs:

- Wi-Fi can also be set from a `wifi.txt` file on the TF card (line 1 SSID, line 2 password).
- USB (TinyUSB on the OTG port) runs in one of two modes, chosen in settings:
  - HID "N7 Dial": a single report descriptor, Generic Desktop "System Multi-Axis Controller" with a dial and button, as used by the Windows Surface Dial. It is not a keyboard, mouse or consumer-control device, so it cannot type keystrokes.
  - MSC "N7 Disk": exposes the TF card to the PC as a drive.
  - A "UAC" (USB audio) mode is in the menus but logs "UAC mode is not allowed". There is no USB CDC class in TinyUSB here. The console uses the chip's own USB-Serial-JTAG or UART.
- Bluetooth: menus offer "BLE mode", "Bluetooth Audio", "Clear BLE HID pair" and "BT scan", but no Bluetooth controller or host stack is linked (no Bluedroid, NimBLE or controller library). These menu items cannot do anything in this build.
- Apps present: pomodoro timer, spectrum, AIDA64 monitor, MJPEG player, music player (MP3/FLAC from TF card), text reader, clock themes, G-meter, settings. A "WEATHER" app name, "City code" and "API KEY" messages exist, but no code makes any weather request.

## 3. Data sent off the device, cloud services, secrets

- No cloud or vendor service: no xiaozhi, Baidu, Tuya, Aliyun IoT, MQTT, WebSocket or similar. The only Chinese host is the NTP server ntp.aliyun.com.
- No telemetry. The NTP request and the AIDA64 GET carry no device ID, MAC or serial. The MAC and chip-ID format strings (`%02x%02x%02x%02x%02x%02x`, `%016llx`) are used only for local display.
- Hardcoded credentials: only the AP password `12345678`.
- Keys and certificates: none. The only PEM text is mbedTLS's header strings, and there is no DER certificate in the image.
- Privacy: the `/configwifi` handler logs "Try to connect to : <ssid> ,use password : <password>" to the serial console.

## 4. NVS, SPIFFS and assets

- NVS (0x9000): all five pages are blank (0xFF). No namespaces, no keys, no factory Wi-Fi credentials, no activation data.
- At runtime the app would create:
  - namespace `GODVISION` for settings (keys `c001` to `c026`, `pod`, `hid`)
  - namespace `WIFIDATA` for the saved SSID and password, unencrypted (no NVS encryption)
  - `ACTKEY` and `devname` strings also appear as NVS names. Nothing in the image fills them.
- SPIFFS (0x610000): blank, not even formatted. The app mounts `/spiffs` but this image ships nothing there.
- assets (0xa10000): the vendor's own "ASST" format (manifest with name, offset, size, CRC). Three entries: `SYHT_BOLD_16.bin` (an LVGL font, about 1.7 MB), `gear12.bin` and `gear36.bin` (images). These are data read by the UI. Nothing executable, nothing odd.
- app1 is empty. There is no second firmware hiding in the other OTA slot.

On a used device, NVS will hold the owner's Wi-Fi password and settings, and SPIFFS may hold runtime files. The TF card holds `wifi.txt` (plaintext Wi-Fi password, if used) plus media.

## 5. Secure boot and flash encryption

- The bootloader contains no secure boot or flash encryption code (no related strings, no signature block).
- The app has no Secure Boot v2 signature block appended. Its secure version is 0, and it has no flash-encryption or eFuse-burning code beyond normal eFuse reads (MAC, ADC calibration, chip revision).
- No partition is marked encrypted.
- Both images accept chip revision v0.0 to v0.99 and eFuse block revision 0.0 to 1.99.

So this firmware expects a chip with secure boot and flash encryption off, and it never turns them on. If the real device boots this image, its eFuses must be in that state. Confirm read-only with `espefuse --port <port> summary` (look at `SECURE_BOOT_EN`, `SPI_BOOT_CRYPT_CNT`, `DIS_DOWNLOAD_MODE`, `DIS_USB_SERIAL_JTAG`, and the key purpose fields) before wiping. Do not burn anything.

## Clean wipe

1. Optional: dump the whole flash first for comparison: `esptool read-flash 0 0x1000000 device.bin`.
2. Run `esptool erase-flash`. This clears all 16 MB, including NVS (Wi-Fi password, settings), SPIFFS, the second app slot, the tail of the assets partition and the coredump area. Writing the vendor image back would not do this, because the image stops at 0xbc1000 and leaves whatever is above that untouched.
3. Nothing is stored outside the SPI flash. RTC memory and PSRAM clear on power-off. eFuses hold only factory calibration and the MAC, and this firmware never writes them.
4. Wipe or reformat the TF card separately if it was used. It may hold `wifi.txt` with a plaintext Wi-Fi password.
5. If the owner joined the device to their home Wi-Fi, consider that password exposed to anyone who had the device or its serial console. The risk is small, but a change is cheap.

## Method

- `esptool image-info` on the bootloader and app. Partition table and NVS were parsed with a small script, and every region of the image was checked for blank (0xFF) pages.
- `strings` on all regions. The app was split into its segments and disassembled with the Xtensa objdump. Every `l32r` literal was resolved to its string, which shows which function uses which string, the HTTP route table, the AP config struct (SSID, password, channel, auth mode) and the socket ports.
- Callers of `esp_http_client_init` and the HTTP server's register function were enumerated by scanning literal pools.
- The USB HID report descriptor was decoded from the raw bytes.

Limits: this is static analysis of one file. Code reached only through indexed tables was judged by which libraries and call sites exist, not by tracing every branch. The device may run a different build. Work files are in the session scratchpad and are not kept.
