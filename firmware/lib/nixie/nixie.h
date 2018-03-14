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

class Nixie
{
    // Initialize the display. This function configures pinModes based on .h file.
    const uint16_t pinmap[11] =
    {
        0b0000010000, // 0
        0b0000100000, // 1
        0b0001000000, // 2
        0b0010000000, // 3
        0b0100000000, // 4
        0b1000000000, // 5
        0b0000000001, // 6
        0b0000000010, // 7
        0b0000000100, // 8
        0b0000001000, // 9
        0b0000000000  // digit off
    };
   
const char *UserAgent = "NixieTap";
// openssl s_client -connect maps.googleapis.com:443 | openssl x509 -fingerprint -noout
const char *gMapsCrt = "‎‎67:7B:99:A4:E5:A7:AE:E4:F0:92:01:EF:F5:58:B8:0B:49:CF:53:D4";
const char *gMapsKey = "AIzaSyCuaNaMISYldK2xqIsBfkI3UaHvUonNlTs"; // You can get your key here: https://developers.google.com/maps/documentation/geolocation/get-api-key
const char *gTimeZoneKey = "AIzaSyA33HiwHg1ejenWHW2SW-UAy0BGOuEzHHU"; // You can get your key here: https://developers.google.com/maps/documentation/timezone/get-api-key

public:
    Nixie();
    void begin();
    void write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);
    void write_time(time_t local, bool dot_state);
    void write_date(time_t local, bool dot_state);
    int getTimeZoneOffset(time_t now, String loc,  uint8_t *dst);
    String getLocFromFreegeo();
    String getLocFromGoogle();
    String UrlEncode(const String url);
};

#ifdef nixieTap
#undef nixieTap // workaround for Arduino Due, which defines "nixieTap"...
#endif

extern Nixie nixieTap;

#endif // _NIXIE_h
