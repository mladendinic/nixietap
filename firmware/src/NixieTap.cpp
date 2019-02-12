#include <Arduino.h>
#include <nixie.h>
#include <BQ32000RTC.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <Ticker.h>
// This service locates the nearest NTP server from your location and attempts to synchronize the date (UTC time) from it.
// Another option, if you live in the United States, is to use the NIST Internet Clock with the name of the server: "time.nist.gov".
#define NTP_SERVER "pool.ntp.org"

// When testing the code you should leave DEBUG defined if you want to see messages on the serial monitor.
#define DEBUG

void irq_1Hz_int();         // Interrupt function for changing the dot state every 1 second.
void buttonPressed();       // Interrupt function when button is pressed.
void enableSecDot();
void enableScrollDots(uint8_t movingSpeed);
void disableSecDot();
void scrollDots();          // Interrupt function for scrolling dots.

volatile bool dot_state = LOW;
bool resetDone = true, stopDef = false, secDotDef = false;
bool wifiFirstConnected = false, syncEventTriggered = false; // True if a time event has been triggered.
int16_t timeZone = 0, minutesTimeZone = 0;
uint16_t yearInt = 0;
uint8_t monthInt = 0, dayInt = 0, hoursInt = 0, minutesInt = 0, timeFormat = 1, weatherFormat = 1, dst = 0, firstSyncEvent = 1;
volatile uint8_t state = 0, tuchState = 0, dotPosition = 0b10, weatherRefreshFlag = 1, cryptoRefreshFlag = 1;
char WMTimeFormat[3] = "", WMYear[6] = "", WMMonth[4] = "", WMDay[4] = "", WMHours[4] = "", WMMinutes[4] = "", WMweatherFormat[3] = "";
char tzdbKey[50] = "", stackKey[50] = "", googleLKey[50] = "", googleTZkey[50] = "", currencyID[6] = "", weatherKey[50] = "";
unsigned long previousMillis = 0;
time_t prevTime = 0;        // The last time when the nixie tubes were sync. This prevents the change of the nixie tubes unless the time has changed.
String cryptoCurrencyPrice = "", temperature = "";

Ticker movingDot, priceRefresh, temperatureRefresh; // Initializing software timer interrupt called movingDot and priceRefresh.

void setup() {
    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(BUTTON), buttonPressed, RISING);

    movingDot.attach(0.2, scrollDots); // This is the software timer interrupt which calls function scrollDots every 0,4s.
}

void loop() {
    enableSecDot();
    
    // When the button is pressed nixie tubes will change the displaying mode from time to date, and vice verse. 
    if(state >= 3) state = 0;
    switch(state) {
        case 0 : // Display time.
                if(now() != prevTime) { // Update the display only if time has changed.
                    prevTime = now();
                    nixieTap.writeTime(now(), dot_state, timeFormat);
                }
                break;
        case 1 : // Display date.
                nixieTap.writeDate(now(), 1);
                break;
        case 2 : // Cryptocurrency price
                break;
       default:       
                #ifdef DEBUG
                    Serial.println("Error. Unknown state of a button!");
                #endif // DEBUG
                break;
    }
}


/*                                                           *
 *  Enables the center dot to change its state every second. *
 *                                                           */
void enableSecDot() {
    if(secDotDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(1);              // Configures the 512Hz interrupt from RTC.
        attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
        secDotDef = true;
        stopDef = false;
    }
}
/*                                                *
 * Disaling the dots function on nixie display.   *
 *                                                */
void disableSecDot() {
    if(stopDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(0);              // Configures the interrupt from RTC.
        dotPosition = 0b10;         // Restast dot position.
        stopDef = true;
        secDotDef = false;
    }
}
/*                                                                                       *
 * An interrupt function that changes the state and position of the dots on the display. *
 *                                                                                       */
void scrollDots() {
    if(dotPosition == 0b100000) dotPosition = 0b10;
    nixieTap.write(11, 11, 11, 11, dotPosition);
    dotPosition = dotPosition << 1;
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
void cryptoRefresh() {
    cryptoRefreshFlag = 1;
}
void weatherRefresh() {
    weatherRefreshFlag = 1;
}
