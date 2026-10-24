#pragma once

// HAConfig.local.h holds the real Wi-Fi and HA credentials and is gitignored.
// Without it the build still succeeds and the app reports "not configured".
// HA_LOCAL_CONFIG is set by CMakeLists.txt when the local file exists.
#if defined(HA_LOCAL_CONFIG)
#include "HAConfig.local.h"
#else
#include "HAConfig.example.h"
#endif
