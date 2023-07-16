# Ultimate ESP logger (Elog)
## Main features:
This library was created to do easy an super efficient logging from your Arduino ESP32 programs. It uses a ringbuffer to handle logging extremely fast. This means that you can leave the log statements in your code with pretty much no performance impact!
- Log to any Serial (0, 1, 2 or soft serial)
- Log to SD-card
- Log to internal SPIFFS flash memory in ESP32
- Multiple log output
- SD-card hotplug. You can just eject it and reinsert it. Logging will continue.
- Very fast logging with no busy waiting!
- Buffered output to both serial or SD-card
- Timing helpers to measure performance of your code

 ### Log output example:
```
000:00:09:41:536 [MAI] [INFO ] : ECEF: X=350075848.43, Y=52893867.26, Z=528754469.13, Accuracy=8667.2
000:00:09:41:538 [MAI] [INFO ] : HP Lat: 56.32148122, Lon: 8.39157915, Height: 78.7636, HeightMSL: 35.2965, vAcc: 7314.9, hAcc: 4649.0
000:00:09:41:547 [UBX] [NOTIC] : Survey-in. Active=0, Valid=0, Duration=0s, Accuracy=0, MeanX=0, MeanY=0, MeanZ=0
000:00:09:41:572 [UBX] [INFO ] : Feeding RTCM frame ID=1005, length=25 to GPS
000:00:09:41:577 [UBX] [INFO ] : Feeding RTCM frame ID=1074, length=129 to GPS
000:00:09:42:467 [LOG] [DEBUG] : Msg buffered = 23482, SD written = 23482, discarded = 0, Max Buffer usage = 31%
000:00:09:42:538 [LOG] [DEBUG] : Syncronizing all logfiles. Writing dirty cache
 ```
## Description

You can log to any serial device or any file on a SD-card. It can handle multiple outputs very easily.
This library supports HOTPLUGGING the SD-card. Filesystem is flushed every 5 seconds, so you will maximum loose 5 sec of data if you eject the card.

A log statement that is filtered out and never sent to a log device takes about 1-2 microseconds! A typical log statement with 40 chars takes about 10 microseconds. All this is possible because of the buffering. If you have time sensitive applications you can still log without affecting the timings much!

You can create different log instances for different places in your code. Each instance can be configured with different output and target log levels.

## Log levels
Logging can be logged at different levels.

- EMERGENCY
- ALERT
- CRITICAL
- ERROR
- WARNING
- NOTICE
- INFO
- DEBUG

 The lowest level is EMERGENCY and the highest is DEBUG.
 Every time you log something you give it a loglevel.

 When you create your log instance and set your outputs you also set the target log level. Everything logged with lower or equal level than the target level will be outputted.

 ## Basic usage
 
 If you use Arduino IDE you need to install the dependency "SdFat - Adafruit fork" for this to work. If you use PlatformIO the dependency will automatically be installed.

 First you need to create an instance of your logger:
 ```
 #include <ELog.h>
Elog mylog;
 ```
Then you need to specify where the output of the logging should go and what our target loglevel is. We also give it a servicename. That is a label that is attached to each log message.
 ```
mylog.addSerialLogging(Serial, "Main", DEBUG);
 ```
Now you can do some logging in your application:
```
myLog.log(INFO, "Hello! Variable x is %d", x); 
```
Here your can use the normal printf format for your messages. (eg: %d %s %f)

## SD card logging
If you want to log to SD-card you need a litle more bootstrapping.
```
SPIClass spi = SPIClass(VSPI); // Prepare the spi bus for talking to card reader
Elog myLog; // Create a log instance

void setup()
{
    spi.begin(18, 19, 17, 5); // Set your pins of your card reader here.
    Elog::configureSd(spi, 5, 2000000); // Speed set to 2mhz. Should be ok with dupont wires.
    myLog.addSdLogging("File1", DEBUG);

    myLog.log(INFO, "Hello! Variable x is %d", x); 
}
```
Each time the ESP is restarted a new folder is created in the root of the SD-card. It will be in format LOG00001 and will be incremented at each restart. All files created by all your log instances will be placed under this folder. The filenames are defined when you run addFileLogging().

## SPIFFS logging
You can log to the internal flash memory of the ESP32! The main problem is getting the logs out of your board. To do this i included a serial command line interface (CLI), where you can list all log files and dump them to serial.

Again you need a little bootstrapping to get SPIFFS logging going:
```
Elog myLog; // Create a log instance
void setup()
{
    Elog::configureSpiffs();
    myLog.addSpiffsLogging("svc", DEBUG);
    myLog.log(INFO, "Hello! Variable x is %d", x); 
}
```
Checkout the example that shows usage of SPIFFS logging. When you want to see the logs call:
```
myLog.spiffsQuery(Serial);
```
You have to include this somehow in your code. It could be triggered by a digital input, or you could listen on the serial port, like in the provided example.

In the CLI you have these commands available:
- "L" can list the logs on the SPIFFS filesystem,
- "P" can print a logfile. Provide your log number like: "P 123", 
- "F" Format the filesystem with "F"
- "Q" Quit and return to your code again.

If printing very long logs you can pause it by pressing SPACE. You can also abort by pressing Q.

## Real human times in logs:
If you want some real time stamps on your log entries you can provide the time for the library. Your output could look like this:
```
2023-07-15 08:25:09 577 [MAI] [INFO ] : ECEF: X=350075848.43, Y=52893867.26, Z=528754469.13, Accuracy=8667.2
2023-07-15 08:25:10 584 [MAI] [INFO ] : HP Lat: 56.32148122, Lon: 8.39157915, Height: 78.7636, HeightMSL: 35.2965
2023-07-15 08:25:12 598 [UBX] [NOTIC] : Survey-in. Active=0, Valid=0, Duration=0s, Accuracy=0, MeanX=0, MeanY=0, MeanZ=0
2023-07-15 08:25:14 612 [UBX] [INFO ] : Feeding RTCM frame ID=1005, length=25 to GPS
2023-07-15 08:25:15 619 [UBX] [INFO ] : Feeding RTCM frame ID=1074, length=129 to GPS
2023-07-15 08:25:17 633 [LOG] [DEBUG] : Msg buffered = 23482, SD written = 23482, discarded = 0, Max Buffer usage = 31%
2023-07-15 08:25:17 843 [LOG] [DEBUG] : Syncronizing all logfiles. Writing dirty cache
 ```
There are so many ways of getting real time. RTC clock, GPS, NTP etc. The only thing you need to do is to give the time to Elog:
 ```
Elog::provideTime(2023, 7, 15, 8, 12, 34);  // We make up the time: 15th of july 2023 at 08:12:34
 ```
You can regularly provide the time. The esp is a few seconds wrong each day, so you can correct it when needed. 
**If you provide time your files on SD-card and the SPIFFS will be correctly stamped!** This makes it much easier to find the correct logfile.

## Configuration
If you dont like the default settings you can run this static method in the beginning of the code. It must be called before adding serial or file logging to your instance.
```
Elog::globalSettings(100, 150, Serial, DEBUG);
```
The parameters are in this order:

- **maxLogMessageSize** = Maximum characters of the logmesage (default 250)
- **maxLogMessages** = Size of the buffer. How many messages to hold. (default 20)
- **internalLogDevice** = When this library output internal messages, where should it go (default Serial)
- **internalLogLevel** = Internal messages from this library is only shown equal to or below this level (default WARNING)
- **discardMsgWhenBufferFull** = If true all messages will be discarded if the buffer is full. If your application is time sensitive you might want to do this (default false)
- **reportStatusEvery** = Regularly an internal buffer status is show every (default 5000 ms)

Remember memoryusage is maxLogMessageSize x maxLogMessages! Dont reserve more than needed

# Timer helpers (Etimer)
Somtimes you need to check how fast your code is. This library comes with a timer class for doing this.
Again the focus is on speed. Starting, lapsing or stopping the timer takes less than 1 microsecond! 
Here is a small example to get you started. Look in the examples folder for more information.

```
#include <ELog.h>
#include <Etimer.h>

Elog elog;
Etimer timer("Timer1", elog, 20); // 20 lapses max on this timer

void setup()
{
    Serial.begin(115200);
    elog.addSerialLogging(Serial, "Main", INFO);

    timer.start();
    for (uint8_t lap = 1; lap <= 20; lap++) {
        timer.lap();
        elog.log(INFO, "Lap number = %d", lap);
        delay(15);
    }
    timer.show(); 
}
```