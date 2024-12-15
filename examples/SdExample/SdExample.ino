#include <Elog.h>

// Define the log IDs. These are used to identify the different logfiles
#define MAIN 0
#define SUB_COUNTER 1
#define HEX_LOG 2

SPIClass spi = SPIClass(SPI);
uint32_t mainCounter = 0;
void getRandomString(char* str, int maxLength); // declaration

void setup()
{
    Serial.begin(115200);

    // If you want to see the internal logs from the elog library, uncomment the line below
    // logger.configureInternalLogging(Serial, ELOG_LEVEL_DEBUG, 60000);

    logger.configure(100, true); // Big buffer

    // Set up the SD card reader with hardware pins
    spi.begin(18, 19, 23, 5); // Set your pins of your card reader here. (SCK, MISO, MOSI, SS pin)
    logger.configureSd(spi, 5, 2000000); // Set the speed of the SD card reader

    // Register some log IDs for logging
    logger.registerSerial(MAIN, ELOG_LEVEL_DEBUG, "count", Serial, FLAG_NO_SERVICE | FLAG_NO_LEVEL); // On serial, we don't want to show the service name and log level
    logger.registerSd(MAIN, ELOG_LEVEL_DEBUG, "main");
    logger.registerSd(SUB_COUNTER, ELOG_LEVEL_ERROR, "subcount"); // We only want to log errors
    logger.registerSd(HEX_LOG, ELOG_LEVEL_DEBUG, "hex");

    // Simulate the time by providing a fixed time to the RTC (You can also use the NTP time)
    logger.provideTime(2023, 7, 31, 10, 12, 51);

    // This enables the query mode. The query mode allows you to send commands to the device. Press space to enter the command mode
    logger.enableQuery(Serial);

    logger.log(MAIN, ELOG_LEVEL_INFO, "Setup completed. Press space to enter the command mode and view the logs");
}

void loop()
{
    char randomString[50];
    for (long subCounter = 0; subCounter < 40; subCounter++) { // Fast burst of data to almost fill the buffer
        getRandomString(randomString, sizeof(randomString));
        uint8_t randomLogLevel = random(0, 8);
        logger.logHex(HEX_LOG, ELOG_LEVEL_DEBUG, "HEX:", (uint8_t*)randomString, strlen(randomString));
        logger.log(SUB_COUNTER, randomLogLevel, "Main Counter: %d, Subcounter: %d, random string: %s", mainCounter, subCounter, randomString);
    }
    logger.log(MAIN, ELOG_LEVEL_INFO, "Main Counter: %d", mainCounter);

    mainCounter++;
    delay(10000);
}

// Returns a random string with a random length, just to simulate some data
void getRandomString(char* str, int maxLength)
{
    int length = random(1, maxLength);
    for (int strCount = 0; strCount < length; strCount++) {
        str[strCount] = char(random(65, 91));
    }
    str[length] = '\0';
}
