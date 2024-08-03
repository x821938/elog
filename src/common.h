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

enum LogFlags {
    FLAG_NONE = 0x00,
    FLAG_NO_TIME = 0x01,
    FLAG_NO_SERVICE = 0x02,
    FLAG_NO_LEVEL = 0x04,
    FLAG_TIME_SIMPLE = 0x08,
    FLAG_TIME_SHORT = 0x10,
    FLAG_TIME_LONG = 0x20,
    FLAG_SERVICE_LONG = 0x40
};

enum LogLevel {
    EMERGENCY = 0,
    ALERT = 1,
    CRITICAL = 2,
    ERROR = 3,
    WARNING = 4,
    NOTICE = 5,
    INFO = 6,
    DEBUG = 7,
    NOLOG = 8
};

enum LogFacility {
    FAC_KERN = 0,
    FAC_USER = 1,
    FAC_MAIL = 2,
    FAC_DAEMON = 3,
    FAC_AUTH = 4,
    FAC_SYSLOG = 5,
    FAC_LPR = 6,
    FAC_NEWS = 7,
    FAC_UUCP = 8,
    FAC_CRON = 9,
    FAC_AUTHPRIV = 10,
    FAC_FTP = 11,
    FAC_NTP = 12,
    FAC_LOG_AUDIT = 13,
    FAC_LOG_ALERT = 14,
    FAC_CLOCK_DAEMON = 15,
    FAC_LOCAL0 = 16,
    FAC_LOCAL1 = 17,
    FAC_LOCAL2 = 18,
    FAC_LOCAL3 = 19,
    FAC_LOCAL4 = 20,
    FAC_LOCAL5 = 21,
    FAC_LOCAL6 = 22,
    FAC_LOCAL7 = 23
};

#endif // COMMON_H
