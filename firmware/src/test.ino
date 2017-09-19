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

static const uint8_t PWM_PIN = 16;

void setup() {

    // configure pins
    pinMode(PWM_PIN, OUTPUT);

    // fire up the serial
    Serial.begin(9600);

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

    // fire up the Nixies
    display.init();



}


void loop() {

    // get time from RTC
    rtc_time = rtc.now();
    // write on Nixie display
    display.write(rtc_time.hour() / 10, rtc_time.hour() % 10, rtc_time.minute()  / 10, rtc_time.minute() % 10, 0);

    delay(500);


}
