#pragma once

// #include "config.h"
#define DEBUG

#define FLASHVERSION 0 //Each Flash data strcture change will cause this to increase
#define FWVERSION_STRING "2.0 Dev" //String(MAJOR_VER)+ "." +MINOR_VER+"." +PATCH_VER+(BUILD_VER == 0)?"":("b"+BUILD_VER) 
#define MAJOR_VER 2
#define MINOR_VER 0
#define PATCH_VER 0
#define BUILD_VER 1 //0 for Release, any other number will repensent beta ver

#ifndef DEBOUNCE_THRESHOLD
inline uint16_t debounce_threshold = 24;
#endif

inline uint16_t hold_threshold = 400;

inline uint8_t brightness_level[8] = {255, 127, 51, 25, 25, 51, 127, 255};


