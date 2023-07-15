#include "Elog.h"

/* Compiler options available:
    LOGGER_DISABLE_SD (not default)
    MAX_LOG_HEX_STRING_SIZE (default = 250)
 */

// Init static vars
std::vector<Elog*> Elog::loggerInstances;
LogSettings Elog::settings;
LogStatus Elog::loggerStatus = { 0, 0, 0, 0, 0, 0, 0, 0 };
LogRingBuff Elog::logRingBuff;
char Elog::spiffsFileName[] = "";
File Elog::spiffsFileHandle;
bool Elog::spiffsConfigured = false;
bool Elog::spiffsMounted = false;
bool Elog::writerTaskHold = false;

#ifndef LOGGER_DISABLE_SD

// Init static vars for sd-card
bool Elog::sdCardPresent = false;
int32_t Elog::sdCardLastReconnect = 0;
char Elog::directoryName[] = "";
uint16_t Elog::sdLogNumber = 0;
uint8_t Elog::sdChipSelect; // Chipselect for sd reader
uint32_t Elog::sdSpeed;
bool Elog::sdConfigured = false;

SdFat Elog::sd;
SPIClass Elog::spi;

/* This is called from writerTask when some data has been popped from the ringbuffer.
   We try to write the data to sd card. If not successfull, it will try to connect to the sd card and create the logfile */
void Elog::outputSd(LogLineEntry& logLineEntry, char* logLineMessage)
{
    static char logStamp[45];

    if (sdConfigured) {
        if (sdCardPresent) {
            LogService* svc = logLineEntry.service;

            if (svc->sdOptions == FILE_NO_STAMP) {
                logStamp[0] = 0;
            } else {
                getLogStamp(logLineEntry.logTime, logLineEntry.loglevel, logStamp);
            }

            createLogFileIfClosed(svc);
            if (svc->sdFileHandle) { // Are we working on a valid file?
                size_t bytesWritten; // Number of bytes written should be the same as content length
                size_t expectedBytes = strlen(logStamp) + strlen(logLineMessage) + 2; // 2 chars for endline

                bytesWritten = svc->sdFileHandle.print(logStamp);
                bytesWritten += svc->sdFileHandle.print(logLineMessage);
                if (strlen(logLineMessage) == settings.maxLogMessageSize - 1) {
                    bytesWritten += svc->sdFileHandle.print("...");
                    expectedBytes += 3;
                }
                bytesWritten += svc->sdFileHandle.println();

                if (bytesWritten != expectedBytes) { // If not everything is written, then the SD must be ejected.
                    sdCardPresent = false;
                    sdCardLastReconnect = millis();
                    logInternal(WARNING, "SD card ejected");
                } else { // Data written succesfully.
                    loggerStatus.sdMsgWritten++;
                    loggerStatus.sdBytesWritten += bytesWritten;
                }
            } else { // If we dont have a valid filehandle we try do create it periodically
                loggerStatus.sdMsgNotWritten++;
            }

            syncAllSdFiles(); // Keep files synced to card periodically
        } else { // If no sd card is present, try to connect to it.
            loggerStatus.sdMsgNotWritten++;
            reconnectSd();
        }
    }
}

/*  This should be called to set up logging to the SD card
    It connects to the sd card reader. Parameters:
    spi: The SPI object where the SD-reader is connected to.
    cs: Chip-select pin for the SD-reader
    speed: How fast to talk to SD-reader. in Hz - default is 2Mhz. Don´t put it to high or you will get errors writing
    It connects to the sd card */
void Elog::configureSd(SPIClass& _spi, uint8_t _cs, uint32_t _speed, uint32_t sdReconnectEvery, uint32_t sdSyncFilesEvery, uint32_t sdTryCreateFileEvery)
{
    if (!sdConfigured) {
        spi = _spi;
        sdChipSelect = _cs;
        sdSpeed = _speed;
        sdCardPresent = false;
        settings.sdReconnectEvery = sdReconnectEvery;
        settings.sdSyncFilesEvery = sdSyncFilesEvery;
        settings.sdTryCreateFileEvery = sdTryCreateFileEvery;

        logInternal(DEBUG, "Configuring filesystem");
        sdCardLastReconnect = LONG_MIN;
        reconnectSd();

        sdConfigured = true; // This is for our writerTask. When true it will start writing to sd card.
    } else {
        logInternal(ERROR, "You can only configure file logging once");
    }
}

/*
    Add file logging to Logger instance, parameters:
    fileName: The name of the file to attach to the log handle.
    wantedLogLevel: Everything equal or lower than this loglevel will be logged
    FileOptions: Can be used to change output format written to the logfile
*/
void Elog::addSdLogging(const char* fileName, const Loglevel wantedLogLevel, const LogFileOptions options)
{
    writerTaskStart(); // Make sure that the writerTask is running
    if (sdConfigured) {
        if (!service.sdEnabled) {
            logInternal(INFO, "Adding file logging for filename=\"%s\"", fileName);
            service.sdFileName = fileName;
            service.sdWantedLoglevel = wantedLogLevel;
            service.sdOptions = options;
            service.sdEnabled = true;
            service.sdFileCreteLastTry = LONG_MIN; // This triggers log file creation immediately
        } else {
            logInternal(ERROR, "You can only add file logging once");
        }
    } else {
        logInternal(ERROR, "Please run configureSd() before adding SD file logging");
    }
}

/*  Tries to create a logfile in our logdirectory. If it fails, the next time we will try again is after
    SD_FILE_TRY_CREATE_EVERY milliseconds.
*/
void Elog::createLogFileIfClosed(LogService* svc)
{
    if (!svc->sdFileHandle) { // Only do something if we dont have a valid filehandle
        if (millis() - svc->sdFileCreteLastTry > settings.sdReconnectEvery) {
            char filename[50];
            sprintf(filename, "%s/%s", directoryName, svc->sdFileName);

            svc->sdFileHandle.open(filename, O_CREAT | O_WRITE);
            if (svc->sdFileHandle) {
                logInternal(INFO, "Created logfile \"%s\"", filename);
            } else {
                logInternal(ERROR, "Could not create logfile \"%s\"", filename);
            }
            svc->sdFileCreteLastTry = millis();
        }
    }
}

/* Connects to the sd card via the card reader. It used the previously provided SPI and cs for connecting.
   It will be called as soon as filelogging is configured. It will also be called after a sd card has been ejected
   If success on the SPI bus then lognumber.txt is opened to find the next logdir name.
   logdirs are named LOGXXXXX and are incremented. A new logdir is created and lognumber.txt is updated
   */
void Elog::reconnectSd()
{
    // Only if we are not connected, we will try to connect. Also not too often.
    if (sdCardPresent == false && millis() - sdCardLastReconnect > settings.sdReconnectEvery) {
        sdCardLastReconnect = millis();
        logInternal(DEBUG, "Trying to connect to SD card");

        if (!sd.begin(SdSpiConfig(sdChipSelect, DEDICATED_SPI, sdSpeed, &spi))) {
            logInternal(WARNING, "SD card initialization failed");
        } else {
            logInternal(INFO, "SD card detected");
            SdFile file;

            // the lognumber.txt file holds the last LOGxxxxx
            file.open("/lognumber.txt", O_READ);
            if (file) {
                char fileContent[10];
                file.read(fileContent, 10);
                sdLogNumber = atoi(fileContent);

                logInternal(DEBUG, "Read file /lognumber.txt and got log number %d", sdLogNumber);
                file.close();
            } else {
                logInternal(DEBUG, "No /lognumber.txt file");
                sdLogNumber = 1; // We start from number 1 if there is no lognumber.txt file
            }

            // Start from the lognumber and count up until we find a directory that does not exist
            while (true) {
                sprintf(directoryName, "/LOG%05d", sdLogNumber);
                if (!file.open(directoryName, O_READ)) {
                    file.close();
                    break;
                }
                sdLogNumber++;
                file.close();
            }

            // Create a LOGxxxxx dir for our logfiles
            sprintf(directoryName, "/LOG%05d", sdLogNumber);
            if (!sd.mkdir(directoryName)) {
                logInternal(ALERT, "Error creating log directory %s. No file logging!", directoryName);
            } else {
                logInternal(INFO, "Created new log directory %s", directoryName);
            }

            // Save the incremented lognumber in the lognumber.txt file.
            file.open("/lognumber.txt", O_WRITE | O_CREAT);
            logInternal(DEBUG, "Writing /lognumber.txt file with lognumber %d", sdLogNumber);
            if (file) {
                file.print(sdLogNumber);
                file.close();
                closeAllFiles(); // If we had something opened, we need to close it.
                sdCardPresent = true;
            } else {
                logInternal(ALERT, "Error writing to lognumber.txt. No file logging!");
            }
        }
    }
}

/* Traverses all Logger instances and closes each file associated with each instnace.
   This is needed after a sd card reconnect */
void Elog::closeAllFiles()
{
    logInternal(INFO, "Resetting all logfiles");
    for (auto loggerInstance : loggerInstances) {
        LogService* service = &loggerInstance->service;
        if (service->sdEnabled) {
            service->sdFileHandle.close();
            service->sdFileCreteLastTry = LONG_MIN; // This triggers log file creation immediately
        }
    }
}

/* Traverses all Logger instances and forces a write of each file associated with each instance.
   This method can be called as often as wanted, but only every SD_SYNC_FILES_EVERY milliseconds
   the cache is synced to the sd card */
void Elog::syncAllSdFiles()
{
    static uint32_t lastSynced = 0;

    if (millis() - lastSynced > settings.sdSyncFilesEvery) {
        logInternal(DEBUG, "Syncronizing all SD logfiles. Writing dirty cache");
        for (auto loggerInstance : loggerInstances) {
            LogService* service = &loggerInstance->service;
            if (service->sdEnabled && service->sdFileHandle) {
                service->sdFileHandle.sync();
            }
        }
        lastSynced = millis();
    }
}

#endif

/* Constructor for Logger. It saves the instance and disables logging for file and serial */
Elog::Elog()
{
    static bool firstInstance = true;
    if (firstInstance) {
        globalSettings(); // Set default global settings logger is first instanciated
        firstInstance = false;
    }

    service.sdEnabled = false; // Default disabled for a new instance
    service.serialEnabled = false; // Default disabled for a new instance
    service.spiffsEnabled = false; // Default disabled for a new instance

    loggerInstances.push_back(this); // Save this instance for later traversal
}

void Elog::spiffsPrepare()
{
    if (LittleFS.begin(true)) { // Format it if we fail
        logInternal(INFO, "SPIFFS mounted");
        if (!LittleFS.exists("/logs")) {
            logInternal(DEBUG, "Creating Logs directory on spiffs");
            LittleFS.mkdir("/logs");
        }

        uint16_t logNumber = 1;

        File file = LittleFS.open("/lognumber.txt", FILE_READ);
        if (file) {
            char fileContent[10];
            file.read((uint8_t*)fileContent, 10);
            logNumber = atoi(fileContent);

            logInternal(DEBUG, "Read file /lognumber.txt and got log number %d", logNumber);
            file.close();
        } else {
            logInternal(DEBUG, "No /lognumber.txt file");
            logNumber = 1; // We start from number 1 if there is no lognumber.txt file
        }

        // Start from the lognumber and count up until we find a directory that does not exist
        while (true) {
            sprintf(spiffsFileName, "/logs/%05d", logNumber);
            if (!LittleFS.exists(spiffsFileName)) {
                break;
            }
            logNumber++;
        }

        // Save the incremented lognumber in the lognumber.txt file.
        file = LittleFS.open("/lognumber.txt", FILE_WRITE);
        logInternal(DEBUG, "Writing /lognumber.txt file with lognumber %d", logNumber);
        if (file) {
            file.print(logNumber);
            file.close();
        } else {
            logInternal(ALERT, "Error writing to /lognumber.txt. No file logging!");
        }

        spiffsMounted = true;
    } else {
        logInternal(ERROR, "Failed to mount SPIFFS. No logging will be done to SPIFFS");
    }
}

/* Reports to serial the status of the ringbuffer. It shows how many messages are written and how many are discarded
   because of buffer overflow. It can be called as often as you want, but it only reports every SD_REPORT_BUFFER_STATUS_EVERY ms
   This is internal logging from this library and is logged with level DEBUG. Normal users don´t want this info
   Also Gives a warning if buffer is full. We only do this if discardMsgWhenBufferFull is false.
   If this is set to true, the user doesn´t care about lost messages. Thats why we don´t care telling him
   Also shows status for SD and SPIFFS if it is enabled */
void Elog::reportStatus()
{
    static bool bufferFullWarningSent = false;
    static uint8_t maxBuffPct = 0;
    static uint32_t lastStatus = 0;

    if (millis() - lastStatus > settings.reportStatusEvery) {
        logInternal(DEBUG, "Status (Buffer): Msgs added %d, discarded %d, Max Buffer usage %d%%",
            loggerStatus.bufferMsgAdded,
            loggerStatus.bufferMsgNotAdded,
            maxBuffPct);

#ifndef LOGGER_DISABLE_SD
        if (sdConfigured) {
            logInternal(DEBUG, "Status (SD): Msgs written %d (%d bytes), discarded %d, free space %d bytes",
                loggerStatus.sdMsgWritten,
                loggerStatus.sdBytesWritten,
                loggerStatus.sdMsgNotWritten,
                sd.freeClusterCount() * sd.bytesPerCluster());
        }
#endif
        if (spiffsMounted) {
            logInternal(DEBUG, "Status (SPIFFS): Msgs written %d (%d bytes), discarded %d, free space %d bytes",
                loggerStatus.spiffsMsgWritten,
                loggerStatus.spiffsBytesWritten,
                loggerStatus.spiffsMsgNotWritten,
                LittleFS.totalBytes() - LittleFS.usedBytes());
        }

        maxBuffPct = 0;
        lastStatus = millis();
    }

    if (logRingBuff.percentageFull() > maxBuffPct) {
        maxBuffPct = logRingBuff.percentageFull();
    }
    if (logRingBuff.percentageFull() < 50) { // When buffer under half full, we clear "full warning".
        bufferFullWarningSent = false;
    }
    if (!bufferFullWarningSent && logRingBuff.isFull() && !settings.discardMsgWhenBufferFull) {
        logInternal(WARNING, "Log Buffer was full. Please increase its size.");
        bufferFullWarningSent = true;
    }
}

/* All public settings for the Logger library is set here. Remember to call this before calling any
   instance methods like addSerialLogging(), addSdLogging(), or log()
   Parameters are optional. Parameters:

    maxLogMessageSize: How long each logged messages can be in bytes
    maxLogMessages:  Number of messages to reserve space for in the buffer
    internalLogDevice: When this lib makes an internal logging, this is where its sent
    internalLogLevel: The loglevel of the internal messages.
    discardMsgWhenBufferFull: If true messages will be discarded if buffer full. Otherwise the logging will depleat the buffer before returning.
    sdReconnectEvery: If connection to sd card is lost, it wil reconnect every x ms
    sdSyncFilesEvery: cache is written to sd card evey x ms
    sdTryCreateFileEvery: if a file could not be created it will retry every x ms
    reportStatusEvery: if debugging in this lib is enabled, the buffer status will be logged every x ms

    Remember this buffer takes memory! maxLogMessageSize * maxLogMessages bytes at least */
void Elog::globalSettings(uint16_t maxLogMessageSize,
    uint16_t maxLogMessages,
    Stream& internalLogDevice,
    Loglevel internalLogLevel,
    bool discardMsgWhenBufferFull,
    uint32_t reportStatusEvery)
{
    // Default values for all settings
    settings.maxLogMessageSize = maxLogMessageSize > 5 ? maxLogMessageSize : 50;
    settings.maxLogMessages = maxLogMessages > 0 ? maxLogMessages : 1; // Sanity check
    settings.internalLogDevice = &internalLogDevice;
    settings.internalLogLevel = internalLogLevel;
    settings.discardMsgWhenBufferFull = discardMsgWhenBufferFull;
    settings.reportStatusEvery = reportStatusEvery;
};

/* This starts the writerTask, if it hasn't already been started (writerTask is responsible for
   emptying the ringbuffer for both serial and sd card.)
   It should be called when addFileLoggin or addSerialLogging is called.
   Here we also create the ringbuffer */
void Elog::writerTaskStart()
{
    static bool started = false;
    if (started == false) {
        logInternal(INFO, "Allocating buffer for %d log messages. Max message size %d chars. %d bytes of memory used",
            settings.maxLogMessages,
            settings.maxLogMessageSize,
            settings.maxLogMessages * settings.maxLogMessageSize);
        logRingBuff.createBuffer(settings.maxLogMessages, settings.maxLogMessageSize);

        logInternal(DEBUG, "Creating Writer task");
        xTaskCreate(
            writerTask, // Task function.
            "writeTask", // String with name of task.
            5000, // Stack size in bytes. This seems enough for it not to crash.
            NULL, // Parameter passed as input of the task.
            1, // Priority of the task.
            NULL); // Task handle.
        started = true;
    }
}

/* This is the writer task that is started with writerTaskStart() almost in the begining.
   It loops forever and runs two methods. One for handling the sd card, and one for serial */
void Elog::writerTask(void* parameter)
{
    static LogLineEntry logLineEntry;
    static char* logLineMessage = new char[settings.maxLogMessageSize];

    while (true) {
        while (writerTaskHold) { // Do nothing if we are instructed to pause.
            vTaskDelay(1);
        }

        reportStatus();

        if (logRingBuff.pop(logLineEntry, logLineMessage)) {
            if (logLineEntry.logSerial) {
                outputSerial(logLineEntry, logLineMessage);
            }
            if (spiffsMounted && logLineEntry.logSpiffs) {
                outputSpiffs(logLineEntry, logLineMessage);
            }
#ifndef LOGGER_DISABLE_SD
            if (logLineEntry.logFile) {
                outputSd(logLineEntry, logLineMessage);
            }
#endif
        }
        vTaskDelay(1); // Feed the watchdog. It will trigger and crash if we dont give it some time
    }
}

/* Every 10 seconds we check the space on spiffs. This is time consuming, thats why we do it rarely
   If we have less than 20k free space, then the oldest log files are removed until we have more
   than 20k free again.
   When filesystem gets full deleting seems to crash the esp. */
void Elog::spiffsEnsureFreeSpace()
{
    static uint32_t lastStatus = 0;

    if (millis() - lastStatus > settings.spiffsCheckSpaceEvery) {
        logInternal(DEBUG, "Checking diskspace on spiffs");
        File root = LittleFS.open("/logs");
        File file = root.openNextFile();

        while ((LittleFS.totalBytes() - LittleFS.usedBytes()) < settings.spiffsMinimumSpace) {
            spiffsFileHandle.close(); // Close our open file, just in case we are deleting ourself
            char filename[30];
            sprintf(filename, "/logs/%s", file.name());
            logInternal(WARNING, "Deleting log file \"%s\" to free space", filename);
            file = root.openNextFile();
            LittleFS.remove(filename);
        }
        lastStatus = millis();
    }
}

/* This is called from writerTask when some data has been popped from the ringbuffer.
   then it's written to the serial device. It just dumps the logline to the serial interface.
   It is also called from logInternal, because we want to log here without touching the buffer */
void Elog::outputSerial(const LogLineEntry& logLineEntry, const char* logLineMessage)
{
    static char logStamp[45];

    const char* serviceName;
    Stream* serialPtr;

    if (logLineEntry.internalLogMessage) {
        serviceName = "LOG";
        serialPtr = settings.internalLogDevice;
    } else {
        LogService* service = logLineEntry.service;
        serviceName = service->serialServiceName;
        serialPtr = service->serialPortPtr;
    }
    getLogStamp(logLineEntry.logTime, logLineEntry.loglevel, serviceName, logStamp);
    serialPtr->print(logStamp);
    serialPtr->print(logLineMessage);

    if (strlen(logLineMessage) == settings.maxLogMessageSize - 1) {
        serialPtr->print("...");
    }
    serialPtr->println();
}

/* This is called from writerTask when some data has been popped from the ringbuffer.
   then it's written to the spiffs filesystem. It just dumps the logline to the current spiffs logfile. */
void Elog::outputSpiffs(const LogLineEntry& logLineEntry, const char* logLineMessage)
{
    spiffsEnsureFreeSpace();

    static char logStamp[45];
    const char* serviceName;
    static uint32_t lastFlushTime = 0;

    LogService* service = logLineEntry.service;
    serviceName = service->spiffsServiceName;
    getLogStamp(logLineEntry.logTime, logLineEntry.loglevel, serviceName, logStamp);

    if (!spiffsFileHandle) {
        logInternal(INFO, "Apending to spiffs file \"%s\"", spiffsFileName);
        spiffsFileHandle = LittleFS.open(spiffsFileName, FILE_APPEND);
    }

    size_t expectedBytes = strlen(logStamp) + strlen(logLineMessage) + 2; // 2 chars for endline
    size_t bytesWritten;
    if (spiffsFileHandle) {
        bytesWritten = spiffsFileHandle.print(logStamp);
        bytesWritten += spiffsFileHandle.print(logLineMessage);

        if (strlen(logLineMessage) == settings.maxLogMessageSize - 1) {
            bytesWritten += spiffsFileHandle.print("...");
            expectedBytes += 3;
        }
        bytesWritten += spiffsFileHandle.println();

        if (bytesWritten != expectedBytes) {
            logInternal(WARNING, "COULD NOT WRITE TO SPIFFS");
            loggerStatus.spiffsMsgNotWritten++;
        } else {
            loggerStatus.spiffsMsgWritten++;
            loggerStatus.spiffsBytesWritten += bytesWritten;
        }

        if (millis() - lastFlushTime > settings.spiffsSyncEvery) {
            logInternal(DEBUG, "Syncronizing spiffs logfile. Writing dirty cache");
            spiffsFileHandle.flush();
            lastFlushTime = millis();
        }
    } else {
        logInternal(WARNING, "Could not append to spiffs log file %s", spiffsFileName);
    }
}

/* Attach spiffs logging to the Logger instance. Parameters:
   serviceName: The servicename that is stamped on each logline (will be truncated to 3 characters)
   wantedLogLevel: Everything under or equal to this level will be logged to the serial device */
void Elog::addSpiffsLogging(const char* serviceName, const Loglevel wantedLogLevel)
{
    writerTaskStart(); // Make sure that the writerTask is running
    if (spiffsConfigured) {
        if (!service.spiffsEnabled) {
            logInternal(INFO, "Adding spiffs logging with service name=\"%s\"", serviceName);
            service.spiffsServiceName = (char*)serviceName;
            service.spiffsEnabled = true;
            service.spiffsWantedLoglevel = wantedLogLevel;
        } else {
            logInternal(ERROR, "You can only add spiffs logging once");
        }
    } else {
        logInternal(ERROR, "Please run configureSpiffs() before adding spiffs logging");
    }
}

/* Attach serial logging to the Logger instance. Parameters:
   serialPort: a straming devices where logging should go to. eg Serial, Serial1, Serial2
   serviceName: The servicename that is stamped on each logline (will be truncated to 3 characters)
   wantedLogLevel: Everything under or equal to this level will be logged to the serial device */
void Elog::addSerialLogging(Stream& serialPort, const char* serviceName, const Loglevel wantedLogLevel)
{
    writerTaskStart(); // Make sure that the writerTask is running
    if (!service.serialEnabled) {
        logInternal(INFO, "Adding serial logging with service name=\"%s\"", serviceName);
        service.serialPortPtr = &serialPort;
        service.serialServiceName = (char*)serviceName;
        service.serialWantedLoglevel = wantedLogLevel;
        service.serialEnabled = true;
    } else {
        logInternal(ERROR, "You can only add serial logging once");
    }
}

/* Private internal logging method that is only used inside this logging class.
   It's for sending a formatted logline to serial device INTERNAL_LOG_DEVICE. Parameters:
   loglevel: Loglevel of the message
   format: sprintf format of the logmessages (contains %s %d %f etc)
   ...: Optional parameters for sprintf
   It is not buffering output, because most logging is comming from the writer task*/
void Elog::logInternal(const Loglevel loglevel, const char* format, ...)
{
    static LogLineEntry logLineEntry; // Static to be kind to stack.
    static char* logLineMessage = new char[settings.maxLogMessageSize];

    if (loglevel <= settings.internalLogLevel) {
        logLineEntry.logTime = millis(); // Get the time as early as possible
        logLineEntry.internalLogMessage = true;
        logLineEntry.loglevel = loglevel;

        va_list args; // variable arguments list
        va_start(args, format); // initialize the list
        vsnprintf(logLineMessage, settings.maxLogMessageSize, format, args); // format the string
        va_end(args); // end the list

        outputSerial(logLineEntry, logLineMessage); // Not buffered output.
    }
}

/* Public method. When called it will do logging both to serial and the filesystem depending
   on how this is configured. Parameters:
   loglevel: Loglevel of the message (EMERGENCY, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO, DEBUG)
   format: vsnprintf format of the logmessages (contains %s %d %f etc. Eg: "Var=%s")
   ...: Optional parameters for vsnprintf */
void Elog::log(const Loglevel logLevel, const char* format, ...)
{
    static LogLineEntry logLineEntry; // Static to be kind to stack.
    static char* logLineMessage = new char[settings.maxLogMessageSize];
    logLineEntry.logTime = millis(); // We want the current time as early as possible. Close to realtime.

    if (service.sdEnabled || service.serialEnabled || service.spiffsEnabled) {
        Loglevel wantedLogLevelSerial = service.serialWantedLoglevel;
        Loglevel wantedLogLevelFile = service.sdWantedLoglevel;
        Loglevel wantedLogLevelSpiffs = service.spiffsWantedLoglevel;

        // Do we want logging? Is the loglevel correct for logging this log entry?
        logLineEntry.logFile = (logLevel <= wantedLogLevelFile && service.sdEnabled);
        logLineEntry.logSerial = (logLevel <= wantedLogLevelSerial && service.serialEnabled);
        logLineEntry.logSpiffs = (logLevel <= wantedLogLevelSpiffs && service.spiffsEnabled);

        // For optimization va-args are checked and handled in here. It's cpu intensive. Only if needed!
        if (logLineEntry.logFile || logLineEntry.logSerial || logLineEntry.logSpiffs) {

            logLineEntry.loglevel = logLevel;
            logLineEntry.service = &service;

            va_list args; // variable arguments list
            va_start(args, format); // initialize the list
            vsnprintf(logLineMessage, settings.maxLogMessageSize, format, args); // format the log message
            va_end(args); // end the list

            addToRingbuffer(logLineEntry, logLineMessage);
        }
    } else {
        logInternal(WARNING, "Please run addFileLogging(), addSerialLogging() or addSpiffsLogging() on your instance before logging.");
    }
}

/* Add a log line to the ringbuffer. In this method we handle if message should be discarded
   if the buffer is full, or if we should wait for space in the buffer before we give back time to the user.
   if settings.discardMsgWhenBufferFull is false we will never loose any messages, but the logger
   may pause sometimes if the buffer gets full (busy waiting). If your application is time sensitive it
   can be better to discard messages if the buffer is full. Then set discardMsgWhenBufferFull to true */
void Elog::addToRingbuffer(const LogLineEntry& logLineEntry, const char* logLineMessage)
{
    if (logRingBuff.push(logLineEntry, logLineMessage)) {
        loggerStatus.bufferMsgAdded++;
    } else {
        if (settings.discardMsgWhenBufferFull) { // BUFFER FULL - bad, it will just be discarded.
            loggerStatus.bufferMsgNotAdded++;
        } else {
            while (logRingBuff.isFull()) { // Get one space in buffer to add the message
                yield();
            }
            logRingBuff.push(logLineEntry, logLineMessage);
            loggerStatus.bufferMsgAdded++;
        }
    }
}

void Elog::configureSpiffs(uint32_t spiffsSyncEvery, uint32_t spiffsCheckSpaceEvery, uint32_t spiffsMinimumSpace)
{
    if (!spiffsConfigured) {
        logInternal(DEBUG, "Configuring spiffs");
        settings.spiffsSyncEvery = spiffsSyncEvery;
        settings.spiffsCheckSpaceEvery = spiffsCheckSpaceEvery;
        settings.spiffsMinimumSpace = spiffsMinimumSpace;

        spiffsPrepare();
        spiffsConfigured = true;
    } else {
        logInternal(ERROR, "You can only configure spiffs logging once");
    }
}

/* Whenever you want to inspect the spiffs logs, you call this. It gives a simple command line interface with these commands:
   L<enter> list all logfiles on filesystem
   Pxx<enter> prints/dumps the logfile with number xx to the terminal
   F<enter> format the spiffs file system
   Q<enter> Quit and return to the code calling.

   This method busywaits on serial interface and runs spiffsProcessCommand() when a linefeed is received on terminal. */
void Elog::spiffsQuery(Stream& serialPort)
{
    writerTaskHold = true;
    Serial.println("\nQuery spiffs log!");
    Serial.println("\nCommands: L (List files), Px (Print file number x), F (Format spiffs), Q (Quit)");

    bool end = false;
    static char commandBuffer[10];
    static int commandLength = 0;

    do {
        while (serialPort.available()) {
            char c = serialPort.read();

            if (c == '\r') {
                continue;
            } else if (c == '\n') {
                commandBuffer[commandLength] = '\0';
                serialPort.println();
                end = spiffsProcessCommand(serialPort, commandBuffer); // Process the command when newline character is received
                commandLength = 0; // Clear the command buffer
            } else {
                if (commandLength < 9) {
                    serialPort.print(c);
                    commandBuffer[commandLength] = c;
                    commandLength++;
                }
            }
        }
    } while (!end);

    writerTaskHold = false;
    logInternal(INFO, "Query log ended. Returning to code.");
}

/* When commands are received on serial they are sent here. Commands are processed
   returns true if "Q" has been received. The user wants to get out of the QuerySpiffsLog() */
bool Elog::spiffsProcessCommand(Stream& serialPort, const char* command)
{
    if (command[0] == 'L' || command[0] == 'l') {
        spiffsListLogFiles(serialPort);
    } else if (command[0] == 'P' || command[0] == 'p') {
        spiffsPrintLogFile(serialPort, command + 1);
    } else if (command[0] == 'F' || command[0] == 'f') {
        spiffsFormat(serialPort);
    } else if (command[0] == 'Q' || command[0] == 'q') {
        return true;
    } else {
        serialPort.printf("Unknown command: \"%s\"", command);
    }
    serialPort.println("\nCommands: L (List files), Px (Print file number x), F (Format spiffs), Q (Quit)");
    return false;
}

/* Formats the spiffs filesystem */
void Elog::spiffsFormat(Stream& serialPort)
{
    serialPort.println("Formatting spiffs. Please wait");
    spiffsFileHandle.close();
    LittleFS.format();
    serialPort.println("Formatted!");
    spiffsPrepare();
}

/* Given a filename, the file is read from spiffs and dumped to the serial terminal */
void Elog::spiffsPrintLogFile(Stream& serialPort, const char* filename)
{
    char fullFilename[30];
    sprintf(fullFilename, "/logs/%05d", atoi(filename)); // All logfiles are in format "00001"
    File logFile = LittleFS.open(fullFilename, FILE_READ);

    if (!logFile) {
        serialPort.printf("Log file \"%s\" not found\n", filename);
    } else {
        serialPort.printf("Print logfile \"%s\"\n---------------------------\n", filename);
        while (logFile.available()) {
            serialPort.write(logFile.read());
        }
    }
    logFile.close();
}

/* List all log files with size on the spiffs */
void Elog::spiffsListLogFiles(Stream& serialPort)
{
    File root = LittleFS.open("/logs");
    File file = root.openNextFile();

    serialPort.println("List of logfiles\n----------------");
    while (file) {
        uint16_t logNumber = atoi(file.name());
        serialPort.printf("%d (%d bytes)\n", logNumber, file.size());
        file = root.openNextFile();
    }
}

/* Method to take some byte data and present it nicely in hexformat. Eg: 12:5f:24:02...  Parameters:
   data: a pointer to some byte data
   len: Length of the data in bytes
   Returns a char* to the hex-string. This is static, so be carefull how you call and use this */
char* Elog::toHex(byte* data, uint16_t len)
{
    static char hexString[LOG_MAX_HEX_STRING_LENGTH];
    bool tooLong = false;

    if (len * 3 - 1 >= LOG_MAX_HEX_STRING_LENGTH) {
        logInternal(WARNING, "Hex size is longer than %d. Please change LOG_MAX_HEX_STRING_LENGTH", LOG_MAX_HEX_STRING_LENGTH);
        tooLong = true;
        len = LOG_MAX_HEX_STRING_LENGTH / 3 - 3; // Leave space for dots in the end
    }

    char* ptr = hexString;
    for (uint16_t i = 0; i < len; i++) {
        sprintf(ptr, "%02X", data[i]);
        ptr += 2;
        if (i < len - 1) {
            ptr++[0] = ':';
        }
    }
    if (tooLong) {
        strcpy(ptr, "...");
    } else {
        ptr++[0] = '\0';
    }

    return hexString;
}

/* Same as the byte version. This just takes a char* instead */
char* Elog::toHex(char* data)
{
    return toHex((byte*)data, strlen(data));
}

/*  Returns time in "output" since boot in format eg: 000:00:00:01:065
    Return string length in chars */
uint8_t Elog::getTimeStringMillis(uint32_t milliSeconds, char* output)
{
    uint32_t seconds, minutes, hours, days; // avoid memory fragmentation.
    seconds = milliSeconds / 1000;
    minutes = seconds / 60;
    hours = minutes / 60;
    days = hours / 24;
    uint16_t length = sprintf(output, "%03u:%02u:%02u:%02u:%03u", days, hours % 24, minutes % 60, seconds % 60, milliSeconds % 1000);

    return length;
}

/* Given a string with a logservice name it returns to "output" a string formatted like [XXX].
   It will always be 5 chars long. If length of str is less than 3 it will be padded with spaces.
   Returns string length in chars */
uint8_t Elog::getServiceString(const char* serviceName, char* output)
{
    output[0] = '[';
    for (int i = 0; i < 3; i++) {
        if (serviceName[i] != '\0') {
            output[i + 1] = std::toupper(serviceName[i]);
        } else {
            output[i + 1] = ' ';
        }
    }
    output[4] = ']';
    output[5] = '\0';
    return 5;
}

/* Given a loglevel it returns a string formatted like [ALERT] in "output"
   It will always be 7 chars long.
   Returns string length in chars */
uint8_t Elog::getLogLevelString(const Loglevel logLevel, char* output)
{
    const char* logLevelStrings[] = { "[EMERG]", "[ALERT]", "[CRIT ]", "[ERROR]", "[WARN ]", "[NOTIC]", "[INFO ]", "[DEBUG]" };
    strcpy(output, logLevelStrings[logLevel]);
    return 7;
}

/* Formats the stamp for the logline. This is use for serial output. like this:
   000:00:01:35:365 [LOG] [DEBUG] : xxxxxxxxxxxx
   stamp is returned to "output" var */
void Elog::getLogStamp(const uint32_t logTime, const Loglevel loglevel, const char* service, char* output)
{
    output += getTimeStringMillis(logTime, output);
    output[0] = ' ';
    output++;

    output += getServiceString(service, output);
    output[0] = ' ';
    output++;

    output += getLogLevelString(loglevel, output);

    strcpy(output, " : ");
}

/* Formats the stamp for the logline. This is use for sd card output. like this:
   000:00:01:35:365 [DEBUG] : xxxxxxxxxxxx
   stamp is returned to "output" var */
void Elog::getLogStamp(const uint32_t logTime, const Loglevel loglevel, char* output)
{
    output += getTimeStringMillis(logTime, output);
    output[0] = ' ';
    output++;

    output += getLogLevelString(loglevel, output);

    strcpy(output, " : ");
}

/* ----------------------------------- Ring Buffer ----------------------------------- */

/* This ringbuffer is useless unless this method is called first.
   It dynamically allocates memory for the ringbuffer. Don´t make it too large
   or it will fail misserably!
   Only call it once! */
void LogRingBuff::createBuffer(size_t logLineCapacity, size_t logLineMessageSize)
{
    this->logLineCapacity = logLineCapacity;
    this->logLineMessageSize = logLineMessageSize;

    try {
        logLineEntries = new LogLineEntry[logLineCapacity];
        logMessages = new char*[logLineCapacity];

        for (uint32_t i = 0; i < logLineCapacity; i++) {
            logMessages[i] = new char[logLineMessageSize];
        }
    } catch (const std::bad_alloc& e) {
        Serial.println("PANIC: Not enough memory for the Logger buffer! You will crash soon!");
        delay(60 * 1000);
    }
}

/* Push data to the ringbuffer. parameters:
   LogLineEntry: Info about the logline (if it goes to file, sd, loglevel etc)
   LogLineMessage: The raw formattet log text after vsnprintf
   Returns false if buffer is full */
bool LogRingBuff::push(const LogLineEntry& logLineEntry, const char* logLineMessage)
{
    if (isFull()) {
        return false;
    }

    noInterrupts();
    logLineEntries[rear_] = logLineEntry; // Structs copied by assignment
    strncpy(logMessages[rear_], logLineMessage, logLineMessageSize); // char arrays need to be copied
    rear_ = (rear_ + 1) % logLineCapacity;
    size_++;
    interrupts();
    return true;
}

/* Pops data from the ringbuffer. parameters:
   LogLineEntry: Info about the logline (goes to file, sd, loglevel etc)
   LogLineMessage: The raw formattet log text
   Returns false if there is nothing in the buffer to pop */
bool LogRingBuff::pop(LogLineEntry& logLineEntry, char* logLineMessage)
{
    if (isEmpty()) {
        return false;
    }

    noInterrupts();
    logLineEntry = logLineEntries[front_]; // Structs copied by assignment
    strncpy(logLineMessage, logMessages[front_], logLineMessageSize); // char arrays need to be copied
    front_ = (front_ + 1) % logLineCapacity;
    size_--;
    interrupts();
    return true;
}

/* Returns true if the ringbuffer is empty */
bool LogRingBuff::isEmpty()
{
    noInterrupts();
    size_t s = size_;
    interrupts();
    return s == 0;
}

/* Returns true if the ringbuffer is full */
bool LogRingBuff::isFull()
{
    noInterrupts();
    size_t s = size_;
    size_t c = logLineCapacity;
    interrupts();
    return s == c;
}

/* Returns the number of elements in the ringbuffer */
size_t LogRingBuff::size()
{
    noInterrupts();
    size_t s = size_;
    interrupts();
    return s;
}

/* Returns the maximum allowed elements in the ringbuffer */
size_t LogRingBuff::capacity()
{
    noInterrupts();
    size_t c = logLineCapacity;
    interrupts();
    return c;
}

/* Return how many percent of the ringbuffer is used. */
uint8_t LogRingBuff::percentageFull()
{
    noInterrupts();
    uint8_t ringBuffPercentage = size_ * 100 / logLineCapacity;
    interrupts();
    return ringBuffPercentage;
}