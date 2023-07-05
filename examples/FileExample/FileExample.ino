#include <Elog.h>

SPIClass spi = SPIClass(VSPI);

Elog logger;
Elog logger2;

void setup()
{
    Serial.begin(115200);
    spi.begin(18, 19, 17, 5); // Set your pins of your card reader here.

    Elog::globalSettings(100, 150, Serial, DEBUG); // We want a big buffer and internal debugging from the Elog lib.
    Elog::configureFilesystem(spi, 5, 2000000); // Speed set to 2mhz. Should be ok with dupont wires.

    // Whatever is sent to "logger" goes both to serial and to File1
    logger.addSerialLogging(Serial, "Main", DEBUG);
    logger.addFileLogging("File1", DEBUG);

    // Whatever is sent to "logger2" goes only to File2
    logger2.addFileLogging("File2", DEBUG);
}

void loop()
{
    for (uint32_t bigCounter = 0; bigCounter < 10000000; bigCounter++) {
        logger2.log(NOTICE, "Big counter is %d", bigCounter); // Log Only to file2
        for (uint8_t smallCounter = 0; smallCounter < 100; smallCounter++) {
            logger2.log(INFO, "Small Counter is %d", smallCounter); // Log Only to file2
        }
        logger.log(INFO, "Big counter is %d", bigCounter); // Log both to serial and file1

        /* If this is very short, we will fill up the buffer. Needs time to empty.
           It´s not so bad. The log call will just be busy waiting. */
        delay(500);
    }
}
