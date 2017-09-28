#include <Arduino.h>
#include <nixie.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char *ssid     = "Skynet";
const char *password = "privremenasifra666";

WiFiUDP ntpUDP;
nixie display;
RTC_BQ32000 rtc;
DateTime rtc_time;
NTPClient timeClient(ntpUDP);
uint8_t seconds_buffer;
uint8_t dot_state;

static const uint8_t PWM_PIN = 16;

void setup() {


    // fire up the serial
    Serial.begin(19200);

    // configure pins
    pinMode(PWM_PIN, OUTPUT);

    // fire up the RTC
    rtc.begin();
    rtc.enableCharger();

    // fire up the WiFi
    WiFi.begin(ssid, password);
    delay(1000);
    if ( WiFi.status() == WL_CONNECTED ) {
        // fire up the timeClient
        timeClient.begin();
        // update the local time
        timeClient.update();
        // send the time to the RTC
        rtc.adjust(DateTime(2017, 9, 3,
            timeClient.getHours()+2, timeClient.getMinutes(), timeClient.getSeconds()));
        Serial.println("Connected");
    }
    else {
        rtc.adjust(DateTime(2017, 9, 3,
            20, 3, 15));
        Serial.println("Not connected");
    }

    // fire up the Nixies
    display.init();

}


void loop() {

    // get time from RTC
    rtc_time = rtc.now();
    // write on Nixie display
    if (rtc_time.second() > seconds_buffer)
    {
        seconds_buffer = rtc_time.second();
        dot_state = (~dot_state) & 0b1000;
        display.write(rtc_time.hour() / 10, rtc_time.hour() % 10, rtc_time.minute()  / 10, rtc_time.minute() % 10, dot_state);
        if(seconds_buffer == 59) { seconds_buffer = 0; }
    }

    delay(250);
}
