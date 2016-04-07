#include <OTRadioLink.h>
#include <Wire.h>

OTV0P2BASE::HumiditySensorSHT21 RelHumidity;
OTV0P2BASE::RoomTemperatureC16_SHT21 TemperatureC16; // SHT21 impl.

OTV0P2BASE::MinimalOneWire<> MinOW_DEFAULT;

OTV0P2BASE::TemperatureC16_DS18B20 extDS18B20 = OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION);
int numberDS18B20 = 0;

#define ARRAY_LENGTH(x)  (sizeof((x))/sizeof((x)[0]))

void setup()
{
    Serial.begin(4800);
    OTV0P2BASE::serialPrintlnAndFlush("Temp/Humidity test");

    numberDS18B20 = extDS18B20.getSensorCount();
    OTV0P2BASE::serialPrintAndFlush("number DS18B20: ");
    OTV0P2BASE::serialPrintAndFlush(numberDS18B20, DEC);
    OTV0P2BASE::serialPrintlnAndFlush();

    unsigned long time = millis();
    uint8_t address[8];
    MinOW_DEFAULT.reset_search();
    while (MinOW_DEFAULT.search(address))
    {
      unsigned long delay = millis() - time;
        OTV0P2BASE::serialPrintAndFlush("addr: ");
        for (int i = 0; i < 8; ++i)
        {
            OTV0P2BASE::serialPrintAndFlush(' ');
            OTV0P2BASE::serialPrintAndFlush(address[i], HEX);
        }
        OTV0P2BASE::serialPrintAndFlush(" in ");
        OTV0P2BASE::serialPrintAndFlush(delay, DEC);
        OTV0P2BASE::serialPrintAndFlush("ms");
        OTV0P2BASE::serialPrintlnAndFlush();

        time = millis();
    }
    MinOW_DEFAULT.reset_search();
}

void loop()
{
    unsigned long time = millis();
    int16_t temp = TemperatureC16.read();
    unsigned long delay = millis() - time;
    OTV0P2BASE::serialPrintAndFlush("SHT21 temp: ");
    OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
    OTV0P2BASE::serialPrintAndFlush('.');
    OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
    OTV0P2BASE::serialPrintAndFlush('C');
    OTV0P2BASE::serialPrintAndFlush(" in ");
    OTV0P2BASE::serialPrintAndFlush(delay, DEC);
    OTV0P2BASE::serialPrintAndFlush("ms");
    OTV0P2BASE::serialPrintlnAndFlush();

    if (RelHumidity.isAvailable())
    {
        time = millis();
        int16_t humid = TemperatureC16.read();
        delay = millis() - time;
        OTV0P2BASE::serialPrintAndFlush("humidity: ");
        OTV0P2BASE::serialPrintAndFlush(humid, DEC);
        OTV0P2BASE::serialPrintAndFlush(" in ");
        OTV0P2BASE::serialPrintAndFlush(delay, DEC);
        OTV0P2BASE::serialPrintAndFlush("ms");
        OTV0P2BASE::serialPrintlnAndFlush();
    }

    time = millis();
    temp = extDS18B20.read();
    delay = millis() - time;
    OTV0P2BASE::serialPrintAndFlush("DS18B20 (first): ");
    OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
    OTV0P2BASE::serialPrintAndFlush('.');
    OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
    OTV0P2BASE::serialPrintAndFlush('C');
    OTV0P2BASE::serialPrintAndFlush(" in ");
    OTV0P2BASE::serialPrintAndFlush(delay, DEC);
    OTV0P2BASE::serialPrintAndFlush("ms");
    OTV0P2BASE::serialPrintlnAndFlush();


    int16_t allTemp[10];
    time = millis();
    numberDS18B20 = extDS18B20.readMultiple(allTemp, ARRAY_LENGTH(allTemp));
    delay = millis() - time;

    for (int i = 0; i < numberDS18B20; i++)
    {
        OTV0P2BASE::serialPrintAndFlush("DS18B20[");
        OTV0P2BASE::serialPrintAndFlush(i);
        OTV0P2BASE::serialPrintAndFlush("] temp: ");
        temp = allTemp[i];
        OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
        OTV0P2BASE::serialPrintAndFlush('.');
        OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
        OTV0P2BASE::serialPrintAndFlush('C');
        OTV0P2BASE::serialPrintAndFlush(" in ");
        OTV0P2BASE::serialPrintAndFlush(delay, DEC);
        OTV0P2BASE::serialPrintAndFlush("ms");
        OTV0P2BASE::serialPrintlnAndFlush();
    }

}
