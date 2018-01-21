#include <Arduino.h>
#include <Nixie.h>
#include <BQ32000RTC.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define RTC_IRQ_PIN D1
#define RTC_SDA_PIN D3
#define RTC_SCL_PIN D4
#define BUTTON D2

void irq_1Hz_int();     // interrupt function for changing the dot state every 1 second.
void buttonPressed();   // interrupt function when button is pressed.

Nixie nixie;
BQ32000RTC bq32000;
time_t utcTime, localTime;
time_t prevDisplay = 0; // when the digital clock was displayed
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
uint8_t state = 0;

//Australia Eastern Time Zone (Sydney, Melbourne)
TimeChangeRule aEDT = {"AEDT", First, Sun, Oct, 2, 660};    //UTC + 11 hours
TimeChangeRule aEST = {"AEST", First, Sun, Apr, 3, 600};    //UTC + 10 hours
Timezone ausET(aEDT, aEST);

//Central European Time (Frankfurt, Paris, Belgrade);
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time, UTC + 2 hours
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time, UTC + 1 hour
Timezone CE(CEST, CET);

//United Kingdom (London, Belfast)
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};        //British Summer Time
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};         //Standard Time
Timezone UK(BST, GMT);

//US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //Eastern Standard Time = UTC - 5 hours
Timezone usET(usEDT, usEST);

//US Central Time Zone (Chicago, Houston)
TimeChangeRule usCDT = {"CDT", Second, dowSunday, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, dowSunday, Nov, 2, -360};
Timezone usCT(usCDT, usCST);

//US Mountain Time Zone (Denver, Salt Lake City)
TimeChangeRule usMDT = {"MDT", Second, dowSunday, Mar, 2, -360};
TimeChangeRule usMST = {"MST", First, dowSunday, Nov, 2, -420};
Timezone usMT(usMDT, usMST);

//Arizona is US Mountain Time Zone but does not use DST
Timezone usAZ(usMST, usMST);

//US Pacific Time Zone (Las Vegas, Los Angeles)
TimeChangeRule usPDT = {"PDT", Second, dowSunday, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, dowSunday, Nov, 2, -480};
Timezone usPT(usPDT, usPST);

volatile bool buttonState = HIGH;
volatile bool dot_state = LOW;
unsigned long currentMillis = 0, previousMillis = 0;
uint8_t timeZone = 1;

void setup() {
    // fire up the serial
    Serial.begin(115200);

    // Initialise Nixie's
    nixie.init();
    nixie.write(11, 11, 11, 11, 0);

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

    // RTC IRQ interrupt
    pinMode(RTC_IRQ_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
    // Touch button interrupt
    pinMode(BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, FALLING);

    // get time from server and update it on a RTC
    while(!timeClient.update()) {
        currentMillis = millis();
        if((currentMillis - previousMillis) > 5000)
        break;
    }
    if((currentMillis - previousMillis) > 5000) {
        Serial.println("Can't pull time from server!");
        nixie.write(0, 0, 0, 1, 0);
    } else {
        utcTime = timeClient.getEpochTime();
        switch(timeZone) {
            case 0: //Australia Eastern Time Zone (Sydney, Melbourne)
                    localTime = ausET.toLocal(utcTime);
                    break;
            case 1: //Central European Time (Frankfurt, Paris, Belgrade)
                    localTime = CE.toLocal(utcTime);
                    break;
            case 2: //United Kingdom (London, Belfast)
                    localTime = UK.toLocal(utcTime);
                    break;
            case 3: //US Eastern Time Zone (New York, Detroit)
                    localTime = usET.toLocal(utcTime);
                    break;
            case 4: //US Central Time Zone (Chicago, Houston)
                    localTime = usCT.toLocal(utcTime);
                    break;
            case 5: //US Mountain Time Zone (Denver, Salt Lake City)
                    localTime = usMT.toLocal(utcTime);
                    break;
            case 6: //US Pacific Time Zone (Las Vegas, Los Angeles)
                    localTime = usPT.toLocal(utcTime);
                    break;
            default: // Leave time zone as UTC
                     localTime = utcTime;
                     break;
        }
        setTime(localTime);
        bq32000.set(localTime);
        setSyncProvider(RTC.get);
        setSyncInterval(1);
    }
}

void loop() {
   
    // When the button is pressed nixie display will change the displaying mode from time to date, and vice verse.    
    switch (state) {
        case 0: // Display time
            if(now() != prevDisplay) { //update the display only if time has changed
                prevDisplay = now();
                nixie.write_time(bq32000.get(), dot_state);
            }   
            break;
        case 1: // Display date
            nixie.write_date(bq32000.get(), 1);
            break;
    }

}

void irq_1Hz_int() {
    dot_state = !dot_state;
}

void buttonPressed() {
    buttonState = !buttonState;
    state++;
    if (state == 2) state = 0;
    Serial.println(state);
}
