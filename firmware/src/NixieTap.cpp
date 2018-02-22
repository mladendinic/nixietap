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

void irq_1Hz_int();         // Interrupt function for changing the dot state every 1 second.
void buttonPressed();       // Interrupt function when button is pressed.
void startScrollingDots();
void stopScrollingDots();
void scroll_dots();         // Interrupt function for scrolling dots.
void checkForAPInvoke();    // Checks if the user tapped 5 times in a rapid succession. If yes, invokes AP mode.

volatile bool buttonState = HIGH, dot_state = LOW;
unsigned long currentMillis = 0, previousMillis = 0;
volatile uint8_t state = 0, tuchState = 0, dotPosition = 0;
volatile uint16_t counter = 0;
bool timeClientFlag = true;

Nixie nixieTap;
BQ32000RTC bq32000;
time_t utcTime, localTime;
time_t prevDisplay = 0; // When the digital clock was displayed.
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Time Zone Standard and Daylight saving time.
TimeChangeRule SDT = {"SDT", Last, Sun, Mar, 2, 120};     // Standard daylight time
TimeChangeRule DST = {"DST", Last, Sun, Oct, 3, 60};      // Daylight saving time
Timezone TZ(SDT, DST);
Timezone* timezones[] = { &TZ };
Timezone* tz;                   //Pointer to the time zone.
uint8_t tzIndex = 0;            //Indexes the timezones[] array.

void setup() {
    // Fire up the serial.
    Serial.begin(115200);
    // Initialise Nixie's.
    nixieTap.init();
    nixieTap.write(11, 11, 11, 11, 0);
    startScrollingDots();
    // WiFiManager
    WiFiManager wifiManager;
    // wifiManager.resetSettings();
    // Sets timeout(in seconds) until configuration portal gets turned off.
    wifiManager.setTimeout(600);
    // Fetches ssid and pass from eeprom and tries to connect,
    // if it does not connect it starts an access point with the specified name "NixieTapAP"
    // and goes into a blocking loop awaiting configuration.
    if(!wifiManager.autoConnect("NixieTapAP", "Nixie123")) {
        Serial.println("Failed to connect and hit timeout!");
        // Nixie display will show this error code:
        nixieTap.write(0, 0, 0, 2, 0);
        delay(3000);
    }
    // If you get here you have connected to the WiFi.
    Serial.println("Connected to a network!");
    // Run the NTPC client.
    timeClient.begin();

    currentMillis = millis();
    previousMillis = currentMillis;
    // Get time from the server and update it on a RTC.
    while(!timeClient.update()) {
        currentMillis = millis();
        if((currentMillis - previousMillis) > 10000) {
            timeClientFlag = false;
            break;
        }
    }
    if(!timeClientFlag) {
        Serial.println("Can not pull time from the server!");
        // Nixie display will show this error code:
        nixieTap.write(0, 0, 0, 1, 0);
        delay(3000);
    } else {
        // Requests UTC time from the server and modifies it depending on the selected time zone. 
        // After that the time is sent to the RTC and Time library.
        tz = timezones[tzIndex];
        localTime = (*tz).toLocal(timeClient.getEpochTime());
        setTime(localTime);
        bq32000.set(localTime);
    }
    setSyncProvider(RTC.get);
    setSyncInterval(1);
    stopScrollingDots();

    // RTC IRQ interrupt.
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, FALLING);
}

void loop() {
    checkForAPInvoke();
    // When the button is pressed nixie display will change the displaying mode from time to date, and vice verse. 
    if (state == 2) state = 0;  
    switch (state) {
        case 0: // Display time
            if(now() != prevDisplay) { //update the display only if time has changed
                prevDisplay = now();
                nixieTap.write_time(bq32000.get(), dot_state);
            }   
            break;
        case 1: // Display date
            nixieTap.write_date(bq32000.get(), 1);
            break;
    }

}

void checkForAPInvoke() {
    currentMillis = millis();
    // By tapping the button 5 times in a time gap of a 800 ms. You can manually start the WiFi Manager and access its settings.
    if(tuchState == 1) previousMillis = currentMillis;
    if((tuchState >= 5) && ((currentMillis - previousMillis) < 800)) {
        // This will run a new config portal if the conditions are met.
        startScrollingDots();
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        // Sets timeout(in seconds) until configuration portal gets turned off.
        wifiManager.setTimeout(600);
        if(!wifiManager.startConfigPortal("NixieTapAP", "Nixie123")) {
            Serial.println("Failed to connect and hit timeout!");
            // Nixie display will show this error code:
            nixieTap.write(0, 0, 0, 2, 0);
            delay(3000);
        }
        // If you get here you have connected to the config portal.
        Serial.println("Connected to a new config portal!");
        tuchState = 0;
        stopScrollingDots();
    } else if((currentMillis - previousMillis) > 800) tuchState = 0;
}

void startScrollingDots() {
    detachInterrupt(RTC_IRQ_PIN);
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), scroll_dots, FALLING);
    bq32000.setIRQ(2);

}

void stopScrollingDots() {
    detachInterrupt(RTC_IRQ_PIN);
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
    bq32000.setIRQ(1);
}

void scroll_dots() {
    counter++;
    if(counter >= DOTS_MOVING_SPEED) {
        if(dotPosition == 0) {
            nixieTap.write(11, 11, 11, 11, 0b10);
            dotPosition++;
        } else if(dotPosition == 1) {
              nixieTap.write(11, 11, 11, 11, 0b100);
              dotPosition++;
          } else if(dotPosition == 2) {
                nixieTap.write(11, 11, 11, 11, 0b1000);
                dotPosition++;
            } else if(dotPosition == 3) {
                  nixieTap.write(11, 11, 11, 11, 0b10000);
                  dotPosition++;
              } else {
                    nixieTap.write(11, 11, 11, 11, 0);
                    dotPosition = 0;
                }
        counter = 0;
    }

}

void irq_1Hz_int() {
    dot_state = !dot_state;
}

void buttonPressed() {
    buttonState = !buttonState;
    tuchState++;
    state++;
}