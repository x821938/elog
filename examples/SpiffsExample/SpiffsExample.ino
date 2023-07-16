/* Dont let this sketch run for days on your ESP. It's writing to internal flash!

   There is about 100000 write cycles before your device starts failing.
   With this sketch it will fill up the flash in about 8 minutes. Then it will
   delete the logfile and start again.

   Local flash is ok for logging if it's not too extensive.

   In console press any key, and you can review the logs on the flash */

#include <Elog.h>

Elog logger;
Elog logger2;

void setup()
{
    Serial.begin(115200);

    Elog::globalSettings(150, 150); // We want a big buffer!
    Elog::configureSpiffs(); // Must be done before adding spiffs logging;

    logger.addSpiffsLogging("spf", DEBUG); // All logging to "logger" goes to spiffs

    logger2.addSerialLogging(Serial, "Main", DEBUG); // All logging to "logger2" goes to serial
    logger2.addSpiffsLogging("2nd", DEBUG); // Also all logging to "logger2" goes to spiffs
}

void loop()
{
    for (uint32_t bigCounter = 0; bigCounter < 10000000; bigCounter++) {
        logger2.log(NOTICE, "Big counter is %d. At any time send a char to serial terminal to see the spiffs logs...", bigCounter); // Goes to serial
        for (uint8_t smallCounter = 0; smallCounter < 100; smallCounter++) {
            // Here we make a fast burst of 100 log messages... Should be consumed by our buffer
            logger.log(INFO, "Big counter is %d, small counter=%d", bigCounter, smallCounter); // Goes to spiffs
        }

        logger2.log(DEBUG, "We wait 5 seconds to give spiffs time to write the content of the buffer.");
        uint32_t t = millis();
        while (millis() - t < 5000) {
            if (Serial.available()) { // Press any key on serial console to be able to dump the logs.
                logger.spiffsQuery(Serial);
            }
        }
    }
}
