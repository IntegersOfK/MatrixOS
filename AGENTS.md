# MatrixOS: AJ's Home Assistant controller branch

This is AJ's fork of 203 Systems' MatrixOS, for a **Mystrix (likely Rev C, ESP32-S3-MINI-1)** 8×8 grid controller.
The goal is a dedicated Home Assistant remote with user-described pages, sub-menus and press/hold actions.

Read before doing anything:
1. `Custom/README.md`: the audit of AJ's 2024 work vs. the vendor, the hardware facts, the recommended design and the phased runbook.
2. `AGENT.md`: the vendor's own rules for layers, registration, build (ESP-IDF v5.3.4) and code style.

Hard rules:
- Back up the full flash (`esptool read_flash 0 0x800000 …`) before the first flash of anything.
- Never run `espefuse.py burn_*`. eFuses are one-time.
- No Wi‑Fi/HA credentials or tokens in source. Use NVS, or a gitignored local header.
- `origin` (IntegersOfK/MatrixOS) is **public**; don't push without AJ's OK.
- Work on branch `ha-controller`; the 2024 prototype is preserved at tag `legacy/ha-wifi-2024`.
