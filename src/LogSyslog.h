#ifndef LOGSYSLOG_H
#define LOGSYSLOG_H

#include <Arduino.h>

#ifndef LOGGING_SYSLOG_DISABLE

#include <LogFormat.h>
#include <WiFi.h>

class LogSyslog {
    struct Setting {
        uint8_t logId;
        const char* appName;
        uint8_t facility;
        uint8_t logLevel;
    };

    struct Stats {
        uint32_t bytesWrittenTotal;
        uint32_t messagesWrittenTotal;
        uint32_t messagesDiscardedTotal;
    };

public:
    void begin();
    void configure(const char* serverName, const uint16_t port, const char* hostname, const uint8_t maxRegistrations);
    void registerSyslog(const uint8_t logId, const uint8_t loglevel, const uint8_t facility, const char* appName);
    void outputFromBuffer(const LogLineEntry logLineEntry);
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex);
    bool mustLog(const uint8_t logId, const uint8_t logLevel);
    void outputStats();

    void enableQuery(Stream& querySerial);
    void queryCmdHelp();
    bool queryCmdPeek(const char* appName, const char* loglevel, const char* textFilter);
    void queryCmdStatus();
    void queryPrintPrompt();

    void peekStop();

private:
    Formatting formatter;
    Stats stats;
    WiFiUDP syslogUdp;

    Setting* settings; // Array of registered syslog settings
    uint8_t maxRegistrations = 0; // Maximum number of registered syslog settings
    uint8_t syslogSettingsCount = 0; // number of registered syslog settings

    bool peekEnabled = false;
    uint8_t peekLoglevel = NOLOG;
    uint8_t peekSettingIndex = 0;
    bool peekAllApps = false;
    bool peekFilter = false; // Text filter enabled
    char peekFilterText[30];

    bool syslogConfigured = false;
    const char* syslogServer;
    uint16_t syslogPort;
    const char* syslogHostname;

    Stream* querySerial = nullptr;

    void write(const LogLineEntry logLineEntry, Setting& setting);
};

#else // LOGGING_SYSLOG_DISABLE
class LogSyslog {
public:
    void begin() { }
    void configure(const char* serverName, const uint16_t port, const char* hostname) { }
    void registerSyslog(const uint8_t logId, const uint8_t loglevel, const uint8_t facility, const char* appName) { }
    void outputFromBuffer(const LogLineEntry logLineEntry) { }
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex) { }
    bool mustLog(const uint8_t logId, const uint8_t logLevel) { return false; }
    void outputStats() { }

    void enableQuery(Stream& querySerial) { }
    void queryCmdHelp() { }
    bool queryCmdPeek(const char* appName, const char* loglevel, const char* textFilter) { return false; }
    void queryCmdStatus() { }
    void queryPrintPrompt() { }

    void peekStop() { }
};
#endif // LOGGING_SYSLOG_DISABLE

#endif // LOGSYSLOG_H