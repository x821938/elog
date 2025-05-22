#ifndef ELOG_LOGTIMER_H
#define ELOG_LOGTIMER_H

#include <Arduino.h>

#ifdef ELOG_TIMER_ENABLE

using namespace std;

/* LogTimer is used together with Logger. You can use it for timing parts of your programs
   It's very fast and doest use much time itself. To start, laps or stop the timer
   takes less than 5 microseconds. This means you will get almost real timing of your programs. */
class LogTimer {
    struct TimerSetting {
        uint32_t timerStartedMicros;
        uint32_t lapStartedMicros;
        uint32_t* lapMicros;
        uint8_t currentLap;
        uint8_t maxLaps;
        bool running;
    };

public:
    // Ensure that the class is a singleton
    LogTimer(const LogTimer&) = delete;
    LogTimer& operator=(const LogTimer&) = delete;
    static LogTimer& getInstance();

    void configure(const uint8_t maxTimers = 3, const uint8_t maxLaps = 10);
    void start(const uint8_t timerId = 0);
    void end(const uint8_t timerId = 0);
    void lap(const uint8_t timerId);
    void show(const uint8_t timerId, const uint8_t logId, const uint8_t logLevel, const char* message);

private:
    TimerSetting* timerSettings;
    void getTimeStringMicros(uint32_t microSeconds, char* output);
    bool configured = false;
    uint8_t maxTimers = 3;

    LogTimer() { } // Private constructor
};

extern LogTimer& timer; // Make an instance available to user when he includes the library

#endif // ELOG_TIMER_ENABLE

#endif // ELOG_LOGTIMER_H
