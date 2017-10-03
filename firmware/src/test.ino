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
uint8_t dot_state = 0;
uint8_t timer_int = 0;
uint8_t state = 0;

static const uint8_t PWM_PIN = 16;
static const uint8_t IRQ_PIN = 5;

void setup() {


    // fire up the serial
    Serial.begin(19200);

    // configure pins
    pinMode(PWM_PIN, OUTPUT);
    pinMode(IRQ_PIN, INPUT);


    // fire up the RTC
    rtc.begin();

    // interrupts
    attachInterrupt(IRQ_PIN, rtc_int, RISING);

    // fire up the WiFi
    WiFi.begin(ssid, password);
    delay(3000);
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
        // rtc.adjust(DateTime(2017, 9, 3,
        //     20, 3, 15));
        Serial.println("Not connected");
    }

    // fire up the Nixies
    display.init();

}


void loop() {

    if(timer_int)
    {
        switch (state)
        {
            case 0:
                timer_int = 0;
                rtc_time = rtc.now();
                display.write_time(rtc_time, dot_state);
        }


    }
}

void rtc_int()
{
    timer_int = 1;
    dot_state = ~dot_state;
}
