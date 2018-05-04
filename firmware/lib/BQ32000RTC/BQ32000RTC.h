/*
 * BQ32000RTC.h - library for BQ32000 RTC
 * This library is intended to be uses with Arduino Time library functions
 */

#ifndef _BQ32000RTC_h
#define _BQ32000RTC_h

#include <TimeLib.h>

class BQ32000RTC {
public:
    BQ32000RTC();
    static void begin(uint8_t sda, uint8_t scl);
    static time_t get();
    static bool set(time_t t);
    static bool read(tmElements_t &tm);
    static bool write(tmElements_t &tm);
    static bool chipPresent() { return exists; }
    static unsigned char isRunning();

    static void setIRQ(uint8_t state);
    /* Set IRQ output state: 0=disabled, 1=1Hz, 2=512Hz.
     */
    static void setIRQLevel(uint8_t level);
    /* Set IRQ output active state to LOW or HIGH.
     */
    static void setCalibration(int8_t value);
    /* Sets the calibration value to given value in the range -31 - 31, which
     * corresponds to -126ppm - +63ppm; see table 13 in th BQ32000 datasheet.
     */
    static void setCharger(int state);
    /* If using a super capacitor instead of a battery for backup power, use this
       method to set the state of the trickle charger: 0=disabled, 1=low-voltage
       charge, 2=high-voltage charge. In low-voltage charge mode, the super cap is
       charged through a diode with a voltage drop of about 0.5V, so it will charge
       up to VCC-0.5V. In high-voltage charge mode the diode is bypassed and the super
       cap will be charged up to VCC (make sure the charge voltage does not exceed your
       super cap's voltage rating!!). */

    // utility functions:
    static uint8_t readRegister(uint8_t address);
    static void writeRegister(uint8_t address, uint8_t value);

private:
    static bool exists;
    static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
    static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }
};

#ifdef RTC
#undef RTC // workaround for Arduino Due, which defines "RTC"...
#endif

extern BQ32000RTC RTC;

#endif // _BQ32000RTC_h