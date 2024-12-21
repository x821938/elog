#ifndef ELOG_LOGSERIAL_H
#define ELOG_LOGSERIAL_H

#include <Arduino.h>
#include <LogFormat.h>
#include <common.h>

using namespace std;

class LogSerial {
    struct Setting {
        uint8_t logId;
        Stream* serial;
        const char* serviceName;
        uint8_t logLevel;
        uint8_t logFlags;
    };

    struct Stats {
        uint32_t bytesWrittenTotal;
        uint32_t messagesWrittenTotal;
    };

public:
    void begin();
    void configure(const uint8_t maxRegistrations);
    void registerSerial(const uint8_t logId, const uint8_t loglevel, const char* serviceName, Stream& serial, const uint8_t logFlags);
    uint8_t getLogLevel(const uint8_t logId, Stream& serial);
    void setLogLevel(const uint8_t logId, const uint8_t loglevel, Stream& serial);
    void outputFromBuffer(const LogLineEntry logLineEntry, bool muteSerialOutput);
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
    uint8_t maxSerialRegistrations = 0;
    uint8_t registeredSerialCount = 0;

    bool peekEnabled = false;
    uint8_t peekLoglevel = ELOG_LEVEL_NOLOG;
    uint8_t peekSettingIndex = 0;
    bool peekAllServices = false;
    bool peekFilter = false; // Text filter enabled
    char peekFilterText[30];

    Stream* querySerial = nullptr;

    void write(const LogLineEntry logLineEntry, Setting& setting);
};

#endif // ELOG_LOGSERIAL_H
