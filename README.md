# Elog Library

The ElogPlus library is a powerful library for logging and monitoring events in your arduino ESP32 applications.

**Key features of the ElogPlus library include:**

- Output to one or more devices: spiffs, sd card, syslog or serial.
- Each message can be given a log level (VERBOSE, TRACE, DEBUG, INFO, NOTICE, WARNING, ERROR, CRITICAL, ALERT, EMERGENCY, ALWAYS).
- Super fast logging
  1.  Logging is fast because messages are queued in your app, and then handled later in background by the logging library
  1.  If a message is not outputted it takes around 0.5 μs
  1.  If a message is outputted it typically takes around 25 μs.
- Automatic delete oldest logfiles on SD or SPIFFS if it runs out of space.
- Stamping each log message with real time (from eg. NTP)
- Internal file browser/viewer for looking at logs even when your code is running.
- A timer to time your code down to the microseconds.

## Simple usage example

First of all you need to decide where your logs should go. Most users will start with serial, but you have the options of spiffs(internal EEPROM of the esp32), SD-card and Syslog.
Then you decide a log Id for that logging. If you use this Id many places in your code it's a good idea to define it somewhere globally:

```
#include <Elog.h>
#define MYLOG 0
```

The you register your log Id in order to start logging to it. This will often be done in setup():

```
void setup()
{
    Serial.begin(115200);
    Logger.registerSerial(MYLOG, LOG_LEVEL_DEBUG, "tst"); // We want messages with DEBUG level and lower
}
```

Then inside your code you can send messages to the serial port.

```
void loop()
{
    for (int i = 0; i < 1000; i++) {
        Logger.log(MYLOG, LOG_LEVEL_DEBUG, "Counter is %d", i);
        if (i % 10 == 0) {
            Logger.log(MYLOG, LOG_LEVEL_NOTICE, "Counter divisible by 10");
        }
        delay(500);
    }
}
```

Output:

```
000:00:00:00:047 [TST] [DEBUG] Counter is 0
000:00:00:00:047 [TST] [NOTIC] Counter divisible by 10
000:00:00:00:547 [TST] [DEBUG] Counter is 1
000:00:00:01:047 [TST] [DEBUG] Counter is 2
000:00:00:01:547 [TST] [DEBUG] Counter is 3
000:00:00:02:047 [TST] [DEBUG] Counter is 4
```

## Loglevels

The log levels in the ElogPlus library are as follows:

- 0: ALWAYS: Always logged, regardless of settings. For tombstone-type information.
- 1: EMERGENCY: Used to indicate critical errors that require immediate attention. These errors may lead to the termination of the application.
- 2: ALERT: Used to indicate critical errors that require immediate attention. These errors may lead to the termination of the application.
- 3: CRITICAL: Used to indicate critical errors that require immediate attention. These errors may lead to the termination of the application.
- 4: ERROR: Used to report errors that occurred during the execution of the application. These errors may impact the functionality of the program.
- 5: WARNING: Used to indicate potential issues or situations that could lead to errors or unexpected behavior.
- 6: NOTICE: Used to highlight noteworthy events or conditions that may require attention.
- 7: INFO: Used to provide general information about the application's execution. Helpful for tracking the flow of the program.
- 8: DEBUG: Used for detailed debugging information. Typically used during development and not in production.
- 9: TRACE: Used for more detailed debugging information. Typically used during development and not in production.
- 10: VERBOSE: Used for even more detailed debugging information. Typically used during development and not in production.

The lower the level, the more serious the log line is.
When registering a log id you always tell what loglevel you want. Everything at that level and lower will be logged. So if you register a device with
loglevel ERROR, you will only get messages logged at level ERROR, CRITICAL, ALERT, EMERGENCY and ALWAYS

# Output devices

#### Serial logging

This is the most simple form of using the logger. You register a handle and send a log message:

```
#define MYLOG 0
Logger.registerSerial(MYLOG, LOG_LEVEL_DEBUG, "tst");
Logger.log(MYLOG, LOG_LEVEL_INFO, "Here is a message");
```

The service name is "tst". It will always be upper cased and truncated to 3 chars when outputted.

In this case all logleves equal to or lower than DEBUG is sent to serial. This will be the output will look like this:

```
000:00:00:00:047 [TST] [INFO ] Here is a message
```

#### SD card logging

Logging to a SD-card can be done by configuring SPI for the cardreader and register a loghandle for logging:

```
#define MYLOG 0

#include <SPI.h>
SPIClass spi = SPIClass(SPI);
spi.begin(18, 19, 23, 5); // Set your pins of your card reader here. SCK=19, MISO=19, MOSI=23, SS=5
Logger.configureSd(spi, 5, 2000000); // SS pin 5, bus speed = 2Mhz
Logger.registerSd(INFO, LOG_LEVEL_DEBUG, "mylog");

Logger.log(MYLOG, LOG_LEVEL_INFO, "Here is a message");
```

Every time the ESP is booted a new directory on the SD-card is created. The folder name is in format 0001 and is increased on every boot. In this example the first logfile is called mylog.001. When the file reaches the size of 100Kb a new file named mylog.002 will be created. If you want another max size of the logfile use:

```
Logger.registerSd(MYLOG, LOG_LEVEL_INFO, "mylog", FLAG_NONE, 20000);
```

When registing the SD card file you decide the loglevel that should go to the file system. In this case it is loglevel equal or lower than INFO

You could pop out the SD card for reading the files. This logger is pretty resistent to ejecting the card while logging. Sometimes you might experience a crash due to the sdfat library.

You could also use the "Query command prompt". Read more in this help.

#### SPIFFS (Internal EEPROM) logging

Logging to the internal EEPROM of the ESP32 is as simple as register a loghandle

```
#define MYLOG 0
Logger.registerSpiffs(MYLOG, LOG_LEVEL_DEBUG, "mylog");
Logger.log(MYLOG, LOG_LEVEL_INFO, "Here is a message");
```

Every time the ESP is booted a new directory is created. The folder name is in format 0001 and is increased on every boot. In this example the first logfile is called mylog.001. When the file reaches the size of 100Kb a new file named mylog.002 will be created. If you want another max size of the logfile use:

```
Logger.registerSpiffs(MYLOG, LOG_LEVEL_DEBUG, "mylog", FLAG_NONE, 20000);
```

When registing the SPIFFS you decide the loglevel that should go to the file system. In this case it is loglevel equal or lower than DEBUG

The only realistic way of accessing the logfiles is using the "Query command prompt". Read more in this help.

#### Syslog logging

Logging to the an external syslog server is as simple as configuring the syslog host, port and then register a loghandle:

```
#define MYLOG 0
Logger.configureSyslog("192.168.1.20", 514, "esp32"); // syslog host, port and name of the esp32 device host name
Logger.registerSyslog(MYLOG, LOG_LEVEL_NOTICE, FAC_LOCAL4, "mylog"); // Log facility and app name that is sent to syslog server
Logger.log(MYLOG, ELOG_LEVEL_ERROR, "Here is an error message, error code: %d", 17);
```

When registring the SYSLOG handle you decide the loglevel that should go to the syslog server. In this case it is loglevel equal to or lower than NOTICE

Facilities allowed: FAC_KERN, FAC_USER, FAC_MAIL, FAC_DAEMON, FAC_AUTH, FAC_SYSLOG, FAC_LPR, FAC_NEWS, FAC_UUCP, FAC_CRON, FAC_AUTHPRIV, FAC_FTP, FAC_NTP, FAC_LOG_AUDIT, FAC_LOG_ALERT, FAC_CLOCK_DAEMON, FAC_LOCAL0, FAC_LOCAL1, FAC_LOCAL2, FAC_LOCAL3, FAC_LOCAL4, FAC_LOCAL5, FAC_LOCAL6, FAC_LOCAL7

#### Multiple output devices

You can send messages to several output at the same time and filter them differently. Example:

```
#define MYLOG 0
#define OTHERLOG 1

Logger.registerSpiffs(MYLOG, ELOG_LEVEL_ERROR, "mylog");
Logger.registerSerial(MYLOG, LOG_LEVEL_DEBUG, "tst");
Logger.registerSerial(OTHERLOG, LOG_LEVEL_DEBUG, "xxx");

Logger.log(MYLOG, LOG_LEVEL_INFO, "Here is a message that goes to serial with service name [TST], but not spiffs");
Logger.log(OTHERLOG, LOG_LEVEL_INFO, "Here is a message that goes to serial with service name [XXX]");
```

## Formatting logfiles

All registrations can configure how the text in the logfile should appear. You can use these log options:

- FLAG_NONE
- FLAG_NO_TIME (No time)
- FLAG_NO_SERVICE (No service. eg [XXX])
- FLAG_NO_LEVEL (No loglevel. eg [INFO])
- FLAG_TIME_SIMPLE (Format: "000000001" milliseconds since boot)
- FLAG_TIME_SHORT (Format: "HH:MM:SS")
- FLAG_TIME_LONG (Format: YYYY-MM-DD HH:MM:SS.mmm (if real time is provided) or ddd:HH:MM:SS.mmm)
- FLAG_SERVICE_LONG (Format: [XXXXXX] instead of [XXX])

Options can be applied to all device registrations except syslog. Examples:

```
Logger.registerSerial(MYLOG, LOG_LEVEL_DEBUG, "mylog", Serial2, FLAG_NO_TIME | FLAG_NO_SERVICE);
Logger.registerSpiffs(MYLOG, ELOG_LEVEL_ERROR, "mylog", FLAG_TIME_SIMPLE);
Logger.registerSd(INFO, LOG_LEVEL_DEBUG, "mylog", FLAG_NONE);
```

## Using real time clock (RTC)

If the RTC clock of the ESP is set, then the logging library automatically starts stamping all lines in the logfiles with real time. If you get network connection and use NTP, all this will happen automatically.

Files on SPIFFS and SD card will also be timestamped. Also output of Serial will get real time stamps.

You can also set the RTC time on the ESP with this function if you get time from somewhere else than NTP (GPS etc...)

```
Logger.provideTime(2024, 6, 15, 10, 12, 51); // (June 15 2024 time 10:12:51)
```

## Macros to make life easier

To make your code shorter the log library has some macros. The following to lines does exactly the same:

```
Logger.log(MYLOG, LOG_LEVEL_INFO, "Here is an info message %d", 1);
info(MYLOG,"Here is an info message %d", 1)
```

Available macros are:

- debug(logId, message, ...)
- info(logId, message, ...)
- notice(logId, message, ...)
- warning(logId, message, ...)
- error(logId, message, ...)
- critical(logId, message, ...)
- alert(logId, message, ...)
- emergency(logId, message, ...)

You just need to include the macros:

```
#include <ElogMacros.h>
```

## Query command prompt

If you want to see what is logged to either SPIFFS, SD-card or both, you can set a hook in your code like this:

```
Logger.enableQuery(Serial);
```

While you app is running the logger listens on the provided serial port for the user to hit **_space_** (ascii 32). Then a command prompt will be presented where you have access to the file systems. It works like a simple dos prompt with these commands:

- cd (change directory)
- dir (list files and directories)
- type (display content of a file)
- rm (remove a file)
- rmdir (remove a directory recursively)
- format (format the entire filesystem. You will not be warned before it happens)
- peek (listen in on what is currently being logged)

You can change between different log devices with these commands (if you have registred any logging to them):

- spiffs (change to on-chip Eeprom of the ESP32)
- sd (change to SD card)
- syslog (change to syslog device)
- serial (change to serial device)

While in the prompt your code will continue running. You can peek at the currently selected log device.

```
SD:/> peek * info
```

This will show all log messages that is sent to SD card to all files, with a loglevel equal to or lower than level INFO.

If you want to peek a certain file you can use:

```
SPIFFS:/> peek mylog info
```

## Compile options

To save space you can define these compile options:

- LOGGING_SPIFFS_DISABLE
- LOGGING_SYSLOG_DISABLE
- LOGGING_SD_DISABLE

Each one saves around 20-40Kb of eeprom. Disable them if you don´t need them. This will leave more space for your own code.

## Size limits and buffering

#### Buffer size

By default when you register the first logging device a buffer of 50 log lines will be created. Heap memory reserved for this is 16x50 = 800 bytes. Each log line that is buffered with timestamp is reserved from heap and typically takes 40-150 bytes per message.

If you need a bigger buffer than the default 50 message size, you can run (**IMPORTANT:** before registring any devices)

```
Logger.configure(200, true); // Bigger buffer with 200 messages, wait if buffer is full
```

If you have short bursts of messages comming fast you might need a big buffer.
When the buffer is full, your code will be haltet until the buffer has been emptied writing the content out to the registred devices.
If you have a very time sensitive application you can make the logger discard the log messages when the buffer is full like this:

```
Logger.configure(200, false); // Bigger buffer, discard messages when buffer is full.
```

#### Max log handles for each device

By default you can register 10 loghandles per device. If you need more (for big projects) you can configure your device before you register any log Id's:

```
Logger.configureSerial(100);
Logger.configureSpiffs(100);
Logger.configureSd(spi, 5, 4000000, DEDICATED_SPI, 100);
Logger.configureSyslog("192.168.1.20", 514, "esp32", 100);
```

In these examples we extend the max handles to 100 instead of 10.

## Examples

Have a look in the examples folder. Here are some examples of all use cases.
