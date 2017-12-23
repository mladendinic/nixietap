// A library for handling real-time clocks, dates, etc.
// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
#ifndef _RTCLIB_H_
#define _RTCLIB_H_

class DateTime {
public:
    DateTime (long t =0);
    DateTime (uint16_t year, uint8_t month, uint8_t day,
                uint8_t hour =0, uint8_t min =0, uint8_t sec =0);
    DateTime (const char* date, const char* time);

    uint16_t year() const       { return 2000 + yOff; }
    uint8_t month() const       { return m; }
    uint8_t day() const         { return d; }
    uint8_t hour() const        { return hh; }
    uint8_t minute() const      { return mm; }
    uint8_t second() const      { return ss; }
    uint8_t dayOfWeek() const;

    // 32-bit times as seconds since 1/1/2000
    long get() const;

protected:
    uint8_t yOff, m, d, hh, mm, ss;
};

// TI BQ32000 I2C RTC
class RTC_BQ32000 {
public:
    static void begin(uint8_t sda, uint8_t scl);
    static void adjust(const DateTime& dt);
    static DateTime now();
    static uint8_t isrunning();

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
    static uint8_t writeRegister(uint8_t address, uint8_t value);
    static uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
    static uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }
};

// RTC using the internal millis() clock, has to be initialized before use
// NOTE: this clock won't be correct once the millis() timer rolls over (>49d?)
class RTC_Millis {
public:
    static void begin(const DateTime& dt) { adjust(dt); }
    static void adjust(const DateTime& dt);
    static DateTime now();

protected:
    static long offset;
};

#endif // _RTCLIB_H_