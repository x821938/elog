#ifdef ELOG_SYSLOG_ENABLE

#include <Elog.h>
#include <LogSyslog.h>

void LogSyslog::begin()
{
    stats.bytesWrittenTotal = 0;
    stats.messagesWrittenTotal = 0;
    stats.messagesDiscardedTotal = 0;
}

/* This should be called to set up logging syslog
 * serverName: the name of the syslog server
 * port: the port of the syslog server
 * hostname: the hostname of the device
 * maxRegistrations: the maximum number of registrations
 */
void LogSyslog::configure(const char* serverName, const uint16_t port, const char* hostname, bool waitIfNotReady, const uint16_t maxWaitMilliseconds, const uint8_t maxRegistrations)
{
    if (syslogConfigured) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Syslog already configured with %s:%d, hostname %s", syslogServer, syslogPort, syslogHostname);
        return;
    }

    settings = new Setting[maxRegistrations];
    this->maxRegistrations = maxRegistrations;

    this->syslogServer = serverName;
    this->syslogPort = port;
    this->syslogHostname = hostname;
    this->waitIfNotReady = waitIfNotReady;
    this->maxWaitMilliseconds = maxWaitMilliseconds;
    syslogConfigured = true;

    Logger.logInternal(ELOG_LEVEL_INFO, "Configured syslog server %s:%d, hostname %s, max registrations %d", serverName, port, hostname, maxRegistrations);
}

/* Register syslog for logging
 * logId: unique id for the log
 * loglevel: the log level that should be logged
 * facility: the syslog facility. See LogSyslog.h for available facilities
 * appName: the name of the app. Will be printed in the log
 */
void LogSyslog::registerSyslog(const uint8_t logId, const uint8_t loglevel, const uint8_t facility, const char* appName)
{
    if (!syslogConfigured) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Syslog not configured. Call configureSyslog first");
        return;
    }

    if (syslogSettingsCount >= maxRegistrations) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Maximum number of syslog registrations reached: %d", maxRegistrations);
        return;
    }

    Setting* setting = &settings[syslogSettingsCount++];

    setting->logId = logId;
    setting->appName = appName;
    setting->facility = facility;
    setting->logLevel = loglevel;
    setting->lastMsgLogLevel = ELOG_LEVEL_NOLOG;

    char logLevelStr[10];
    formatter.getLogLevelStringRaw(logLevelStr, loglevel);
    Logger.logInternal(ELOG_LEVEL_INFO, "Registered syslog id %d, level %s, facility %d, app name %s", logId, logLevelStr, facility, appName);
}

uint8_t LogSyslog::getLogLevel(const uint8_t logId, const uint8_t facility)
{
    for (uint8_t i = 0; i < syslogSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->facility == facility) {
            return setting->logLevel;
        }
    }

    return ELOG_LEVEL_NOLOG;
}

void LogSyslog::setLogLevel(const uint8_t logId, const uint8_t loglevel, const uint8_t facility)
{
    for (uint8_t i = 0; i < syslogSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->facility == facility) {
            setting->logLevel = loglevel;
        }
    }
}

uint8_t LogSyslog::getLastMsgLogLevel(const uint8_t logId, const uint8_t facility)
{
    for (uint8_t i = 0; i < syslogSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->facility == facility) {
            return setting->lastMsgLogLevel;
        }
    }

    return ELOG_LEVEL_NOLOG;
}

/* Output the logline to the registered syslogs
 * logLineEntry: the log line entry
 */
void LogSyslog::outputFromBuffer(const LogLineEntry logLineEntry)
{
    for (uint8_t i = 0; i < syslogSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logLineEntry.logId &&
            (setting->logLevel != ELOG_LEVEL_NOLOG || logLineEntry.logLevel == ELOG_LEVEL_ALWAYS)) {
            if (logLineEntry.logLevel <= setting->logLevel) {
                setting->lastMsgLogLevel = logLineEntry.logLevel;
                write(logLineEntry, *setting);
            }
            handlePeek(logLineEntry, i); // If peek is enabled from query command
        }
    }
}

/* Write the logline to the syslog server if wifi is connected
 * logLineEntry: the log line entry
 * setting: the syslog setting
 */
void LogSyslog::write(LogLineEntry logLineEntry, Setting& setting)
{
    static int const syslogLevel[ELOG_NUM_LOG_LEVELS] = { 6, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7 };

    while (true) {
        uint32_t sentBytes = 0;
        int success = 0;
        if (WiFi.isConnected()) {
            uint8_t priority = syslogLevel[logLineEntry.logLevel] | (setting.facility << 3);

            uint8_t buffer[256];
            // Date and time is not included in the syslog message. It is assumed that the syslog server will add it
            int len = snprintf((char*)buffer, 256, "<%d>%s %s: %s", priority, syslogHostname, setting.appName, logLineEntry.logMessage);

            // Remove any non-printing characters at the end of the line
            while (len >= 1 && !isprint(buffer[len - 1])) {
                len--;
            }

            if (syslogUdp.beginPacket(syslogServer, syslogPort) == 1) {
                sentBytes = syslogUdp.write(buffer, len);
                success = syslogUdp.endPacket();
            }

            if (success && sentBytes == len) {
                stats.bytesWrittenTotal += len;
                stats.messagesWrittenTotal++;
                return;
            }
        }

        // WiFi is not ready, or sending the log failed

        if (!waitIfNotReady || maxWaitMilliseconds == 0) {
            stats.messagesDiscardedTotal++;
            Logger.logInternal(ELOG_LEVEL_WARNING, "WiFi not connected or could not send syslog message");
            return;
        }

        // Wait 250 ms at a time
        unsigned long delayTime = (maxWaitMilliseconds > 250) ? 250 : maxWaitMilliseconds;
        delay(delayTime);
        // It is intentional that after a cumulative delay equal to the configured
        // maxWaitMilliseconds, we stop waiting.
        maxWaitMilliseconds -= delayTime;
    }
}

/* Handle peeking at log messages.  If peek is enabled, the log message will be printed to the querySerial if it matches the peek criteria
 * logLineEntry: the log line entry
 * settingIndex: the index of the setting in the serialSettings array
 */
void LogSyslog::handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex)
{
    if (peekEnabled) {
        if (peekAllApps || settingIndex == peekSettingIndex) {
            if (logLineEntry.logLevel <= peekLoglevel) {
                char logStamp[LENGTH_OF_LOG_STAMP];
                formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, settings[settingIndex].appName, 0);

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

/* Traverse all the registered syslogs and check if the logId and logLevel match the setting
 * logId: the log id
 * logLevel: the log level
 */
bool LogSyslog::mustLog(const uint8_t logId, const uint8_t logLevel)
{
    for (uint8_t i = 0; i < syslogSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId) {
            if (logLevel <= setting->logLevel &&
                (setting->logLevel != ELOG_LEVEL_NOLOG || logLevel == ELOG_LEVEL_ALWAYS)) {
                return true;
            }
        }
    }
    return false;
}

/* Output the statistics for syslog
 */
void LogSyslog::outputStats()
{
    if (syslogConfigured) {
        Logger.logInternal(ELOG_LEVEL_INFO, "Syslog stats. Messages written: %d, Bytes written: %d, Messages discarded: %d", stats.messagesWrittenTotal, stats.bytesWrittenTotal, stats.messagesDiscardedTotal);
    }
}

/* Return the number of registrations
 */
uint8_t LogSyslog::registeredCount()
{
    return syslogSettingsCount;
}

/* Enable the query serial port
 * querySerial: the serial port for query commands
 */
void LogSyslog::enableQuery(Stream& querySerial)
{
    this->querySerial = &querySerial;
}

/* Print help for syslog query commands
 */
void LogSyslog::queryCmdHelp()
{
    querySerial->println("peek <appname> <loglevel> <filtertext> (filename can be *, filtertext is optional)");
}

/* Start peeking at syslog messages
 * appName: the name of the app to peek at. Use * for all apps
 * loglevel: the loglevel to peek at
 * textFilter: the text to filter on
 */
bool LogSyslog::queryCmdPeek(const char* appName, const char* loglevel, const char* textFilter)
{
    peekLoglevel = formatter.getLogLevelFromString(loglevel);
    if (peekLoglevel == ELOG_LEVEL_NOLOG) {
        querySerial->printf("Invalid loglevel %s. Allowed values are: verbo, trace, debug, info, notic, warn, error, crit, alert, emerg, alway\n", loglevel);
        return false;
    }

    if (strcmp(appName, "*") == 0) {
        peekAllApps = true;
    } else {
        bool found = false;
        for (uint8_t i = 0; i < syslogSettingsCount; i++) {
            if (strcasecmp(settings[i].appName, appName) == 0) {
                peekSettingIndex = i;
                peekAllApps = false;
                found = true;
            }
        }
        if (!found) {
            querySerial->printf("App \"%s\" not found. Use * for all apps\n", appName);
            return false;
        }
    }

    peekFilter = false;
    if (strlen(textFilter) > 0) {
        peekFilter = true;
        strncpy(peekFilterText, textFilter, sizeof(peekFilterText) - 1);
    }

    peekEnabled = true;
    querySerial->printf("Peeking at app \"%s\" with loglevel %s(%d), Textfilter =\"%s\" Press Q to quit\n", appName, loglevel, peekLoglevel, textFilter);

    return peekEnabled;
}

/* Output the status of syslog
 */
void LogSyslog::queryCmdStatus()
{
    querySerial->println();
    querySerial->printf("Syslog total, messages written: %d\n", stats.messagesWrittenTotal);
    querySerial->printf("Syslog total, bytes written: %d\n", stats.bytesWrittenTotal);
    querySerial->printf("Syslog total, messages discarded: %d\n", stats.messagesDiscardedTotal);
    for (uint8_t i = 0; i < syslogSettingsCount; i++) {
        char logLevelStr[10];
        formatter.getLogLevelStringRaw(logLevelStr, settings[i].logLevel);
        querySerial->printf("Syslog reg, App:%s, (ID %d, level %s)\n", settings[i].appName, settings[i].logId, logLevelStr);
    }
}

/* Print the syslog prompt
 */
void LogSyslog::queryPrintPrompt()
{
    querySerial->print("\nSyslog> ");
}

/* Stop peeking at syslog messages
 */
void LogSyslog::peekStop()
{
    peekEnabled = false;
}

#endif // ELOG_SYSLOG_ENABLE
