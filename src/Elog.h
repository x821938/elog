#ifndef Elog_h
#define Elog_h

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

#ifndef LOGGER_DISABLE_SD
#include <SdFat.h>
#endif

#ifndef LOG_MAX_HEX_STRING_LENGTH
#define LOG_MAX_HEX_STRING_LENGTH 250
#endif

// Loglevels. The lower level, the more serious.
enum Loglevel {
    EMERGENCY = 0,
    ALERT = 1,
    CRITICAL = 2,
    ERROR = 3,
    WARNING = 4,
    NOTICE = 5,
    INFO = 6,
    DEBUG = 7,
};

// Options that can be used for addFileLogging.
enum LogFileOptions {
    FILE_NO_OPTIONS = 0,
    FILE_NO_STAMP = 1,
};

/* All user provided settings of the loginstance is stored in this structure */
struct LogService {
    Stream* serialPortPtr; // Pointer to the serial port where log messages should be sent to
    char* serialServiceName; // The servicename that is stamped on each log message
    Loglevel serialWantedLoglevel; // Target loglevel. Everything below or equal to this level is logged
    bool serialEnabled; // Serial logging is enabled

    const char* sdFileName; // Filename of the logfile on sd-card (it will be inside LOGXXXXX dir)
    Loglevel sdWantedLoglevel; // Target loglevel. Everything below or equal to this level is logged
    LogFileOptions sdOptions; // Options about timestamps inside the logfile
    bool sdEnabled; // File logging is enabled
    int32_t sdFileCreteLastTry; // Keep track of when we last time tried to create the logfile

#ifndef LOGGER_DISABLE_SD
    SdFile sdFileHandle; // sdfat filehandle for the open file
#endif

    char* spiffsServiceName; // The servicename that is stamped on each log message
    Loglevel spiffsWantedLoglevel; // Target loglevel. Everything below or equal to this level is logged
    bool spiffsEnabled; // Spiffs logging enabled
};

/* All global settings for this lib is store in this structure */
struct LogSettings {
    uint16_t maxLogMessageSize;
    uint16_t maxLogMessages;

    Stream* internalLogDevice;
    Loglevel internalLogLevel;
    bool discardMsgWhenBufferFull;
    uint32_t reportStatusEvery;

    uint32_t sdReconnectEvery;
    uint32_t sdSyncFilesEvery;
    uint32_t sdTryCreateFileEvery;

    uint32_t spiffsSyncEvery;
    uint32_t spiffsCheckSpaceEvery;
    uint32_t spiffsMinimumSpace;
};

/* Structure that is written to the ringbuffer. It gives the WriterTask all the information
   needed to generate the logline with stamp, level, service etc. */
struct LogLineEntry {
    bool logFile; // Do we want logging to file on this handle?
    bool logSerial; // Do we want logging to serial on this handle?
    bool logSpiffs; // Do we want logging to spiffs on this handle?
    Loglevel loglevel; // Loglevel
    uint32_t logTime; // Time in milliseconds the log message was created
    LogService* service;
    bool internalLogMessage; // If true, this is sent to internal log device
};

/* Structure to store status about how the messages are sent or discarded */
struct LogStatus {
    uint32_t bufferMsgAdded;
    uint32_t bufferMsgNotAdded;
    uint32_t sdMsgWritten;
    uint32_t sdMsgNotWritten;
    uint32_t sdBytesWritten;
    uint32_t spiffsMsgWritten;
    uint32_t spiffsMsgNotWritten;
    uint32_t spiffsBytesWritten;
};

/* The ringbuffer thats is used for buffering data for both serial and filesystem
   its a double buffer. When you push or pop data you provide both the LogLineEntry
   structure and the LogLineMessage (Text that is actually logged)
   When critical data manipulation is done we disable interrupts to avoid
   data damage. We disable this as short as possible */
class LogRingBuff {
public:
    void createBuffer(size_t logLineCapacity, size_t logLineMessageSize);
    bool push(const LogLineEntry& logLineEntry, const char* logLineMessage);
    bool pop(LogLineEntry& logLineEntry, char* logLineMessage);
    bool isEmpty();
    bool isFull();
    size_t size();
    size_t capacity();
    uint8_t percentageFull();

private:
    LogLineEntry* logLineEntries = nullptr;
    char** logMessages;
    size_t logLineCapacity = 0;
    size_t logLineMessageSize = 0;
    size_t size_ = 0;
    size_t front_ = 0;
    size_t rear_ = 0;
};

/* When an instance is created we disable logging to file and serial.
   this has to be added with methods addFileLogging() and addSerialLogging()
   If you want to change any settings for this library, call Logger::globalSettings()
   before calling any other methods in this library */
class Elog {
private:
    static std::vector<Elog*> loggerInstances; // for traversing logger instances from static methods

    static bool writerTaskHold; // If you want to pause the writerTask, set this to true.

    LogService service; // All instance info about sd, spiffs, serial, loglevel is stored here

    static File spiffsFileHandle;
    static bool spiffsConfigured;
    static bool spiffsMounted; // True when spiffs is mounted
    static char spiffsFileName[30]; // The filename of the current log file

    static LogRingBuff logRingBuff; // Our ringbuffer that stores all log messages
    static LogSettings settings; // Global settings for this library
    static LogStatus loggerStatus; // Status for logging

    static void writerTask(void* parameter);
    static void outputSerial(const LogLineEntry& logLineEntry, const char* logLineMessage);
    static void outputSpiffs(const LogLineEntry& logLineEntry, const char* logLineMessage);

    static void getLogStamp(const uint32_t logTime, const Loglevel loglevel, char* output);
    static void getLogStamp(const uint32_t logTime, const Loglevel loglevel, const char* service, char* output);
    static uint8_t getServiceString(const char* serviceName, char* output);
    static uint8_t getLogLevelString(const Loglevel logLevel, char* output);

    static void logInternal(const Loglevel loglevel, const char* format, ...);
    static void writerTaskStart();
    static void addToRingbuffer(const LogLineEntry& logLineEntry, const char* logLineMessage);
    static void reportStatus();

    static void spiffsPrepare();
    static void spiffsListLogFiles(Stream& serialPort);
    static bool spiffsProcessCommand(Stream& serialPort, const char* command);
    static void spiffsPrintLogFile(Stream& serialPort, const char* filename);
    static void spiffsFormat(Stream& serialPort);
    static void spiffsEnsureFreeSpace();

#ifndef LOGGER_DISABLE_SD
    static uint16_t sdLogNumber;
    static char directoryName[8];

    static uint8_t sdChipSelect;
    static uint32_t sdSpeed;

    static bool sdCardPresent;
    static int32_t sdCardLastReconnect;

    static bool sdConfigured;

    static SdFat sd;
    static SPIClass spi;
    static void createLogFileIfClosed(LogService* service);
    static void reconnectSd();
    static void closeAllFiles();
    static void syncAllSdFiles();
    static void outputSd(LogLineEntry& logLineEntry, char* logLineMessage);
#endif

protected:
    // This is kept protected for the LoggerTimer to access these things
    static uint8_t getTimeStringMillis(uint32_t milliSeconds, char* output);

public:
    Elog();

    static void globalSettings(uint16_t maxLogMessageSize = 250,
        uint16_t maxLogMessages = 20,
        Stream& internalLogDevice = Serial,
        Loglevel internalLogLevel = WARNING,
        bool discardMsgWhenBufferFull = false,
        uint32_t sdReportBufferStatusEvery = 5000);

    void addSerialLogging(Stream& serialPort, const char* serviceName, const Loglevel wantedLogLevel);
    static void configureSpiffs(uint32_t spiffsSyncEvery = 5000, uint32_t spiffsCheckSpaceEvery = 10000, uint32_t spiffsMinimumSpace = 20000);
    void addSpiffsLogging(const char* serviceName, const Loglevel wantedLogLevel);
    void log(const Loglevel logLevel, const char* format, ...);
    void spiffsQuery(Stream& serialPort);

    char* toHex(byte* data, uint16_t len);
    char* toHex(char* data);

#ifndef LOGGER_DISABLE_SD
    static void configureSd(SPIClass& spi, uint8_t cs, uint32_t speed = 2000000, uint32_t sdReconnectEvery = 5000, uint32_t sdSyncFilesEvery = 5000, uint32_t sdTryCreateFileEvery = 5000);
    void addSdLogging(const char* fileName, const Loglevel wantedLogLevel, const LogFileOptions options = FILE_NO_OPTIONS);
#endif
};

#endif