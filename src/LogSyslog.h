#ifndef LOGSYSLOG_H
#define LOGSYSLOG_H

#include <Arduino.h>

#ifndef LOGGING_SYSLOG_DISABLE

#include <LogFormat.h>
#include <WiFi.h>

struct SyslogSetting {
    uint8_t logId;
    const char* appName;
    uint8_t facility;
    uint8_t logLevel;
};

struct SyslogStats {
    uint32_t bytesWrittenTotal;
    uint32_t messagesWrittenTotal;
    uint32_t messagesDiscardedTotal;
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

class LogSyslog {
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
    SyslogStats syslogStats;
    WiFiUDP syslogUdp;

    SyslogSetting* syslogSettings; // Array of registered syslog settings
    uint8_t maxRegistrations = 0; // Maximum number of registered syslog settings
    uint8_t syslogSettingsCount = 0; // number of registered syslog settings

    bool peekEnabled = false;
    uint8_t peekLoglevel = 8; // FIXME: NOLOG instead
    uint8_t peekSettingIndex = 0;
    bool peekAllApps = false;
    bool peekFilter = false; // Text filter enabled
    char peekFilterText[30];

    bool syslogConfigured = false;
    const char* syslogServer;
    uint16_t syslogPort;
    const char* syslogHostname;

    Stream* querySerial = nullptr;

    void write(const LogLineEntry logLineEntry, SyslogSetting& setting);
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