#include <FS.h>
#include <Arduino.h>
#include <Nixie.h>
#include <NixieAPI.h>
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
void syncParameters();
void processSyncEvent(NTPSyncEvent_t ntpEvent);

volatile bool dot_state = LOW;
bool resetDone = true, stopDef = false, secDotDef = false, scrollDotsDef = false;
bool wifiFirstConnected = false, syncEventTriggered = false; // True if a time event has been triggered.
uint8_t dotsSpeed = 100, timeZone = 0, minutesTimeZone = 0, dst = 0;
volatile uint8_t state = 0, tuchState = 0, dotPosition = 0b10;
volatile uint16_t counter = 0;
char customYear[6] = "2016", customMonth[4] = "1", customDay[4] = "1", customHours[4] = "14", customMinutes[4] = "14";
unsigned long currentMillis = 0, previousMillis = 0;
time_t prevTime = 0;        // The last time when the nixie tubes were sync. This prevents the change of the nixie tubes unless the time has changed.

WiFiManager wifiManager;
// Initialization of parameters for manual configuration of time and date.
WiFiManagerParameter customText1("<h1><center>Manual time adjustment</center></h1>");
WiFiManagerParameter customText2("<p><b>Please fill in all fields with the current date and time: </b></p>");
WiFiManagerParameter yearWM("Year", "Year: ", customYear, 4);
WiFiManagerParameter monthWM("Month", "Month: (1-January,..., 12-December)", customMonth, 2);
WiFiManagerParameter dayWM("Day", "Day: (From 1 to 31)", customDay, 2);
WiFiManagerParameter hoursWM("Hours", "Hours: (24h format)", customHours, 2);
WiFiManagerParameter minutesWM("Minutes", "Minutes: ", customMinutes, 2);
WiFiManagerParameter customText3("<h1><center>Configuration of API Key</center></h1>");
WiFiManagerParameter customText4("<p><b><center>Work in progress...</center></b></p>");

NTPSyncEvent_t ntpEvent;    // Last triggered event.

void setup() {
    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, FALLING);
    // WiFiManager. For configuring WiFi access point, setting up the NixieTap parameters and so on...
    // Sets timeout(in seconds) until configuration portal gets turned off.
    wifiManager.setConfigPortalTimeout(600);
    // Adding parameters to Settings window in WiFiManager AP.
    wifiManager.addParameter(&customText1);
    wifiManager.addParameter(&customText2);
    wifiManager.addParameter(&yearWM);
    wifiManager.addParameter(&monthWM);
    wifiManager.addParameter(&dayWM);
    wifiManager.addParameter(&hoursWM);
    wifiManager.addParameter(&minutesWM);
    wifiManager.addParameter(&customText3);
    wifiManager.addParameter(&customText4);

    // Determining the look of the WiFiManager Web Server, which buttons will be visible on a main tab.
    std::vector<const char *> menu = {"wifi","param","info","sep","erase","exit"};
    wifiManager.setMenu(menu);

    // Fetches ssid and pass from eeprom and tries to connect,
    // if it does not connect it starts an access point with the specified name "NixieTapAP"
    // and goes into a blocking loop awaiting configuration.
    if(!wifiManager.autoConnect("NixieTap", "Nixie123")) {
        Serial.println("Failed to connect or AP is manually closed!");
        // If the NixieTap is not connected to WiFi, it will collect the entered parameters and configure the RTC according to them.
        wifiFirstConnected = false;
        syncParameters();
    } else {
        Serial.println("NixieTap is connected to WiFi!");
        wifiFirstConnected = true;
    }
}

void loop() {
    if(wifiFirstConnected) {
        wifiFirstConnected = false;
        // Configuring NTP server.
        NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {ntpEvent = event; syncEventTriggered = true;});
        // While time is not first adjusted yet, synchronization will be attempted every 60 seconds. When first sync is done, UTC time will be sync every 24 hours.
        NTP.setInterval(60, 3600);
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
            disableDots(); // If dots are not disabled, precisely RTC_IRQ_PIN interrupt, ConfigPortal will chrach.
            nixieTap.write(10, 10, 10, 10, 0);
            wifiManager.setConfigPortalTimeout(600);
            // This will run a new config portal if the conditions are met.
            if(!wifiManager.startConfigPortal("NixieTap", "Nixie123")) {
                Serial.println("Failed to connect or AP is manually closed!");
                // If the NixieTap is not connected to WiFi, it will collect the entered parameters and configure the RTC according to them.
                wifiFirstConnected = false;
                syncParameters();
            } else {
                Serial.println("NixieTap is connected to WiFi!");
                wifiFirstConnected = true;
            }
            enableSecondsDot();
        }
    } else if((currentMillis - previousMillis) > 1000) tuchState = 0;
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
        timeZone = nixieTapAPI.getGoogleTimeZoneOffset(now(), nixieTapAPI.getLocFromFreegeo(), &dst);
        NTP.setTimeZone((timeZone/60), (timeZone%60));
        NTP.setDayLight(dst);
        RTC.set(now());
    }
}

void syncParameters() {
    // Store updated parameters to local memory.
    strcpy(customYear, yearWM.getValue());
    strcpy(customMonth, monthWM.getValue());
    strcpy(customDay, dayWM.getValue());
    strcpy(customHours, hoursWM.getValue());
    strcpy(customMinutes, minutesWM.getValue());
    // Convert parameters from char to int.
    uint16_t customYearInt = atoi(customYear);
    uint8_t customMonthInt = atoi(customMonth);
    uint8_t customDayInt = atoi(customDay);
    uint8_t customHoursInt = atoi(customHours);
    uint8_t customMinutesInt = atoi(customMinutes);
    // Check if the parameters are changed. If so, then enter in manual date and time configuration mode.
    if (customYearInt != 2016 || customMonthInt != 1 || customDayInt != 1 || customHoursInt != 14 || customMinutesInt != 14) {
        // Basic check if entered data. It is not precise, there is still room for improvement.
        if(customYearInt >= 2000 && (customMonthInt >= 1 && customMonthInt <= 12) && (customDayInt >= 1 && customDayInt <= 31) && (customHoursInt >= 0 && customHoursInt <= 23) && (customMinutesInt >= 0 && customMinutesInt <= 59)) {
            setTime(customHoursInt, customMinutesInt, 0, customDayInt, customMonthInt, customYearInt);
            RTC.set(now());
            NTP.stop();     // NTP sync is disableded to avoid sync errors.
            enableSecondsDot();
        } else {
            Serial.println("Incorect date and time parameters! Please restart your device or tap 5 times on NixieTap case and try again.");
        }
    }
}
/*                                                           *
 *  Enables the center dot to change its state every second. *
 *                                                           */
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
// <div class="dropdown">
//         <div class="dropdown-selected"><span id="selected-value">Please select...</span></div>
//         <span class="dropdown-select-btn" onclick="sd('selects');return false;">▾</span>
//         <ul id="selects">
//             <li onclick="c(&quot;{v}&quot;)">{v}<i>{i}<b>{r}%</b></i></li>
//             <li onclick="c(&quot;{v}&quot;)">{v}<i><b>100%</b></i></li>
//             <li onclick="c(&quot;Vila17&quot;)">Vila17<i><b>95%</b></i></li>
//             <li onclick="c(&quot;LilRidingHoodz&quot;)">LilRidingHoodz<i><b>100%</b></i></li>
//             <li onclick="c(&quot;Marcelica&quot;)">Marcelica<i><b>100%</b></i></li>
//             <li onclick="c(&quot;PMica&quot;)">PMica<i><b>100%</b></i></li>
//         </ul>
//         <div style="clear:both;"></div>
//     </div>