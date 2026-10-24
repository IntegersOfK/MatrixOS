#pragma once

// Template for the Home Assistant controller's local config.
// Copy this file to HAConfig.local.h (gitignored) next to it and fill in the values.
// Never put real credentials in this file: the repo is public.

#define HA_WIFI_SSID ""
#define HA_WIFI_PASSWORD ""

// Plain HTTP on the LAN
#define HA_HOST "192.168.1.55"
#define HA_PORT 8123

// Long-lived access token (HA profile -> Security -> Long-lived access tokens)
#define HA_TOKEN ""

// Light used by the network test page
#define HA_TEST_ENTITY "light.hue_go"
