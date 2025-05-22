#include <Arduino.h>
#include <Elog.h>
#include <LogTimer.h>

#define LOG_TIMER 0
#define LOG_TEST 1
#define TIMER1 0
#define TIMER_TOTAL 1

void loggedNo();
void loggedYes();
void loggedYesBufferFull();

void setup()
{
    Serial.begin(115200);

    Logger.configure(50, true); // Logbuffer max 50 lines. Wait if buffer full.
    timer.configure(5, 50); // 5 timers, 20 laps each
    Logger.registerSerial(LOG_TIMER, ELOG_LEVEL_DEBUG, "TIMER");
    Logger.registerSerial(LOG_TEST, ELOG_LEVEL_ERROR, "OUTPUT", Serial2);
}

void loop()
{
    timer.start(TIMER_TOTAL);
    loggedNo();
    loggedYes();
    loggedYesBufferFull();
    timer.show(TIMER_TOTAL, LOG_TIMER, ELOG_LEVEL_ALERT, "Total time all 3 tasks");
}

void loggedYesBufferFull()
{
    Logger.log(LOG_TIMER, ELOG_LEVEL_INFO, "Timer starting. Logging 1000 times with ERROR level. Should be logged. But exceeding buffer size. Should be slower");
    timer.start(TIMER1);
    for (int i = 0; i < 1000; i++) {
        Logger.log(LOG_TEST, ELOG_LEVEL_ERROR, "Lap %d", i); // Should  be logged to Serial2
    }
    timer.show(TIMER1, LOG_TIMER, ELOG_LEVEL_NOTICE, "LoggedYesBufferFull");
}

void loggedYes()
{
    Logger.log(LOG_TIMER, ELOG_LEVEL_INFO, "Timer starting. Logging 10 times with ERROR level. Should be logged.");
    timer.start(TIMER1);
    for (int i = 0; i < 10; i++) {
        Logger.log(LOG_TEST, ELOG_LEVEL_ERROR, "Lap %d", i); // Should  be logged to Serial2
        timer.lap(TIMER1);
    }
    timer.show(TIMER1, LOG_TIMER, ELOG_LEVEL_NOTICE, "LoggedYes");
    delay(5000);
}

void loggedNo()
{
    Logger.log(LOG_TIMER, ELOG_LEVEL_INFO, "Timer starting. Logging 1000 times with DEBUG level. Should not be logged.");
    timer.start(TIMER1);
    for (int i = 0; i < 1000; i++) {
        Logger.log(LOG_TEST, ELOG_LEVEL_DEBUG, "Lap %d", i); // Should not be logged, is under ERROR level
    }
    timer.show(TIMER1, LOG_TIMER, ELOG_LEVEL_NOTICE, "LoggedNo");
    delay(5000);
    vTaskDelay(5000);
}
