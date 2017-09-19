// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

#ifndef _RTCLIB_H_
#define _RTCLIB_H_

#include <Arduino.h>

#define RTC_IRQ_PIN 12

#define BQ32000_ADDRESS  0x68
#define BQ32000_CONTROL  0x07
#define BQ32000_NVRAM    0x08

#define SECONDS_PER_DAY 86400L

#define SECONDS_FROM_1970_TO_2000 946684800

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
public:
    DateTime (uint32_t t =0);
    DateTime (uint16_t year, uint8_t month, uint8_t day,
                uint8_t hour =0, uint8_t min =0, uint8_t sec =0);
    DateTime (const DateTime& copy);
    DateTime (const char* date, const char* time);
    DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time);
    uint16_t year() const       { return 2000 + yOff; }
    uint8_t month() const       { return m; }
    uint8_t day() const         { return d; }
    uint8_t hour() const        { return hh; }
    uint8_t minute() const      { return mm; }
    uint8_t second() const      { return ss; }
    uint8_t dayOfTheWeek() const;

    // 32-bit times as seconds since 1/1/2000
    long secondstime() const;
    // 32-bit times as seconds since 1/1/1970
    uint32_t unixtime(void) const;


protected:
    uint8_t yOff, m, d, hh, mm, ss;
};


// RTC based on the BQ32000 chip connected via I2C and the Wire library
enum BQ32000SqwPinMode { OFF = 0x00, ON = 0x80, SquareWave1HZ = 0x10, SquareWave4kHz = 0x11, SquareWave8kHz = 0x12, SquareWave32kHz = 0x13 };

class RTC_BQ32000 {
public:
    boolean begin(void);
    static void adjust(const DateTime& dt);
    uint8_t isrunning(void);
    static DateTime now();
    static BQ32000SqwPinMode readSqwPinMode();
    static void writeSqwPinMode(BQ32000SqwPinMode mode);
    uint8_t readnvram(uint8_t address);
    void readnvram(uint8_t* buf, uint8_t size, uint8_t address);
    void writenvram(uint8_t address, uint8_t data);
    void writenvram(uint8_t address, uint8_t* buf, uint8_t size);
    void enableCharger();
    void configure_interrrupts();
};

#endif // _RTCLIB_H_
