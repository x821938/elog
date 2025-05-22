// LogCommon.h is a workaround for importing littleFS and sdfat libraries in correct order.
// This is to avoid a lot of compilation warnings.

#ifndef ELOG_COMMON_H
#define ELOG_COMMON_H

#ifdef ELOG_SPIFFS_ENABLE
#include <LittleFS.h>
#endif

#ifdef ELOG_SD_ENABLE
#include <SdFatConfig.h>
#include <SdFat.h>
#endif

struct LogLineEntry {
    uint32_t timestamp;
    uint8_t logId;
    uint8_t logLevel;
    uint8_t lastMsgLogLevel;
    Stream* internalLogDevice;
    const char* logMessage;
};

enum LogFlags {
    ELOG_FLAG_NONE = 0x00,
    ELOG_FLAG_NO_TIME = 0x01,
    ELOG_FLAG_NO_SERVICE = 0x02,
    ELOG_FLAG_NO_LEVEL = 0x04,
    ELOG_FLAG_TIME_SIMPLE = 0x08,
    ELOG_FLAG_TIME_SHORT = 0x10,
    ELOG_FLAG_TIME_LONG = 0x20,
    ELOG_FLAG_SERVICE_LONG = 0x40
};

// Don't forget to update logLevelStrings in LogFormat.cpp
// and syslogLevel in LogSyslog.cpp
enum LogLevel {
    ELOG_LEVEL_ALWAYS = 0,
    ELOG_LEVEL_EMERGENCY = 1,
    ELOG_LEVEL_ALERT = 2,
    ELOG_LEVEL_CRITICAL = 3,
    ELOG_LEVEL_ERROR = 4,
    ELOG_LEVEL_WARNING = 5,
    ELOG_LEVEL_NOTICE = 6,
    ELOG_LEVEL_INFO = 7,
    ELOG_LEVEL_DEBUG = 8,
    ELOG_LEVEL_TRACE = 9,
    ELOG_LEVEL_VERBOSE = 10,
    ELOG_LEVEL_NOLOG = 11,
    ELOG_NUM_LOG_LEVELS = ELOG_LEVEL_NOLOG
};

enum LogFacility {
    ELOG_FAC_KERN = 0,
    ELOG_FAC_USER = 1,
    ELOG_FAC_MAIL = 2,
    ELOG_FAC_DAEMON = 3,
    ELOG_FAC_AUTH = 4,
    ELOG_FAC_SYSLOG = 5,
    ELOG_FAC_LPR = 6,
    ELOG_FAC_NEWS = 7,
    ELOG_FAC_UUCP = 8,
    ELOG_FAC_CRON = 9,
    ELOG_FAC_AUTHPRIV = 10,
    ELOG_FAC_FTP = 11,
    ELOG_FAC_NTP = 12,
    ELOG_FAC_LOG_AUDIT = 13,
    ELOG_FAC_LOG_ALERT = 14,
    ELOG_FAC_CLOCK_DAEMON = 15,
    ELOG_FAC_LOCAL0 = 16,
    ELOG_FAC_LOCAL1 = 17,
    ELOG_FAC_LOCAL2 = 18,
    ELOG_FAC_LOCAL3 = 19,
    ELOG_FAC_LOCAL4 = 20,
    ELOG_FAC_LOCAL5 = 21,
    ELOG_FAC_LOCAL6 = 22,
    ELOG_FAC_LOCAL7 = 23
};

#endif // ELOG_COMMON_H
