#include <Elog.h>

Elog elog;

void setup()
{
    Serial.begin(115200);
    elog.addSerialLogging(Serial, "Main", INFO); // Enable serial logging. We want only INFO or lower logleve.
    elog.log(ALERT, "Sketch started");
}

void loop()
{
    for (uint16_t counter = 0; counter < 1000; counter++) {
        elog.log(INFO, "Our counter is %d", counter);
        elog.log(DEBUG, "This is not shown, because we want loglevel lower or equal to INFO");
        if (counter % 5 == 0) {
            elog.log(NOTICE, "Counter is dividable by 5");
        }
        delay(1000);
    }
}
