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
volatile uint8_t state = 0, tuchState = 0, dotPosition = 0b10;
volatile uint16_t counter = 0;
bool timeClientFlag = true;

Nixie nixieTap;
BQ32000RTC bq32000;
time_t utcTime, localTime;
time_t prevTime = 0;        // The last time when the nixie tubes were sync. This prevents the change of the nixie tubes unless the time has changed.
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiManager wifiManager;

// Time Zone Standard and Daylight saving time.
TimeChangeRule SDT = {"SDT", Last, Sun, Mar, 2, 120};     // Standard daylight time
TimeChangeRule DST = {"DST", Last, Sun, Oct, 3, 60};      // Daylight saving time
Timezone TZ(SDT, DST);

void setup() {
    // Fire up the serial.
    Serial.begin(115200);
    // Initialise Nixie's.
    nixieTap.init();
    // Turn off the Nixie tubes. If this is not called nixies might show some random stuff on startup.
    nixieTap.write(11, 11, 11, 11, 0);
    // Start scrolling dots on a Nixie tubes while the starting procedure is in proces.
    startScrollingDots();
    // WiFiManager. For configuring WiFi access point, setting up the NixieTap parameters and so on...
    // wifiManager.resetSettings(); // Implement onely if you need it. You will erase all settings and parameters in WiFiManager.
    // Sets timeout(in seconds) until configuration portal gets turned off.
    wifiManager.setTimeout(600);
    // Fetches ssid and pass from eeprom and tries to connect,
    // if it does not connect it starts an access point with the specified name "NixieTapAP"
    // and goes into a blocking loop awaiting configuration.
    if(!wifiManager.autoConnect("NixieTap", "Nixie123")) {
        Serial.println("Failed to connect and hit timeout!");
        // Nixie display will show this error code:
        nixieTap.write(0, 0, 0, 2, 0);
        delay(3000);
    }
    // If you get here you have connected to the WiFi.
    Serial.println("Connected to a network!");
    // Run the NTPC client. This function requests the UTC time from a server.
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
        // Modifies UTC depending on the selected time zone. 
        // After that the time is sent to the RTC and Time library.
        localTime = TZ.toLocal(timeClient.getEpochTime());
        setTime(localTime);
        bq32000.set(localTime);
    }
    setSyncProvider(RTC.get);   // Tells the time library from where to sink the time.
    setSyncInterval(1);         // Sync interval is in seconds.

    // If this function is not called or don't have a proper place in the code, one second dot status in time display mode will not work properly.
    stopScrollingDots();

    // RTC IRQ interrupt.
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, FALLING);
}

void loop() {
    checkForAPInvoke(); // Allows you to manually start the access point on demand. (By tapping the button 5 times in a rapid succesion)
    // When the button is pressed nixie tubes will change the displaying mode from time to date, and vice verse. 
    if (state >= 2) state = 0;
    switch (state) {
        case 0: // Display time.
            if(now() != prevTime) { // Update the display only if time has changed.
                prevTime = now();
                nixieTap.write_time(bq32000.get(), dot_state);
            }
            break;
        case 1: // Display date.
            nixieTap.write_date(bq32000.get(), 1);
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
        // This will run a new config portal if the conditions are met.
        startScrollingDots();
        // wifiManager.resetSettings();
        if(!wifiManager.startConfigPortal("NixieTap", "Nixie123")) {
            Serial.println("Failed to connect and hit timeout!");
            // Nixie display will show this error code:
            nixieTap.write(0, 0, 0, 2, 0);
            delay(3000);
        }
        // If you get here you have connected to the config portal.
        Serial.println("Connected to a new config portal!");
        tuchState = 0;
        stopScrollingDots();
    } else if((currentMillis - previousMillis) > 1000) tuchState = 0;
}

void startScrollingDots() {
    detachInterrupt(RTC_IRQ_PIN);
    bq32000.setIRQ(2);              // Configures the 512Hz interrupt from RTC.
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), scroll_dots, FALLING);
}

void stopScrollingDots() {
    detachInterrupt(RTC_IRQ_PIN);
    bq32000.setIRQ(1);              // Configures the 1Hz interrupt from RTC.
    attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
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