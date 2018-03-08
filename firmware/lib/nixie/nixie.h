#ifndef _NIXIE_h   /* Include guard */
#define _NIXIE_h

#include <Arduino.h>
#include <TimeLib.h>
#include <SPI.h>
#include <BQ32000RTC.h>
#include <ESP8266HTTPClient.h>  // This request has been included in this library: https://github.com/esp8266/Arduino/pull/2821
                                // If it is not included, Google Maps API would not work properly.
#include <ArduinoJson.h>

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
    Nixie();
    static void begin();
    static void write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);
    static void write_time(time_t local, bool dot_state);
    static void write_date(time_t local, bool dot_state);
    String getIPlocation();
    String getLocation();
    String UrlEncode(const String url);
    int getTimeZone(time_t now, String loc,  uint8_t *dst);

};

#ifdef nixieTap
#undef nixieTap // workaround for Arduino Due, which defines "RTC"...
#endif

extern Nixie nixieTap;

#endif // _NIXIE_h
