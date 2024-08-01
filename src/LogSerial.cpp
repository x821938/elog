#include <Elog.h>
#include <LogSerial.h>

void LogSerial::begin()
{
    serialStats.bytesWrittenTotal = 0;
    serialStats.messagesWrittenTotal = 0;
}

/* Configure the serial port for logging
 * maxRegistrations: the maximum number of registrations
 */
void LogSerial::configure(const uint8_t maxRegistrations)
{
    if (this->maxSerialRegistrations > 0) {
        logger.logInternal(ERROR, "Serial logging already configured with %d registrations", this->maxSerialRegistrations);
        return;
    }

    this->maxSerialRegistrations = maxRegistrations;
    serialSettings = new SerialSetting[maxRegistrations];
    logger.logInternal(INFO, "Serial logging configured with %d registrations", maxRegistrations);
}

/* Register a serial port for logging
 * logId: unique id for the log
 * loglevel: the log level that should be logged
 * serviceName: the name of the service. Will be printed in the log
 * serial: the serial port to log to
 * logFlags: flags for the log
 */
void LogSerial::registerSerial(const uint8_t logId, const uint8_t loglevel, const char* serviceName, Stream& serial, const uint8_t logFlags)
{
    if (maxSerialRegistrations == 0) {
        configure(10); // If configure is not called, call it with default values
    }

    if (registeredSerialCount >= maxSerialRegistrations) {
        logger.logInternal(ERROR, "Max number of serial registrations reached : %d", maxSerialRegistrations);
        return;
    }

    SerialSetting* setting = &serialSettings[registeredSerialCount++];

    setting->logId = logId;
    setting->serial = &serial;
    setting->serviceName = serviceName;
    setting->logLevel = loglevel;
    setting->logFlags = logFlags;

    char logLevelStr[10];
    formatter.getLogLevelStringRaw(logLevelStr, loglevel);
    logger.logInternal(INFO, "Registered Serial log id %d, level %s, serviceName %s", logId, logLevelStr, serviceName);
}

/* Output the logline to the registered serial ports
 * logLineEntry: the log line entry
 * muteSerialOutput: if true, the logline will not be output to the serial port
 */
void LogSerial::outputFromBuffer(const LogLineEntry logLineEntry, bool muteSerialOutput)
{
    if (logLineEntry.internalLogDevice != nullptr) {
        SerialSetting settingUnusable = { 0, nullptr, nullptr, NOLOG };
        write(logLineEntry, settingUnusable);
    } else {
        for (uint8_t i = 0; i < registeredSerialCount; i++) {
            SerialSetting* setting = &serialSettings[i];
            if (setting->logId == logLineEntry.logId && setting->logLevel != NOLOG) {
                if (logLineEntry.logLevel <= setting->logLevel && !muteSerialOutput) {
                    write(logLineEntry, *setting);
                }
                handlePeek(logLineEntry, i); // If peek is enabled from query command
            }
        }
    }
}

/* Handle peeking at log messages.  If peek is enabled, the log message will be printed to the querySerial if it matches the peek criteria
 * logLineEntry: the log line entry
 * settingIndex: the index of the setting in the serialSettings array
 */
void LogSerial::handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex)
{
    if (peekEnabled) {
        if (peekAllServices || settingIndex == peekSettingIndex) {
            if (logLineEntry.logLevel <= peekLoglevel) {
                char logStamp[LENGTH_OF_LOG_STAMP];
                formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, serialSettings[settingIndex].serviceName, serialSettings[settingIndex].logFlags);

                if (peekFilter) {
                    if (strcasestr(logLineEntry.logMessage, peekFilterText) != NULL) {
                        querySerial->print(logStamp);
                        querySerial->println(logLineEntry.logMessage);
                    }
                } else {
                    querySerial->print(logStamp);
                    querySerial->println(logLineEntry.logMessage);
                }
            }
        }
    }
}

/* Traverse all the registered serial ports and check if the logId and logLevel match the setting
 * logId: the log id
 * logLevel: the log level
 */
bool LogSerial::mustLog(const uint8_t logId, const uint8_t logLevel)
{
    for (uint8_t i = 0; i < registeredSerialCount; i++) {
        SerialSetting* setting = &serialSettings[i];
        if (setting->logId == logId) {
            if (logLevel <= setting->logLevel && setting->logLevel != NOLOG) {
                return true;
            }
        }
    }
    return false;
}

/* Write the logline to the serial port
 * logLineEntry: the log line entry
 * setting: the setting for the serial port
 */
void LogSerial::write(const LogLineEntry logLineEntry, SerialSetting& setting)
{
    static char logStamp[LENGTH_OF_LOG_STAMP];
    char* service;
    Stream* logSerial;

    if (logLineEntry.internalLogDevice != nullptr) {
        service = (char*)"LOG";
        logSerial = logLineEntry.internalLogDevice;

        formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, service, setting.logFlags);
        logSerial->print(logStamp);
        logSerial->println(logLineEntry.logMessage);
    } else {
        service = (char*)setting.serviceName;
        logSerial = setting.serial;

        formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, service, setting.logFlags);
        serialStats.bytesWrittenTotal += logSerial->print(logStamp);
        serialStats.bytesWrittenTotal += logSerial->println(logLineEntry.logMessage);
        serialStats.messagesWrittenTotal++;
    }
}

/* Output the statistics for the serial port
 */
void LogSerial::outputStats()
{
    logger.logInternal(INFO, "Serial stats. Messages written: %d, Bytes written: %d", serialStats.messagesWrittenTotal, serialStats.bytesWrittenTotal);
}

/* Enable the query serial port
 * querySerial: the serial port for query commands
 */
void LogSerial::enableQuery(Stream& querySerial)
{
    this->querySerial = &querySerial;
}

/* Print the help for the query commands specific to the serial port

 */
void LogSerial::queryCmdHelp()
{
    querySerial->println("peek <service> <loglevel> <textfilter> - Peek at log messages. Quit with Q");
    querySerial->println("peek * <loglevel> <textfilter> - Peek at all log messages. Quit with Q");
}

/* Parse the peek command and set the peek variables
 * serviceName: the name of the service
 * loglevel: the log level
 * textFilter: the text filter
 */
bool LogSerial::queryCmdPeek(const char* serviceName, const char* loglevel, const char* textFilter)
{
    peekLoglevel = formatter.getLogLevelFromString(loglevel);
    if (peekLoglevel == NOLOG) {
        querySerial->printf("Invalid loglevel %s. Allowed values are: debug, info, notic, warn, error, crit, alert, emerg\n", loglevel);
        return false;
    }

    if (strcmp(serviceName, "*") == 0) {
        peekAllServices = true;
    } else {
        bool found = false;
        for (uint8_t i = 0; i < registeredSerialCount; i++) {
            if (strcasecmp(serialSettings[i].serviceName, serviceName) == 0) {
                peekSettingIndex = i;
                peekAllServices = false;
                found = true;
            }
        }
        if (!found) {
            querySerial->printf("Service \"%s\" not found. Use * for all files\n", serviceName);
            return false;
        }
    }

    peekFilter = false;
    if (strlen(textFilter) > 0) {
        peekFilter = true;
        strncpy(peekFilterText, textFilter, sizeof(peekFilterText) - 1);
    }

    peekEnabled = true;
    querySerial->printf("Peeking at \"%s\" with loglevel %s(%d), Textfilter =\"%s\" Press Q to quit\n", serviceName, loglevel, peekLoglevel, textFilter);

    return peekEnabled;
}

/* Print the status of the serial port
 */
void LogSerial::queryCmdStatus()
{
    querySerial->println();
    querySerial->printf("Serial total, messages written: %d\n", serialStats.messagesWrittenTotal);
    querySerial->printf("Serial total, bytes written: %d\n", serialStats.bytesWrittenTotal);
    for (uint8_t i = 0; i < registeredSerialCount; i++) {
        char logLevelStr[10];
        formatter.getLogLevelStringRaw(logLevelStr, serialSettings[i].logLevel);
        querySerial->printf("Serial reg, Service:%s, (ID %d, level %s)\n", serialSettings[i].serviceName, serialSettings[i].logId, logLevelStr);
    }
}

/* Print the prompt for the query commands
 */
void LogSerial::queryPrintPrompt()
{
    querySerial->print("\nSerial> ");
}

/* Stop peeking at log messages
 */
void LogSerial::peekStop()
{
    peekEnabled = false;
}
