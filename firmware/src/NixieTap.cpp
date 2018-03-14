#include <FS.h>
#include <Arduino.h>
#include <Nixie.h>
#include <BQ32000RTC.h>
#include <NtpClientLib.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// This service locates the nearest NTP server from your location and attempts to synchronize the date (UTC time) from it.
// Another option, if you live in the United States, is to use the NIST Internet Clock with the name of the server: "time.nist.gov".
#define NTP_SERVER "pool.ntp.org"

void irq_1Hz_int();         // Interrupt function for changing the dot state every 1 second.
void buttonPressed();       // Interrupt function when button is pressed.
void enableSecondsDot();
void enableScrollDots(uint8_t speedOfDots);
void disableDots();
void scrollDots();          // Interrupt function for scrolling dots.
void checkForAPInvoke();    // Checks if the user tapped 5 times in a rapid succession. If yes, invokes AP mode.
void nixieTapConnected(const WiFiEventStationModeConnected& evt);
void nixieTapDisconnected(const WiFiEventStationModeDisconnected& evt);
void nixieTapGotIP(const WiFiEventStationModeGotIP& evt);
void processSyncEvent(NTPSyncEvent_t ntpEvent);

volatile bool dot_state = LOW;
bool resetDone = true, stopDef = false, secDotDef = false, scrollDotsDef = false;
bool wifiFirstConnected = true, syncEventTriggered = false; // True if a time even has been triggered

uint8_t dotsSpeed = 100, timeZone = 0, minutesTimeZone = 0, dst = 0;
volatile uint8_t state = 0, tuchState = 0, dotPosition = 0b10;
volatile uint16_t counter = 0;

unsigned long currentMillis = 0, previousMillis = 0;
time_t prevTime = 0;        // The last time when the nixie tubes were sync. This prevents the change of the nixie tubes unless the time has changed.

WiFiManager wifiManager;
NTPSyncEvent_t ntpEvent;    // Last triggered event
WiFiEventHandler stationDisconnectedHandler;
WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationGotIPdHandler;

void setup() {
    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, FALLING);
    // WiFiManager. For configuring WiFi access point, setting up the NixieTap parameters and so on...
    // Sets timeout(in seconds) until configuration portal gets turned off.
    wifiManager.setConfigPortalTimeout(600);
    // Fetches ssid and pass from eeprom and tries to connect,
    // if it does not connect it starts an access point with the specified name "NixieTapAP"
    // and goes into a blocking loop awaiting configuration.
    if(!wifiManager.autoConnect("NixieTap", "Nixie123")) {
        Serial.println("Failed to connect and hit timeout!");
        // Nixie display will show this error code:
        nixieTap.write(0, 0, 0, 2, 0);
        delay(3000);
    }
    // Configuring NTP server.
    NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {ntpEvent = event; syncEventTriggered = true;});
    // Register event handlers.
    // Callback functions will be called as long as these handler objects exist.
    // Call "onStationModeConnected" each time a station connects.
    stationConnectedHandler = WiFi.onStationModeConnected(&nixieTapConnected);
    // Call "onStationModeDisconnected" each time a station disconnects.
    stationDisconnectedHandler = WiFi.onStationModeDisconnected(&nixieTapDisconnected);
    // Call "onStationModeGotIP" each time a station gets IP address.
    stationGotIPdHandler = WiFi.onStationModeGotIP(&nixieTapGotIP);
}

void loop() {
    if(wifiFirstConnected) {
        wifiFirstConnected = false;
        // While time is not first adjusted yet, synchronization will be attempted every 60 seconds. When first sync is done, UTC time will be sync every 24 hours.
        NTP.setInterval(60, 86400);
        NTP.setNTPTimeout(5000);
        NTP.begin(NTP_SERVER, timeZone, false, minutesTimeZone);
        enableSecondsDot();
    }
    if(syncEventTriggered) {
        // When the syncEvent is triggered depending on a set interval of synchronization this function will be executed.
        processSyncEvent(ntpEvent);
        syncEventTriggered = false;
    }
    // This function allows you to manually start the access point on demand. (By tapping the button 5 times in a rapid succesion)
    checkForAPInvoke();
    // When the button is pressed nixie tubes will change the displaying mode from time to date, and vice verse. 
    if(state >= 2) state = 0;
    switch (state) {
        case 0: // Display time.
            if(now() != prevTime) { // Update the display only if time has changed.
                prevTime = now();
                nixieTap.write_time(now(), dot_state);
            }
            break;
        case 1: // Display date.
            nixieTap.write_date(now(), 1);
            break;
        default:
            Serial.println("Error. Unknown state of a button!");
            break;
    }
}

void checkForAPInvoke() {
    currentMillis = millis();
    // By tapping the button 5 times in a time gap of a 800 ms. You can manually start the WiFi Manager and access its settings.
    if(tuchState == 1) previousMillis = currentMillis;
    if((tuchState >= 5) && ((currentMillis - previousMillis) <= 1000)) {
        resetDone = false;
        tuchState = 0;
        if(!resetDone) {
            resetDone = true;
            wifiManager.setConfigPortalTimeout(600);
            // This will run a new config portal if the conditions are met.
            if(!wifiManager.startConfigPortal("NixieTap", "Nixie123")) {
                Serial.println("Failed to connect and hit timeout!");
                // Nixie display will show this error code:
                nixieTap.write(0, 0, 0, 2, 0);
                delay(3000);
            }
            // If you get here you have connected to the config portal.
            Serial.println("Connected to a new config portal!");
        }
    } else if((currentMillis - previousMillis) > 1000) tuchState = 0;
}

void nixieTapConnected(const WiFiEventStationModeConnected &evt) {
    Serial.printf("Connected to %s\r\n", evt.ssid.c_str());
}
// Manage network disconnection
void nixieTapDisconnected(const WiFiEventStationModeDisconnected &evt) {
    Serial.printf("Disconnected from SSID: %s\n", evt.ssid.c_str());
    Serial.printf("Reason: %d\n", evt.reason);
    NTP.stop(); // NTP sync is disableded to avoid sync errors.
}
// Start NTP only after IP network is connected
void nixieTapGotIP(const WiFiEventStationModeGotIP &evt) {
    Serial.printf("Got IP: %s\r\n", evt.ip.toString().c_str());
    Serial.printf("Connected: %s\r\n", WiFi.status() == WL_CONNECTED ? "yes" : "no");
    wifiFirstConnected = true;
}
void processSyncEvent(NTPSyncEvent_t ntpEvent) {
    if(ntpEvent) {
        Serial.print("Time Sync error: ");
        if(ntpEvent == noResponse)
            Serial.println("NTP server not reachable.");
        else if(ntpEvent == invalidAddress)
            Serial.println("Invalid NTP server address.");
    } else {
        Serial.println("Got NTP time! UTC: " + NTP.getTimeStr());
        // Modifies UTC depending on the selected time zone. 
        // After that the time is sent to the RTC and Time library.
        timeZone = nixieTap.getTimeZoneOffset(now(), nixieTap.getLocFromFreegeo(), &dst);
        NTP.setTimeZone((timeZone/60), (timeZone%60));
        NTP.setDayLight(dst);
        RTC.set(now());
    }
}
/*                                                            *
 * Enables the center dot to change its state every second.   *
 *                                                            */
void enableSecondsDot() {
    if(secDotDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(1);              // Configures the 512Hz interrupt from RTC.
        attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
        secDotDef = true;
        stopDef = false;
    }
}
/*                                                                             *
 *  Enables the dots to scroll acros nixie tube display.                       *
 *  @param[in] speedOfDots - how fast the dots change position on the display. *
 *                                                                             */
void enableScrollDots(uint8_t speedOfDots) {
    if(scrollDotsDef == false) {
        if(speedOfDots > 0 && speedOfDots <= 512) {
            dotsSpeed = speedOfDots;
        } else {
            Serial.print("Invalid value! Scrolling speed of dots must be between 1 and 512. ");
            Serial.println("Scrolling dots speed is set to 100!");
            dotsSpeed = 100;
        }
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(2);              // Configures the 512Hz interrupt from RTC.
        attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), scrollDots, FALLING);
        scrollDotsDef = true;
        stopDef = false;
    }
}
/*                                                *
 * Disaling the dots function on nixie display.   *
 *                                                */
void disableDots() {
    if(stopDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(0);              // Configures the interrupt from RTC.
        dotPosition = 0b10;         // Restast dot position.
        stopDef = true;
        secDotDef = false;
        scrollDotsDef = false;
    }
}
/*                                                                                       *
 * An interrupt function that changes the state and position of the dots on the display. *
 *                                                                                       */
void scrollDots() {
    counter++;
    if(counter >= dotsSpeed) {
        if(dotPosition == 0b100000) dotPosition = 0b10;
        nixieTap.write(11, 11, 11, 11, dotPosition);
        dotPosition = dotPosition << 1;
        counter = 0;
    }
}
/*                                                                  *
 * An interrupt function for changing the dot state every 1 second. *
 *                                                                  */
void irq_1Hz_int() {
    dot_state = !dot_state;
}
/*                                                                *
 * An interrupt function for the touch sensor when it is touched. *
 *                                                                */
void buttonPressed() {
    tuchState++;
    state++;
}