#include <LogFormat.h>

/* Get the log stamp for the log line. The format is [TIME][SERVIC][LOGLEVEL]. It can be customized with flags
 * logTime: the time of the log
 * logLevel: the log level
 * serviceName: the name of the service
 * logFlags: the flags for the log
 * output: the output string
 */
void Formatting::getLogStamp(char* output, const uint32_t logTime, const uint8_t logLevel, const char* serviceName, const uint8_t logFlags)
{
    char timeStr[LENGTH_OF_TIME] = { 0 };
    char logServiceStr[LENGTH_OF_SERVICE] = { 0 };
    char logLevelStr[LENGTH_OF_LEVEL] = { 0 };

    if (!(logFlags & ELOG_FLAG_NO_TIME)) {
        if (logFlags & ELOG_FLAG_TIME_SIMPLE)
            getSimpleTimeString(timeStr, logTime);
        else if (logFlags & ELOG_FLAG_TIME_LONG)
            getTimeLongString(timeStr, logTime);
        else if (logFlags & ELOG_FLAG_TIME_SHORT)
            getTimeMillisString(timeStr, logTime, true);
        else // default to long time
            getTimeLongString(timeStr, logTime);
    }

    if (!(logFlags & ELOG_FLAG_NO_SERVICE)) {
        getServiceString(logServiceStr, serviceName, logFlags & ELOG_FLAG_SERVICE_LONG);
    }

    if (!(logFlags & ELOG_FLAG_NO_LEVEL)) {
        getLogLevelString(logLevelStr, logLevel);
    }
    strcpy(output, timeStr);
    strcat(output, logServiceStr);
    strcat(output, logLevelStr);
}

/* Get the time string in the format of YYYY-MM-DD HH:MM:SS.mmm (if real time is provided) or ddd:HH:MM:SS.mmm (if real time is not provided
 * milliSeconds: the time in milliseconds
 * output: the output string
 */
void Formatting::getTimeLongString(char* output, const uint32_t milliSeconds)
{
    if (realTimeProvided()) {
        getTimeRtcString(output, milliSeconds);
    } else {
        getTimeMillisString(output, milliSeconds, false);
    }
}

/* Get the time string in the format of YYYY-MM-DD HH:MM:SS.mmm
 * output: the output string
 * milliseconds: the time in milliseconds
 */
void Formatting::getTimeRtcString(char* output, const uint32_t milliseconds)
{
    uint32_t millisSinceStamps = millis() - milliseconds;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Adjust the time with the time passed since the log time
    tv.tv_sec -= millisSinceStamps / 1000;
    if (tv.tv_usec < (millisSinceStamps % 1000) * 1000) {
        tv.tv_sec--;
        tv.tv_usec += 1000000;
    }
    tv.tv_usec -= (millisSinceStamps % 1000) * 1000;

    struct tm* tmstruct = localtime(&tv.tv_sec);
    sprintf(output, "%04d-%02d-%02d %02d:%02d:%02d.%03d ", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec, tv.tv_usec / 1000);
}

/* Get the time string in the format of ddd:HH:MM:SS.mmm
 * milliseconds: the time in milliseconds
 * shortTimeFormat: if true, the output will be in the format of HH:MM:SS
 * output: the output string
 */
void Formatting::getTimeMillisString(char* output, uint32_t milliSeconds, const bool shortTimeFormat)
{
    uint32_t seconds, minutes, hours, days;
    seconds = milliSeconds / 1000;
    minutes = seconds / 60;
    hours = minutes / 60;
    days = hours / 24;

    if (shortTimeFormat) {
        sprintf(output, "%02u:%02u:%02u ", hours % 24, minutes % 60, seconds % 60);
    } else {
        sprintf(output, "%03u:%02u:%02u:%02u.%03u ", days, hours % 24, minutes % 60, seconds % 60, milliSeconds % 1000);
    }
}

/* Get the time string in the format of xxxxxxxxx (ms)
 * milliseconds: the time in milliseconds
 * output: the output string
 */
void Formatting::getSimpleTimeString(char* output, const uint32_t milliseconds)
{
    sprintf(output, "%09d ", milliseconds);
}

/* Get the service string in the format of [SERVIC]
 * serviceName: the name of the service
 * longFormat: if true, the output will be in the format of [SERVIC], otherwise [SER]
 * output: the output string
 */
void Formatting::getServiceString(char* output, const char* serviceName, const bool longFormat)
{
    uint8_t maxLength = longFormat ? 6 : 3;

    if (strlen(serviceName) == 0) {
        output[0] = '\0'; // return empty string if serviceName is empty
        return;
    }
    output[0] = '[';
    for (int i = 0; i < maxLength; i++) {
        output++;
        if (serviceName[i] != '\0') {
            output[0] = toupper(serviceName[i]);
        } else {
            output[0] = ' ';
        }
    }
    output[1] = ']';
    output[2] = ' ';
    output[3] = '\0';
}

/* Get the log level string in the format of [LOGLEVEL]
 * logLevel: the log level
 * output: the output string
 */
void Formatting::getLogLevelString(char* output, const uint8_t logLevel)
{
    char logLevelRawString[8];
    getLogLevelStringRaw(logLevelRawString, logLevel);
    sprintf(output, "[%-5s] ", logLevelRawString);
}

static const char* logLevelStrings[ELOG_NUM_LOG_LEVELS] = { "ALWAY", "EMERG", "ALERT", "CRIT", "ERROR", "WARN", "NOTIC", "INFO", "DEBUG", "TRACE", "VERBO" };

/* Get the log level string in the format of LOGLEVEL
 * logLevel: the log level
 * output: the output string
 */
void Formatting::getLogLevelStringRaw(char* output, const uint8_t logLevel)
{
    strcpy(output, logLevelStrings[logLevel]);
}

/* Get the log level from the string
 * logLevel: the log level string
 * return: the log level
 */
uint8_t Formatting::getLogLevelFromString(const char* logLevel)
{
    for (uint8_t i = 0; i < ELOG_NUM_LOG_LEVELS; i++) {
        if (strcasecmp(logLevel, logLevelStrings[i]) == 0) {
            return i;
        }
    }
    return ELOG_LEVEL_NOLOG;
}

/* Check if the real time has been provided
 * return: true if real time has been provided, false otherwise
 */
bool Formatting::realTimeProvided()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec > 100000000; // We are after year 1973. Time must have been provided
}

/* Get the human readable size (in bytes, kbytes, Mbytes)
 * size: the size in bytes
 * output: the output string
 */
void Formatting::getHumanSize(char* output, uint32_t size)
{
    if (size < 1024) {
        sprintf(output, "%d bytes", size);
    } else if (size < 1024 * 1024) {
        sprintf(output, "%.2f kbytes", static_cast<float>(size) / 1024);
    } else {
        sprintf(output, "%.2f Mbytes", static_cast<float>(size) / (1024 * 1024));
    }
}

/* Get the time string from epoch
 * epoch: The epoch time
 * timeStr: The output buffer
 */
void Formatting::getTimeStrFromEpoch(char* output, const time_t epoch)
{
    struct tm* tmstruct = localtime(&epoch);
    snprintf(output, 20, "%04d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
}

/* Get the human readable uptime (in seconds, minutes, hours, days)
 * output: the output string
 */
void Formatting::getHumanUptime(char* output, size_t outputSize)
{
    uint32_t uptime = millis() / 1000;
    uint32_t seconds = uptime % 60;
    uint32_t minutes = (uptime / 60) % 60;
    uint32_t hours = (uptime / 3600) % 24;
    uint32_t days = uptime / 86400;

    if (days > 0) {
        snprintf(output, outputSize, "%d days, %d hours, %d minutes, %d seconds", days, hours, minutes, seconds);
    } else if (hours > 0) {
        snprintf(output, outputSize, "%d hours, %d minutes, %d seconds", hours, minutes, seconds);
    } else if (minutes > 0) {
        snprintf(output, outputSize, "%d minutes, %d seconds", minutes, seconds);
    } else {
        snprintf(output, outputSize, "%d seconds", seconds);
    }
}

/* Get the RTC time in the format of YYYY-MM-DD HH:MM:SS
 * output: the output string
 */
void Formatting::getRTCtime(char* output, size_t outputSize)
{
    struct tm timeInfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeInfo);
    snprintf(output, outputSize, "%d-%02d-%02d %02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
}
