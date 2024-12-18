#ifndef ELOG_H
#define ELOG_H

#include <Arduino.h>
#include <LogFormat.h>
#include <LogRingBuff.h>
#include <LogSd.h>
#include <LogSerial.h>
#include <LogSpiffs.h>
#include <LogSyslog.h>
#include <esp_task_wdt.h>

#define LENGTH_COMMAND 10
#define LENGTH_COMMAND_BUFFER 50
#define LENGTH_ABSOLUTE_PATH 30

class Elog {
    enum QueryDevice {
        NONE,
        SPIFFS,
        SD,
        SER,
        SYSLOG
    };

    enum QueryState {
        QUERY_DISABLED = 0,
        QUERY_WAITING_FOR_COMMAND = 1,
        QUERY_WAITING_FOR_PEEK_QUIT = 2,
        QUERY_WAITING_FOR_TYPE_CMD = 3
    };

    struct BufferStats {
        uint32_t messagesBuffered;
        uint32_t messagesDiscarded;
    };

    friend class LogTimer;
    friend class LogSpiffs;
    friend class LogSerial;
    friend class LogSD;
    friend class LogSyslog;

public:
    // Ensure that the class is a singleton
    Elog(const Elog&) = delete;
    Elog& operator=(const Elog&) = delete;
    static Elog& getInstance();

    void configure(uint16_t logLineCapacity = 50, bool waitIfBufferFull = true);
    void log(uint8_t logId, uint8_t logLevel, const char* format, ...);
    void logHex(uint8_t logId, uint8_t logLevel, const char* message, const uint8_t* data, uint16_t length);
    void configureSerial(const uint8_t maxRegistrations = 10);
    void registerSerial(const uint8_t logId, const uint8_t logLevel, const char* serviceName, Stream& serial = Serial, const uint8_t logFlags = 0);
#ifdef ELOG_SPIFFS_ENABLE
    void configureSpiffs(const uint8_t maxRegistrations = 10);
    void registerSpiffs(const uint8_t logId, const uint8_t logLevel, const char* fileName, const uint8_t logFlags = FLAG_NONE, const uint32_t maxLogFileSize = 100000);
#endif // ELOG_SPIFFS_ENABLE
#ifdef ELOG_SD_ENABLE
    void configureSd(SPIClass& spi, const uint8_t cs, const uint32_t speed = 2000000, const uint8_t spiOption = DEDICATED_SPI, const uint8_t maxFilesettings = 10);
    void registerSd(const uint8_t logId, const uint8_t logLevel, const char* fileName, const uint8_t logFlags = FLAG_NONE, const uint32_t maxLogFileSize = 100000);
#endif // ELOG_SD_ENABLE
#ifdef ELOG_SYSLOG_ENABLE
    void configureSyslog(const char* server, uint16_t port = 514, const char* hostname = "esp32", const uint8_t maxRegistrations = 10);
    void registerSyslog(const uint8_t logId, const uint8_t logLevel, const uint8_t facility, const char* appName);
#endif // ELOG_SYSLOG_ENABLE
    void configureInternalLogging(Stream& internalLogDevice, uint8_t internalLogLevel = ELOG_LEVEL_ERROR, uint16_t statsEvery = 10000);
    void enableQuery(Stream& serialPort);
    void provideTime(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t hour, const uint8_t minute, const uint8_t second);

    template <typename ...Args>
    inline void debug(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_DEBUG, format, args...);
    }

    template <typename ...Args>
    inline void info(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_INFO, format, args...);
    }

    template <typename ...Args>
    inline void notice(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_NOTICE, format, args...);
    }

    template <typename ...Args>
    inline void warning(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_WARNING, format, args...);
    }

    template <typename ...Args>
    inline void error(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_ERROR, format, args...);
    }

    template <typename ...Args>
    inline void critical(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_CRITICAL, format, args...);
    }

    template <typename ...Args>
    inline void alert(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_ALERT, format, args...);
    }

    template <typename ...Args>
    inline void emergency(uint16_t logId, const char* format, Args ...args)
    {
        log(logId, ELOG_LEVEL_EMERGENCY, format, args...);
    }

private:
    Elog() { } // Private constructor. Part of singleton pattern

    LogSpiffs logSpiffs;
    LogSerial logSerial;
    LogSD logSD;
    LogSyslog logSyslog;
    Formatting formatter;
    LogRingBuff<LogLineEntry> ringBuff;

    uint8_t registeredSpiffsCount = 0;
    uint8_t registeredSdCount = 0;
    uint8_t registeredSyslogCount = 0;
    uint8_t registeredSerialCount = 0;

    Stream* internalLogDevice = &Serial;
    uint8_t internalLogLevel = ELOG_LEVEL_ERROR; // Tell library user when he is doing something wrong by default

    BufferStats bufferStats;
    uint16_t statsEvery = 10000;

    char queryCmdBuf[LENGTH_COMMAND_BUFFER];
    int queryCmdBufLen = 0;
    QueryState queryState = QUERY_DISABLED;
    bool queryEnabled = false;
    Stream* querySerial; // serial port for query commands
    QueryDevice queryDevice = SPIFFS;

    bool logStarted = false;
    bool waitIfBufferFull = false;

    void writerTaskStart();
    static void writerTask(void* parameter);
    void outputFromBuffer();
    void buffAddLogLine(LogLineEntry& logLineEntry);
    bool mustLog(uint8_t logId, uint8_t logLevel);
    void logInternal(const uint8_t logLevel, const char* format, ...);
    void outputStats();
    void panic(const char* message);

    void queryHandleSerialInput();
    void queryProcessIncomingCmd(const char* command);

    void queryStateDisabled(char c);
    void queryStateWaitCommand(char c);
    void queryStateWaitPeekQuit(char c);

    void queryCmdHelp();
    void queryCmdSpiffs();
    void queryCmdSd();
    void queryCmdSerial();
    void queryCmdSyslog();
    void queryCmdDir(const char* directory);
    void queryCmdCd(const char* directory);
    void queryCmdRm(const char* filename);
    void queryCmdRmdir(const char* directory);
    void queryCmdFormat();
    void queryCmdType(const char* filename);
    void queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter);
    void queryCmdStatus();
    void queryPrintPrompt();
};

extern Elog& Logger; // Make an instance available to user when he includes the library

#endif // ELOG_H
