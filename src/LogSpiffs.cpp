#ifndef LOGGING_SPIFFS_DISABLE

#include <Elog.h>
#include <LogSpiffs.h>

/* reset the buffer stats
 */
void LogSpiffs::begin()
{
    stats.bytesWrittenTotal = 0;
    stats.messagesWrittenTotal = 0;
    stats.messagesDiscardedTotal = 0;
}

/* Configure the SPIFFS logging
 * maxRegistrations: The maximum number of registrations
 */
void LogSpiffs::configure(const uint8_t maxRegistrations)
{
    if (this->maxRegistrations > 0) {
        logger.logInternal(ELOG_LEVEL_ERROR, "SPIFFS logging already configured with %d registrations", this->maxRegistrations);
        return;
    }

    settings = new Setting[maxRegistrations];
    fileSettingsCount = 0;
    this->maxRegistrations = maxRegistrations;
    logger.logInternal(ELOG_LEVEL_INFO, "Configured SPIFFS logging with max %d registrations", maxRegistrations);
}

/* Register a SPIFFS log file
 * logId: The id of the log
 * loglevel: The log level that should be logged
 * fileName: The name of the file (max 8 characters)
 * logFlags: The log flags (FLAG_NONE, FLAG_NO_SERVICE, FLAG_NO_TIMESTAMP, FLAG_NO_LEVEL, FLAG_NO_TIME, FLAG_TIME_SIMPLE, FLAG_TIME_LONG, FLAG_TIME_SHORT, FLAG_SERVICE_LONG)
 * maxLogFileSize: The maximum size of the log file before it is rotated
 */
void LogSpiffs::registerSpiffs(const uint8_t logId, const uint8_t loglevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize)
{
    if (maxRegistrations == 0) {
        configure(10); // If configure is not called, call it with default values
    }

    if (!isValidFileName(fileName)) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Invalid filename %s", fileName);
        return;
    }
    if (isFileNameRegistered(fileName)) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Filename %s already registered with logId %d", fileName, logId);
        return;
    }

    if (fileSettingsCount >= maxRegistrations) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Maximum number of registered SPIFFS logfiles reached: %d", maxRegistrations);
        return;
    }

    Setting* setting = &settings[fileSettingsCount++];

    setting->logId = logId;
    setting->fileName = fileName;
    setting->logLevel = loglevel;
    setting->fileNumber = 0;
    setting->bytesWritten = 0;
    setting->maxLogFileSize = maxLogFileSize;

    if (logFlags == FLAG_NONE) {
        setting->logFlags = FLAG_NO_SERVICE; // Servicename makes no sense in a file
    } else {
        setting->logFlags = logFlags;
    }

    char logLevelStr[10];
    formatter.getLogLevelStringRaw(logLevelStr, loglevel);
    logger.logInternal(ELOG_LEVEL_INFO, "Registered SPIFFS log id %d, level %s, filename %s", logId, logLevelStr, fileName);
}

/* Output the logline to the SPIFFS log files. Traverse all registered log files and output to the ones that match the logId and logLevel
 * logLineEntry: The logline to output
 */
void LogSpiffs::outputFromBuffer(const LogLineEntry logLineEntry)
{
    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logLineEntry.logId && setting->logLevel != ELOG_LEVEL_NOLOG) {
            if (logLineEntry.logLevel <= setting->logLevel) {
                if (ensureFilesystemConfigured()) {
                    write(logLineEntry, *setting);
                    allFilesSync();
                }
            }
            handlePeek(logLineEntry, i); // If peek is enabled from query command
        }
    }
}

/* if peek is enabled, print the logline to the query serial port if it matches the peek parameters
 * logLineEntry: The logline to print
 * settingIndex: The index of the setting
 */
void LogSpiffs::handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex)
{
    if (peekEnabled) {
        if (peekAllFiles || peekSettingIndex == settingIndex) {
            if (logLineEntry.logLevel <= peekLoglevel) {
                char logStamp[LENGTH_OF_LOG_STAMP];
                formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, "", settings[settingIndex].logFlags);

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

/* Write the logline to the SPIFFS log file
 * logLineEntry: The logline to write
 * setting: The setting for the file
 */
void LogSpiffs::write(const LogLineEntry logLineEntry, Setting& setting)
{
    static char logStamp[LENGTH_OF_LOG_STAMP];

    if (ensureOpenFile(setting)) {
        formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, "", setting.logFlags);
        size_t bytesWritten; // Number of bytes written should be the same as content length
        size_t expectedBytes = strlen(logStamp) + strlen(logLineEntry.logMessage) + 2; // 2 chars for endline

        bytesWritten = setting.spiffsFileHandle.print(logStamp);
        bytesWritten += setting.spiffsFileHandle.print(logLineEntry.logMessage);
        bytesWritten += setting.spiffsFileHandle.println();

        if (bytesWritten == expectedBytes) {
            stats.bytesWrittenTotal += bytesWritten;
            stats.messagesWrittenTotal++;
            setting.bytesWritten += bytesWritten;
        } else {
            stats.messagesDiscardedTotal++;
            logger.logInternal(ELOG_LEVEL_ERROR, "Failed to write to SPIFFS:%s/%s. Expected writing %d bytes, wrote %d bytes", currentLogDir, setting.fileName, expectedBytes, bytesWritten);
        }
        ensureFreeSpace();
        ensureFileSize(setting);
    }
}

/* Traverse all registered log files and check if the logId and logLevel match the setting
 * logId: The log id
 * logLevel: The log level
 * return: true if the log should be written to filesystem
 */
bool LogSpiffs::mustLog(const uint8_t logId, const uint8_t logLevel)
{
    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && setting->logLevel != ELOG_LEVEL_NOLOG) {
            if (logLevel <= setting->logLevel) {
                return true;
            }
        }
    }
    return false;
}

/* Output the SPIFFS stats
 */
void LogSpiffs::outputStats()
{
    if (fileSystemConfigured) {
        logger.logInternal(ELOG_LEVEL_INFO, "SPIFFS stats. Messages written: %d, Bytes written: %d", stats.messagesWrittenTotal, stats.bytesWrittenTotal);
    }
}

/* must be called before using any of the query commands
 * sets the output device for the query commands
 * querySerial: The output device
 */
void LogSpiffs::enableQuery(Stream& querySerial)
{
    this->querySerial = &querySerial;
}

/* Stop the peek command
 */
void LogSpiffs::peekStop()
{
    peekEnabled = false;
}

/* Print the help specific for the SPIFFS file system
 */
void LogSpiffs::queryCmdHelp()
{
    querySerial->println("dir <directory> (list directory)");
    querySerial->println("cd <directory> (change directory)");
    querySerial->println("type <file> (print file content. Press Q to quit. Space to pause)");
    querySerial->println("rm <file> (remove file)");
    querySerial->println("rmdir <directory> (remove directory recursively)");
    querySerial->println("format (format filesystem with no warning)");
    querySerial->println("peek <filename> <loglevel> <filtertext> (filename can be *, filtertext is optional)");
}

/* List the directory
 * directory: The directory
 */
void LogSpiffs::queryCmdDir(const char* directory)
{
    if (strlen(directory) == 0) {
        directory = queryCwd;
    }

    char absolutePath[LENGTH_ABSOLUTE_PATH];
    getAbsolutePath(absolutePath, directory);

    File root = LittleFS.open(absolutePath);
    if (!root) {
        querySerial->printf("Directory %s not found\n", directory);
        return;
    }
    if (!root.isDirectory()) {
        querySerial->printf("%s is not a directory\n", directory);
        return;
    }

    File file = root.openNextFile();

    while (file) {
        if (file.isDirectory()) {
            querySerial->printf("%s\n", file.name());
        } else {
            time_t t = file.getLastWrite();
            char time[20];
            formatter.getTimeStrFromEpoch(time, t);
            querySerial->printf("%s [%s] (%d bytes)\n", file.name(), time, file.size());
        }
        file = root.openNextFile();
    }
    root.close();
    file.close();

    queryPrintVolumeInfo();
}

/* Change the directory
 * directory: The directory
 */
void LogSpiffs::queryCmdCd(const char* directory)
{
    if (strcmp(directory, "..") == 0) {
        strcpy(queryCwd, SPIFFS_LOG_ROOT); // only one level supported
        return;
    } else if (strcmp(directory, ".") == 0) {
        return;
    } else if (strcmp(directory, "/") == 0) {
        strcpy(queryCwd, SPIFFS_LOG_ROOT);
        return;
    }
    if (directory[0] == '/') {
        strncpy(queryCwd, directory, LENGTH_LOG_DIR - 1);
    } else {
        if (strlen(queryCwd) > 1) {
            snprintf(queryCwd, LENGTH_LOG_DIR, "%s/%s", queryCwd, directory);
        } else {
            snprintf(queryCwd, LENGTH_LOG_DIR, "/%s", directory);
        }
    }

    File file = LittleFS.open(queryCwd);
    if (!file || !file.isDirectory()) {
        querySerial->printf("%s is not a directory\n", directory);
        strcpy(queryCwd, SPIFFS_LOG_ROOT);
        return;
    }
    file.close();
}

/* Remove a file
 * filename: The filename
 */
void LogSpiffs::queryCmdRm(const char* filename)
{
    char absoluteFilePath[LENGTH_ABSOLUTE_PATH];
    getAbsolutePath(absoluteFilePath, filename);

    if (LittleFS.remove(absoluteFilePath)) {
        querySerial->printf("File %s removed\n", filename);
    } else {
        querySerial->printf("Could not remove file %s\n", filename);
    }
}

/* Remove a directory
 * directory: The directory
 */
void LogSpiffs::queryCmdRmdir(const char* directory)
{
    char absolutePath[LENGTH_ABSOLUTE_PATH];
    getAbsolutePath(absolutePath, directory);

    File dir = LittleFS.open(absolutePath);
    if (!dir) {
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        char fullFileName[LENGTH_ABSOLUTE_PATH];
        sprintf(fullFileName, "%s/%s", absolutePath, file.name());
        file.close();

        bool status = LittleFS.remove(fullFileName);
        if (status) {
            querySerial->printf("Removed file %s\n", fullFileName);
        } else {
            querySerial->printf("Failed to remove file %s\n", fullFileName);
        }
        file = dir.openNextFile();
    }
    dir.close();
    bool status = LittleFS.rmdir(absolutePath);
    if (status) {
        querySerial->printf("Removed directory %s\n", absolutePath);
    } else {
        querySerial->printf("Failed to remove directory %s\n", absolutePath);
    }
}

/* Format the filesystem.
 */
void LogSpiffs::queryCmdFormat()
{
    allFilesClose();
    querySerial->print("Formatting spiffs...");
    LittleFS.format();
    querySerial->println("Done!");
    createNextLogDir();
}

/* Print the content of the file to the serial port
 * Output can be paused with SPACE and aborted with Q
 * this blocks the writer task meaning that no logs will be written to the file while typing. They will be queued
 * filename: The filename
 */
void LogSpiffs::queryCmdType(const char* filename)
{
    char absoluteFilePath[LENGTH_ABSOLUTE_PATH];
    getAbsolutePath(absoluteFilePath, filename);

    File logFile = LittleFS.open(absoluteFilePath, FILE_READ);
    if (!logFile) {
        querySerial->printf("Log file \"%s\" not found\n", filename);
    }
    if (logFile.isDirectory()) {
        querySerial->printf("%s is a directory. You can't type a directory\n", filename);
        logFile.close();
        return;
    }

    while (logFile.available()) {
        querySerial->write(logFile.read());
        if (querySerial->available()) {
            char c = querySerial->read();
            if (c == 'Q' || c == 'q') { // Print can be aborted with Q
                querySerial->println("\nAborted!");
                return;
            } // Or paused with space
            if (c == ' ') {
                while (!querySerial->available()) {
                    vTaskDelay(1);
                }
                querySerial->read();
            }
        }
    }
    logFile.close();
}

/* Set the peek parameters. Peek is a command that prints loglines to the serial port in real time.
 * Peeking can be stopped by pressing Q
 * filename: The filename (without extionstion)
 * loglevel: The loglevel (debug, info, notic, warn, error, crit, alert, emerg)
 * textFilter: A text filter. If set, only loglines containing this text will be printed
 */
bool LogSpiffs::queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter)
{
    peekLoglevel = formatter.getLogLevelFromString(loglevel);
    if (peekLoglevel == ELOG_LEVEL_NOLOG) {
        querySerial->printf("Invalid loglevel %s. Allowed values are: debug, info, notic, warn, error, crit, alert, emerg\n", loglevel);
        return false;
    }

    if (strcmp(filename, "*") == 0) {
        peekAllFiles = true;
    } else {
        bool found = false;
        for (uint8_t i = 0; i < fileSettingsCount; i++) {
            if (strcmp(settings[i].fileName, filename) == 0) {
                peekSettingIndex = i;
                peekAllFiles = false;
                found = true;
            }
        }
        if (!found) {
            querySerial->printf("File \"%s\" not found. Use * for all files\n", filename);
            return false;
        }
    }

    peekFilter = false;
    if (strlen(textFilter) > 0) {
        peekFilter = true;
        strncpy(peekFilterText, textFilter, sizeof(peekFilterText) - 1);
    }

    peekEnabled = true;
    querySerial->printf("Peeking at \"%s\" with loglevel %s(%d), Textfilter =\"%s\" Press Q to quit\n", filename, loglevel, peekLoglevel, textFilter);

    return peekEnabled;
}

/* Print the status of the SPIFFS filesystem
 */
void LogSpiffs::queryCmdStatus()
{
    char buffer[20];
    formatter.getHumanSize(buffer, stats.bytesWrittenTotal);
    querySerial->println();
    querySerial->printf("SPIFFS total, bytes written: %s\n", buffer);
    querySerial->printf("SPIFFS total, messages written: %d\n", stats.messagesWrittenTotal);
    querySerial->printf("SPIFFS total, messages discarded: %d\n", stats.messagesDiscardedTotal);

    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting setting = settings[i];
        char logLevelStr[10];
        formatter.getLogLevelStringRaw(logLevelStr, setting.logLevel);
        querySerial->printf("SPIFFS reg, SPIFFS:%s/%s.%03d (ID %d, Level %s) - %d bytes written\n", currentLogDir, setting.fileName, setting.fileNumber, setting.logId, logLevelStr, setting.bytesWritten);
    }
}

/* Print the command prompt
 */
void LogSpiffs::queryPrintPrompt()
{
    querySerial->printf("\nSPIFFS:%s> ", queryCwd);
}

/* Print the volume info. Total space, used space, free space and percent usage
 * Used in the end of the dir command
 */
void LogSpiffs::queryPrintVolumeInfo()
{
    uint32_t usedSpace = LittleFS.usedBytes();
    uint32_t totalSpace = LittleFS.totalBytes();
    uint32_t freeSpace = totalSpace - usedSpace;

    char usedSpaceStr[20];
    char totalSpaceStr[20];
    char freeSpaceStr[20];

    formatter.getHumanSize(usedSpaceStr, usedSpace);
    formatter.getHumanSize(totalSpaceStr, totalSpace);
    formatter.getHumanSize(freeSpaceStr, freeSpace);
    float usage = (float)usedSpace / totalSpace * 100;

    querySerial->printf("\nTotal space: %s, Used space: %s, Free space: %s, Usage: %.2f%%\n", totalSpaceStr, usedSpaceStr, freeSpaceStr, usage);
}

/* Checks if the file is open
 * fileName: The filename
 * return: true if the file is open
 */
bool LogSpiffs::isFileOpen(const char* fileName)
{
    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting* setting = &settings[i];
        char fullFileName[LENGTH_ABSOLUTE_PATH];
        sprintf(fullFileName, "%s/%s.%03d", currentLogDir, setting->fileName, setting->fileNumber);
        if (strcmp(fullFileName, fileName) == 0) {
            if (setting->spiffsFileHandle) {
                return true;
            }
        }
    }
    return false;
}

/* Check if the filename is valid
 * fileName: The filename to check
 * return: true if the filename is valid
 */
bool LogSpiffs::isValidFileName(const char* fileName)
{
    if (strlen(fileName) > 8) {
        return false;
    }
    for (int i = 0; i < strlen(fileName); i++) {
        if (!isalnum(fileName[i])) {
            return false;
        }
    }
    return true;
}

/* Check if the filename is already registered
 * fileName: The filename to check
 * return: true if the filename is already registered
 */
bool LogSpiffs::isFileNameRegistered(const char* fileName)
{
    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting setting = settings[i];
        if (strcmp(setting.fileName, fileName) == 0) {
            return true;
        }
    }
    return false;
}

/* Create the next log directory. This is done after boot or when formatting the filesystem
   Log directories are named 0001, 0002, 0003 etc.
   To keep track of the next log directory a lognumber.txt file is created in the root of the filesystem
*/
void LogSpiffs::createNextLogDir()
{
    if (LittleFS.mkdir(SPIFFS_LOG_ROOT)) {
        logger.logInternal(ELOG_LEVEL_NOTICE, "Created directory SPIFFS:%s", SPIFFS_LOG_ROOT);
    }

    uint16_t logNumber = 0;

    // Load logNumber from lognumber.txt
    File logNumberFile = LittleFS.open(SPIFFS_LOGNUMBER_FILE, "r"); // FIXME: If not found, kernel throws an error???? Can be disabled in platformio.ini with -D CORE_DEBUG_LEVEL=0
    if (logNumberFile) {
        String logNumberStr = logNumberFile.readStringUntil('\n');
        logNumber = logNumberStr.toInt();
        logger.logInternal(ELOG_LEVEL_DEBUG, "Read file SPIFFS:%s and got log number %d", SPIFFS_LOGNUMBER_FILE, logNumber);
        logNumberFile.close();
    } else {
        logger.logInternal(ELOG_LEVEL_WARNING, "No SPIFFS:%s file\n", SPIFFS_LOGNUMBER_FILE);
    }

    bool dirCreated = false;
    do {
        logNumber++;
        sprintf(currentLogDir, "%s/%04d", SPIFFS_LOG_ROOT, logNumber);
        dirCreated = LittleFS.mkdir(currentLogDir);
    } while (!dirCreated);

    logger.logInternal(ELOG_LEVEL_NOTICE, "Created directory SPIFFS:%s", currentLogDir);

    // Store logNumber in lognumber.txt
    logNumberFile = LittleFS.open(SPIFFS_LOGNUMBER_FILE, "w");
    logger.logInternal(ELOG_LEVEL_DEBUG, "Writing SPIFFS:%s file with lognumber %d", SPIFFS_LOGNUMBER_FILE, logNumber);
    if (logNumberFile) {
        logNumberFile.print(logNumber);
        logNumberFile.close();
    } else {
        logger.logInternal(ELOG_LEVEL_ALERT, "Error writing to SPIFFS:%s. No SPIFFS file logging!", SPIFFS_LOGNUMBER_FILE);
    }
}

/* Get the absolute path. If the path is not absolute, the current working directory is prepended
 * output: The output buffer
 */
void LogSpiffs::getAbsolutePath(char* output, const char* path)
{
    if (path[0] == '/') {
        strcpy(output, path);
    } else {
        sprintf(output, "%s/%s", queryCwd, path);
    }
}

/* Remove the oldest file in the filesystem. This is done when the minimum free space is reached
 * Oldest file is always in the first directory of the filesystem. After removing the file, the directory is removed if empty
 * return: The size of the removed file
 */
uint32_t LogSpiffs::removeOldestFile()
{
    char dirName[LENGTH_LOG_DIR];
    File root = LittleFS.open(SPIFFS_LOG_ROOT);
    File dir = root.openNextFile();
    if (!dir) {
        logger.logInternal(ELOG_LEVEL_ERROR, "No files to remove in SPIFFS");
        return 0;
    }
    if (!dir.isDirectory()) {
        logger.logInternal(ELOG_LEVEL_ERROR, "No directories to remove in SPIFFS");
        return 0;
    }
    sprintf(dirName, "%s/%s", SPIFFS_LOG_ROOT, dir.name());
    dir.close();
    root.close();

    char oldestFileName[LENGTH_ABSOLUTE_PATH];
    uint32_t oldetFileSize = 0;
    time_t oldestTime = 0;

    dir = LittleFS.open(dirName);
    while (File file = dir.openNextFile()) {
        if (!file.isDirectory()) {
            time_t fileTime = file.getLastWrite();
            if (oldestTime == 0 || fileTime < oldestTime) {
                char fullFileName[LENGTH_ABSOLUTE_PATH];
                sprintf(fullFileName, "%s/%s", dirName, file.name());
                if (!isFileOpen(fullFileName)) {
                    oldestTime = fileTime;
                    strcpy(oldestFileName, file.name());
                    oldetFileSize = file.size();
                }
            }
        }
        file.close();
    }

    if (oldestTime > 0) {
        char fullFileName[LENGTH_ABSOLUTE_PATH];
        sprintf(fullFileName, "%s/%s", dirName, oldestFileName);
        if (LittleFS.remove(fullFileName)) {
            logger.logInternal(ELOG_LEVEL_NOTICE, "Removed oldest file: SPIFFS:%s", fullFileName);
        } else {
            logger.logInternal(ELOG_LEVEL_ERROR, "Failed to remove oldest file: SPIFFS:%s", fullFileName);
            return 0;
        }
    }

    // remove dir if empty
    dir = LittleFS.open(dirName);
    if (!dir.openNextFile()) {
        if (LittleFS.rmdir(dirName)) {
            logger.logInternal(ELOG_LEVEL_NOTICE, "Removed empty directory: %s", dirName);
        } else {
            logger.logInternal(ELOG_LEVEL_ERROR, "Failed to remove empty directory: %s", dirName);
        }
    }
    dir.close();

    return oldetFileSize;
}

/* Ensure that the filesystem is configured. If not, try to mount it
 * return: true if the filesystem is configured. False if it failed to mount. No SPIFFS file logging in that case
 * if it failed once, it will not try again - no more logging to SPIFFS
 */
bool LogSpiffs::ensureFilesystemConfigured()
{
    static bool failedOnce = false;

    if (fileSystemConfigured) {
        return true;
    }
    if (failedOnce) {
        return false;
    }

    if (!LittleFS.begin(true)) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Failed to mount SPIFFS. No SPIFFS file logging!");
        failedOnce = true;
        return false;
    } else {
        logger.logInternal(ELOG_LEVEL_INFO, "SPIFFS mounted");
        createNextLogDir();
        fileSystemConfigured = true;
    }
    return true;
}

/* Ensure that the file is open. If not, try to open it
 * setting: The setting for the file
 * return: true if the file is open
 */
bool LogSpiffs::ensureOpenFile(Setting& setting)
{
    if (!setting.spiffsFileHandle) { // No valid file handle
        setting.fileNumber++;
        char fullFileName[LENGTH_ABSOLUTE_PATH];
        sprintf(fullFileName, "%s/%s.%03d", currentLogDir, setting.fileName, setting.fileNumber);

        setting.spiffsFileHandle = LittleFS.open(fullFileName, FILE_WRITE);
        if (!setting.spiffsFileHandle) {
            logger.logInternal(ELOG_LEVEL_WARNING, "Could not create logfile SPIFFS:%s", fullFileName);
            return false;
        } else {
            logger.logInternal(ELOG_LEVEL_INFO, "Created logfile SPIFFS:%s", fullFileName);
            return true;
        }
        return false; // things has not changed. still no file handle
    } else {
        return true; // we have a valid file handle
    }
}

/* Ensure that there is free space on the filesystem. If not, remove the oldest files
 */
void LogSpiffs::ensureFreeSpace()
{
    static uint32_t checkAfterBytes = 0; // Start checking on boot
    static uint32_t bytesWrittenAtLastCheck = stats.bytesWrittenTotal;

    if (stats.bytesWrittenTotal - bytesWrittenAtLastCheck > checkAfterBytes) {
        uint32_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
        checkAfterBytes = freeSpace / 2; // check more often when free space is low
        bytesWrittenAtLastCheck = stats.bytesWrittenTotal;
        logger.logInternal(ELOG_LEVEL_DEBUG, "SPIFFS: Free space: %d bytes, check after: %d bytes", freeSpace, checkAfterBytes);
        if (freeSpace < SPIFFS_MIN_FREE_SPACE) {
            logger.logInternal(ELOG_LEVEL_DEBUG, "SPIFFS: Free space is lower than %d bytes. Removing oldest files", SPIFFS_MIN_FREE_SPACE);
            uint32_t removedBytes = 0;
            do {
                removedBytes += removeOldestFile();
                vTaskDelay(1); // feed the watchdog
            } while (removedBytes < SPIFFS_MIN_FREE_SPACE);
        }
    }
}

/* Ensure that the file size is not exceeded. If it is, close the file and a new one will be created by ensureOpenFile
 * setting: The setting for the file
 */
void LogSpiffs::ensureFileSize(Setting& setting)
{
    if (setting.bytesWritten > setting.maxLogFileSize) {
        setting.spiffsFileHandle.close();
        setting.spiffsFileHandle = File();
        setting.bytesWritten = 0;
    }
}

/* Syncronize all files. Write dirty cache to disk
 */
void LogSpiffs::allFilesSync()
{
    static uint32_t lastSynced = 0;

    if (millis() - lastSynced > SPIFFS_SYNC_FILES_EVERY) {
        logger.logInternal(ELOG_LEVEL_INFO, "Syncronizing all SPIFFS logfiles. Writing dirty cache");

        for (uint8_t i = 0; i < fileSettingsCount; i++) {
            Setting* setting = &settings[i];
            if (setting->spiffsFileHandle) {
                logger.logInternal(ELOG_LEVEL_DEBUG, "Syncronizing SPIFFS:%s/%s.%03d", currentLogDir, setting->fileName, setting->fileNumber);
                setting->spiffsFileHandle.flush();
            }
        }
        lastSynced = millis();
    }
}

/* Close all open files.
 * They will be reopened when needed by ensureOpenFile
 */
void LogSpiffs::allFilesClose()
{
    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (setting->spiffsFileHandle) {
            setting->spiffsFileHandle.close();
            setting->bytesWritten = 0;
            setting->fileNumber = 0;
        }
    }
}

/* Open all files that has been registered
 */
void LogSpiffs::allFilesOpen()
{
    for (uint8_t i = 0; i < fileSettingsCount; i++) {
        Setting* setting = &settings[i];
        if (!setting->spiffsFileHandle) {
            char fullFileName[LENGTH_ABSOLUTE_PATH];
            sprintf(fullFileName, "%s/%s.%03d", currentLogDir, setting->fileName, setting->fileNumber);
            setting->spiffsFileHandle = LittleFS.open(fullFileName, FILE_APPEND);
        }
    }
}

#endif // LOGGING_SPIFFS_DISABLE
