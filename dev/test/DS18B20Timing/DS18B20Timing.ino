#include <Wire.h>
#include "OTRadioLink.h"

#define DS18B20_MODEL_ID 0x28

 // OneWire commands
#define CMD_START_CONVO       0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define CMD_COPY_SCRATCH      0x48  // Copy EEPROM
#define CMD_READ_SCRATCH      0xBE  // Read EEPROM
#define CMD_WRITE_SCRATCH     0x4E  // Write to EEPROM
#define CMD_RECALL_SCRATCH    0xB8  // Reload from last known
#define CMD_READ_POWER_SUPPLY 0xB4  // Determine if device needs parasite power
#define CMD_ALARM_SEARCH      0xEC  // Query bus for devices with an alarm condition

 // Scratchpad locations
#define LOC_TEMP_LSB          0
#define LOC_TEMP_MSB          1
#define LOC_HIGH_ALARM_TEMP   2
#define LOC_LOW_ALARM_TEMP    3
#define LOC_CONFIGURATION     4
#define LOC_INTERNAL_BYTE     5
#define LOC_COUNT_REMAIN      6
#define LOC_COUNT_PER_C       7
#define LOC_SCRATCHPAD_CRC    8

OTV0P2BASE::MinimalOneWire<> minOW;

static const uint8_t setPrecision(const uint8_t address[8], const uint8_t precision)
{
  uint8_t write = ((precision - 9) << 5) | 0x1f;
  OTV0P2BASE::serialPrintAndFlush(">");
  OTV0P2BASE::serialPrintAndFlush(write, BIN);
  
  // Write scratchpad/config.

  minOW.reset();
  minOW.select(address);
  minOW.write(CMD_WRITE_SCRATCH);
  minOW.write(0); // Th: not used.
  minOW.write(0); // Tl: not used.
  minOW.write(write); // Config register; lsbs all 1.

  minOW.reset();
  minOW.select(address);
  minOW.write(CMD_READ_SCRATCH);

  uint8_t d = 0;
  for (uint8_t i = 0; i <= LOC_CONFIGURATION; i++) {
    d = minOW.read();
  }
  OTV0P2BASE::serialPrintAndFlush("<");
  OTV0P2BASE::serialPrintAndFlush(d, BIN);
  OTV0P2BASE::serialPrintlnAndFlush();
}

static int16_t readTemp(const uint8_t address[8])
{
  minOW.reset();
  minOW.select(address);
  while (minOW.read_bit() == 0) {
  }

  minOW.reset();
  minOW.select(address);
  minOW.write(CMD_READ_SCRATCH);

  // Read first two bytes of 9 available.  (No CRC config or check.)
  const uint8_t d0 = minOW.read();
  const uint8_t d1 = minOW.read();

  // Extract raw temperature, masking any undefined lsbit.
  // TODO: mask out undefined LSBs if precision not maximum.
  return (d1 << 8) | (d0);
}

uint8_t firstAddress[8];

void setup()
{
  uint8_t count = 0;
  uint8_t address[8];

  Serial.begin(4800);

  // Ensure no bad search state.
  minOW.reset_search();

  unsigned long time = millis();
  while (minOW.search(address))
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

    if(DS18B20_MODEL_ID == address[0]) 
    {
      if (0 == count) {
        memcpy(firstAddress, address, 8);
      }
      count++;
    }

    time = millis();
  }
  minOW.reset_search();
}

uint8_t precision = OTV0P2BASE::TemperatureC16_DS18B20::MIN_PRECISION;

void loop()
{
  uint8_t address[8];

  // Set the prescision
  OTV0P2BASE::serialPrintAndFlush("Setting precision to ");
  OTV0P2BASE::serialPrintAndFlush(precision, DEC);
  OTV0P2BASE::serialPrintlnAndFlush();

  while (minOW.search(address))
  {
    if (DS18B20_MODEL_ID == address[0]) 
    {
      setPrecision(address, precision);
    }
  }

  // time reading a single sensor by address
  unsigned long time = millis();
  minOW.reset();
  minOW.select(firstAddress);
  minOW.write(CMD_START_CONVO); // Start conversion without parasite power.

  int16_t temp = readTemp(firstAddress);
  unsigned long delay = millis() - time;

  OTV0P2BASE::serialPrintAndFlush("DS18B20 (first): ");
  OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
  OTV0P2BASE::serialPrintAndFlush('.');
  OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
  OTV0P2BASE::serialPrintAndFlush('C');
  OTV0P2BASE::serialPrintAndFlush(" in ");
  OTV0P2BASE::serialPrintAndFlush(delay, DEC);
  OTV0P2BASE::serialPrintAndFlush("ms");
  OTV0P2BASE::serialPrintlnAndFlush();

  // Read all the sensors by searching
  uint16_t i = 0;
  unsigned long total = 0;
  time = millis();

  minOW.reset();
  minOW.skip();
  minOW.write(CMD_START_CONVO); // Start conversion without parasite power.

  while (minOW.search(address))
  {
    // Is this a DS18B20?
    if (DS18B20_MODEL_ID != address[0]) {
      continue;
    }

    temp = readTemp(address);
    delay = millis() - time;
    total += delay;

    OTV0P2BASE::serialPrintAndFlush("DS18B20[");
    OTV0P2BASE::serialPrintAndFlush(i++);
    OTV0P2BASE::serialPrintAndFlush("] temp: ");
    OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
    OTV0P2BASE::serialPrintAndFlush('.');
    OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
    OTV0P2BASE::serialPrintAndFlush('C');
    OTV0P2BASE::serialPrintAndFlush(" in ");
    OTV0P2BASE::serialPrintAndFlush(delay, DEC);
    OTV0P2BASE::serialPrintAndFlush("ms (");
    OTV0P2BASE::serialPrintAndFlush(total, DEC);
    OTV0P2BASE::serialPrintAndFlush("ms total)");
    OTV0P2BASE::serialPrintlnAndFlush();

    time = millis();
  }

  // Increase the presision
  if (++precision > OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION) {
    precision = OTV0P2BASE::TemperatureC16_DS18B20::MIN_PRECISION;
  }
}
