#include "LogCommon.h"
#include "Elog.h"
#include "LogCallback.h"

void LogCallback::begin()
{
    stats.bytesWrittenTotal = 0;
    stats.messagesWrittenTotal = 0;
}

/* Configure the callback for logging
 * maxRegistrations: the maximum number of registrations
 */
void LogCallback::configure(const uint8_t maxRegistrations)
{
    if (this->maxCallbackRegistrations > 0) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Callback logging already configured with %d registrations", this->maxCallbackRegistrations);
        return;
    }

    this->maxCallbackRegistrations = maxRegistrations;
    settings = new Setting[maxRegistrations];
    Logger.logInternal(ELOG_LEVEL_INFO, "Callback logging configured with %d registrations", maxRegistrations);
}

/* Register a callback function for logging
 * logId: unique id for the log
 * loglevel: the log level that should be logged
 * serviceName: the name of the service. Will be printed in the log
 * funcPtr: the callback function to log to
 * logFlags: flags for the log
 */
void LogCallback::registerCallback(const uint8_t logId, const uint8_t loglevel, const char* serviceName, callbackFunc_t funcPtr, const uint8_t logFlags)
{
    if (maxCallbackRegistrations == 0) {
        configure(10); // If configure is not called, call it with default values
    }

    if (registeredCallbackCount >= maxCallbackRegistrations) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Max number of callback registrations reached : %d", maxCallbackRegistrations);
        return;
    }

    Setting* setting = &settings[registeredCallbackCount++];

    setting->logId = logId;
    setting->callback = funcPtr;
    setting->serviceName = serviceName;
    setting->logLevel = loglevel;
    setting->lastMsgLogLevel = ELOG_LEVEL_NOLOG;
    setting->logFlags = logFlags;

    char logLevelStr[10];
    formatter.getLogLevelStringRaw(logLevelStr, loglevel);
    Logger.logInternal(ELOG_LEVEL_INFO, "Registered Callback log id %d, level %s, serviceName %s", logId, logLevelStr, serviceName);
}

uint8_t LogCallback::getLogLevel(const uint8_t logId, callbackFunc_t funcPtr)
{
    for (uint8_t i = 0; i < registeredCallbackCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->callback == funcPtr){
            return setting->logLevel;
        }
    }

    return ELOG_LEVEL_NOLOG;
}

void LogCallback::setLogLevel(const uint8_t logId, const uint8_t loglevel, callbackFunc_t funcPtr)
{
    for (uint8_t i = 0; i < registeredCallbackCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->callback == funcPtr) {
            setting->logLevel = loglevel;
        }
    }
}

uint8_t LogCallback::getLastMsgLogLevel(const uint8_t logId, callbackFunc_t funcPtr)
{
    for (uint8_t i = 0; i < registeredCallbackCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->callback == funcPtr) {
            return setting->lastMsgLogLevel;
        }
    }

    return ELOG_LEVEL_NOLOG;
}

/* Output the logline to the registered callback functions
 * logLineEntry: the log line entry
 */
void LogCallback::outputFromBuffer(const LogLineEntry logLineEntry)
{
    if (logLineEntry.internalLogDevice != nullptr) {
        Setting settingUnusable = { 0, nullptr, nullptr, ELOG_LEVEL_NOLOG };
        write(logLineEntry, settingUnusable);
    } else {
        for (uint8_t i = 0; i < registeredCallbackCount; i++) {
            Setting* setting = &settings[i];
            if (setting->logId == logLineEntry.logId && (setting->logLevel != ELOG_LEVEL_NOLOG || logLineEntry.logLevel == ELOG_LEVEL_ALWAYS)) {
                if (logLineEntry.logLevel <= setting->logLevel) {
                    setting->lastMsgLogLevel = logLineEntry.logLevel;
                    write(logLineEntry, *setting);
                }
                handlePeek(logLineEntry, i); // If peek is enabled from query command
            }
        }
    }
}

/* Handle peeking at log messages.  If peek is enabled, the log message will be printed to the queryCallback if it matches the peek criteria
 * logLineEntry: the log line entry
 * settingIndex: the index of the setting in the callbackSettings array
 */
void LogCallback::handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex)
{
    if (peekEnabled) {
        if (peekAllServices || settingIndex == peekSettingIndex) {
            if (logLineEntry.logLevel <= peekLoglevel) {
                char logStamp[LENGTH_OF_LOG_STAMP];
                formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, settings[settingIndex].serviceName, settings[settingIndex].logFlags);

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

/* Traverse all the registered callbacks and check if the logId and logLevel match the setting
 * logId: the log id
 * logLevel: the log level
 */
bool LogCallback::mustLog(const uint8_t logId, const uint8_t logLevel)
{
    for (uint8_t i = 0; i < registeredCallbackCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId) {
            if (logLevel <= setting->logLevel && (setting->logLevel != ELOG_LEVEL_NOLOG || logLevel == ELOG_LEVEL_ALWAYS)) {
                return true;
            }
        }
    }
    return false;
}

/* Call the callback function
 * logLineEntry: the log line entry
 * setting: the setting for the callback
 */
void LogCallback::write(LogLineEntry logLineEntry, Setting& setting)
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
        stats.bytesWrittenTotal += settings->callback(logLineEntry);
        stats.messagesWrittenTotal++;
    }
}

/* Output the statistics for the callbacks
 */
void LogCallback::outputStats()
{
    Logger.logInternal(ELOG_LEVEL_INFO, "Callback stats. Messages written: %d, Bytes written: %d", stats.messagesWrittenTotal, stats.bytesWrittenTotal);
}

/* Return the number of registrations
 */
uint8_t LogCallback::registeredCount()
{
    return registeredCallbackCount;
}

/* Enable the query serial port
 * querySerial: the serial port for query commands
 */
void LogCallback::enableQuery(Stream& querySerial)
{
    this->querySerial = &querySerial;
}

/* Print the help for the query commands specific to the serial port

 */
void LogCallback::queryCmdHelp()
{
    querySerial->println("peek <service> <loglevel> <textfilter> - Peek at log messages. Quit with Q");
    querySerial->println("peek * <loglevel> <textfilter> - Peek at all log messages. Quit with Q");
}

/* Parse the peek command and set the peek variables
 * serviceName: the name of the service
 * loglevel: the log level
 * textFilter: the text filter
 */
bool LogCallback::queryCmdPeek(const char* serviceName, const char* loglevel, const char* textFilter)
{
    peekLoglevel = formatter.getLogLevelFromString(loglevel);
    if (peekLoglevel == ELOG_LEVEL_NOLOG) {
        querySerial->printf("Invalid loglevel\n\npeek <filename> <loglevel> <filtertext>\nAllowed loglevels are: verbo, trace, debug, info, notic, warn, error, crit, alert, emerg\n");
        return false;
    }

    if (strcmp(serviceName, "*") == 0) {
        peekAllServices = true;
    } else {
        bool found = false;
        for (uint8_t i = 0; i < registeredCallbackCount; i++) {
            if (strcasecmp(settings[i].serviceName, serviceName) == 0) {
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
        peekFilterText[sizeof(peekFilterText) - 1] = '\0';
    }

    peekEnabled = true;
    querySerial->printf("Peeking at \"%s\" with loglevel %s(%d), Textfilter =\"%s\" Press Q to quit\n", serviceName, loglevel, peekLoglevel, textFilter);

    return peekEnabled;
}

/* Print the status of the callbacks
 */
void LogCallback::queryCmdStatus()
{
    querySerial->println();
    querySerial->printf("Callback total, messages written: %d\n", stats.messagesWrittenTotal);
    querySerial->printf("Callback total, bytes written: %d\n", stats.bytesWrittenTotal);
    for (uint8_t i = 0; i < registeredCallbackCount; i++) {
        char logLevelStr[10];
        formatter.getLogLevelStringRaw(logLevelStr, settings[i].logLevel);
        querySerial->printf("Callback reg, Service:%s, (ID %d, level %s)\n", settings[i].serviceName, settings[i].logId, logLevelStr);
    }
}

/* Print the prompt for the query commands
 */
void LogCallback::queryPrintPrompt()
{
    querySerial->print("\nCallback> ");
}

/* Stop peeking at log messages
 */
void LogCallback::peekStop()
{
    peekEnabled = false;
}
