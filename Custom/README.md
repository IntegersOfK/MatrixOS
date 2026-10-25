# Mystrix × Home Assistant: audit and plan

Written 2026-10-12, updated 2026-10-24. **To pick up the work, start at "Where things stand" below.**
Branch `ha-controller` starts from vendor `upstream/main` @ `e1a4c080` (nightly-20260925, after V3.2.0).
Vendor rules for working in this repo are in `/AGENT.md`; follow them.

## TL;DR

- **Your custom work is small and fully captured.** It's 4 commits (Dec 9–15, 2024, about 525 lines): one app, a test echo server and two build-wiring lines. Separately there's one Home Assistant automation. The original is preserved at tag **`legacy/ha-wifi-2024`** (= fork `main` @ `6950495`). A copy with the Wi‑Fi password stripped is in `Custom/legacy-2024/`, alongside the HA automation export.
- **The vendor has moved a long way** (2.6-dev → 3.2.0 + nightly; 1,100 commits; full restructure). It **still has no Wi‑Fi**: `Devices/Mystrix1/Drivers/WIFI.cpp` is a commented-out stub. Your prototype is still the only Wi‑Fi path, but it's a reference rather than a foundation.
- **Recommendation: keep MatrixOS as the board-support layer and build one Home Assistant controller app on 3.x.** Make it boot straight into that app so the device *feels* like custom firmware. Don't go to ESPHome or bare ESP-IDF; see "Do we even need this OS?" below.

## Where things stand (2026-10-24)

Phases 0 and 1 are done. Phase 2's network layer is **verified on the board** (2026-10-24, on the second laptop): Wi‑Fi connects, `GET /api/` returns 200 with the device token, and the centre button toggles the Hue Go.

**On the board now:** a dev build of vendor 4.0.0-dev, plus the `Home Assistant` app (a light-blue tile in the Shell). The app opens a network test page:

| Key | What it does |
|---|---|
| (0,0) | Wi‑Fi status: green = connected, breathing amber = connecting, red = failed, grey = no config. Hold it to scroll the detail (e.g. the disconnect reason). |
| (1,0) | `GET /api/`: green = 200, orange = HTTP error (401 = missing/bad token), pink = no answer. Hold it to scroll the result. |
| 2×2 at (3,3) | `light/toggle` on `HA_TEST_ENTITY` (`light.hue_go`). The result flashes for 1.5 s. |

The build on the board has the 2024 Wi‑Fi credentials (SSID `AAA`: the ESP32-S3 is 2.4 GHz only, so not `AAA-5G`) and the dedicated "Mystrix" HA token.

**The crash that blocked Phase 2, now fixed:** opening the app rebooted the board as soon as Wi‑Fi started connecting. The cause was `***ERROR*** A stack overflow in task sys_evt`. Dev builds compile everything at `-O0` (`CONFIG_COMPILER_OPTIMIZATION_NONE`), including ESP-IDF's own esp_netif/Wi‑Fi event handlers, and the 2304-byte default `sys_evt` stack is too small for that. The fix is `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096` in `Devices/Mystrix1/sdkconfig.development`; release builds (`-O2`) are untouched. `HANet`'s Wi‑Fi event handler also no longer calls `MLOG`, which is heavy (std::string building + vprintf + CDC printf). The 2024 prototype's reboots may well have had the same cause.
- Changing an `sdkconfig.*` defaults file doesn't update an existing `build/Mystrix/sdkconfig`. Edit that too, or delete it to regenerate (full rebuild).

**Code** (`Applications/HomeAssistant/`):
- `HANet.{h,cpp}`: the Wi‑Fi station plus a queued HTTP client.
  - Wi‑Fi starts once, auto-reconnects (1–30 s backoff) and survives app exit.
  - HTTP runs on its own task (`haNet`, 8 KB stack). The UI queues requests with a tag and polls for responses, so it never blocks.
- `HomeAssistant.{h,cpp}`: the test page above. On launch it logs the reset reason, free heap and the last panic, if any.
- `PanicCapture.{h,cpp}`: a diagnostic. It wraps `esp_panic_handler` (`-Wl,--wrap`) and keeps the panic reason, abort message and backtrace in RTC memory across the reboot.
  - It exists because the panic handler prints to UART0 (on the buried FLASH header), and the flash core dump never lands (`esp_core_dump_image_check` always fails after a panic).
  - To read a crash: reproduce it, open the app once more, read the USB log, then decode the PCs with `xtensa-esp32s3-elf-addr2line -pfiaC -e build/Mystrix/MatrixOS-Mystrix.elf <pc>…`.
- `HAConfig.h` uses `HAConfig.local.h` (gitignored, real values) if it exists, otherwise `HAConfig.example.h` (committed, blank values).
  - CMake sets `HA_LOCAL_CONFIG` when the local file exists, so creating that file forces a rebuild. `__has_include` didn't.
  - The long-term plan is still NVS provisioning over USB.
- Build wiring: `Devices/Mystrix1/ApplicationList.txt`, `esp_http_client` in `Devices/Mystrix1/CMakeLists.txt`, and the `sys_evt` stack in `Devices/Mystrix1/sdkconfig.development`.
- `Custom/tools/`: helper scripts (they need `pip install hidapi pyserial`).
  - `mxquery.py`: read-only HID info queries.
  - `uf2decode.py`: turns a UF2 into a bin and prints the app descriptor.
  - `seriallog.py`: serial capture.
  - `phase0-backup.ps1`: raw esptool dump, if one is ever needed.

**Setting up another PC:**
1. **ESP-IDF v5.3.4** in `C:\Espressif\frameworks\esp-idf-v5.3.4`. Run its `install.ps1` with `$env:IDF_TOOLS_PATH='C:\Espressif'`: `install.bat` failed from agent shells. Then run `git submodule update --init --recursive`.
   You also need **GNU Make**: `winget install ezwinports.make`. Its `make.exe` lands in `%LOCALAPPDATA%\Microsoft\WinGet\Packages\ezwinports.make_*\bin`; add that to `PATH` if a new shell still can't find `make`.
2. **Config:** copy `HAConfig.example.h` to `HAConfig.local.h` and fill it in. The Wi‑Fi values used so far are the 2024 ones: `git show legacy/ha-wifi-2024:applications/WebExample/WebExample.h` (`WEB_EXAMPLE_WIFI_SSID` / `_PASS`).
3. **Build:** load the environment with `$env:IDF_TOOLS_PATH='C:\Espressif'; . C:\Espressif\frameworks\esp-idf-v5.3.4\export.ps1`. Then run `make DEVICE=Mystrix build-dev` and `make DEVICE=Mystrix uf2`, which gives `build/Mystrix/MatrixOS-Mystrix.uf2`.
4. **Flash:** first get into UF2 mode, either way:
   - hold FN at plug-in, or
   - without unplugging: send HID report `[0xCB, 0x40, 0, …]` (64 bytes) on the collection `mxquery.py` finds.

   Then copy the `.uf2` onto the `MATRIXDFU` drive. Don't use `idf.py flash`.

**Open items, in order:**
1. **Phase 2 acceptance checks** (below): the basic toggle works; run through the rest.
2. **Downstairs layout: waiting on AJ.** The light list has been pulled and given to AJ. HA has no "downstairs" floor (the floors are Upstairs, Ground Floor and Basement); "downstairs" means the Hue group `light.downstairs` (29 lights: Living Room A1–B4, Kitchen D2–D6/E2–E3 plus strips, Dining, Front Door, Powder Room, E1 and the Festavia string). AJ still has to confirm the scope (include Backyard, GLORB or the Basement?) and place the lights on the 8×8 grid.
   - Query: `POST /api/template` with `floors()`, `floor_areas()`, `area_name()`, `floor_name()` over `states.light`. The device token works for it.
   - Not every light is colour-capable: `light.tube_lights_*_basement`, `light.doorbell_button`, `light.red_alarm` and the Ensuite overhead switch are on/off only, and GLORB (`light.wled`) is RGB only.
3. **UI design (Phase 3): AJ is deciding.** The direction so far:
   - The main view is a ground-floor plan with one key per light; press toggles it.
   - Each light also needs colour, brightness and presets.
   - AJ thinks this unit has no touch bar, so brightness has to live on the grid.
   - Keeping the vendor OS (option A) is still the recommendation, but AJ wants to revisit it.
4. **Serial logging works.** Dev-build `MLOG` reaches USB CDC (it was COM6 on the second laptop). It starts a few seconds into boot, so the earliest boot lines (e.g. `Loading config for …`) are missed.

**Only on a dev PC, not in git:**
- `HAConfig.local.h` (on both laptops now)
- `Custom/backups/20261017-uf2/` (the `CURRENT.UF2` app backup)
- `build/Mystrix-2024/` (the 2024 rollback UF2)

## Hardware

| | |
|---|---|
| Product | **Mystrix** (vendor family `Devices/Mystrix1`). `Mystrix2` is a different, newer 2026 product (Pro/Ultra with a coprocessor) and isn't yours. |
| Model | **Mystrix Standard, `MX1S`** (confirmed 2026-10-17 over the HID command API). Not the Pro: for an `S` model the firmware turns velocity (pressure) sensitivity **off** and drops the underglow partition, so keys act as on/off. Serial `8C6C219401D0A831369F3EEEC1C4B569`. |
| Revision | Most likely **Rev C** (`REVC` config in `Devices/Mystrix1/Variants/Mystrix/RevC/Config.h`; "C2" is probably how it's printed). Still unconfirmed: the revision is only logged on the boot serial log (`Loading config for …`), not exposed over HID. |
| MCU module | ESP32-S3-MINI-1 (FCC ID `2AC7Z-ESPS3MINI1`; 2AC7Z is Espressif's grantee code) |
| Flash / RAM | 8 MB flash (the vendor partition table spans 0x800000) → the **-N8 variant, no PSRAM** (the vendor sdkconfig has no SPIRAM). That's about 512 KB SRAM shared by Wi‑Fi, BLE MIDI, the UI and (if used) MicroPython. Prefer plain HTTP on the LAN over TLS. |
| I/O | 8×8 FSR key grid scanned via ADC (binary on the `S` model), FN key, 16-segment touch bar (per the vendor config; AJ believes this unit has none), WS2812 LEDs (GPIO38, 24-bit colour), microSD (4-bit SDMMC), native USB (TinyUSB: MIDI/HID/CDC/MSC), BLE MIDI |
| Flashing | UF2 bootloader partition (`uf2` @ 0x7C0000), "Matrix Bootloader V1.2-1-gb063d90" (Mar 27 2023). Enter it by holding FN while plugging in, from Settings, or with `MatrixOS::SYS::Bootloader()`. A UF2 copy rewrites only the app partition (bootloader, NVS and the UF2 loader are untouched), so FN-at-plug-in recovery always survives a bad app. No BOOT button: ROM download mode needs the 6-pin FLASH header (1 GND, 2 BOOT/IO0, 3 TX0, 4 RX0, 5 Reset, 6 VBUS; vendor schematic `203-Systems/Matrix-Block6-Hardware/PCB/Matrix.pdf` p.3), which is buried under the panel. The 5 pins visible near the bottom are the MATRIX MOD header (GND, TX1, ModGPIO, RX1, VBUS), not FLASH. |
| Rollback (2024 firmware) | `Custom/backups/20261017-uf2/CURRENT.UF2` (read off the board) or `build/Mystrix-2024/MatrixOS-Mystrix.uf2`. They're byte-identical: `V2.5-22-g3b7d5ee-dirty`, built Dec 15 2024 17:27:33, IDF v5.3.2, app sha256 `1407b2d4…341d`. Both are gitignored and local only. |

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

- Fork base `9a062c4` (2024-12-09, 2.6-dev) → V3.0.0 (2025-11-07), V3.1.0, **V3.2.0 (2026-01-29)**, nightlies up to **2026-09-25** (this branch). Note that this nightly reports itself as **4.0.0** (`OS/System/Parameters.h`): it's 4.0-dev, not 3.x. See the wiki's "Migration from 3 to 4" page when the APIs differ from this doc.
- Restructured: `Applications/ Devices/ OS/ Platform/ Library/`. Family `MatrixBlock6` → `Mystrix1`. Apps register via their own `CMakeLists.txt` (`RegisterApplicationClass(...)`) plus `Devices/Mystrix1/ApplicationList.txt`. `Setup(const vector<string>& args)`; `KeyEvent` → `InputEvent`; `UI::SetKeyEventHandler` → `SetInputEventHandler`.
- **The UI API you used is unchanged:** `UI`, `UIButton::SetColor/SetColorFunc/SetSize/OnPress/OnHold`, `AddUIComponent`, `AllowExit`. `SetColorFunc` makes live state colours easy.
- Needs **ESP-IDF v5.3.4**: installed 2026-10-17 at `C:\Espressif\frameworks\esp-idf-v5.3.4` (Python env `idf5.3_py3.13_env`). Load it with `$env:IDF_TOOLS_PATH='C:\Espressif'; . C:\Espressif\frameworks\esp-idf-v5.3.4\export.ps1`. v5.3.2 is still alongside.
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

**Phase 0: done 2026-10-17, using the UF2 path instead of a full raw dump (AJ's call).**
- What happened: the board has no reachable BOOT button (see Hardware), so there was no esptool `read_flash`. Instead it was put in UF2 mode (FN at plug-in) and `CURRENT.UF2` (the app partition, 4 MiB from the app start) was copied to `Custom/backups/20261017-uf2/`. `Custom/tools/uf2decode.py` decodes it to `current-app.bin`, and it matched the 2024 build byte-for-byte.
- Not backed up: NVS (settings and the old force calibration, which is moot on an `MX1S`), the bootloader and the partition table. A UF2 flash never writes those.
- If a full raw dump is ever needed: bridge FLASH pin 2 to pin 1 at plug-in, then run `Custom/tools/phase0-backup.ps1` (read-only esptool/espefuse; **never `burn_*`**).
- The installed firmware prints nothing on CDC while idle. Use `Custom/tools/mxquery.py` (raw HID report `0xCB`; on Windows only the second `0xFF00` collection accepts writes) for read-only OS version, model, serial and current app.

**Phase 1: toolchain + stock vendor firmware.** Flashed 2026-10-17. ESP-IDF v5.3.4 is installed and the submodules are initialized. `make DEVICE=Mystrix build-release` then `make DEVICE=Mystrix uf2` built the stock 4.0.0-dev firmware (app 0x2a9b80 bytes, 28% of the partition free, UF2 sha256 `9fa375bc…e1da`), which was copied onto the `MATRIXDFU` drive. The board came back as `Mystrix` / `MX1S`, OS 4.0.0, running the Shell. AJ checked it by hand: all keys, FN and the LEDs work; AJ believes this unit has no touch bar. Rollback: hold FN at plug-in, then copy `build/Mystrix-2024/MatrixOS-Mystrix.uf2` onto the drive.

**Phase 2: port the prototype properly. In progress:** the network layer and test page are written and flashed but not yet verified (see "Where things stand"). Config is a gitignored header for now; NVS provisioning comes later. Create `Applications/HomeAssistant` with the network service above; reproduce "press a colour → Hue Go changes" through `light.turn_on` (REST). Acceptance:
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
