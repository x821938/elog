/**
 * @file logTimer.cpp
 * @brief Implementation file for the LogTimer class.
 *
 * This file contains the implementation of the LogTimer class, which provides functionality for timing operations
 * and logging the elapsed time. It includes methods for configuring the timer, starting and ending timers,
 * recording laps, displaying timer information, and formatting time strings.
 */

#ifdef ELOG_TIMER_ENABLE

#include "LogTimer.h"
#include "Elog.h"

/**
 * @brief Gets the instance of the LogTimer class.
 *
 * This method returns the instance of the LogTimer class. It ensures that only one instance of the class is created.
 *
 * @return The instance of the LogTimer class.
 */
LogTimer& LogTimer::getInstance()
{
    static LogTimer instance;
    return instance;
}

/**
 * @brief Configures the LogTimer.
 *
 * This method configures the LogTimer with the specified maximum number of timers and maximum number of laps per timer.
 * If this method is not called, it will be called internally with default values. Default values are 3 timers and 10 laps per timer.
 *
 * @param maxTimers The maximum number of timers that can be used.
 * @param maxLaps The maximum number of laps that can be stored for each timer.
 */
void LogTimer::configure(const uint8_t maxTimers, const uint8_t maxLaps)
{
    if (configured) {
        logger.logInternal(ELOG_LEVEL_ERROR, "LogTimer already configured");
        return;
    }

    timerSettings = new TimerSetting[maxTimers];
    for (uint8_t i = 0; i < maxTimers; i++) {
        timerSettings[i].lapMicros = new uint32_t[maxLaps];
        timerSettings[i].currentLap = 0;
        timerSettings[i].maxLaps = maxLaps;
        timerSettings[i].running = false;
    }
    configured = true;
    this->maxTimers = maxTimers;
}

/**
 * @brief Starts a timer.
 *
 * This method starts the timer with the specified timer ID. It records the start time and sets the timer as running.
 * This method is super fast and takes less than a few microseconds to execute.
 *
 * @param timerId The ID of the timer to start. Must be less than the maximum number of timers.
 */
void LogTimer::start(const uint8_t timerId)
{
    if (!configured) {
        configure();
    }

    if (timerId >= maxTimers) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Timer id %d out of range", timerId);
        return;
    }

    TimerSetting& timer = timerSettings[timerId];
    timer.lapStartedMicros = micros();
    timer.currentLap = 0;
    timer.running = true;
    timer.timerStartedMicros = timer.lapStartedMicros;
}

/**
 * @brief Ends a timer.
 *
 * This method ends the timer with the specified timer ID. It records a lap and sets the timer as not running.
 *
 * @param timerId The ID of the timer to end. Must be less than the maximum number of timers.
 */
void LogTimer::end(const uint8_t timerId)
{
    lap(timerId);
    timerSettings[timerId].running = false;
}

/**
 * @brief Records a lap for a timer.
 *
 * This method records a lap for the timer with the specified timer ID.
 *
 * @param timerId The ID of the timer to lap. Must be less than the maximum number of timers.
 */
void LogTimer::lap(const uint8_t timerId)
{
    if (!configured) {
        configure();
    }

    if (timerId >= maxTimers) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Timer id %d out of range", timerId);
        return;
    }

    TimerSetting& timer = timerSettings[timerId];

    if (!timer.running) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Timer %d not started", timerId);
        return;
    }

    uint32_t currentTime = micros();
    timer.lapMicros[timer.currentLap] = currentTime - timer.lapStartedMicros;
    timer.lapStartedMicros = currentTime;

    timer.currentLap++;
    if (timer.currentLap >= timer.maxLaps) {
        logger.logInternal(ELOG_LEVEL_WARNING, "Timer %d has reached max laps. Wrapping around", timerId);
        timer.currentLap = 0;
    }
}

/**
 * @brief Displays the timer information and logs it.
 *
 * This method displays the timer information, including lap times and total time elapsed, and logs it using the
 * specified log ID and log level. The timer must be configured and running for this method to work properly.
 *
 * @param timerId The ID of the timer.
 * @param logId The ID of the log to use for logging.
 * @param logLevel The log level to use for logging.
 * @param message The message to include in the log.
 */
void LogTimer::show(const uint8_t timerId, const uint8_t logId, const uint8_t logLevel, const char* message)
{
    if (!configured) {
        configure();
    }
    if (timerId >= maxTimers) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Timer id %d out of range", timerId);
        return;
    }

    char timeString[50];

    TimerSetting& timer = timerSettings[timerId];
    uint32_t currentTime = micros();

    if (!timer.running) {
        logger.logInternal(ELOG_LEVEL_ERROR, "Timer %d not running", timerId);
        return;
    }

    if (timer.currentLap == 0) {
        lap(timerId);
    }

    if (timer.currentLap == 1) {
        getTimeStringMicros(timer.lapMicros[0], timeString);
        logger.log(logId, logLevel, "%s / Time elapsed: %s", message, timeString);
        return;
    }

    for (uint8_t lap = 0; lap < timer.currentLap; lap++) {
        getTimeStringMicros(timer.lapMicros[lap], timeString);
        logger.log(logId, logLevel, "%s / Lap %d: %s", message, lap, timeString);
    }

    if (timer.currentLap > 1) {
        getTimeStringMicros(currentTime - timer.timerStartedMicros, timeString);
        logger.log(logId, logLevel, "%s / Total time elapsed: %s", message, timeString);
    }
}

/**
 * @brief Formats the given microseconds into a time string.
 *
 * This method formats the given microseconds into a time string. The time string is formatted as follows:
 * - If the time is less than 1 second, it is presented in microseconds.
 * - If the time is less than 1 minute, it is presented in seconds and milliseconds.
 * - If the time is less than 1 hour, it is presented in minutes, seconds, and milliseconds.
 * - If the time is 1 hour or more, it is presented in hours, minutes, seconds, and milliseconds.
 *
 * @param microSeconds The number of microseconds.
 * @param output The output buffer to store the formatted time string.
 */
void LogTimer::getTimeStringMicros(const uint32_t microSeconds, char* output)
{
    uint32_t minutes = microSeconds / 60000000;
    uint32_t seconds = (microSeconds / 1000000) % 60;
    uint32_t milliseconds = (microSeconds / 1000) % 1000;
    uint32_t remainingMicroseconds = microSeconds % 1000;

    int index = 0;

    if (minutes > 0) {
        index += sprintf(output + index, "%d min, ", minutes);
    }
    if (seconds > 0) {
        index += sprintf(output + index, "%d sec, ", seconds);
    }
    if (milliseconds > 0) {
        index += sprintf(output + index, "%d ms, ", milliseconds);
    }
    index += sprintf(output + index, "%d μs", remainingMicroseconds);

    // Remove trailing comma and space
    if (index >= 2 && output[index - 2] == ',') {
        output[index - 2] = '\0';
    }
}

// This is the only instance of the logtimer
// It is available to all files that include LogTimer.h
LogTimer& timer = LogTimer::getInstance();

#endif // ELOG_TIMER_ENABLE
