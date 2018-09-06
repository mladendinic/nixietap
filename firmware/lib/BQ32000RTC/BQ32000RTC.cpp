// A library for handling real-time clocks, dates, etc.
// Only ESP8266 compatible!

#include <Wire.h>
#include <Arduino.h>
#include <pgmspace.h>
#include "BQ32000RTC.h"

BQ32000RTC::BQ32000RTC() {
    begin(D3, D4);
}

void BQ32000RTC::begin(uint8_t sda, uint8_t scl) {
    Wire.begin(sda, scl);
    Wire.setClock(100000);
}

time_t BQ32000RTC::get() {
    tmElements_t tm;
    if(read(tm) == false) return 0;
    return(makeTime(tm));
}

bool BQ32000RTC::set(time_t t) {
    tmElements_t tm;
    breakTime(t, tm);
    return write(tm); 
}

bool BQ32000RTC::read(tmElements_t &tm) {
    uint8_t sec;
    Wire.beginTransmission(BQ32000_ADDRESS);
    Wire.write((byte) 0);
    if (Wire.endTransmission() != 0) {
        exists = false;
        return false;
    }
    exists = true;
    delayMicroseconds(60);
    Wire.requestFrom(BQ32000_ADDRESS, 7);
    sec = Wire.read();
    tm.Second = bcd2bin(sec & 0x7f);
    tm.Minute = bcd2bin(Wire.read());
    tm.Hour = bcd2bin(Wire.read());
    Wire.read();
    tm.Day = bcd2bin(Wire.read());
    tm.Month = bcd2bin(Wire.read());
    tm.Year = bcd2bin(Wire.read());
    delayMicroseconds(60);
    if(sec & 0x80) return false;
    return true;
}

bool BQ32000RTC::write(tmElements_t &tm) {
    Wire.beginTransmission(BQ32000_ADDRESS);
    Wire.write((byte) 0);
    Wire.write(bin2bcd(tm.Second));
    Wire.write(bin2bcd(tm.Minute));
    Wire.write(bin2bcd(tm.Hour));
    Wire.write(bin2bcd(0));
    Wire.write(bin2bcd(tm.Day));
    Wire.write(bin2bcd(tm.Month));
    Wire.write(bin2bcd(tm.Year));
    if(Wire.endTransmission() != 0) {
        exists = false;
        return false;
    }
    delayMicroseconds(60);
    exists = true;
    return true;
}

void BQ32000RTC::setIRQ(uint8_t state) {
    /* Set IRQ square wave output state: 0=disabled, 1=1Hz, 2=512Hz.
     */
    uint8_t value;
    if (state) {
      // Setting the frequency is a bit complicated on the BQ32000:
        Wire.beginTransmission(BQ32000_ADDRESS);
        Wire.write(BQ32000_SFKEY1);
        Wire.write(BQ32000_SFKEY1_VAL);
        Wire.write(BQ32000_SFKEY2_VAL);
        Wire.write((state == 1) ? BQ32000_FTF_1HZ : BQ32000_FTF_512HZ);
        Wire.endTransmission();
        delayMicroseconds(60);
    }
    value = readRegister(BQ32000_CAL_CFG1);
    value = (!state) ? value & ~(1<<BQ32000__FT) : value | (1<<BQ32000__FT);
    writeRegister(BQ32000_CAL_CFG1, value);
}

void BQ32000RTC::setIRQLevel(uint8_t level) {
    /* Set IRQ output level when IRQ square wave output is disabled to
     * LOW or HIGH.
     */
    uint8_t value;
    // The IRQ active level bit is in the same register as the calibration
    // settings, so we preserve its current state:
    value = readRegister(BQ32000_CAL_CFG1);
    value = (!level) ? value & ~(1<<BQ32000__OUT) : value | (1<<BQ32000__OUT);
    writeRegister(BQ32000_CAL_CFG1, value);
}

void BQ32000RTC::setCalibration(int8_t value) {
    /* Sets the calibration value to given value in the range -31 - 31, which
     * corresponds to -126ppm - +63ppm; see table 13 in th BQ32000 datasheet.
     */
    uint8_t val;
    if (value > 31) value = 31;
    if (value < -31) value = -31;
    val = (uint8_t) (value < 0) ? -value | (1<<BQ32000__CAL_S) : value;
    val |= readRegister(BQ32000_CAL_CFG1) & ~0x3f;
    writeRegister(BQ32000_CAL_CFG1, val);
}

void BQ32000RTC::setCharger(int state) {
    /* If using a super capacitor instead of a battery for backup power, use this
     * method to set the state of the trickle charger: 0=disabled, 1=low-voltage
     * charge, 2=high-voltage charge. In low-voltage charge mode, the super cap is
     * charged through a diode with a voltage drop of about 0.5V, so it will charge
     * up to VCC-0.5V. In high-voltage charge mode the diode is bypassed and the super
     * cap will be charged up to VCC (make sure the charge voltage does not exceed your
     * super cap's voltage rating!!).
     */
    // First disable charger regardless of state (prevents it from
    // possible starting up in the high voltage mode when the low
    // voltage mode is requested):
    uint8_t value;
    writeRegister(BQ32000_TCH2, 0);
    if (state <= 0 || state > 2) return;
    value = BQ32000_CHARGE_ENABLE;
    if (state == 2) {
        // High voltage charge enable:
        value |= (1 << BQ32000__TCFE);
    }
    writeRegister(BQ32000_CFG2, value);
    // Now enable charger:
    writeRegister(BQ32000_TCH2, 1 << BQ32000__TCH2_BIT);
}

uint8_t BQ32000RTC::readRegister(uint8_t address) {
    /* Read and return the value in the register at the given address.
     */
    Wire.beginTransmission(BQ32000_ADDRESS);
    Wire.write((byte) address);
    Wire.endTransmission();
    Wire.requestFrom(BQ32000_ADDRESS, 1);
    // Get register state:
    return Wire.read();
}

void BQ32000RTC::writeRegister(uint8_t address, uint8_t value) {
    /* Write the given value to the register at the given address.
     */
    Wire.beginTransmission(BQ32000_ADDRESS);
    Wire.write(address);
    Wire.write(value);
    Wire.endTransmission();
    delayMicroseconds(60);
}

unsigned char BQ32000RTC::isRunning() {
    return !(readRegister(0x0)>>7);
}

bool BQ32000RTC::exists = false;

BQ32000RTC RTC = BQ32000RTC();