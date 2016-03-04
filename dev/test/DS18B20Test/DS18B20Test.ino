#include <OTRadioLink.h>
#include <Wire.h>

OTV0P2BASE::HumiditySensorSHT21 RelHumidity;
OTV0P2BASE::RoomTemperatureC16_SHT21 TemperatureC16; // SHT21 impl.

OTV0P2BASE::MinimalOneWire<> MinOW_DEFAULT;

OTV0P2BASE::TemperatureC16_DS18B20 extDS18B20s[] =
{
    OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, 0, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION),
    OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, 1, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION),
    OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, 2, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION),
    OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, 3, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION),
    OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, 4, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION),
    OTV0P2BASE::TemperatureC16_DS18B20(MinOW_DEFAULT, 5, OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION),
};
int numberDS18B20 = 0;

void setup()
{
    Serial.begin(4800);
    OTV0P2BASE::serialPrintlnAndFlush("Temp/Humidity test");

    numberDS18B20 = OTV0P2BASE::TemperatureC16_DS18B20::numberSensors(MinOW_DEFAULT);
    OTV0P2BASE::serialPrintAndFlush("number DS18B20: ");
    OTV0P2BASE::serialPrintAndFlush(numberDS18B20, DEC);
    OTV0P2BASE::serialPrintlnAndFlush();


    uint8_t address[8];
    MinOW_DEFAULT.reset_search();
    while (MinOW_DEFAULT.search(address))
    {
        OTV0P2BASE::serialPrintAndFlush("addr: ");
        for (int i = 0; i < 8; ++i)
        {
            OTV0P2BASE::serialPrintAndFlush(' ');
            OTV0P2BASE::serialPrintAndFlush(address[i], HEX);
        }
        OTV0P2BASE::serialPrintlnAndFlush();
    }
    MinOW_DEFAULT.reset_search();
}

void loop()
{
    int temp = TemperatureC16.read();
    OTV0P2BASE::serialPrintAndFlush("SHT21 temp: ");
    OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
    OTV0P2BASE::serialPrintAndFlush('.');
    OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
    OTV0P2BASE::serialPrintAndFlush('C');
    OTV0P2BASE::serialPrintlnAndFlush();

    if (RelHumidity.isAvailable())
    {
        int humid = TemperatureC16.read();
        OTV0P2BASE::serialPrintAndFlush("humidity: ");
        OTV0P2BASE::serialPrintAndFlush(humid, DEC);
        OTV0P2BASE::serialPrintlnAndFlush();
    }

    numberDS18B20 = OTV0P2BASE::TemperatureC16_DS18B20::numberSensors(MinOW_DEFAULT);
    OTV0P2BASE::serialPrintAndFlush("number DS18B20: ");
    OTV0P2BASE::serialPrintAndFlush(numberDS18B20, DEC);
    OTV0P2BASE::serialPrintlnAndFlush();

    for (int i = 0; i < numberDS18B20; i++)
    {
        temp = extDS18B20s[i].read();
        OTV0P2BASE::serialPrintAndFlush("DS18B20[");
        OTV0P2BASE::serialPrintAndFlush(i);
        OTV0P2BASE::serialPrintAndFlush("] addr: ");
        uint8_t address[8];
        extDS18B20s[i].getAddress(address);
        for (int i = 0; i < 8; ++i)
        {
            OTV0P2BASE::serialPrintAndFlush(' ');
            OTV0P2BASE::serialPrintAndFlush(address[i], HEX);
        }
        OTV0P2BASE::serialPrintAndFlush(" temp: ");
        OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
        OTV0P2BASE::serialPrintAndFlush('.');
        OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
        OTV0P2BASE::serialPrintAndFlush('C');
        OTV0P2BASE::serialPrintlnAndFlush();
    }

}
