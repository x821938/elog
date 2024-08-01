// common.h is a workaround for importing littleFS and sdfat libraries in correct order.
// This is to avoid a lot of compilation warnings.

#ifndef COMMON_H
#define COMMON_H

#ifndef LOGGING_SPIFFS_DISABLE
#include <littleFS.h>
#endif

#ifndef LOGGING_SD_DISABLE
#include <SdFatConfig.h>
#include <sdfat.h>
#endif

struct LogLineEntry {
    uint32_t timestamp;
    uint8_t logId;
    uint8_t logLevel;
    Stream* internalLogDevice;
    const char* logMessage;
};

#endif // COMMON_H
