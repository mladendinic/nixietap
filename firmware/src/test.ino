#include <Arduino.h>
#include <nixie.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EasyNTPClient.h>

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
uint8_t interrupt_counter = 0;
uint8_t state = 0;
uint8_t state_counter = 0;
uint8_t adc_reading = 0;
char i;
const int knock_sensor = A0; // the piezo is connected to analog pin 0
const int threshold = 5;  // threshold value to decide when the detected sound is a knock or not
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
        rtc.adjust(DateTime(2017, 17, 10,
            timeClient.getHours()+1, timeClient.getMinutes(), timeClient.getSeconds()));
        Serial.println("Connected");
    }
    else {
        // rtc.adjust(DateTime(2017, 9, 3,
        //     20, 3, 15));
        Serial.println("Not connected");
        rtc_time=rtc.now();
    }

    // fire up the Nixies
    display.init();
    display.write(1,2,3,4,1);



}


void loop() {

    // for (i = 0; i <= 5; i++)
    // {
    //     adc_reading += analogRead(knock_sensor);
    // }
    // adc_reading = adc_reading / 5;

    // adc_reading = analogRead(knock_sensor);
    // if (adc_reading > (threshold))
    // {
    //     delay(5);
    //     adc_reading = analogRead(knock_sensor);
    //     if (adc_reading > (threshold))
    //     {
    //         state++;
    //         if (state == 10) { state = 0; };
    //         display.write(state,state,state,state,1);
    //     }
    // }
    // delay(10);


    if(timer_int)
    {
        timer_int = 0;
        switch (state)
        {
            case 0:
                rtc_time = rtc.now();
                display.write_time(rtc_time, dot_state);
                break;
            // case 1:
            //     rtc_time = rtc.now();
            //     display.write_date(rtc_time, 255);
            //     break;

        }


    }
}

void change_state()
{

}

void rtc_int()
{

    interrupt_counter++;
    if (!interrupt_counter)
    {
        timer_int = 1;
        dot_state = ~dot_state;
        state_counter++;
        // if (state_counter == 10)
        // {
        //     state_counter = 0;
        //     state++;
        //     if (state == 2)
        //     {
        //         state = 0;
        //     }
        //
        // }

    }

}
