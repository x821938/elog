#include <ELog.h>
#include <Etimer.h>

Elog elog;
Etimer timer("Timer1", elog, 20); // 20 lapses max on this timer

void setup()
{
    Serial.begin(115200);
    Elog::globalSettings(200, 10, &Serial, DEBUG); // Small buffer of 10 messages. We want internal logging from library
    elog.addSerialLogging(Serial, "Main", INFO); // Enable serial logging
}

void loop()
{
    elog.log(INFO, "Now we push the buffer to the limit by sending a fast burst of log message in a short time");
    timer.start();
    for (uint8_t lap = 1; lap <= 20; lap++) { // This will fill the buffer at some point
        timer.lap();
        elog.log(INFO, "Lap number = %d", lap);
    }
    timer.show(); // Have a look at the times. Its fast when buffer is not full

    delay(5000);

    elog.log(INFO, "Now we log something 100 times that is never logged because of loglevel. Should be fast!");
    timer.start();
    for (uint8_t lap = 1; lap <= 100; lap++) {
        elog.log(DEBUG, "This message is never showed");
    }
    timer.show();

    delay(5000);
}
