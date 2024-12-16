#ifndef ELOG_LOGSPIFFS_H
#define ELOG_LOGSPIFFS_H

#include <Arduino.h>

#ifdef ELOG_SPIFFS_ENABLE

#include <LogFormat.h>
#include <LogRingBuff.h>
#include <common.h>

#define SPIFFS_MIN_FREE_SPACE 20000 // 20kB
#define SPIFFS_SYNC_FILES_EVERY 5000 // 5s

#define SPIFFS_LOGNUMBER_FILE "/lognumber.txt"
#define SPIFFS_LOG_ROOT "/logs"

#define LENGTH_LOG_DIR 30

class LogSpiffs {
    struct Setting {
        uint8_t logId;
        const char* fileName;
        uint8_t logLevel;
        uint8_t logFlags;
        File spiffsFileHandle;
        uint8_t fileNumber;
        uint32_t maxLogFileSize;
        uint32_t bytesWritten;
    };

    struct Stats {
        uint32_t bytesWrittenTotal;
        uint32_t messagesWrittenTotal;
        uint32_t messagesDiscardedTotal;
    };

public:
    void begin();
    void configure(const uint8_t maxRegistrations);
    void registerSpiffs(const uint8_t logId, const uint8_t loglevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize);
    void outputFromBuffer(const LogLineEntry logLineEntry);
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex);
    void write(const LogLineEntry logLineEntry, Setting& setting);
    bool mustLog(const uint8_t logId, const uint8_t logLevel);
    void outputStats();
    void enableQuery(Stream& querySerial);
    void peekStop();

    void queryCmdHelp();
    void queryCmdDir(const char* directory);
    void queryCmdCd(const char* directory);
    void queryCmdRm(const char* filename);
    void queryCmdRmdir(const char* directory);
    void queryCmdFormat();
    void queryCmdType(const char* filename);
    bool queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter);
    void queryCmdStatus();
    void queryPrintPrompt();
    void queryPrintVolumeInfo();

private:
    Formatting formatter;
    Stats stats;

    Setting* settings; // Array of registered file settings
    uint8_t maxRegistrations = 0; // Maximum number of registered files
    uint8_t fileSettingsCount = 0; // number of registered files

    bool fileSystemConfigured = false;
    char currentLogDir[LENGTH_LOG_DIR]; // log directory in format "/0000"
    char queryCwd[LENGTH_LOG_DIR] = SPIFFS_LOG_ROOT; // current working directory for query commands

    bool peekEnabled = false;
    uint8_t peekLoglevel = ELOG_LEVEL_NOLOG;
    uint8_t peekSettingIndex = 0;
    bool peekAllFiles = false;
    bool peekFilter = false; // Text filter enabled
    char peekFilterText[30];

    Stream* querySerial; // output device for query commands

    bool isFileOpen(const char* fileName);
    bool isValidFileName(const char* fileName);
    bool isFileNameRegistered(const char* fileName);
    void createNextLogDir();
    void getAbsolutePath(char* output, const char* path);
    uint32_t removeOldestFile();

    bool ensureFilesystemConfigured();
    bool ensureOpenFile(Setting& setting);
    void ensureFreeSpace();
    void ensureFileSize(Setting& setting);

    void allFilesSync();
    void allFilesClose();
    void allFilesOpen();
};

#else // ELOG_SPIFFS_ENABLE
class LogSpiffs {
public:
    void begin() {};
    void registerSpiffs(const uint8_t logId, const uint8_t loglevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize) {};
    void outputFromBuffer(const LogLineEntry logLineEntry) {};
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex) {};
    bool mustLog(const uint8_t logId, const uint8_t logLevel) { return false; };
    void outputStats() {};
    void enableQuery(Stream& querySerial) {};
    void peekStop() {};

    void queryCmdHelp() {};
    void queryCmdDir(const char* directory) {};
    void queryCmdCd(const char* directory) {};
    void queryCmdRm(const char* filename) {};
    void queryCmdRmdir(const char* directory) {};
    void queryCmdFormat() {};
    void queryCmdType(const char* filename) {};
    bool queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter) { return false; };
    void queryCmdStatus() {};
    void queryPrintPrompt() {};
    void queryPrintVolumeInfo() {};
};
#endif // ELOG_SPIFFS_ENABLE

#endif // ELOG_LOGSPIFFS_H
