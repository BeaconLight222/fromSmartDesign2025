#include "Simple_misc_sensor.h"

#include "i2c_api.h"
extern i2c_t i2cwire0;
extern "C" void i2c_restart_enable(i2c_t *obj);

Simple_TMP102::Simple_TMP102() {}

boolean Simple_TMP102::begin() {

  i2c_restart_enable(&i2cwire0); // Enable I2C restart

  // Check if the device is connected
  byte x = 0;
  Wire.beginTransmission(TMP102_I2C_ADDR);
  Wire.write(x);
  if (Wire.endTransmission() != 0) {
    return false; // Device not found
  }

  // TMP102 only has 4 registers
  //  0x00: Temperature register
  //  0x01: Configuration register
  //  0x02: TLOW register
  //  0x03: THIGH register
  //  there is no signature register, so we cannot check if the device is a
  //  TMP102 Assume it is.

  return true;
}

float Simple_TMP102::checkTemperatureData() {
  Wire.beginTransmission(TMP102_I2C_ADDR);
  Wire.write(0x00); // Request temperature register
  Wire.endTransmission(false);

  Wire.requestFrom(TMP102_I2C_ADDR, 2); // Request 2 bytes for temperature
  if (Wire.available() < 2) {
    return NAN; // Not enough data received
  }

  int16_t rawTemp =
      (Wire.read() << 4) | (Wire.read() >> 4); // Read the two bytes
  float temperature = rawTemp * 0.0625;        // Convert to Celsius

  return temperature; // Return temperature in degrees
}

boolean Simple_DS3231::begin() {
  i2c_restart_enable(&i2cwire0); // Enable I2C restart

  // Check if the device is connected
  byte x = 0;
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(x); // Request time register
  if (Wire.endTransmission() != 0) {
    return false; // Device not found
  }

  // DS3231 does not have a signature register to check,

  bool ds3231_lost_power = lostPower();
  if (ds3231_lost_power) {
    Serial.println("DS3231 lost power, Timer is not accurate.");
    // clearLostPowerFlag();  // Clear the lost power flag
  }

  // Assume the device is a DS3231 if we can communicate with it
  return true;
}

bool Simple_DS3231::write_then_read(const uint8_t *write_buffer, int write_len,
                                    uint8_t *read_buffer, int read_len,
                                    bool stop) {
  Wire.beginTransmission(DS3231_ADDRESS);
  for (int i = 0; i < write_len; i++) {
    Wire.write(write_buffer[i]);
  }
  Wire.endTransmission(stop);
  if (read_len > 0) {
    Wire.requestFrom(DS3231_ADDRESS, read_len);
    for (int i = 0; i < read_len; i++) {
      if (Wire.available()) {
        read_buffer[i] = Wire.read();
      } else {
        return false; // Not enough data received
      }
    }
  }
  return true;
}

float Simple_DS3231::getTemperature() {
  uint8_t buffer[2] = {DS3231_TEMPERATUREREG, 0};
  write_then_read(buffer, 1, buffer, 2, false);
  return (float)buffer[0] + (buffer[1] >> 6) * 0.25f;
}

bool Simple_DS3231::lostPower(void) {
  uint8_t statusReg = DS3231_STATUSREG;
  write_then_read(&statusReg, 1, &statusReg, 1, false);
  return (statusReg & 0x80) != 0; // Check the 32KHz output bit
}

void Simple_DS3231::clearLostPowerFlag(void) {
  uint8_t statusReg = DS3231_STATUSREG;
  write_then_read(&statusReg, 1, &statusReg, 1, false);
  statusReg &= ~0x80; // Clear the lost power flag
  uint8_t writeBuffer[2] = {DS3231_STATUSREG, statusReg};
  write_then_read(writeBuffer, 2, writeBuffer, 0,
                  true); // Write back the modified status register
}

void Simple_DS3231::adjust(const DateTime &dt) {
  uint8_t buffer[8] = {DS3231_TIME,
                       bin2bcd(dt.second()),
                       bin2bcd(dt.minute()),
                       bin2bcd(dt.hour()),
                       bin2bcd(dowToDS3231(dt.dayOfTheWeek())),
                       bin2bcd(dt.day()),
                       bin2bcd(dt.month()),
                       bin2bcd(dt.year() - 2000U)};

  write_then_read(buffer, 8, nullptr, 0, true); // Write the time data
  clearLostPowerFlag(); // Clear the lost power flag after adjusting time
}

DateTime Simple_DS3231::now() {
  uint8_t buffer[7];
  buffer[0] = DS3231_TIME;
  write_then_read(buffer, 1, buffer, 7, false);
  return DateTime(bcd2bin(buffer[6]) + 2000U, bcd2bin(buffer[5] & 0x7F),
                  bcd2bin(buffer[4]), bcd2bin(buffer[2]), bcd2bin(buffer[1]),
                  bcd2bin(buffer[0] & 0x7F));
}

const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30};

DateTime::DateTime(uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000; // bring to 2000 timestamp from 1970

  ss = t % 60;
  t /= 60;
  mm = t % 60;
  t /= 60;
  hh = t % 24;
  uint16_t days = t / 24;
  uint8_t leap;
  for (yOff = 0;; ++yOff) {
    leap = yOff % 4 == 0;
    if (days < 365U + leap)
      break;
    days -= 365 + leap;
  }
  for (m = 1; m < 12; ++m) {
    uint8_t daysPerMonth = daysInMonth[m - 1];
    if (leap && m == 2)
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  d = days + 1;
}

DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                   uint8_t min, uint8_t sec) {
  if (year >= 2000U)
    year -= 2000U;
  yOff = year;
  m = month;
  d = day;
  hh = hour;
  mm = min;
  ss = sec;
}

DateTime::DateTime(const DateTime &copy)
    : yOff(copy.yOff), m(copy.m), d(copy.d), hh(copy.hh), mm(copy.mm),
      ss(copy.ss) {}

bool DateTime::isValid() const {
  if (yOff >= 100)
    return false;
  DateTime other(unixtime());
  return yOff == other.yOff && m == other.m && d == other.d && hh == other.hh &&
         mm == other.mm && ss == other.ss;
}

String DateTime::timestamp() {
  String ts;
  ts.reserve(20); // Reserve space for the timestamp string
  ts += String(year());
  ts += '-';
  ts += String(month() < 10 ? "0" : "") + String(month());
  ts += '-';
  ts += String(day() < 10 ? "0" : "") + String(day());
  ts += 'T';
  ts += String(hour() < 10 ? "0" : "") + String(hour());
  ts += ':';
  ts += String(minute() < 10 ? "0" : "") + String(minute());
  ts += ':';
  ts += String(second() < 10 ? "0" : "") + String(second());
  return ts;
}

static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
  if (y >= 2000U)
    y -= 2000U;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += (daysInMonth[i - 1]);
  if (m > 2 && y % 4 == 0)
    ++days;
  return days + 365 * y + (y + 3) / 4 - 1;
}

uint8_t DateTime::dayOfTheWeek() const {
  uint16_t day = date2days(yOff, m, d);
  return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

static uint32_t time2ulong(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
  return ((days * 24UL + h) * 60 + m) * 60 + s;
}

uint32_t DateTime::unixtime(void) const {
  uint32_t t;
  uint16_t days = date2days(yOff, m, d);
  t = time2ulong(days, hh, mm, ss);
  t += SECONDS_FROM_1970_TO_2000; // seconds from 1970 to 2000

  return t;
}

boolean Simple_EEPROM_AT24C08::begin() {
  i2c_restart_enable(&i2cwire0); // Enable I2C restart

  // Check if the device is connected
  byte x = 0;
  Wire.beginTransmission(EEPROM_START_ADDRESS);
  Wire.write(x);
  if (Wire.endTransmission() != 0) {
    return false; // Device not found
  }

  return true; // Device is ready
}

#define WIRE_ACCESS_LEN 128
int Simple_EEPROM_AT24C08::read(uint16_t memAddress, uint8_t *data,
                                uint16_t length) {
  if (length == 0 || data == nullptr) {
    return 0; // Nothing to read
  }

  if (memAddress >= 1024) {
    return 0; // Invalid memory address
  }

  int writeIndex = 0;
  while (length > 0) {
    uint16_t toRead = length;
    if (toRead > (WIRE_ACCESS_LEN / 2)) {
      toRead = (WIRE_ACCESS_LEN / 2); // Limit to WIRE_ACCESS_LEN/2
    }
    uint8_t readAddress =
        EEPROM_START_ADDRESS + (memAddress >> 8); // High byte of address

    // Set the memory address to read from
    Wire.beginTransmission(readAddress);
    Wire.write((uint8_t)(memAddress &
                         0xFF)); // Low byte only as high byte is in the address
    Wire.endTransmission(false); // Send repeated start
    Wire.requestFrom((uint8_t)readAddress, (uint8_t)toRead);
    for (uint16_t i = 0; i < toRead && Wire.available(); i++) {
      data[writeIndex++] = Wire.read();
    }

    // Update remaining length and memory address
    length -= toRead;
    memAddress += toRead;
  }

  return writeIndex;
}

int Simple_EEPROM_AT24C08::write(uint16_t memAddress, uint8_t *data,
                                 uint16_t length) {
  if (length == 0 || data == nullptr) {
    return 0; // Nothing to write
  }

  if (memAddress >= 1024) {
    return 0; // Invalid memory address
  }

  int writtenLength = 0;
  while (length > 0) {
    // we can only write one page at a time
    uint16_t endAddress = memAddress + length;
    if (((endAddress - 1) & (~0x0F)) != (memAddress & (~0x0F))) {
      // if the end address is not in the same page as the start address
      endAddress = (memAddress | 0x0F) + 1;
    }
    int writeLength = endAddress - memAddress;
    // writeLength is at most 16 bytes, no need to check for WIRE_ACCESS_LEN
    uint8_t writeAddress =
        EEPROM_START_ADDRESS + (memAddress >> 8); // High byte of address
    Wire.beginTransmission(writeAddress);
    Wire.write((uint8_t)(memAddress &
                         0xFF)); // Low byte only as high byte is in the address
    for (int i = 0; i < writeLength; i++) {
      Wire.write(data[writtenLength++]); // Write data
    }
    if (Wire.endTransmission() != 0) {
      return writtenLength; // Return how many bytes were written before failure
    }

    // Use retry to do a delay
    for (int retry = 0; retry < 300; retry++) {
      Wire.beginTransmission(EEPROM_START_ADDRESS);
      byte x = 0;
      Wire.write(x);
      if (Wire.endTransmission() == 0) {
        break; // Exit retry loop on success
      }
    }

    memAddress += writeLength;
    length -= writeLength;
  }

  return writtenLength;
}
