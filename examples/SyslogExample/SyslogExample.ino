#include "secrets.h" // Create a secrets.h file with your WiFi credentials (define WIFI_SSID and WIFI_PASS)
#include <Elog.h>

#define COUNTER 0 // The log ID

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // 1 hour ahead of UTC
const int daylightOffset_sec = 3600; // Daylight saving time is 1 hour

void connect_wifi()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        logger.log(COUNTER, ELOG_LEVEL_INFO, "Connecting to WiFi..");
    }
    logger.log(COUNTER, ELOG_LEVEL_INFO, "Connected to the WiFi network");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void setup()
{
    Serial.begin(115200);
    logger.configureSyslog("192.168.50.40", 514, "esp32"); // Syslog server IP, port and device name

    logger.registerSerial(COUNTER, ELOG_LEVEL_DEBUG, "COUNT", Serial); // Log both to serial...
    logger.registerSyslog(COUNTER, ELOG_LEVEL_DEBUG, FAC_USER, "counter"); // ...and syslog. Set the facility to user

    connect_wifi();
}

void loop()
{
    static uint32_t mainCounter = 0;

    uint8_t logLevel = random(0, 8);
    logger.log(COUNTER, logLevel, "Main loop counter: %d", mainCounter++);
    delay(10000);
}
