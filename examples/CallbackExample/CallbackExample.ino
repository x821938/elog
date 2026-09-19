// ============================================================
// PlatformIO: Add this to your platformio.ini:
//   build_flags = -D ELOG_SPIFFS_ENABLE
//
// Arduino IDE: Open the file:
//   <Arduino libraries folder>/Elog/src/ElogConfig.h
//   and uncomment the line:  #define ELOG_CALLBACK_ENABLE
// ============================================================

#include <freertos/FreeRTOS.h>
#include <esp_wifi.h>
#include <Arduino.h>
#include "Elog.h"

// Define the log IDs. These are used to identify the different logfiles
#define MAIN 0

#define RED_LED_PIN 21
#define GREEN_LED_PIN 22
#define BLUE_LED_PIN 4

uint32_t mainCounter = 0;
void getRandomString(char* str, int maxLength); // declaration
void ledBlinkerTask(void* pvParameters);
uint32_t ledCallback(LogLineEntry entry);

volatile uint8_t currentLedLevel = LogLevel::ELOG_LEVEL_NOLOG;

void setup()
{
    Serial.begin(115200);

    // If you want to see the internal logs from the elog library, uncomment the line below
    // Logger.configureInternalLogging(Serial, ELOG_LEVEL_DEBUG, 60000);

    // Register some log IDs for logging
    Logger.registerSerial(MAIN, ELOG_LEVEL_DEBUG, "COUNT", Serial);
    Logger.registerCallback(MAIN, ELOG_LEVEL_WARNING, "LED", ledCallback);

    // Simulate the time by providing a fixed time to the RTC (You can also use the NTP time)
    Logger.provideTime(2023, 7, 31, 10, 12, 51);

    // prepare the pins. since I'm using a common anode LED, the outputs need to be HIGH
    pinMode(RED_LED_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN, HIGH);
    pinMode(GREEN_LED_PIN, OUTPUT);
    digitalWrite(GREEN_LED_PIN, HIGH);
    pinMode(BLUE_LED_PIN, OUTPUT);
    digitalWrite(BLUE_LED_PIN, HIGH);

    // test the leds
    digitalWrite(RED_LED_PIN, LOW);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(BLUE_LED_PIN, LOW);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    digitalWrite(BLUE_LED_PIN, HIGH);

    // create led blinker task
    BaseType_t xReturned = xTaskCreate(ledBlinkerTask, "ledBlinkerTask", 4096, (void *)1, 4, NULL);
    if(!xReturned == pdPASS){
        Logger.alert(MAIN, "ledBlinkerTask not created");
    }

    // get true random numbers from the wifi system
    static wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&config) != ESP_OK){
        Logger.error(MAIN, "WiFi not initialised!");
    }
    if(esp_wifi_start() != ESP_OK){
        Logger.error(MAIN, "WiFi not started!");
    }
    Logger.info(MAIN, "Random Number Generation started. %lu", esp_random());

    // This enables the query mode. The query mode allows you to send commands to the device. Press space to enter the command mode
    Logger.enableQuery(Serial);

    Logger.log(MAIN, ELOG_LEVEL_INFO, "Setup completed. Press space to enter the command mode and view the logs");
}

void loop()
{
    char randomString[50];
    for (long subCounter = 0; subCounter < 40; subCounter++) { // Fast burst of data to almost fill the buffer
        getRandomString(randomString, sizeof(randomString));
        uint8_t randomLogLevel = random(0, 11);
        Logger.log(MAIN, randomLogLevel, "Main Counter: %d, Subcounter: %d, random string: %s", mainCounter, subCounter, randomString);
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }
    Logger.log(MAIN, ELOG_LEVEL_INFO, "Main Counter: %d", mainCounter);

    mainCounter++;
    vTaskDelay(20000/portTICK_PERIOD_MS);
}

// Returns a random string with a random length, just to simulate some data
void getRandomString(char* str, int maxLength)
{
    int length = random(1, maxLength);
    for (int strCount = 0; strCount < length; strCount++) {
        str[strCount] = char(random(65, 91));
    }
    str[length] = '\0';
}

void ledBlinkerTask(void* pvParameters){
    static bool isLedOn = true;

    for(;;){
        bool isRedOn = false;
        bool isGreenOn = false;
        bool isBlueOn = false;

        switch(currentLedLevel){
            case ELOG_LEVEL_ALWAYS:
                isRedOn = true;
                isGreenOn = true;
                isBlueOn = true;
                break;
            case ELOG_LEVEL_EMERGENCY:
                isBlueOn = true;
                break;
            case ELOG_LEVEL_ALERT:
                isRedOn = true;
                isBlueOn = true;
                break;
            case ELOG_LEVEL_CRITICAL:
                isRedOn = true;
                break;
            case ELOG_LEVEL_ERROR:
                isGreenOn = true;
                isRedOn = true;
                break;
            case ELOG_LEVEL_WARNING:
                isGreenOn = true;
                break;
        }

        // since I'm using a common anode LED, the outputs need to be inverted
        digitalWrite(RED_LED_PIN, !(isLedOn && isRedOn));
        digitalWrite(GREEN_LED_PIN, !(isLedOn && isGreenOn));
        digitalWrite(BLUE_LED_PIN, !(isLedOn && isBlueOn));

        isLedOn = !isLedOn;
        vTaskDelay(750/portTICK_PERIOD_MS);
    }
}

uint32_t ledCallback(LogLineEntry entry){
    if(entry.logLevel < currentLedLevel){
        currentLedLevel = entry.logLevel;
    }
    return 1;
}
