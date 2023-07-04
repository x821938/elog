
#ifndef Etimer_h
#define Etimer_h

#include "Elog.h"
#include <Arduino.h>

/* LogTimer is used together with Logger. You can use it for timing parts of your programs
   It's very fast and doest use much time itself. To start, laps or stop the timer
   takes less than 5 microseconds. This means you will get almost real timing of your programs. */
class Etimer : private Elog {
private:
    const char* name; // The name of the timer. Is printed when timerShow is called
    Elog* logger; // Where logging should be sent to.
    uint32_t startedMillis; // When the timer is started
    bool running; // true if it is running
    uint8_t currentLap; // Keeping track of what lapnumber we are at
    uint8_t maxLaps; // Number of laps reserved on this timer
    uint32_t* lapMicros; // Pointer to the laps array that holds each lap-time

    uint8_t getTimeStringMicros(uint32_t microSeconds, char* output);

public:
    Etimer(const char* name, Elog& logger, uint8_t laps = 2);

    void start();
    void end();
    void lap();
    void show();
};

#endif