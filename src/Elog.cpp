#include <Elog.h>

Elog& Elog::getInstance() /**< Singleton pattern */
{
    static Elog instance; /**< Guaranteed to be destroyed, instantiated on first use. */
    return instance;
}

/** Start the logger
 * @param logLineCapacity the capacity of the log line buffer (number of log lines)
 * @param waitIfBufferFull if true, the logger will wait for space in the buffer. If false, it will discard the log message
 * if this is not called by user, it will be called internally by the first log message with default values
 */
void Elog::configure(uint16_t logLineCapacity, bool waitIfBufferFull)
{
    if (logStarted) {
        logInternal(ELOG_LEVEL_ERROR, "Logger already started!");
        return;
    }

    this->waitIfBufferFull = waitIfBufferFull;
    bufferStats.messagesBuffered = 0;
    bufferStats.messagesDiscarded = 0;

    if (!ringBuff.buffCreate(logLineCapacity)) { //  Create ring buffer for log lines
        panic("Failed to create log buffer! Not enough heap memory!");
        return;
    }

    logSerial.begin();
    logSD.begin();
    logSpiffs.begin();

    logStarted = true;
    writerTaskStart(); /**< background task to write logs to the output devices */

    logInternal(ELOG_LEVEL_NOTICE, "Logger started with buffer capacity: %d messages", logLineCapacity);
}

/** Log a message
 * @param logId the id of the log (must first be registered with registerSerial, registerSd or registerSpiffs)
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS)
 * @param format the format of the log message (like printf)
 */
void Elog::log(uint8_t logId, uint8_t logLevel, const char* format, ...)
{
    if (!logStarted) {
        Logger.configure();
    }
    if (logLevel > ELOG_LEVEL_VERBOSE) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS are the valid levels!");
        return;
    }

    if (mustLog(logId, logLevel)) {
        va_list args; /**< First find size of the log message */
        va_start(args, format); /**< initialize the list */
        uint16_t logLineSize = vsnprintf(NULL, 0, format, args); /**< check the size of the log message */
        va_end(args); /**< end the list */

        char* logLineMessage;
        try {
            logLineMessage = new char[logLineSize + 1]; /**< reserve memory for the log message + null terminator */
        } catch (const std::bad_alloc& e) {
            panic("Failed to allocate heap memory for log message! Not logged!");
            return;
        }

        va_start(args, format);
        vsnprintf(logLineMessage, logLineSize + 1, format, args); /**< format the log message */
        va_end(args); /**< end the list */

        LogLineEntry logLineEntry;
        logLineEntry.timestamp = millis();
        logLineEntry.logId = logId;
        logLineEntry.logLevel = logLevel;
        logLineEntry.internalLogDevice = nullptr;
        logLineEntry.logMessage = logLineMessage;

        buffAddLogLine(logLineEntry);
    }
}

void Elog::log(uint8_t logId, uint8_t logLevel, const __FlashStringHelper* format, ...)
{
    const char* p = (const char*)format;

    if (!logStarted) {
        Logger.configure();
    }
    if (logLevel > ELOG_LEVEL_VERBOSE) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS are the valid levels!");
        return;
    }

    if (mustLog(logId, logLevel)) {
        va_list args; /**< First find size of the log message */
        va_start(args, format); /**< initialize the list */
        uint16_t logLineSize = vsnprintf_P(NULL, 0, p, args); /**< check the size of the log message */
        va_end(args); /**< end the list */

        char* logLineMessage;
        try {
            logLineMessage = new char[logLineSize + 1]; /**< reserve memory for the log message + null terminator */
        } catch (const std::bad_alloc& e) {
            panic("Failed to allocate heap memory for log message! Not logged!");
            return;
        }

        va_start(args, format);
        vsnprintf(logLineMessage, logLineSize + 1, p, args); /**< format the log message */
        va_end(args); /**< end the list */

        LogLineEntry logLineEntry;
        logLineEntry.timestamp = millis();
        logLineEntry.logId = logId;
        logLineEntry.logLevel = logLevel;
        logLineEntry.internalLogDevice = nullptr;
        logLineEntry.logMessage = logLineMessage;

        buffAddLogLine(logLineEntry);
    }
}

/** Log a message with hex data
 * @param logId the id of the log (must first be registered with registerSerial, registerSd or registerSpiffs)
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS)
 * @param message a message to prepend to the hex data
 * @param data the data to log in hex format (should be typecasted to uint8_t*)
 * @param length the length of the data
 */
void Elog::logHex(uint8_t logId, uint8_t logLevel, const char* message, const uint8_t* data, uint16_t length)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_VERBOSE) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS are the valid levels!");
        return;
    }

    char hexData[length * 3 + 1]; /**< +1 for null terminator */
    for (uint16_t i = 0; i < length; i++) {
        sprintf(hexData + i * 3, "%02X:", data[i]);
    }
    hexData[length * 3 - 1] = '\0'; /**< replace the last ':' with null terminator */

    char* logLineMessage;
    try {
        logLineMessage = new char[strlen(message) + strlen(hexData) + 2]; // +2 for space and null terminator */
    } catch (const std::bad_alloc& e) {
        panic("Failed to allocate heap memory for loghex message! Not logged!");
        return;
    }

    sprintf(logLineMessage, "%s %s", message, hexData);

    LogLineEntry logLineEntry;
    logLineEntry.timestamp = millis();
    logLineEntry.logId = logId;
    logLineEntry.logLevel = logLevel;
    logLineEntry.internalLogDevice = nullptr;
    logLineEntry.logMessage = logLineMessage;

    buffAddLogLine(logLineEntry);
}

/** Configure the serial port for logging. If this is not called by the user a default configuration of 10 will be used
 * @param maxRegistrations the maximum number of serial ports to register. Default is 10
 */
void Elog::configureSerial(const uint8_t maxRegistrations)
{
    if (!logStarted) {
        configure();
    }
    logSerial.configure(maxRegistrations);
}

/** Register a serial port for logging
 * @param logId the id of the log
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG)
 * @param serviceName the name of the service
 * @param serial the serial port to log to (hardware or software serial). Default is "Serial"
 * @param logFlags flags for the log (see LogFlags.h)
 */
void Elog::registerSerial(const uint8_t logId, const uint8_t logLevel, const char* serviceName, Stream& serial, const uint8_t logFlags)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSerial.registerSerial(logId, logLevel, serviceName, serial, logFlags);
}

uint8_t Elog::getSerialLogLevel(const uint8_t logId, Stream& serial)
{
    if (!logStarted) {
        configure();
    }
    return logSerial.getLogLevel(logId, serial);
}

void Elog::setSerialLogLevel(const uint8_t logId, const uint8_t logLevel, Stream& serial)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSerial.setLogLevel(logId, logLevel, serial);
}

uint8_t Elog::getSerialLastMsgLogLevel(const uint8_t logId, Stream& serial)
{
    if (!logStarted) {
        configure();
    }
    return logSerial.getLastMsgLogLevel(logId, serial);
}

#ifdef ELOG_SPIFFS_ENABLE
/**
 * Configure the SPIFFS. If this is not called by the user a default configuration of 10 will be used
 * @param maxRegistrations the maximum number of log files to register. Default is 10
 */
void Elog::configureSpiffs(const uint8_t maxRegistrations)
{
    if (!logStarted) {
        configure();
    }
    logSpiffs.configure(maxRegistrations);
}

/**
 * Register a SPIFFS for logging
 * @param logId the id of the log
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG)
 * @param fileName the name of the file to log to
 * @param logFlags flags for the log (see LogFlags.h)
 * @param maxLogFileSize the maximum size of each log file in bytes. When the file reaches this size, it will be closed and a new file will be created
 */
void Elog::registerSpiffs(const uint8_t logId, const uint8_t logLevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSpiffs.registerSpiffs(logId, logLevel, fileName, logFlags, maxLogFileSize);
}

uint8_t Elog::getSpiffsLogLevel(const uint8_t logId, const char* fileName)
{
    if (!logStarted) {
        configure();
    }
    return logSpiffs.getLogLevel(logId, fileName);
}

void Elog::setSpiffsLogLevel(const uint8_t logId, const uint8_t logLevel, const char* fileName)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSpiffs.setLogLevel(logId, logLevel, fileName);
}

uint8_t Elog::getSpiffsLastMsgLogLevel(const uint8_t logId, const char* fileName)
{
    if (!logStarted) {
        configure();
    }
    return logSpiffs.getLastMsgLogLevel(logId, fileName);
}

#endif // ELOG_SPIFFS_ENABLE

#ifdef ELOG_SD_ENABLE
/**
 * Configure the SD card
 * @param spi the SPI bus to use
 * @param cs the chip select pin
 * @param speed the speed of the SPI bus
 * @param spiOption the SPI option (DEDICATED_SPI or SHARED_SPI). Default is DEDICATED_SPI
 * @param maxRegistrations the maximum number of log files to register. Default is 10
 */
void Elog::configureSd(SPIClass& spi, uint8_t cs, uint32_t speed, uint8_t spiOption, uint8_t maxRegistrations)
{
    if (!logStarted) {
        configure();
    }
    logSD.configure(spi, cs, speed, spiOption, maxRegistrations);
    logInternal(ELOG_LEVEL_INFO, "SD configured with max registrations: %d", maxRegistrations);
}

/** Register a SD card for logging
 * @param logId the id of the log
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG)
 * @param fileName the name of the file to log to
 * @param logFlags flags for the log (see LogFlags.h)
 * @param maxLogFileSize the maximum size of each log file in bytes. When the file reaches this size, it will be closed and a new file will be created
 */
void Elog::registerSd(const uint8_t logId, const uint8_t logLevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSD.registerSd(logId, logLevel, fileName, logFlags, maxLogFileSize);
}

uint8_t Elog::getSdLogLevel(const uint8_t logId, const char* fileName)
{
    if (!logStarted) {
        configure();
    }
    return logSD.getLogLevel(logId, fileName);
}

void Elog::setSdLogLevel(const uint8_t logId, const uint8_t logLevel, const char* fileName)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSD.setLogLevel(logId, logLevel, fileName);
}

uint8_t Elog::getSdLastMsgLogLevel(const uint8_t logId, const char* fileName)
{
    if (!logStarted) {
        configure();
    }
    return logSD.getLastMsgLogLevel(logId, fileName);
}

#endif // ELOG_SD_ENABLE

#ifdef ELOG_SYSLOG_ENABLE
/**
 * Configure the syslog server
 * @param server the IP address of the syslog server
 * @param port the port of the syslog server. Default is 514
 * @param hostname the hostname of the device. Default is "esp32"
 * @param maxRegistrations the maximum number of registrations. Default is 10
 */
void Elog::configureSyslog(const char* server, uint16_t port, const char* hostname, const uint8_t maxRegistrations)
{
    if (!logStarted) {
        configure();
    }
    logSyslog.configure(server, port, hostname, maxRegistrations);
}

/** Register a Syslog server for logging
 * @param logId the id of the log
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG)
 * @param facility the facility of the log (FAC_KERN, FAC_USER, FAC_MAIL, FAC_DAEMON, FAC_AUTH, FAC_SYSLOG, FAC_LPR, FAC_NEWS, FAC_UUCP, FAC_CRON, FAC_AUTHPRIV, FAC_FTP, FAC_NTP, FAC_LOG_AUDIT, FAC_LOG_ALERT, FAC_CLOCK_DAEMON, FAC_LOCAL0, FAC_LOCAL1, FAC_LOCAL2, FAC_LOCAL3, FAC_LOCAL4, FAC_LOCAL5, FAC_LOCAL6, FAC_LOCAL7)
 * @param appName the name of the application
 */
void Elog::registerSyslog(const uint8_t logId, const uint8_t logLevel, const uint8_t facility, const char* appName)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSyslog.registerSyslog(logId, logLevel, facility, appName);
}

uint8_t Elog::getSyslogLogLevel(const uint8_t logId, const uint8_t facility)
{
    if (!logStarted) {
        configure();
    }
    return logSyslog.getLogLevel(logId, facility);
}

void Elog::setSyslogLogLevel(const uint8_t logId, const uint8_t logLevel, const uint8_t facility)
{
    if (!logStarted) {
        configure();
    }
    if (logLevel > ELOG_LEVEL_NOLOG) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid logLevel! VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, NOLOG are the valid levels!");
        return;
    }
    logSyslog.setLogLevel(logId, logLevel, facility);
}

uint8_t Elog::getSyslogLastMsgLogLevel(const uint8_t logId, const uint8_t facility)
{
    if (!logStarted) {
        configure();
    }
    return logSyslog.getLastMsgLogLevel(logId, facility);
}

#endif // ELOG_SYSLOG_ENABLE

/**
 * Configure the internal logging
 * @param internalLogDevice the device to log internal messages to (like Serial)
 * @param internalLogLevel the level of the internal log messages (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS, NOLOG). Default is ERROR
 * @param statsEvery the time in milliseconds to output the log stats. Default is 10000 ms
 */
void Elog::configureInternalLogging(Stream& internalLogDevice, uint8_t internalLogLevel, uint16_t statsEvery)
{
    this->internalLogDevice = &internalLogDevice;
    this->internalLogLevel = internalLogLevel;
    this->statsEvery = statsEvery;
}

/**
 * Enable query mode. This will allow the user to interact with the logger using the serial port
 * Here you can list directories, change directories, remove files, format the filesystem, etc.
 * To enter query mode, send a space character to the serial port. This will give you the help text and a prompt
 * @param serialPort the serial port to enable query mode on
 */
void Elog::enableQuery(Stream& serialPort)
{
    if (!logStarted) {
        configure();
    }
    logSpiffs.enableQuery(serialPort);
    logSD.enableQuery(serialPort);
    logSerial.enableQuery(serialPort);
    logSyslog.enableQuery(serialPort);

    queryEnabled = true;
    querySerial = &serialPort;
    Logger.logInternal(ELOG_LEVEL_INFO, "Query enabled on serial port! Send a space character to activate the query mode");
}

/**
 * Provide the time to the Logger. This will set the RTC clock time (used for timestamping log files)
 * You can also just point set the time with NTP using configTime() from time.h
 * @param year the year
 * @param month the month
 * @param day the day
 * @param hour the hour
 * @param minute the minute
 * @param second the second
 */
void Elog::provideTime(const uint16_t year, const uint8_t month, const uint8_t day, const uint8_t hour, const uint8_t minute, const uint8_t second)
{
    logInternal(ELOG_LEVEL_INFO, "Time provided: %d-%d-%d %d:%d:%d", year, month, day, hour, minute, second);

    // Set RTC clock time (used for timestamping log files)
    struct tm timeInfo = { second, minute, hour, day, month - 1, year - 1900 };
    struct timeval tv = { mktime(&timeInfo), 0 };
    settimeofday(&tv, NULL);
}

/**
 * Start the writer task. This task will write the logs to the output devices
 */
void Elog::writerTaskStart()
{
    TaskHandle_t handleWriterTask = (TaskHandle_t)xTaskCreate(
        writerTask, // Task function.
        "writeTask", // String with name of task.
        5000, // Stack size in bytes. This seems enough for it not to crash.
        this, // Parameter passed as input of the task.
        1, // Priority of the task.
        NULL); // Task handle.
    if (handleWriterTask == NULL) {
        panic("Failed to create log task!");
        return;
    }
    logInternal(ELOG_LEVEL_DEBUG, "Log writer task started.");
}

/**
 * The writer task. This task will write the logs to the output devices
 * if query mode is enabled, it will also handle the query commands
 * @param parameter the parameter passed from the task creation. It is the Elog instance
 */
void Elog::writerTask(void* parameter)
{
    Elog& elog = *(Elog*)parameter;
    while (true) {
        elog.outputStats();
        elog.outputFromBuffer();
        if (elog.queryEnabled) {
            elog.queryHandleSerialInput();
        }
        vTaskDelay(1);
    }
}

/**
 * Output the logs from the ring buffer to the output devices
 * if query mode is enabled, serial output will be disabled
 */
void Elog::outputFromBuffer()
{
    uint32_t started = millis();
    if (!ringBuff.buffIsEmpty()) {
        LogLineEntry logLineEntry;
        ringBuff.buffPop(logLineEntry);

        bool muteSerial = queryState != QUERY_DISABLED; // if query mode is enabled, mute the serial output

        logSerial.outputFromBuffer(logLineEntry, muteSerial);
        logSD.outputFromBuffer(logLineEntry);
        logSpiffs.outputFromBuffer(logLineEntry);
        logSyslog.outputFromBuffer(logLineEntry);

        delete logLineEntry.logMessage; // clear the memory allocated for the log message
    }
    if (millis() - started > 1000) {
        logInternal(ELOG_LEVEL_WARNING, "It took more than a second to process the last log message! Time used: %d ms", millis() - started);
    }
}
/**
 * Add a log line to the buffer
 * @param logLineEntry the log line entry
 */
void Elog::buffAddLogLine(LogLineEntry& logLineEntry)
{
    if (ringBuff.buffPush(logLineEntry)) {
        bufferStats.messagesBuffered++;
    } else {
        if (waitIfBufferFull) { // BUFFER FULL - bad, it will just be discarded.
            while (ringBuff.buffIsFull()) { // Get one space in buffer to add the message
                delayMicroseconds(100);
            }
            ringBuff.buffPush(logLineEntry);
            bufferStats.messagesBuffered++;
        } else {
            bufferStats.messagesDiscarded++;
            delete logLineEntry.logMessage; // free the memory allocated for the log message
        }
    }
}

/**
 * Check if the log must be output to the output devices
 * @param logId the id of the log
 * @param logLevel the level of the log
 * @return true if the log must be output, false otherwise
 */
bool Elog::mustLog(uint8_t logId, uint8_t logLevel)
{
    bool status = false;
    status |= logSerial.mustLog(logId, logLevel);
    status |= logSD.mustLog(logId, logLevel);
    status |= logSpiffs.mustLog(logId, logLevel);
    status |= logSyslog.mustLog(logId, logLevel);
    status |= (queryState == QUERY_WAITING_FOR_PEEK_QUIT); // if in peek mode, always log
    return status;
}

/**
 * Log an internal message to the configured internal log device (provided with configureInternalLogging)
 * @param logLevel the level of the log (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY)
 * @param format the format of the log message (like printf)
 * @param ... additional arguments to be formatted into the log message
 */
void Elog::logInternal(const uint8_t logLevel, const char* format, ...)
{
    if (queryState == QUERY_DISABLED && logLevel <= internalLogLevel && internalLogLevel != ELOG_LEVEL_NOLOG) {
        va_list args; // first find size of the log message
        va_start(args, format); // initialize the list
        uint16_t logLineSize = vsnprintf(NULL, 0, format, args); // check the size of the log message
        va_end(args); // end the list

        char* logLineMessage;
        try {
            logLineMessage = new char[logLineSize + 1]; // reserve memory for the log message + null terminator
        } catch (const std::bad_alloc& e) {
            panic("Failed to allocate memory for loginternal message! Not logged!");
            return;
        }

        va_start(args, format); // then reserve memory for the log message
        vsnprintf(logLineMessage, logLineSize + 1, format, args); // format the log message
        va_end(args); // end the list

        LogLineEntry logLineEntry;
        logLineEntry.timestamp = millis();
        logLineEntry.logId = 0; // is not used for internal logs
        logLineEntry.logLevel = logLevel;
        logLineEntry.internalLogDevice = internalLogDevice;
        logLineEntry.logMessage = logLineMessage;

        logSerial.outputFromBuffer(logLineEntry, false);
        free(logLineMessage);
    }
}

/**
 * Output the log stats
 * if buffer is full, it will output a warning message
 */
void Elog::outputStats()
{
    static bool bufferFullWarningSent = false;
    static uint8_t maxBuffPct = 0;

    if (ringBuff.buffPercentageFull() > maxBuffPct) {
        maxBuffPct = ringBuff.buffPercentageFull();
    }

    if (ringBuff.buffPercentageFull() < 50) { // When buffer under half full, we clear "full warning".
        bufferFullWarningSent = false;
    }
    if (!bufferFullWarningSent && ringBuff.buffIsFull() && waitIfBufferFull) {
        logInternal(ELOG_LEVEL_WARNING, "Log Buffer was full. Please increase its size.");
        bufferFullWarningSent = true;
    }

    static uint32_t lastOutput = 0;
    if (millis() - lastOutput > statsEvery) {
        logInternal(ELOG_LEVEL_INFO, "Log stats. Messages Buffered: %d, Discarded: %d, Max Buff Pct: %d", bufferStats.messagesBuffered, bufferStats.messagesDiscarded, maxBuffPct);
        logSD.outputStats();
        logSerial.outputStats();
        logSpiffs.outputStats();
        logSyslog.outputStats();

        lastOutput = millis();
        maxBuffPct = 0;
    }
}

/**
 * Panic function. Print the message and enter an infinite loop
 * if the logger cannot continue anymore. Could be memory allocation failure, etc.
 * @param message the message to print
 */
void Elog::panic(const char* message)
{
    Serial.print("PANIC: ");
    Serial.println(message);
}

/**
 * Handle the serial input for query mode
 * is called from the writerTask often. It will handle the query commands typed in the serial port
 */
void Elog::queryHandleSerialInput()
{
    if (querySerial->available()) {
        char c = querySerial->read();
        switch (queryState) {
        case QUERY_DISABLED:
            queryStateDisabled(c);
            break;
        case QUERY_WAITING_FOR_COMMAND:
            queryStateWaitCommand(c);
            break;
        case QUERY_WAITING_FOR_PEEK_QUIT:
            queryStateWaitPeekQuit(c);
            break;
        }
    }
}
/**
 * Process the incoming command from the serial port
 * @param command the command to process
 */
void Elog::queryProcessIncomingCmd(const char* command)
{
    char cmd[LENGTH_COMMAND] = { 0 };
    char param[LENGTH_ABSOLUTE_PATH] = { 0 };
    char param2[LENGTH_ABSOLUTE_PATH] = { 0 };
    char param3[LENGTH_ABSOLUTE_PATH] = { 0 };

    sscanf(command, "%9s %22s %22s %22s", cmd, param, param2, param3);

    if (strcmp(cmd, "dir") == 0) {
        queryCmdDir(param);
    } else if (strcmp(cmd, "cd") == 0) {
        queryCmdCd(param);
    } else if (strcmp(cmd, "type") == 0) {
        queryCmdType(param);
    } else if (strcmp(cmd, "format") == 0) {
        queryCmdFormat();
    } else if (strcmp(cmd, "exit") == 0) {
        queryState = QUERY_DISABLED;
        querySerial->println("Exiting query mode");
        return;
    } else if (strcmp(cmd, "rmdir") == 0) {
        queryCmdRmdir(param);
    } else if (strcmp(cmd, "rm") == 0) {
        queryCmdRm(param);
    } else if (strcmp(cmd, "help") == 0) {
        queryCmdHelp();
    } else if (strcmp(cmd, "spiffs") == 0) {
        queryCmdSpiffs();
    } else if (strcmp(cmd, "sd") == 0) {
        queryCmdSd();
    } else if (strcmp(cmd, "serial") == 0) {
        queryCmdSerial();
    } else if (strcmp(cmd, "syslog") == 0) {
        queryCmdSyslog();
    } else if (strcmp(cmd, "peek") == 0) {
        queryCmdPeek(param, param2, param3);
        return;
    } else if (strcmp(cmd, "status") == 0) {
        queryCmdStatus();
    } else if (strlen(command) == 0) {
        // Do nothing
    } else {
        querySerial->printf("Unknown command: \"%s\"\n", command);
    }
    queryPrintPrompt();
}

/**
 * Handle the serial input when query mode is in state disabled.
 * If a space character is received, it will check if SPIFFS or SD is registered and set the queryDevice
 * @param c the character read from the serial port
 */
void Elog::queryStateDisabled(char c)
{
    if (c == ' ') {
        if (logSpiffs.registeredCount() > 0) {
            queryDevice = SPIFFS;
        } else if (logSD.registeredCount() > 0) {
            queryDevice = SD;
        } else if (logSerial.registeredCount() > 0) {
            queryDevice = SER;
        } else {
            querySerial->println("No SPIFFS,SD or serial registered. Exiting query mode");
            return;
        }

        queryCmdHelp();
        queryPrintPrompt();
        queryState = QUERY_WAITING_FOR_COMMAND;
    }
}

/**
 * Handle the serial input when query mode is in state waiting for command.
 * If a newline character is received, it will process the command using queryProcessIncomingCmd
 * @param c the character read from the serial port
 */
void Elog::queryStateWaitCommand(char c)
{
    if (c == '\r') {
        return;
    } else if (c == '\n') {
        queryCmdBuf[queryCmdBufLen] = '\0';
        querySerial->println();
        queryProcessIncomingCmd(queryCmdBuf); // Process the command when newline character is received
        queryCmdBufLen = 0; // Clear the command buffer
    } else if (c == '\b') {
        if (queryCmdBufLen > 0) {
            querySerial->print("\b \b"); // Print backspace, space, and backspace again to erase the character on the terminal
            queryCmdBufLen--;
        }
    } else {
        if (queryCmdBufLen < sizeof(queryCmdBuf) - 1) {
            querySerial->print(c);
            queryCmdBuf[queryCmdBufLen] = c;
            queryCmdBufLen++;
        }
    }
}

/**
 * Handle the serial input when query mode is in state waiting for peek quit.
 * If a 'q' character is received, it will stop the peek
 * @param c the character read from the serial port
 */
void Elog::queryStateWaitPeekQuit(char c)
{
    if (c == 'q') {
        logSpiffs.peekStop();
        logSD.peekStop();
        logSerial.peekStop();
        logSyslog.peekStop();

        querySerial->println("Peek stopped");
        queryPrintPrompt();

        queryState = QUERY_WAITING_FOR_COMMAND;
    }
}

/**
 * Print the help text for the query mode
 */
void Elog::queryCmdHelp()
{
    querySerial->println("\nQuery commandline help. Commands:\n");
    querySerial->println("help (print this help)");
    querySerial->println("exit (exit query mode)");
    if (logSD.registeredCount() > 0)
        querySerial->println("sd (change to SD filesystem)");
    if (logSpiffs.registeredCount() > 0)
        querySerial->println("spiffs (change to SPIFFS filesystem)");
    if (logSerial.registeredCount() > 0)
        querySerial->println("serial (change to Serial port)");
    if (logSyslog.registeredCount() > 0)
        querySerial->println("syslog (change to Syslog)");
    querySerial->println("status (print the status of the logger)");

    // Then device specific help
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdHelp();
    } else if (queryDevice == SD) {
        logSD.queryCmdHelp();
    } else if (queryDevice == SER) {
        logSerial.queryCmdHelp();
    } else if (queryDevice == SYSLOG) {
        logSyslog.queryCmdHelp();
    }
}
/**
 * Select the SPIFFS filesystem for query mode
 */
void Elog::queryCmdSpiffs()
{
    if (logSpiffs.registeredCount() == 0) {
        querySerial->println("No SPIFFS registered");
        return;
    }

    queryDevice = SPIFFS;
    querySerial->println("SPIFFS selected");
}

/**
 * Select the SD filesystem for query mode
 */
void Elog::queryCmdSd()
{
    if (logSD.registeredCount() == 0) {
        querySerial->println("No SD registered");
        return;
    }

    queryDevice = SD;
    querySerial->println("SD selected");
}

/**
 * Select the Serial port for query mode
 */
void Elog::queryCmdSerial()
{
    if (logSerial.registeredCount() == 0) {
        querySerial->println("No Serial registered");
        return;
    }

    queryDevice = SER;
    querySerial->println("Serial selected");
}

/**
 * Select the Syslog for query mode
 */
void Elog::queryCmdSyslog()
{
    if (logSyslog.registeredCount() == 0) {
        querySerial->println("No Syslog registered");
        return;
    }

    queryDevice = SYSLOG;
    querySerial->println("Syslog selected");
}

/**
 * List the directory
 * @param directory the directory to list
 */
void Elog::queryCmdDir(const char* directory)
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdDir(directory);
    } else if (queryDevice == SD) {
        logSD.queryCmdDir(directory);
    } else if (queryDevice == SER || queryDevice == SYSLOG) {
        querySerial->println("Unsupported command for this device");
    }
}

/**
 * Change the directory
 * @param directory the directory to change to
 */
void Elog::queryCmdCd(const char* directory)
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdCd(directory);
    } else if (queryDevice == SD) {
        logSD.queryCmdCd(directory);
    } else if (queryDevice == SER || queryDevice == SYSLOG) {
        querySerial->println("Unsupported command for this device");
    }
}

/**
 * Remove a file
 * @param filename the file to remove
 */
void Elog::queryCmdRm(const char* filename)
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdRm(filename);
    } else if (queryDevice == SD) {
        logSD.queryCmdRm(filename);
    } else if (queryDevice == SER || queryDevice == SYSLOG) {
        querySerial->println("Unsupported command for this device");
    }
}

/**
 * Remove a directory
 * @param directory the directory to remove
 */
void Elog::queryCmdRmdir(const char* directory)
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdRmdir(directory);
    } else if (queryDevice == SD) {
        logSD.queryCmdRmdir(directory);
    } else if (queryDevice == SER || queryDevice == SYSLOG) {
        querySerial->println("Unsupported command for this device");
    }
}

/**
 * Format the filesystem
 */
void Elog::queryCmdFormat()
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdFormat();
    } else if (queryDevice == SD) {
        logSD.queryCmdFormat();
    } else if (queryDevice == SER || queryDevice == SYSLOG) {
        querySerial->println("Unsupported command for this device");
    }
}

/**
 * Output the contents of a file
 * @param filename the file to output
 */
void Elog::queryCmdType(const char* filename)
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryCmdType(filename);
    } else if (queryDevice == SD) {
        logSD.queryCmdType(filename);
    } else if (queryDevice == SER || queryDevice == SYSLOG) {
        querySerial->println("Unsupported command for this device");
    }
}
/**
 * Peek the contents of a file
 * @param filename the file to peek
 * @param loglevel the log level to peek
 * @param textFilter the text filter to peek
 */
void Elog::queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter)
{
    bool peekStarted = false;
    if (queryDevice == SPIFFS) {
        peekStarted = logSpiffs.queryCmdPeek(filename, loglevel, textFilter);
    } else if (queryDevice == SD) {
        peekStarted = logSD.queryCmdPeek(filename, loglevel, textFilter);
    } else if (queryDevice == SER) {
        peekStarted = logSerial.queryCmdPeek(filename, loglevel, textFilter);
    } else if (queryDevice == SYSLOG) {
        peekStarted = logSyslog.queryCmdPeek(filename, loglevel, textFilter);
    }

    if (peekStarted) {
        queryState = QUERY_WAITING_FOR_PEEK_QUIT;
    } else {
        queryPrintPrompt();
    }
}

/**
 * Print the status of the logger
 */
void Elog::queryCmdStatus()
{
    char buffer[50];

    formatter.getHumanUptime(buffer, sizeof(buffer));
    querySerial->printf("Uptime: %s\n", buffer);
    querySerial->printf("RTC set: %s\n", formatter.realTimeProvided() ? "yes" : "no");
    if (formatter.realTimeProvided()) {
        formatter.getRTCtime(buffer, sizeof(buffer));
        querySerial->printf("RTC time: %s\n", buffer);
    }

    querySerial->println();
    querySerial->printf("log buffer, capacity: %d\n", ringBuff.buffCapacity());
    querySerial->printf("log buffer, percentage full: %d\n", ringBuff.buffPercentageFull());
    querySerial->printf("log buffer, lines buffered: %d\n", bufferStats.messagesBuffered);
    querySerial->printf("log buffer, lines discarded: %d\n", bufferStats.messagesDiscarded);

    if (logSerial.registeredCount() > 0) {
        logSerial.queryCmdStatus();
    }
    if (logSpiffs.registeredCount() > 0) {
        logSpiffs.queryCmdStatus();
    }
    if (logSD.registeredCount() > 0) {
        logSD.queryCmdStatus();
    }
    if (logSyslog.registeredCount() > 0) {
        logSyslog.queryCmdStatus();
    }
}

/**
 * Print the prompt for the query mode
 */
void Elog::queryPrintPrompt()
{
    if (queryDevice == SPIFFS) {
        logSpiffs.queryPrintPrompt();
    } else if (queryDevice == SD) {
        logSD.queryPrintPrompt();
    } else if (queryDevice == SER) {
        logSerial.queryPrintPrompt();
    } else if (queryDevice == SYSLOG) {
        logSyslog.queryPrintPrompt();
    }
}

// This is the only instance of the logger
// It is available to all files that include Elog.h
Elog& Logger = Elog::getInstance();
