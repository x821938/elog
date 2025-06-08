#ifdef ELOG_SD_ENABLE

#include <Elog.h>
#include <LogSd.h>

SPIClass LogSD::spi;

/* reset statistics
 */
void LogSD::begin()
{
    stats.bytesWrittenTotal = 0;
    stats.messagesWrittenTotal = 0;
    stats.messagesDiscardedTotal = 0;
}

/*  This should be called to set up logging to the SD card
    It connects to the sd card reader. Parameters:
    spi: The SPI object where the SD-reader is connected to.
    cs: Chip-select pin for the SD-reader
    speed: How fast to talk to SD-reader. in Hz - default is 2Mhz. Don´t put it to high or you will get errors writing
    spiOption: If you have multiple SPI devices, you can use DEDICATED_SPI or SHARED_SPI
    maxRegistrations: How many log files you want to register. Default is 10
*/
void LogSD::configure(SPIClass& spi, const uint8_t cs, const uint32_t speed, uint8_t spiOption, const uint8_t maxRegistrations)
{
    Logger.logInternal(ELOG_LEVEL_INFO, "Configuring file logging to SD card");
    if (!sdConfigured) {
        this->spi = spi;
        this->sdChipSelect = cs;
        this->sdSpeed = speed;
        this->sdSpiOption = spiOption;
        this->maxRegistrations = maxRegistrations;

        sdCardPresent = false;
        sdCardLastReconnect = LONG_MIN;
        sdConfigured = true; // This is for our writerTask. When true it will start writing to sd card.

        settings = new Setting[maxRegistrations];
        Logger.logInternal(ELOG_LEVEL_DEBUG, "Max SD registrations: %d", maxRegistrations);
    } else {
        Logger.logInternal(ELOG_LEVEL_ERROR, "SD logging already configured with %d registrations", maxRegistrations);
    }
}

/*  Register a log file for logging to the SD card. Parameters:
    logId: unique id for the log
    loglevel: the log level that should be logged
    fileName: the name of the file. Must be 8 characters or less and only contain alphanumeric characters
    logFlags: flags for the log (see LogFormat.h)
    maxLogFileSize: the maximum size of the log file in bytes. If the file exceeds this size, it will be rotated
*/
void LogSD::registerSd(const uint8_t logId, const uint8_t loglevel, const char* fileName, const uint8_t logFlags, const uint32_t maxLogFileSize)
{
    if (!sdConfigured) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "SD card not configured");
        return;
    }

    if (!isValidFileName(fileName)) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Invalid filename %s", fileName);
        return;
    }

    if (isFileNameRegistered(fileName)) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Filename %s already registered with logId %d", fileName, logId);
        return;
    }

    if (registeredSdCount >= maxRegistrations) {
        Logger.logInternal(ELOG_LEVEL_ERROR, "Maximum number of registered SD logfiles reached: %d", maxRegistrations);
        return;
    }

    Setting* setting = &settings[registeredSdCount++];

    setting->logId = logId;
    setting->sdFileHandle = new file_t();
    setting->fileName = fileName;
    setting->logLevel = loglevel;
    setting->lastMsgLogLevel = ELOG_LEVEL_NOLOG;
    setting->maxLogFileSize = maxLogFileSize;
    setting->fileNumber = 0;
    setting->bytesWritten = 0;

    setting->logFlags = logFlags | ELOG_FLAG_NO_SERVICE; // Servicename makes no sense in a file
    setting->sdFileCreteLastTry = LONG_MIN; // This triggers log file creation immediately

    char logLevelStr[10];
    formatter.getLogLevelStringRaw(logLevelStr, loglevel);
    Logger.logInternal(ELOG_LEVEL_INFO, "Registered SD log id %d, level %s, filename %s", logId, logLevelStr, fileName);
}

uint8_t LogSD::getLogLevel(const uint8_t logId, const char* fileName)
{
    for (uint8_t i = 0; i < registeredSdCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && strcmp(settings->fileName, fileName) == 0) {
            return setting->logLevel;
        }
    }

    return ELOG_LEVEL_NOLOG;
}

void LogSD::setLogLevel(const uint8_t logId, const uint8_t loglevel, const char* fileName)
{
    for (uint8_t i = 0; i < registeredSdCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && strcmp(settings->fileName, fileName) == 0) {
            setting->logLevel = loglevel;
        }
    }
}

uint8_t LogSD::getLastMsgLogLevel(const uint8_t logId, const char* fileName)
{
    for (uint8_t i = 0; i < registeredSdCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId && strcmp(settings->fileName, fileName) == 0) {
            return setting->lastMsgLogLevel;
        }
    }

    return ELOG_LEVEL_NOLOG;
}

/* Output the logline to the SD log files. Traverse all registered log files and output to the ones that match the logId and logLevel
 * logLineEntry: The logline to output
 */
void LogSD::outputFromBuffer(const LogLineEntry logLineEntry)
{
    for (uint8_t i = 0; i < registeredSdCount; i++) {
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

/* Handle peeking at log messages.  If peek is enabled, the log message will be printed to the querySerial if it matches the peek criteria
 * logLineEntry: the log line entry
 * settingIndex: the index of the setting in the fileSettings array
 */
void LogSD::handlePeek(const LogLineEntry logLineEntry, const uint8_t settingIndex)
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

/*  Write the logline to the SD card. If the SD card is not present, we try to reconnect. If we cant reconnect, we discard the message.
    If the file is not open, we try to create it. If we cant create it, we discard the message.
    If the file is too big, we close it and try to create a new one.
    logLineEntry: The logline to write
    setting: The setting for the log file
*/
void LogSD::write(LogLineEntry logLineEntry, Setting& setting)
{
    static char logStamp[LENGTH_OF_LOG_STAMP];
    uint8_t logId = logLineEntry.logId;

    if (sdConfigured && !sdCardPresent) {
        reconnect();
    }

    if (sdConfigured) {
        if (sdCardPresent) {
            formatter.getLogStamp(logStamp, logLineEntry.timestamp, logLineEntry.logLevel, "", setting.logFlags);

            createLogFileIfClosed(setting);
            if (setting.sdFileHandle->isOpen()) { // Are we working on a valid file?
                ensureFreeSpace();
                size_t bytesWritten; // Number of bytes written should be the same as content length
                size_t expectedBytes = strlen(logStamp) + strlen(logLineEntry.logMessage) + 2; // 2 chars for endline

                bytesWritten = setting.sdFileHandle->print(logStamp);
                bytesWritten += setting.sdFileHandle->print(logLineEntry.logMessage);
                bytesWritten += setting.sdFileHandle->println();

                if (bytesWritten != expectedBytes) { // If not everything is written, then the SD must be ejected.
                    sdCardPresent = false;
                    stats.messagesDiscardedTotal++;
                    Logger.logInternal(ELOG_LEVEL_WARNING, "SD card ejected");
                    allFilesClose();
                } else { // Data written succesfully.
                    stats.messagesWrittenTotal++;
                    stats.bytesWrittenTotal += bytesWritten;
                    setting.bytesWritten += bytesWritten;
                }
            } else { // If we dont have a valid filehandle we try do create it periodically
                stats.messagesDiscardedTotal++;
            }
            allFilesSync(); // Keep files synced to card periodically
            ensureFileSize(setting); // Check if we need to rotate the file
        } else { // If we dont have a valid SD card, we discard the message
            stats.messagesDiscardedTotal++;
        }
    }
}

/* Traverse all registered log files and check if the logId and logLevel match the setting
 * logId: The log id
 * logLevel: The log level
 * return: true if the log should be written to filesystem
 */
bool LogSD::mustLog(const uint8_t logId, const uint8_t logLevel)
{
    for (uint8_t i = 0; i < registeredSdCount; i++) {
        Setting* setting = &settings[i];
        if (setting->logId == logId) {
            if (logLevel <= setting->logLevel && (setting->logLevel != ELOG_LEVEL_NOLOG || logLevel == ELOG_LEVEL_ALWAYS)) {
                return true;
            }
        }
    }
    return false;
}

/* Output the statistics for the SD card
 */
void LogSD::outputStats()
{
    if (sdConfigured) {
        Logger.logInternal(ELOG_LEVEL_INFO, "SD stats. Messages written: %d, messages discarded: %d, bytes written: %d", stats.messagesWrittenTotal, stats.messagesDiscardedTotal, stats.bytesWrittenTotal);
    }
}

/*  Enable query commands for the SD card. This is used for the query command interface
    querySerial: The serial port where the query commands are sent
*/
void LogSD::enableQuery(Stream& querySerial)
{
    this->querySerial = &querySerial;
}

/*  Stop peeking at log messages
 */
void LogSD::peekStop()
{
    peekEnabled = false;
}

/* Return the number of registrations
 */
uint8_t LogSD::registeredCount()
{
    return registeredSdCount;
}

/* Print the help specific for the SD file system
 */
void LogSD::queryCmdHelp()
{
    querySerial->println("dir <directory> (list directory)");
    querySerial->println("cd <directory> (change directory)");
    querySerial->println("type <file> (print file content. Press Q to quit. Space to pause)");
    querySerial->println("rm <file> (remove file)");
    querySerial->println("rmdir <directory> (remove directory recursively)");
    querySerial->println("format (format filesystem with no warning)");
    querySerial->println("peek <filename> <loglevel> <filtertext> (filename can be *, filtertext is optional)");
}

/*  List the directory.
    directory: The directory to list
*/
void LogSD::queryCmdDir(const char* directory)
{
    if (strlen(directory) == 0) {
        directory = queryCwd;
    }

    char absolutePath[50];
    getPathFromRelative(absolutePath, directory);

    SdFile dirFile, file;
    if (!dirFile.open(absolutePath, O_READ)) {
        querySerial->printf("Directory %s not found\n", absolutePath);
        return;
    }

    while (file.openNext(&dirFile, O_READ)) {
        if (file.isDir()) {
            char name[20];
            file.getName(name, 20);
            querySerial->printf("%s\n", name);
        } else {
            char name[20];
            file.getName(name, 20);

            uint16_t pdate, ptime;
            uint32_t size = file.fileSize();
            file.getModifyDateTime(&pdate, &ptime);

            querySerial->printf("%s [", name);
            fsPrintDateTime(querySerial, pdate, ptime);
            querySerial->printf("] (%d bytes)\n", size);
        }
        file.close();
    }
    dirFile.close();
    queryPrintVolumeInfo();
}

/*  Change directory
    directory: The directory to change to
*/
void LogSD::queryCmdCd(const char* directory)
{
    char oldCwd[20];
    strcpy(oldCwd, queryCwd);

    if (strcmp(directory, "..") == 0) {
        strcpy(queryCwd, SD_LOG_ROOT); // only one level supported
        return;
    } else if (strcmp(directory, ".") == 0) {
        return;
    } else if (strcmp(directory, "/") == 0) {
        strcpy(queryCwd, SD_LOG_ROOT);
        return;
    }
    if (directory[0] == '/') {
        strcpy(queryCwd, directory);
    } else {
        if (strlen(queryCwd) > 1) {
            sprintf(queryCwd, "%s/%s", queryCwd, directory);
        } else {
            sprintf(queryCwd, "/%s", directory);
        }
    }

    SdFile dirFile;
    if (!dirFile.open(queryCwd, O_READ)) {
        querySerial->printf("Directory %s not found\n", queryCwd);
        strcpy(queryCwd, oldCwd);
        return;
    }
    dirFile.close();
}

/* Remove a file
    path: The path to the file
*/
void LogSD::queryCmdRm(const char* path)
{
    char absolutePath[50];
    getPathFromRelative(absolutePath, path);

    if (sd.remove(absolutePath)) {
        querySerial->printf("Removed file %s\n", absolutePath);
    } else {
        querySerial->printf("Failed to remove file %s\n", absolutePath);
    }
}

/* Remove a directory recursively
    path: The path to the directory
*/
void LogSD::queryCmdRmdir(const char* path)
{
    char absolutePath[50];
    getPathFromRelative(absolutePath, path);

    SdFile dir;
    SdFile file;
    if (dir.open(absolutePath, O_DIRECTORY)) {
        char name[20];
        while (file.openNext(&dir, O_READ)) {
            file.getName(name, 20);
            file.close();
            char path[50];
            sprintf(path, "%s/%s", absolutePath, name);
            queryCmdRm(path);
            vTaskDelay(1); // feed the watchdog
        }
        dir.close();

        bool status = sd.rmdir(absolutePath);
        if (status) {
            querySerial->printf("Removed directory %s\n", absolutePath);
        } else {
            querySerial->printf("Failed to remove directory %s\n", absolutePath);
        }
    } else {
        querySerial->printf("Directory %s not found\n", absolutePath);
    }
}

/* Format the SD card
 */
void LogSD::queryCmdFormat()
{
    if (sdConfigured) {
        querySerial->print("Formatting SD card... ");
        if (sd.format()) {
            querySerial->println("Done!");
        } else {
            querySerial->println("Could not format SD card!");
        }
    } else {
        querySerial->println("SD card not configured");
    }
}

/* Print the content of a file
    filename: The filename to print
*/
void LogSD::queryCmdType(const char* filename)
{
    char absolutePath[50];
    getPathFromRelative(absolutePath, filename);

    SdFile file;
    if (!file.open(absolutePath, O_READ)) {
        querySerial->printf("File %s not found\n", absolutePath);
        return;
    }

    while (file.available()) {
        querySerial->write(file.read());
        if (querySerial->available()) {
            char c = querySerial->read();
            if (c == 'Q' || c == 'q') { // Print can be aborted with Q
                querySerial->println("\nAborted!");
                return;
            }
            if (c == 'S' || c == 's') { // Or paused with S
                file.seekCur(50000); // Skip 50k
            }
            if (c == ' ') { // Or paused with space
                while (!querySerial->available()) {
                    vTaskDelay(1);
                }
                querySerial->read();
            }
        }
    }
    file.close();
}

/*  Start peeking at log messages
    filename: The filename to peek at
    loglevel: The loglevel to peek at
    textFilter: The text filter to peek at
    return: true if peeking is enabled
*/
bool LogSD::queryCmdPeek(const char* filename, const char* loglevel, const char* textFilter)
{
    peekLoglevel = formatter.getLogLevelFromString(loglevel);
    if (peekLoglevel == ELOG_LEVEL_NOLOG) {
        querySerial->printf("Invalid loglevel\n\npeek <filename> <loglevel> <filtertext>\nAllowed loglevels are: verbo, trace, debug, info, notic, warn, error, crit, alert, emerg\n", loglevel);
        return false;
    }

    if (strcmp(filename, "*") == 0) {
        peekAllFiles = true;
    } else {
        bool found = false;
        for (uint8_t i = 0; i < registeredSdCount; i++) {
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

/* Print the status of the SD card
 */
void LogSD::queryCmdStatus()
{
    char buffer[20];
    formatter.getHumanSize(buffer, stats.bytesWrittenTotal);

    querySerial->println();
    querySerial->printf("SD Card present: %s\n", sdCardPresent ? "Yes" : "No");
    querySerial->printf("SD total, bytes written: %s\n", buffer);
    querySerial->printf("SD total, messages written: %d\n", stats.messagesWrittenTotal);
    querySerial->printf("SD total, messages discarded: %d\n", stats.messagesDiscardedTotal);

    for (uint8_t i = 0; i < registeredSdCount; i++) {
        Setting setting = settings[i];
        char filename[50];
        char logLevelStr[10];
        getSettingFullFileName(filename, setting);
        formatter.getLogLevelStringRaw(logLevelStr, setting.logLevel);
        querySerial->printf("SD reg, SD:%s, (ID %d, level %s) - %d bytes written\n", filename, setting.logId, logLevelStr, setting.bytesWritten);
    }
}

/* Print the prompt for the query commands
 */
void LogSD::queryPrintPrompt()
{
    querySerial->printf("\nSD:%s> ", queryCwd);
}

/* Print the volume information for the SD card
 */
void LogSD::queryPrintVolumeInfo()
{
    uint32_t freeClusters = sd.vol()->freeClusterCount();
    uint32_t clusterCount = sd.vol()->clusterCount();
    uint16_t bytesPerCluster = sd.vol()->bytesPerCluster();

    uint32_t totalBytes = clusterCount * bytesPerCluster;
    uint32_t freeBytes = freeClusters * bytesPerCluster;
    uint32_t usedBytes = totalBytes - freeBytes;
    float usage = static_cast<float>(usedBytes) / totalBytes * 100;

    char totalBytesStr[20];
    char usedBytesStr[20];
    char freeBytesStr[20];
    formatter.getHumanSize(totalBytesStr, totalBytes);
    formatter.getHumanSize(usedBytesStr, usedBytes);
    formatter.getHumanSize(freeBytesStr, freeBytes);

    querySerial->printf("\nTotal space: %s, Used space: %s, Free space: %s, usage: %.2f%%\n", totalBytesStr, usedBytesStr, freeBytesStr, usage);
}

/* check if the filename is valid. It must be 8 characters or less and only contain alphanumeric characters
 * filename: The filename to check
 * return: true if the filename is valid
 */
bool LogSD::isValidFileName(const char* fileName)
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

/* check if the filename is already registered
 * filename: The filename to check
 * return: true if the filename is already registered
 */
bool LogSD::isFileNameRegistered(const char* fileName)
{
    for (uint8_t i = 0; i < registeredSdCount; i++) {
        if (strcmp(settings[i].fileName, fileName) == 0) {
            return true;
        }
    }
    return false;
}

/*  This method is called from the writerTask to ensure that we have enough space on the SD card.
    If we dont have enough space, we will remove the oldest files in the oldest directory.
    It's very time consuming to check free space on the SD card, so we only do it every X bytes.
*/
void LogSD::ensureFreeSpace()
{
    static uint32_t checkAfterBytes = 0; // Start checking on boot
    static uint32_t bytesWrittenAtLastCheck = stats.bytesWrittenTotal;

    if (stats.bytesWrittenTotal - bytesWrittenAtLastCheck > checkAfterBytes) {
        uint32_t freeSpace = getFreeSpace();
        checkAfterBytes = freeSpace / 2; // check more often when free space is low
        bytesWrittenAtLastCheck = stats.bytesWrittenTotal;
        Logger.logInternal(ELOG_LEVEL_DEBUG, "SD: Free space: %d bytes, check after: %d bytes", freeSpace, checkAfterBytes);
        if (freeSpace < SD_MIN_FREE_SPACE) {
            Logger.logInternal(ELOG_LEVEL_DEBUG, "SD: Free space is lower than %d bytes. Removing oldest files", SD_MIN_FREE_SPACE);
            uint32_t removedBytes = 0;
            do {
                removedBytes += removeOldestFile();
                vTaskDelay(1); // feed the watchdog
            } while (removedBytes < SD_MIN_FREE_SPACE);
        }
    }
}

/* check if the file is too big and if so, close it and a new one will be created
 * setting: The setting for the log file
 */
void LogSD::ensureFileSize(Setting& setting)
{
    if (setting.bytesWritten > setting.maxLogFileSize) {
        setting.sdFileHandle->close();
        setting.sdFileCreteLastTry = LONG_MIN; // This triggers log file creation immediately
        setting.bytesWritten = 0;
    }
}

/* Connects to the SD card if it is not present. If the card is present, it reads the log number from the lognumber.txt file
 * and finds the next log directory. If the directory does not exist, it creates it.
 * All logfiles will be written to this directory.
 */
void LogSD::reconnect()
{
    if (shouldReconnect()) {
        attemptReconnect();
        if (sdCardPresent) {
            readLogNumber();
            findNextLogDir();
            createLogDirectory();
            writeLogNumber();
            filesInLogDir = 0;
        }
    }
}

/* check if we should reconnect to the SD card. We should not do this too often, as it is time consuming
 * return: true if we should reconnect
 */
bool LogSD::shouldReconnect()
{
    return !sdCardPresent && (millis() - sdCardLastReconnect) > SD_RECONNECT_EVERY;
}

/*  Try to reconnect to the SD card. If we can't connect, we will try again after a while
 */
void LogSD::attemptReconnect()
{
    sdCardLastReconnect = millis();
    Logger.logInternal(ELOG_LEVEL_INFO, "Trying to connect to SD card");

    // Dedicated SPI if we only have one SPI device
    if (!sd.begin(SdSpiConfig(sdChipSelect, sdSpiOption, sdSpeed, &spi))) {
        Logger.logInternal(ELOG_LEVEL_WARNING, "SD card initialization failed");
        sd.end();
    } else {
        sdCardPresent = true;
        Logger.logInternal(ELOG_LEVEL_INFO, "SD card detected");
    }
}

/* get the current log number from the lognumber.txt file
 */
void LogSD::readLogNumber()
{
    file_t file;
    if (file.open(SD_LOGNUMBER_FILE, O_READ)) {
        char fileContent[10];
        file.read(fileContent, 10);
        sdLogNumber = atoi(fileContent);

        Logger.logInternal(ELOG_LEVEL_DEBUG, "Read file SD:%s and got log number %d", SD_LOGNUMBER_FILE, sdLogNumber);
        file.close();
    } else {
        Logger.logInternal(ELOG_LEVEL_DEBUG, "No SD:%s file", SD_LOGNUMBER_FILE);
        sdLogNumber = 1; // We start from number 1 if there is no lognumber.txt file
    }
}

/* write the current log number to the lognumber.txt file
 */
void LogSD::writeLogNumber()
{
    file_t file;
    file.open(SD_LOGNUMBER_FILE, O_WRITE | O_CREAT);
    Logger.logInternal(ELOG_LEVEL_DEBUG, "Writing SD:%s file with lognumber %d", SD_LOGNUMBER_FILE, sdLogNumber);
    if (file) {
        file.print(sdLogNumber);
        file.close();
    } else {
        Logger.logInternal(ELOG_LEVEL_ALERT, "Error writing to SD:%s. No file logging!", SD_LOGNUMBER_FILE);
    }
}

/* get the next available log directory
 */
void LogSD::findNextLogDir()
{
    while (true) {
        if (logDirectoryExists()) {
            sdLogNumber++;
        } else {
            break;
        }
    }
}

/* check if the current log directory exists
 * return: true if the log directory exists
 */
bool LogSD::logDirectoryExists()
{
    char dirName[20];
    sprintf(dirName, "%s/%04d", SD_LOG_ROOT, sdLogNumber);
    return sd.exists(dirName);
}

/* create the log directory
 */
void LogSD::createLogDirectory()
{
    sprintf(logCwd, "%s/%04d", SD_LOG_ROOT, sdLogNumber);
    sd.mkdir(logCwd);
    Logger.logInternal(ELOG_LEVEL_DEBUG, "Created directory SD:%s", logCwd);
}

/*Adjust the provided path for cwd
 */
void LogSD::getPathFromRelative(char* output, const char* path)
{
    if (path[0] == '/') {
        strcpy(output, path);
    } else {
        sprintf(output, "%s/%s", queryCwd, path);
    }
}

/* Removes the oldest file in the oldest directory
 * return: the size of the removed file
 */
uint32_t LogSD::removeOldestFile()
{
    char dirName[15];
    char fileName[20];
    uint32_t fileSize = 0;

    uint16_t oldestLogNumber = getOldestLogDir();

    if (oldestLogNumber < UINT16_MAX) {
        sprintf(dirName, "%s/%04d", SD_LOG_ROOT, oldestLogNumber);
        SdFile dir;

        bool status = getOldestLogFileInDir(fileName, dirName, fileSize);
        if (status) {
            char path[50];
            sprintf(path, "%s/%s", dirName, fileName);
            if (sd.remove(path)) {
                Logger.logInternal(ELOG_LEVEL_NOTICE, "Removed oldest file SD:%s", path);
            } else {
                Logger.logInternal(ELOG_LEVEL_WARNING, "Failed to remove oldest file SD:%s", path);
            }
        } else {
            if (sd.rmdir(dirName)) {
                Logger.logInternal(ELOG_LEVEL_NOTICE, "Removed empty directory SD:%s", dirName);
            } else {
                Logger.logInternal(ELOG_LEVEL_WARNING, "Failed to remove empty directory SD:%s", dirName);
            }
        }
    } else {
        Logger.logInternal(ELOG_LEVEL_WARNING, "No files found in root directory of SD card");
    }
    return fileSize;
}

/* find the oldest log dir
 * return: the lognumber of the oldest dir
 */
uint16_t LogSD::getOldestLogDir()
{
    uint16_t oldestLogNumber = UINT16_MAX;
    SdFile logDir;
    if (!logDir.open(SD_LOG_ROOT, O_READ)) {
        return oldestLogNumber;
    }

    SdFile dir;
    while (dir.openNext(&logDir, O_READ)) {
        char dirName[15];
        dir.getName(dirName, 15);
        if (dir.isDir() && isdigit(dirName[0])) { // Only look at directories starting with a number (our log directories)
            uint16_t logNumber = atoi(dirName);
            if (logNumber < oldestLogNumber) {
                oldestLogNumber = logNumber;
            }
        }
        dir.close();
    }
    return oldestLogNumber;
}

/* find the oldest log file in the directory
 * output: the name of the oldest file
 * dirName: the directory to search
 * fileSize: the size of the oldest file
 * return: true if the oldest file was found
 */
bool LogSD::getOldestLogFileInDir(char* output, const char* dirName, uint32_t& fileSize)
{
    uint32_t oldestTimeStamp = UINT32_MAX;
    fileSize = 0;

    SdFile dir;
    if (!dir.open(dirName, O_DIRECTORY)) {
        return false;
    }

    SdFile file;
    while (file.openNext(&dir, O_READ)) {
        uint16_t pdate, ptime;
        file.getModifyDateTime(&pdate, &ptime);
        uint32_t timeStamp = convertToEpoch(pdate, ptime);
        if (timeStamp < oldestTimeStamp) {
            oldestTimeStamp = timeStamp;
            file.getName(output, 20);
            fileSize = file.fileSize();
        }
        file.close();
    }
    dir.close();

    if (oldestTimeStamp == UINT32_MAX) {
        return false;
    }

    return true;
}

/* Get the free space on the SD card. This is a very slow operation, so we do it as little as possible
 */
uint32_t LogSD::getFreeSpace()
{
    uint16_t bytesPerCluster = sd.vol()->bytesPerCluster();
    uint32_t freeClusters = sd.vol()->freeClusterCount();
    return freeClusters * bytesPerCluster;
}

/* Get the full filename for a setting
 * output: the full filename
 * setting: the setting
 */
void LogSD::getSettingFullFileName(char* output, Setting setting)
{
    sprintf(output, "%s/%s.%03d", logCwd, setting.fileName, setting.fileNumber);
}

/* Timestamp the file with the current time
 * setting: the setting for the file
 */
void LogSD::timestampFile(Setting& setting)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tmstruct = localtime(&tv.tv_sec);
    setting.sdFileHandle->timestamp(T_WRITE + T_CREATE, (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
}

/* Convert the pdate and ptime to epoch time
 * pdate: the date
 * ptime: the time
 * return: the epoch time
 */
uint32_t LogSD::convertToEpoch(uint16_t pdate, uint16_t ptime)
{
    // Extract date components
    int year = 1980 + ((pdate >> 9) & 127); // Years since 1980
    int month = (pdate >> 5) & 15; // Months 1-12
    int day = pdate & 31; // Day 1-31

    // Extract time components
    int hour = (ptime >> 11) & 31; // Hours 0-23
    int minute = (ptime >> 5) & 63; // Minutes 0-59
    int second = (ptime & 31) * 2; // Seconds 0-59 (in 2-second increments)

    // Convert to tm struct
    std::tm timeinfo = {};
    timeinfo.tm_year = year - 1900; // tm_year is years since 1900
    timeinfo.tm_mon = month - 1; // tm_mon is 0-11
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    // Convert to time_t (epoch time)
    time_t epochTime = std::mktime(&timeinfo);

    // Adjust for your timezone if necessary
    // epochTime += timeZoneOffsetInSeconds;

    return static_cast<uint32_t>(epochTime);
}

/*  Tries to create a logfile in our logdirectory if we dont have an open file already. If it fails, the next time we will try again is after
    SD_FILE_TRY_CREATE_EVERY milliseconds.
*/
void LogSD::createLogFileIfClosed(Setting& setting)
{
    if (!setting.sdFileHandle->isOpen()) { // Only do something if we dont have a valid filehandle
        if ((millis() - setting.sdFileCreteLastTry) >= SD_RECONNECT_EVERY) {
            if (filesInLogDir >= MAX_LOGFILES_IN_DIR) {
                Logger.logInternal(ELOG_LEVEL_WARNING, "Maximum number of files in directory reached.");
                allFilesClose();
                sdCardPresent = false;
                reconnect();
            }

            setting.fileNumber++;
            char filename[50];
            getSettingFullFileName(filename, setting);

            bool success = setting.sdFileHandle->open(filename, O_CREAT | O_WRITE);
            if (success) {
                Logger.logInternal(ELOG_LEVEL_INFO, "Created logfile SD:%s", filename);
                filesInLogDir++;
            } else {
                Logger.logInternal(ELOG_LEVEL_ERROR, "Could not create logfile SD:%s", filename);
            }
            setting.sdFileCreteLastTry = millis();
            setting.bytesWritten = 0;
        }
    }
}

/* Traverses all Logger instances and closes each file associated with each instnace.
   This is needed after a sd card reconnect */
void LogSD::allFilesClose()
{
    Logger.logInternal(ELOG_LEVEL_INFO, "Closing all logfiles");
    for (uint8_t i = 0; i < registeredSdCount; i++) {
        Setting* setting = &settings[i];
        if (setting->sdFileHandle->isOpen()) {
            char filename[50];
            getSettingFullFileName(filename, *setting);
            Logger.logInternal(ELOG_LEVEL_DEBUG, "Closing SD:%s", filename);
            setting->sdFileHandle->close();
        }
        setting->sdFileCreteLastTry = LONG_MIN; // This triggers log file creation immediately
        setting->bytesWritten = 0;
        setting->fileNumber = 0;
    }
}

/* Traverses all Logger instances and forces a write of each file associated with each instance.
   This method can be called as often as wanted, but only every SD_SYNC_FILES_EVERY milliseconds
   the cache is synced to the sd card */
void LogSD::allFilesSync()
{
    static uint32_t lastSynced = 0;
    bool success = false;

    if (sdCardPresent) {
        if (millis() - lastSynced > SD_SYNC_FILES_EVERY) {
            Logger.logInternal(ELOG_LEVEL_INFO, "Syncronizing all SD logfiles. Writing dirty cache");

            for (uint8_t i = 0; i < registeredSdCount; i++) {
                Setting setting = settings[i];
                if (setting.sdFileHandle->isOpen()) {
                    char filename[50];
                    getSettingFullFileName(filename, setting);
                    Logger.logInternal(ELOG_LEVEL_DEBUG, "Syncronizing SD:%s", filename);
                    if (formatter.realTimeProvided()) {
                        timestampFile(setting);
                    }
                    success = setting.sdFileHandle->sync();
                    if (!success) {
                        Logger.logInternal(ELOG_LEVEL_WARNING, "Could not sync file SD:%s.%03d", setting.fileName, setting.fileNumber);
                    }
                }
            }
            lastSynced = millis();
        }
    }
}

#endif // ELOG_SD_ENABLE
