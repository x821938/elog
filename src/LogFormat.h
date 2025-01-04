#ifndef ELOG_FORMATTING_H
#define ELOG_FORMATTING_H

#include <Arduino.h>
#include <TimeLib.h>
#include <LogCommon.h>

#define LENGTH_OF_TIME 25
#define LENGTH_OF_SERVICE 10
#define LENGTH_OF_LEVEL 9
#define LENGTH_OF_LOG_STAMP LENGTH_OF_TIME + LENGTH_OF_SERVICE + LENGTH_OF_LEVEL + 1

class Formatting {
public:
    static void getLogStamp(char* output, const uint32_t logTime, const uint8_t logLevel, const char* serviceName, const uint8_t logFlags);

    static void getTimeLongString(char* output, const uint32_t milliSeconds);
    static void getTimeRtcString(char* output, const uint32_t milliseconds);
    static void getTimeMillisString(char* output, const uint32_t milliSeconds, const bool shortTimeFormat);
    static void getSimpleTimeString(char* output, const uint32_t milliseconds);

    static void getServiceString(char* output, const char* serviceName, bool longFormat);
    static void getLogLevelString(char* output, const uint8_t logLevel);
    static void getLogLevelStringRaw(char* output, const uint8_t logLevel);
    static uint8_t getLogLevelFromString(const char* logLevel);

    static bool realTimeProvided();
    static void getHumanSize(char* output, uint32_t size);
    static void getTimeStrFromEpoch(char* output, const time_t epoch);
    static void getHumanUptime(char* output, size_t outputSize);
    static void getRTCtime(char* output, size_t outputSize);
};

#endif // ELOG_FORMATTING_H
