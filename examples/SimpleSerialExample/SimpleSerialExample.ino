#include <Elog.h>
#define MYLOG 0
#define RANDOM 1

void randomLog()
{
    if (random(0, 5) == 0) {
        logger.log(RANDOM, WARNING, "Random message");
    }
}

void setup()
{
    Serial.begin(115200);
    logger.registerSerial(MYLOG, DEBUG, "tst"); // We want messages with DEBUG level and lower
    logger.registerSerial(RANDOM, DEBUG, "rnd"); // We want messages with WARNING level and lower
}

void loop()
{
    for (int i = 0; i < 1000; i++) {
        logger.log(MYLOG, DEBUG, "Counter is %d", i);
        if (i % 10 == 0) {
            logger.log(MYLOG, NOTICE, "Counter divisible by 10");
        }
        randomLog();
        delay(500);
    }
}