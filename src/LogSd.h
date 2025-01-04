#ifndef ELOG_LOGSD_H
#define ELOG_LOGSD_H

#include <Arduino.h>
#include <LogCommon.h>

#ifdef ELOG_SD_ENABLE

#include <LogFormat.h>
#include <LogRingBuff.h>
#include <ctime>

#define SD_MIN_FREE_SPACE 10000000 // 10MB

#define SD_LOG_ROOT "/logs"
#define SD_LOGNUMBER_FILE "/lognumber.txt"
#define MAX_LOGFILES_IN_DIR 100

#define SD_RECONNECT_EVERY 5000
#define SD_SYNC_FILES_EVERY 5000

using namespace std;

// Fat32 format expected on sd card. Windows formatted exFat is not supported.
typedef SdFat sd_t;
typedef SdFile file_t;

class LogSD {
    struct Setting {
        uint8_t logId;
        file_t* sdFileHandle;
        const char* fileName;
        uint8_t logLevel;
        uint8_t lastMsgLogLevel;
        uint32_t sdFileCreteLastTry;
        uint8_t logFlags;
        uint8_t fileNumber;
        uint32_t bytesWritten;
        uint32_t maxLogFileSize;
    };

    struct Stats {
        uint32_t bytesWrittenTotal;
        uint32_t messagesWrittenTotal;
        uint32_t messagesDiscardedTotal;
    };

public:
    void begin();
    void configure(SPIClass& spi, const uint8_t cs, const uint32_t speed, uint8_t spiOption, const uint8_t maxRegistrations);
    void registerSd(const uint8_t logId, const uint8_t loglevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize);
    uint8_t getLogLevel(const uint8_t logId, const char* fileName);
    void setLogLevel(const uint8_t logId, const uint8_t loglevel, const char* fileName);
    uint8_t getLastMsgLogLevel(const uint8_t logId, const char* fileName);
    void outputFromBuffer(const LogLineEntry logLineEntry);
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex);
    void write(const LogLineEntry logLineEntry, Setting& setting);
    bool mustLog(const uint8_t logId, const uint8_t logLevel);
    void outputStats();
    void enableQuery(Stream& querySerial);
    void peekStop();
    uint8_t registeredCount();

    void queryCmdHelp();
    void queryCmdDir(const char* directory);
    void queryCmdCd(const char* directory);
    void queryCmdRm(const char* path);
    void queryCmdRmdir(const char* path);
    void queryCmdFormat();
    void queryCmdType(const char* filename);
    bool queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter);
    void queryCmdStatus();
    void queryPrintPrompt();
    void queryPrintVolumeInfo();

private:
    Formatting formatter;
    Stats stats;
    sd_t sd;

    Setting* settings; // Array of registred file settings
    uint8_t maxRegistrations; // Maximum number of registered files
    uint8_t registeredSdCount = 0; // number of registered files

    char logCwd[20];
    char queryCwd[20] = SD_LOG_ROOT; // current working directory

    bool peekEnabled = false;
    uint8_t peekLoglevel = ELOG_LEVEL_NOLOG;
    uint8_t peekSettingIndex = 0;
    bool peekAllFiles = false;
    bool peekFilter = false; // Text filter enabled
    char peekFilterText[30];

    Stream* querySerial;

    bool sdConfigured = false;
    static SPIClass spi;
    uint8_t sdChipSelect;
    uint32_t sdSpeed;
    uint8_t sdSpiOption;
    bool sdCardPresent = false;
    int32_t sdCardLastReconnect = LONG_MIN;

    uint16_t sdLogNumber = 0;
    uint8_t filesInLogDir = 0;

    bool isValidFileName(const char* fileName);
    bool isFileNameRegistered(const char* fileName);

    void ensureFreeSpace();
    void ensureFileSize(Setting& setting);

    void reconnect();
    bool shouldReconnect();
    void attemptReconnect();

    void readLogNumber();
    void writeLogNumber();
    void findNextLogDir();
    bool logDirectoryExists();
    void createLogDirectory();

    void getPathFromRelative(char* output, const char* path);
    uint32_t removeOldestFile();
    uint16_t getOldestLogDir();
    bool getOldestLogFileInDir(char* output, const char* dirName, uint32_t& fileSize);
    uint32_t getFreeSpace();
    void getSettingFullFileName(char* output, Setting setting);
    void timestampFile(Setting& setting);
    uint32_t convertToEpoch(uint16_t pdate, uint16_t ptime);

    void createLogFileIfClosed(Setting& setting);
    void allFilesClose();
    void allFilesSync();
};

#else // ELOG_SD_ENABLE
class LogSD {
public:
    void begin() {};
    void configure(void* spi, const uint8_t cs, const uint32_t speed, uint8_t spiOption, const uint8_t maxRegistrations) {};
    void registerSd(const uint8_t logId, const uint8_t loglevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize) {};
    void outputFromBuffer(const LogLineEntry logLineEntry) {};
    void handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex) {};
    bool mustLog(const uint8_t logId, const uint8_t logLevel) { return false; };
    void outputStats() {};
    void enableQuery(Stream& querySerial) {};
    void peekStop() {};
    uint8_t registeredCount() { return 0; };

    void queryCmdHelp() {};
    void queryCmdDir(const char* directory) {};
    void queryCmdCd(const char* directory) {};
    void queryCmdRm(const char* path) {};
    void queryCmdRmdir(const char* path) {};
    void queryCmdFormat() {};
    void queryCmdType(const char* filename) {};
    bool queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter) { return false; };
    void queryCmdStatus() {};
    void queryPrintPrompt() {};
    void queryPrintVolumeInfo() {};
};
#endif // ELOG_SD_ENABLE

#endif // ELOG_LOGSD_H
