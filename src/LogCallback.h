#ifndef ELOG_LOGCALLBACK_H
#define ELOG_LOGCALLBACK_H

#include <Arduino.h>
#include "LogFormat.h"
#include "LogCommon.h"

using namespace std;

class LogCallback {
public:
    // prototype for the callback function
    typedef uint32_t (*callbackFunc_t)(LogLineEntry);

private:
    struct Setting {
        uint8_t logId;
        callbackFunc_t callback;
        const char* serviceName;
        uint8_t logLevel;
        uint8_t lastMsgLogLevel;
        uint8_t logFlags;
    };

    struct Stats {
        uint32_t bytesWrittenTotal;
        uint32_t messagesWrittenTotal;
    };

public:
    void begin();
    void configure(const uint8_t maxRegistrations);
    void registerCallback(const uint8_t logId, const uint8_t loglevel, const char* serviceName, callbackFunc_t funcPtr, const uint8_t logFlags);
    uint8_t getLogLevel(const uint8_t logId, callbackFunc_t funcPtr);
    void setLogLevel(const uint8_t logId, const uint8_t loglevel, callbackFunc_t funcPtr);
    uint8_t getLastMsgLogLevel(const uint8_t logId, callbackFunc_t funcPtr);
    void outputFromBuffer(const LogLineEntry logLineEntry);
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex);
    bool mustLog(const uint8_t logId, const uint8_t logLevel);
    void outputStats();
    uint8_t registeredCount();

    void enableQuery(Stream& querySerial);
    void queryCmdHelp();
    bool queryCmdPeek(const char* serviceName, const char* loglevel, const char* textFilter);
    void queryCmdStatus();
    void queryPrintPrompt();

    void peekStop();

private:
    Formatting formatter;
    Stats stats;

    Setting* settings; // Array of registered serial settings
    uint8_t maxCallbackRegistrations = 0;
    uint8_t registeredCallbackCount = 0;

    bool peekEnabled = false;
    uint8_t peekLoglevel = ELOG_LEVEL_NOLOG;
    uint8_t peekSettingIndex = 0;
    bool peekAllServices = false;
    bool peekFilter = false; // Text filter enabled
    char peekFilterText[30];

    Stream* querySerial = nullptr;

    void write(LogLineEntry logLineEntry, Setting& setting);
};

#endif // ELOG_LOGCALLBACK_H
