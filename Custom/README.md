# Mystrix × Home Assistant: audit and plan

Written 2026-10-12. This is the hand-over for the next session, which will have the board plugged into this PC.
Branch `ha-controller` starts from vendor `upstream/main` @ `e1a4c080` (nightly-20260925, after V3.2.0).
Vendor rules for working in this repo are in `/AGENT.md`; follow them.

## TL;DR

- **Your custom work is small and fully captured.** It's 4 commits (Dec 9–15, 2024, about 525 lines): one app, a test echo server and two build-wiring lines. Separately there's one Home Assistant automation. The original is preserved at tag **`legacy/ha-wifi-2024`** (= fork `main` @ `6950495`). A copy with the Wi‑Fi password stripped is in `Custom/legacy-2024/`, alongside the HA automation export.
- **The vendor has moved a long way** (2.6-dev → 3.2.0 + nightly; 1,100 commits; full restructure). It **still has no Wi‑Fi**: `Devices/Mystrix1/Drivers/WIFI.cpp` is a commented-out stub. Your prototype is still the only Wi‑Fi path, but it's a reference rather than a foundation.
- **Recommendation: keep MatrixOS as the board-support layer and build one Home Assistant controller app on 3.x.** Make it boot straight into that app so the device *feels* like custom firmware. Don't go to ESPHome or bare ESP-IDF; see "Do we even need this OS?" below.

## Hardware

| | |
|---|---|
| Product | **Mystrix** (vendor family `Devices/Mystrix1`). `Mystrix2` is a different, newer 2026 product (Pro/Ultra with a coprocessor) and isn't yours. |
| Revision | Most likely **Rev C** (`REVC` config in `Devices/Mystrix1/Variants/Mystrix/RevC/Config.h`; "C2" is probably how it's printed). The model and revision are burned into eFuse and logged at boot (`Factory config - Mystrix <model> <rev>`). **Confirm in Phase 0.** |
| MCU module | ESP32-S3-MINI-1 (FCC ID `2AC7Z-ESPS3MINI1`; 2AC7Z is Espressif's grantee code) |
| Flash / RAM | 8 MB flash (the vendor partition table spans 0x800000) → the **-N8 variant, no PSRAM** (the vendor sdkconfig has no SPIRAM). That's about 512 KB SRAM shared by Wi‑Fi, BLE MIDI, the UI and (if used) MicroPython. Prefer plain HTTP on the LAN over TLS. |
| I/O | 8×8 pressure-sensing (FSR) key grid scanned via ADC, FN key, 16-segment touch bar, WS2812 LEDs (GPIO38, 24-bit colour), microSD (4-bit SDMMC), native USB (TinyUSB: MIDI/HID/CDC/MSC), BLE MIDI |
| Flashing | UF2 bootloader partition (`uf2` @ 0x7C0000); `MatrixOS::SYS::Bootloader()` reboots into it. Rollback image for the 2024 firmware: `build/Mystrix/MatrixOS-Mystrix.uf2` (Dec 15, 2024, ESP-IDF v5.3.2; untracked, local only, covered by the nightly backup) |

## What you built in 2024

How it behaves (your description, confirmed by the code): a white app tile → it joins home Wi‑Fi → an 8×8 rainbow (64 hues). **Press** sends that colour to the Hue Go. **Hold** opens a shade sub-page for that hue; picking a shade sends it and returns. FN tap closes the menu; FN hold exits the app. You liked the press-and-hold sub-menu pattern; keep it.

| Piece | Where (tag `legacy/ha-wifi-2024`) | Notes |
|---|---|---|
| `WebExample` app | `applications/WebExample/WebExample.{cpp,h}` | ESP-IDF Wi‑Fi station + an AP scan (debug) + `esp_http_client` POST of `{"r":R,"g":G,"b":B}` to `http://192.168.1.18:8123/api/webhook/rgb_webhook`. It's essentially Espressif's station/scan/http-client examples wired into a MatrixOS UI. No OS changes were needed; the OS build already linked `esp_wifi`. |
| Build wiring | `devices/MatrixBlock6/Applications.h`, `devices/MatrixBlock6/Variants/CMakeLists.txt` | include the app; add `esp_http_client` to `REQUIRES` |
| Echo server | `webserver/server.py` | prints POST bodies, for testing without HA |
| HA automation | `automation.matrix_webhook` (id `1733796841159`), exported to `Custom/legacy-2024/ha_automation_matrix_webhook.yaml` | webhook `rgb_webhook` (local_only) → `light.turn_on` `light.hue_go` `rgb_color`. Still enabled; last fired 2025-01-02. |

The fork's other branches (`CherryUSB`, `FE`, `For-Fun-nighty`, `HID_Improve`, `battery`) are all vendor branches. Nothing of yours is there.

### Problems in the 2024 code (don't carry these forward)

1. **Washed-out shade page = maths bug, not the LEDs.** `new_saturation_by_factor(color, j)` uses factor `j / 2` for j = 0…63. So j=0 is grey (which reads as "white"), j=1 is half-saturated, and every j ≥ 2 is at or past full saturation and gets clamped. **62 of the 64 buttons are effectively the same colour.** That's exactly the "white in one corner, deep colour in the other, no contrast between" you saw. The WS2812s can show far more than that.
2. **The webhook target is stale.** HA is now at `192.168.1.55:8123`; `192.168.1.18` is now the Bambu H2D printer. The firmware on the device today posts to the printer.
3. **Relaunching the app probably reboots the board.** `Setup()` re-runs one-time Wi‑Fi init (`esp_event_loop_create_default`, `esp_netif_create_default_wifi_sta`, `esp_wifi_init`) under `ESP_ERROR_CHECK`. The second call returns `ESP_ERR_INVALID_STATE`, which aborts. Your own comment on that line suspected this.
4. **The UI freezes during HTTP.** The POST runs synchronously inside `OnPress`, so an unreachable host freezes the grid for up to ~5 s.
5. **Credentials are hardcoded,** and the fork `IntegersOfK/MatrixOS` is public, so the Wi‑Fi password has been visible since 2024-12-09. (AJ, 2026-10-12: "not too concerned… just home wifi". Stripped from the copy here; history left alone.)
6. Minor: a blocking AP scan on every launch, the Wi‑Fi password printed to the serial log on connect, metadata still says author "203 Systems", and the header comment is the stock Example text.

**Worth keeping:** the Wi‑Fi bring-up sequence, the `esp_http_client` POST, the press/hold → sub-page UI pattern, and the proof that Wi‑Fi + HTTP coexist with MatrixOS on this board.

## Vendor status (`upstream` = github.com/203-Systems/MatrixOS)

- Fork base `9a062c4` (2024-12-09, 2.6-dev) → V3.0.0 (2025-11-07), V3.1.0, **V3.2.0 (2026-01-29)**, nightlies up to **2026-09-25** (this branch).
- Restructured: `Applications/ Devices/ OS/ Platform/ Library/`. Family `MatrixBlock6` → `Mystrix1`. Apps register via their own `CMakeLists.txt` (`RegisterApplicationClass(...)`) plus `Devices/Mystrix1/ApplicationList.txt`. `Setup(const vector<string>& args)`; `KeyEvent` → `InputEvent`; `UI::SetKeyEventHandler` → `SetInputEventHandler`.
- **The UI API you used is unchanged:** `UI`, `UIButton::SetColor/SetColorFunc/SetSize/OnPress/OnHold`, `AddUIComponent`, `AllowExit`. `SetColorFunc` makes live state colours easy.
- Needs **ESP-IDF v5.3.4** (this PC has v5.3.2 at `C:\Espressif\frameworks\esp-idf-v5.3.2-2`).
- New and relevant: a **MicroPython runtime** (Python apps = `AppInfo.json` + `main.py` **on the SD card**; the `MatrixOS` module exposes LED/Input/UI/NVS/MIDI/HID/Timer/FileSystem, **no networking**). Also: Shell launcher, **MystrixSim** (a host simulator, for testing UI without the board), BLE MIDI, Developer/Companion apps, ArduinoJson in `Library/`.
- After boot the OS runs the boot animation, then the Shell. There's no "start this app" setting, but `OS/System/System.cpp:182` has a commented hook for exactly that.

## Do we even need this OS?

You don't need its music apps, but you do want what it does underneath. Doing "something custom directly with the components" means rewriting all of the following:

- FSR keypad scanning through ADC with per-key force calibration (`KeypadFSR.cpp`, `ForceCalibration` app), plus debounce, hold and aftertouch
- the WS2812 driver with layers, fades and brightness, and the touch-bar protocol
- the UI framework (pages, buttons, hold, sub-UIs, FN handling), which is exactly the sub-menu pattern you like
- NVS settings, USB, UF2 flashing and recovery, and a simulator

| Option | Verdict |
|---|---|
| **A. MatrixOS 3.x + one HomeAssistant app, booting straight into it** | **Recommended.** Adding Wi‑Fi + HA is a few hundred lines; the board support is done and maintained by the vendor. The device behaves like a dedicated HA remote. MIDI etc. stay available but out of the way. |
| B. Keep the 2024 fork | No. It's stuck on pre-3.0, and nothing on it is worth staying for. |
| C. ESPHome | Native HA entities and OTA are tempting, but you'd write custom components for the ADC FSR matrix, touch bar and LED mapping, then build the menu system in YAML lambdas. That's a large rewrite for a small gain: the HA link is the easy part. |
| D. Bare ESP-IDF firmware | The same rewrite cost as C, with no HA integration benefits. |
| E. Vendor Python apps | Nice for editing without reflashing, but they need a C++ networking binding anyway, plus the SD card, and RAM is tight. Revisit once A works. |

Either way, **back up the full flash first** (Phase 0), so any path can be rolled back exactly.

## Target design

Goal: **describe an interface** (pages, what each button does, press vs. hold, sub-menus, colours, live state), and the board runs it.

1. **`Applications/HomeAssistant/`**, a native app. Register it in `ApplicationList.txt` and make it the post-boot app.
2. **Network service inside the app first** (smallest diff against the vendor, so rebases stay easy):
   - Wi‑Fi starts once (static guard), auto-reconnects, and isn't torn down on app exit.
   - A request queue on its own FreeRTOS task, so the UI never blocks.
   - Wi‑Fi SSID/password, HA host and token are stored in **NVS**, provisioned over USB serial (never in source; add a gitignored local header if a dev shortcut is needed).
   - Promote it to `Devices/Mystrix1/Drivers/WIFI.cpp` + an OS wrapper only if a second app needs the network.
3. **HA calls via the REST API**: `POST /api/services/<domain>/<service>` with a long-lived token, plain HTTP to `192.168.1.55:8123` (LAN). Keep a `webhook` action type for anything easier to do as an HA automation.
4. **Layout model** (JSON; ArduinoJson is already vendored):
   `pages[] → buttons[] { x, y, w, h, color | stateColor(entity), onPress: Action, onHold: Action }`;
   `Action = call_service{domain, service, target, data} | goto_page{id} | back | webhook{id, json}`.
   FN tap = back one page; FN hold = exit. Later the touch bar becomes a slider (brightness/volume) for the page's "focused" entity.
   - v1: layout JSON compiled into the firmware (`Custom/layouts/*.json`)
   - v2: layout read from SD card
   - v3: layout pushed over USB/HTTP without reflashing
5. **Live state**: v1 polls `GET /api/states/<entity>` for entities on the visible page; later, subscribe over the WebSocket.
6. **Light colour pages (the redo of the 2024 picker):**
   - Send `hs_color` + `brightness_pct`, or `color_temp_kelvin`, instead of raw RGB; Hue works in those terms.
   - Main colour page: rows 1–6 = 48 hues; row 7 = 8 white temperatures (2200 K → 6500 K, probably the most-used Hue setting); row 8 = on/off, scenes and effects ("modes").
   - Hold a hue → 8×8 sub-page: **x = saturation (8 steps), y = brightness (8 steps)**, with steps spaced perceptually so neighbours look different. The touch bar can do fine brightness.
   - The LED preview won't match the bulb exactly: the Hue gamut is narrower than an RGB LED, especially in deep blue/purple. Tune the preview, not the payload.
7. **Authoring loop:** you describe the layout in plain language → Claude writes the JSON → check it in MystrixSim → flash → adjust.

Open decision for the next session: logic **on the device** (layout JSON contains the service calls; recommended, one file to edit) vs. a **thin client** (the board sends `{page, key, event}` to one webhook and HA automations do the mapping).

## Next session runbook (board plugged into this PC)

**Phase 0: interrogate, read-only, back up first**
1. Find the device: `Get-PnpDevice -PresentOnly | ? FriendlyName -match 'COM|Mystrix|Matrix|Espressif|USB Serial'`. MatrixOS enumerates as USB MIDI/HID/CDC.
2. Open the USB CDC serial port and read the boot log. It confirms the model/revision string, the OS version, and that the 2024 build is what's installed.
3. Get a raw ROM-bootloader connection (UF2 mode alone isn't enough for esptool; check https://matrix.203.io for the button combo, or the vendor's `Tools/`). Then:
   - `esptool.py --chip esp32s3 flash_id` (confirms 8 MB)
   - `espefuse.py --chip esp32s3 summary`, **read-only. Never run any `burn_*` command: eFuses are one-time.**
   - `esptool.py --chip esp32s3 read_flash 0 0x800000 Custom/backups/mystrix-YYYYMMDD.bin`, a full image including NVS settings, for exact rollback. Gitignore `Custom/backups/`.

**Phase 1: toolchain + stock vendor firmware.** Install ESP-IDF v5.3.4 next to v5.3.2. Run `git submodule update --init --recursive` (tinyusb, printf, cb0r, ArduinoJson, micropython). Build with `make DEVICE=Mystrix build-release`; the `Makefile` resolves `Mystrix` to `Devices/Mystrix1/Variants/Mystrix`, and `DEVICE=Mystrix1` also resolves via the family. Flash the stock 3.x and verify the grid, FN, touch bar, LEDs and saved settings. Exit criterion: stock 3.x runs on your board. Rollback: the Phase 0 image.

**Phase 2: port the prototype properly.** Create `Applications/HomeAssistant` with the network service above and NVS provisioning; reproduce "press a colour → Hue Go changes" through `light.turn_on` (REST). Acceptance:
- Hue Go follows the grid
- launching the app 5× in a row doesn't reboot
- HA unreachable → no UI freeze, error colour shown

**Phase 3: layout engine + the new colour pages** (sim first, then the board), and boot straight into the app.

**Phase 4: live state colours, touch-bar sliders, layout from SD card.**

**Phase 5: housekeeping.**
- Retire `automation.matrix_webhook` once the board calls services directly.
- Decide where `ha-controller` lives: the fork is public and GitHub won't make a fork of a public repo private, so use a separate private repo if you want it private.
- Periodically `git fetch upstream` and rebase on vendor releases.

## Pointers

- Original code: `git show legacy/ha-wifi-2024:applications/WebExample/WebExample.cpp` (and `.h`, `webserver/server.py`). The tag's `.h` contains the real Wi‑Fi credentials; don't copy them forward.
- Remotes: `origin` = your public fork `IntegersOfK/MatrixOS`; `upstream` = `203-Systems/MatrixOS`.
- Leftover untracked `core/` and `lib/` dirs in the working tree are 2.x submodule checkouts from `main`. They're harmless and get used again if you check out `main`.
- HA: LAN `http://192.168.1.55:8123`; the token is in the claude-general SOPS vault (`homeassistant.token`).
- Vendor docs: https://matrix.203.io · vendor agent rules: `/AGENT.md`
