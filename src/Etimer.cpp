#include "Etimer.h"

/* Constructor for creating the timer. Parameters:
   name: a human readable name that is shown, when the LoggerTimer.show() is called
   logger: the logging instance that all output of the timer is sent to.
   laps: The amount of laps to reserve memory for. Default is 2 (For a start and a stop)*/
Etimer::Etimer(const char* name, Elog& logger, uint8_t laps)
{
    this->name = name;
    this->logger = &logger;
    running = false;
    currentLap = 0;
    maxLaps = laps;
    lapMicros = new uint32_t[laps + 1]; // Start time takes on place in array
    logger.log(DEBUG, "Timer \"%s\" Created", name);
}

/* Start the timer. */
void Etimer::start()
{
    lapMicros[0] = micros(); // Save this as early as possible for most precise time
    startedMillis = millis();
    running = true;
    currentLap = 1;
}

/* Stop the timer. */
void Etimer::end()
{
    if (running) { // We can only stop a running timer
        lap(); // Stopping always create a lap-time.
        running = false;
    } else {
        logger->log(WARNING, "Timer (%s) has not been started, Cant end it", name);
    }
}

/* Create a new lap-time on the timer. If all the laps are used then they are showed and the timer restarted */
void Etimer::lap()
{
    if (running == false) { // Start the timer, if it´s not running.
        start();
    } else {
        lapMicros[currentLap] = micros(); // Save time as early as possible for precision
        if (currentLap <= maxLaps + 1) {
            currentLap++;
        } else { // All laps full, print and restart the timer.
            logger->log(WARNING, "No more space for lapses. Dumping current lapses and restarts timer");
            running = false;
            show();
            start();
        }
    }
}

/* Shows the timer via logging. The setup for this is done in the constructor, when creating the timer instance.
   It Will print starttime, all intermediate lap-times and the endtime.
   This is a locking call because it will wait for the buffer of the logger. When you decide to show the timer
   you probably want all the messages. */
void Etimer::show()
{
    static char timeStr[25];
    static char sinceStartStr[40];
    static char sinceLastLapStr[40];

    if (running) { // If timer is running, we stop it before showing it.
        end();
    }

    for (uint8_t lap = 0; lap < currentLap; lap++) { // Traverse all the recorded laps
        if (lap == 0) {
            getTimeString(startedMillis, timeStr);
            logger->log(INFO, "Timer (%s) started at: %s", name, timeStr);
        } else {
            uint32_t sinceLastLap = lapMicros[lap] - lapMicros[lap - 1];
            getTimeStringMicros(sinceLastLap, sinceLastLapStr);
            uint32_t sinceStart = lapMicros[lap] - lapMicros[0];
            getTimeStringMicros(sinceStart, sinceStartStr);

            if (lap == currentLap - 1) {
                if (lap == 1) {
                    logger->log(INFO, "Timer (%s) ended, since start (%s)", name, sinceLastLapStr, sinceStartStr);
                } else {
                    logger->log(INFO, "Timer (%s) ended, laptime (%s), since start (%s)", name, sinceLastLapStr, sinceStartStr);
                }
            } else {
                logger->log(INFO, "Timer (%s), lap#%d, laptime (%s), since start: (%s)", name, lap, sinceLastLapStr, sinceStartStr);
            }
        }
    }
}

/* Provided microseconds it returns it formattet like one of these:
   "16 μs"
   "5 ms, 16 μs"
   "28 sek, 5 ms, 16 μs"
   "25 min, 28 sek, 5 ms, 16 μs)"
   Depending on the lapsed time, it's presented as short as possible */
uint8_t Etimer::getTimeStringMicros(uint32_t microSeconds, char* output)
{
    uint32_t minutes = microSeconds / 60000000;
    uint32_t seconds = (microSeconds / 1000000) % 60;
    uint32_t milliseconds = (microSeconds / 1000) % 1000;
    uint32_t remainingMicroseconds = microSeconds % 1000;

    if (minutes > 0) {
        if (seconds > 0 || milliseconds > 0 || remainingMicroseconds > 0) {
            sprintf(output, "%d min, %d sec, %d ms, %d μs", minutes, seconds, milliseconds, remainingMicroseconds);
        } else {
            sprintf(output, "%d min", minutes);
        }
    } else if (seconds > 0) {
        if (milliseconds > 0 || remainingMicroseconds > 0) {
            sprintf(output, "%d sec, %d ms, %d μs", seconds, milliseconds, remainingMicroseconds);
        } else {
            sprintf(output, "%d s", seconds);
        }
    } else if (milliseconds > 0) {
        sprintf(output, "%d ms, %d μs", milliseconds, remainingMicroseconds);
    } else {
        sprintf(output, "%d μs", remainingMicroseconds);
    }
    return strlen(output);
}