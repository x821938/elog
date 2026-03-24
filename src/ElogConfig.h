// ElogConfig.h - Feature enable flags for the Elog library
//
// For Arduino IDE users: uncomment the features you want to use.
// For PlatformIO users: you can set these via build_flags in platformio.ini
//   (e.g. build_flags = -D ELOG_SD_ENABLE) instead of editing this file.
//
// IMPORTANT: Do NOT use #define in your sketch to enable features.
// The library source files are compiled separately and will not see
// those defines. Edit this file or use build_flags instead.

#ifndef ELOG_CONFIG_H
#define ELOG_CONFIG_H

// Uncomment to enable SD card logging
// #define ELOG_SD_ENABLE

// Uncomment to enable SPIFFS/LittleFS logging
// #define ELOG_SPIFFS_ENABLE

// Uncomment to enable Syslog (UDP) logging
// #define ELOG_SYSLOG_ENABLE

// Uncomment to enable the LogTimer utility
// #define ELOG_TIMER_ENABLE

#endif // ELOG_CONFIG_H
