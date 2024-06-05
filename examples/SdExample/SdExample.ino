#include <Elog.h>

SPIClass spi = SPIClass(SPI);

Elog elog;
Elog logger2;

SdFat sd;

void setup()
{
    Serial.begin(115200);
    spi.begin(18, 19, 23, 5); // Set your pins of your card reader here.

    Elog::globalSettings(150, 150, Serial, INFO); // We want a big buffer and internal debugging from the Elog lib.
    Elog::configureSd(spi, 5, 2000000); // Speed set to 2mhz. Should be ok with dupont wires.

    // Whatever is sent to "logger" goes both to serial and to File1
    elog.addSerialLogging(Serial, "Main", DEBUG);
    elog.addSdLogging("File1", DEBUG);

    // Whatever is sent to "logger2" goes only to File2
    logger2.addSdLogging("File2", DEBUG);
}

void loop()
{
    for (uint32_t bigCounter = 0; bigCounter < 10000000; bigCounter++) {
        logger2.log(NOTICE, "Big counter is %d", bigCounter); // Log Only to file2
        for (uint8_t smallCounter = 0; smallCounter < 100; smallCounter++) {
            logger2.log(INFO, "Small Counter is %d", smallCounter); // Log Only to file2
        }
        elog.log(INFO, "Big counter is %d", bigCounter); // Log both to serial and file1

        /* If this is very short, we will fill up the buffer. Needs time to empty.
           It´s not so bad. The log call will just be busy waiting. */
        delay(1000);
    }
}
