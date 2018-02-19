#ifndef _NIXIE_h   /* Include guard */
#define _NIXIE_h

#include <TimeLib.h>
#include <BQ32000RTC.h>

#define RTC_SDA_PIN 0
#define RTC_SCL_PIN 2
#define RTC_IRQ_PIN 5
#define SPI_CS 15
#define BUTTON 4
#define DOTS_MOVING_SPEED 100

class Nixie
{
    // volatile uint8_t dotPosition;
    // volatile uint16_t counter;
    static const uint16_t pinmap[11];

public:
    static void init();
    // static void startScrollingDots();
    // static void stopScrollingDots();
    // static void scroll_dots();
    static void write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);
    static void write_time(time_t local, bool dot_state);
    static void write_date(time_t local, bool dot_state);

};

#endif // _NIXIE_h
