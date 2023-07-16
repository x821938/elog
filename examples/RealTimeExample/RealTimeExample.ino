/* There are many ways of getting time. This log library is made generic so you can choose your
   own source of time. It could be from RTC, GPS, NTP or whatever.

   If you for example chose NTP by using "arduino-libraries/NTPClient" library, you would do
   something like this:

   #include <time.h>
   struct tm t;
   getLocalTime(&t)
   elog.provideTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

   But in this example I just make up a starting point for demonstation purpose only
*/

#include <Elog.h>

Elog elog;

void setup()
{
    Serial.begin(115200);
    elog.addSerialLogging(Serial, "Main", DEBUG);
    elog.log(ALERT, "Sketch started");
}

void loop()
{
    for (uint16_t counter = 0; counter < 1000; counter++) {
        elog.log(INFO, "Our counter is %d", counter);
        if (counter == 3) { // Normally we dont have time from the very beginning. This simulates behaveour
            // We come up with 15th of july 2023 at 08:12:34
            // But find the time in your own way. There are many
            elog.log(NOTICE, "Now we provide some real human time");
            Elog::provideTime(2023, 7, 15, 8, 12, 34);
        }
        delay(1007);
    }
}
