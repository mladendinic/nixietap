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

void irq_1Hz_int();         // Interrupt function for changing the dot state every 1 second.
void buttonPressed();       // Interrupt function when button is pressed.
void startScrollingDots();
void stopScrollingDots();
void scroll_dots();         // Interrupt function for scrolling dots.
void checkForAPInvoke();    // Checks if the user tapped 5 times in a rapid succession. If yes, invokes AP mode.
void nixieTapConnected(WiFiEventStationModeConnected ipInfo);
void nixieTapGotIP(WiFiEventStationModeGotIP ipInfo);
void nixieTapDisconnected(WiFiEventStationModeDisconnected event_info);
void processSyncEvent(NTPSyncEvent_t ntpEvent);

volatile bool buttonState = HIGH, dot_state = LOW;
bool startDef = false, stopDef = false, resetDone = true;
bool wifiFirstConnected = false, syncEventTriggered = false; // True if a time even has been triggered

uint8_t timeZone = 0, minutesTimeZone = 0, dst = 0;
volatile uint8_t state = 0, tuchState = 0, dotPosition = 0b10;
volatile uint16_t counter = 0;

unsigned long currentMillis = 0, previousMillis = 0;

time_t prevTime = 0;        // The last time when the nixie tubes were sync. This prevents the change of the nixie tubes unless the time has changed.
WiFiUDP ntpUDP;
WiFiManager wifiManager;
NTPSyncEvent_t ntpEvent;    // Last triggered event

void setup() {
    // Fire up the serial.
    Serial.begin(115200);
    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, FALLING);
    // RTC IRQ interrupt.
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
    // Initialise Nixie's.
    // nixieTap.begin();
    // Turn off the Nixie tubes. If this is not called nixies might show some random stuff on startup.
    nixieTap.write(11, 11, 11, 11, 0);
    // Start scrolling dots on a Nixie tubes while the starting procedure is in proces.
    startScrollingDots();
    // WiFiManager. For configuring WiFi access point, setting up the NixieTap parameters and so on...
    // wifiManager.resetSettings(); // Implement onely if you need it. You will erase all network settings and parameters.
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
    NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
        ntpEvent = event;
        syncEventTriggered = true;
    });
    static WiFiEventHandler e1, e2, e3;
    e1 = WiFi.onStationModeGotIP(nixieTapGotIP);      // As soon WiFi is connected, start NTP Client
    e2 = WiFi.onStationModeDisconnected(nixieTapDisconnected);
    e3 = WiFi.onStationModeConnected(nixieTapConnected);
    
    setSyncProvider(RTC.get);   // Tells the Time.h library from where to sink the time.
    setSyncInterval(60);          // Sync interval is in seconds.

    // If this function is not called after startScrollingDots function, or don not have a proper place in the code. 
    // Then one second dot status in time display mode will not work properly.
    stopScrollingDots();
}

void loop() {
    if(wifiFirstConnected) {
        wifiFirstConnected = false;
        NTP.begin ("pool.ntp.org", timeZone, false, minutesTimeZone);
        NTP.setInterval (63);
    }
    if(syncEventTriggered) {
        processSyncEvent(ntpEvent);
        syncEventTriggered = false;
    }
    // This function allows you to manually start the access point on demand. (By tapping the button 5 times in a rapid succesion)
    checkForAPInvoke();
    // When the button is pressed nixie tubes will change the displaying mode from time to date, and vice verse. 
    if(state >= 2) state = 0;
    switch (state) {
        case 0: // Display time.
            if (now() != prevTime) { // Update the display only if time has changed.
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
            startScrollingDots();
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
            stopScrollingDots();
        }
    } else if((currentMillis - previousMillis) > 1000) tuchState = 0;
}

void nixieTapConnected(WiFiEventStationModeConnected ipInfo) {
    Serial.printf ("Connected to %s\r\n", ipInfo.ssid.c_str ());
}
// Start NTP only after IP network is connected
void nixieTapGotIP(WiFiEventStationModeGotIP ipInfo) {
    Serial.printf ("Got IP: %s\r\n", ipInfo.ip.toString ().c_str ());
    Serial.printf ("Connected: %s\r\n", WiFi.status () == WL_CONNECTED ? "yes" : "no");
    wifiFirstConnected = true;
}
// Manage network disconnection
void nixieTapDisconnected(WiFiEventStationModeDisconnected event_info) {
    Serial.printf ("Disconnected from SSID: %s\n", event_info.ssid.c_str ());
    Serial.printf ("Reason: %d\n", event_info.reason);
    NTP.stop(); // NTP sync is disableded to avoid sync errors.
}
void processSyncEvent(NTPSyncEvent_t ntpEvent) {
    if(ntpEvent) {
        Serial.print ("Time Sync error: ");
        if(ntpEvent == noResponse)
            Serial.println ("NTP server not reachable.");
        else if(ntpEvent == invalidAddress)
            Serial.println ("Invalid NTP server address.");
    } else {
        Serial.print ("Got NTP time!");
        // Modifies UTC depending on the selected time zone. 
        // After that the time is sent to the RTC and Time library.
        timeZone = nixieTap.getTimeZone(now(), nixieTap.getLocation(), &dst);
        NTP.setTimeZone((timeZone/100), (timeZone%100));
        NTP.setDayLight(dst);
        RTC.set(now());
    }
}

void startScrollingDots() {
    if(startDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(2);              // Configures the 512Hz interrupt from RTC.
        attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), scroll_dots, FALLING);
        startDef = true;
        stopDef = false;
    }
}
void stopScrollingDots() {
    if(stopDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(1);              // Configures the 1Hz interrupt from RTC.
        attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
        dotPosition = 0b10; // Restast dot position.
        stopDef = true;
        startDef = false;
    }
}
void scroll_dots() {
    counter++;
    if(counter >= DOTS_MOVING_SPEED) {
        if(dotPosition == 0b100000) dotPosition = 0b10;
        nixieTap.write(11, 11, 11, 11, dotPosition);
        dotPosition = dotPosition << 1;
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