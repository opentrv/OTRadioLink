#include <Wire.h>
#include <OTRadioLink.h>

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
}

static int16_t readTemp(const uint8_t address[8])
{
  minOW.reset();
  minOW.select(address);
  while (minOW.read_bit() == 0) {
    OTV0P2BASE::nap(WDTO_15MS);
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

uint8_t address[8];

void setup()
{
  Serial.begin(4800);

  // Ensure no bad search state.
  minOW.reset_search();

  while (minOW.search(address))
  {
    if(DS18B20_MODEL_ID == address[0]) {
      break;
    }
  }
  minOW.reset_search();

  OTV0P2BASE::serialPrintAndFlush("9,ms,10,ms,11,ms,12,ms");
  OTV0P2BASE::serialPrintlnAndFlush();
}

void loop()
{
  for (uint8_t precision = OTV0P2BASE::TemperatureC16_DS18B20::MIN_PRECISION;
       precision <= OTV0P2BASE::TemperatureC16_DS18B20::MAX_PRECISION;
       precision++)
  {
    // Set the prescision
    setPrecision(address, precision);

    unsigned long time = millis();

    // Read the temp
    minOW.reset();
    minOW.select(address);
    minOW.write(CMD_START_CONVO); // Start conversion without parasite power.

    int16_t temp = readTemp(address);
    unsigned long delay = millis() - time;

    if (OTV0P2BASE::TemperatureC16_DS18B20::MIN_PRECISION != precision) {
      OTV0P2BASE::serialPrintAndFlush(",");
    }
    OTV0P2BASE::serialPrintAndFlush(temp >> 4, DEC);
    OTV0P2BASE::serialPrintAndFlush('.');
    OTV0P2BASE::serialPrintAndFlush((unsigned long)(((temp & 0xf) / 16.0) * 1000), DEC);
    OTV0P2BASE::serialPrintAndFlush(",");
    OTV0P2BASE::serialPrintAndFlush(delay, DEC);
  }
  OTV0P2BASE::serialPrintlnAndFlush();
}
