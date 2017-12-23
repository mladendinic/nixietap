#include <Arduino.h>
#include <Nixie.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define RTC_IRQ_PIN D1
#define RTC_SDA_PIN D3
#define RTC_SCL_PIN D4

void irq_1Hz_int();

Nixie nixie;
RTC_BQ32000 bq32000;
DateTime rtc_time;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 60*60);

volatile bool dot_state = LOW;

void setup() {
    // fire up the serial
    Serial.begin(115200);
    // WiFiManager
    WiFiManager wifiManager;
    // fetches ssid and pass from eeprom and tries to connect
    // if it does not connect it starts an access point with the specified name "NixieTapAP"
    // and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect("NixieTapAP");
    // if you get here you have connected to the WiFi
    Serial.println("Connected to a network!");

    // fire up the RTC
    bq32000.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    bq32000.setCharger(2);
    bq32000.setIRQ(1);
    timeClient.begin();
    nixie.init();

    // interrupts
    pinMode(RTC_IRQ_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);

    // get time from server and update it on a RTC
    while(!timeClient.update());
    rtc_time = timeClient.getEpochTime();
    bq32000.adjust(rtc_time);
}


void loop() {
    nixie.write_time(bq32000.now(), dot_state);
}

void irq_1Hz_int() {
    dot_state = !dot_state;
}