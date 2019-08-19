#ifndef _NIXIE_h   /* Include guard */
#define _NIXIE_h

#include <Arduino.h>
#include <TimeLib.h>
#include <SPI.h>
#include <BQ32000RTC.h>

#define RTC_SDA_PIN D3
#define RTC_SCL_PIN D4
#define RTC_IRQ_PIN D1
#define SPI_CS D8
#define TOUCH_BUTTON D2
#define CONFIG_BUTTON D0

#ifndef DEBUG
    #define DEBUG
#endif // DEBUG

class Nixie {
    
    // Initialize the display. This function configures pinModes based on .h file.
    const uint16_t pinmap[11] =
    {
        0b0000010000, // 0
        0b0000100000, // 1
        0b0001000000, // 2
        0b0010000000, // 3
        0b0100000000, // 4
        0b1000000000, // 5
        0b0000000001, // 6
        0b0000000010, // 7
        0b0000000100, // 8
        0b0000001000, // 9
        0b0000000000  // digit off
    };
    String oldNumber = "";
    uint8_t numberArray[100], numIsNeg;
    int dotPos, numberSize, k = 0;
    unsigned long previousMillis = 0;
    uint8_t orderedDigits[10] = {1,6,2,7,5,0,4,9,8,3};
	uint8_t autoPoisonDoneOnMinute = 0;
	uint8_t oldDigit1, oldDigit2, oldDigit3, oldDigit4;
	bool animate = false;

public:
    Nixie();
    void begin();
    void write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);
    void writeNumber(String newNumber, unsigned int movingSpeed);
    void writeTime(time_t local, bool dot_state, bool timeFormat);
    void writeDate(time_t local, bool dot_state);
    uint8_t checkDate(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm);
	void antiPoison(time_t local);
	void setAnimation(bool animate);
private:
    void writeLowLevel(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);

};
	extern Nixie nixieTap;
#endif // _NIXIE_h
