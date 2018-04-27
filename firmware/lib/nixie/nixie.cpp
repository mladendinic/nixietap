#include "Nixie.h"

Nixie::Nixie() {
    begin();
}

void Nixie::begin()
{
    // Fire up the serial.
    Serial.begin(115200);
    // Turn off the Nixie tubes. If this is not called nixies might show some random stuff on startup.
    write(11, 11, 11, 11, 0);
    // Set SPI chip select as output
    pinMode(SPI_CS, OUTPUT);
    // Configure the ESP to receive interrupts from a RTC. 
    pinMode(RTC_IRQ_PIN, INPUT);
    // Initialise the integrated button in a NixieTap as a input. 
    pinMode(BUTTON, INPUT_PULLUP);
    // fire up the RTC
    RTC.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    RTC.setCharger(2);
    setSyncProvider(RTC.get);   // Tells the Time.h library from where to sink the time.
    setSyncInterval(60);        // Sync interval is in seconds.
}
/**
* Change the state of the nixie Display
*
* This function takes four input digits and one dot (encoded in uint8_t).
* Digits are updated via SPI, dots are updated via GPIO.
* Digits are MSB to LSB (digit1 = H1), dots take binary values: H1 dot is 0b1000,
* H0 dot is 0b100, etc.
* @param digit1 H1 digit value, 0-9, 10 is off
* @param digit2 H0 digit value, 0-9, 10 is off
* @param digit3 M1 digit value, 0-9, 10 is off
* @param digit4 M0 digit value, 0-9, 10 is off
* @param dots dot values, encoded in binary;
* (H1, H0, M1, M0) = (0b1000, 0b100, 0b10, 0b1)
*/
void Nixie::write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots)
{
    uint8_t part1, part2, part3, part4, part5, part6;
    // Display has 4 x 10 positions total, and SPI transfers 8 bits at the time.
    // We need to send it as 5 x 8 positions
    part1 = ~(pinmap[digit1] >> 2);
    part2 = ~(((pinmap[digit1] & 0b0000000011) << 6) | (pinmap[digit2] >> 4));
    part3 = ~(((pinmap[digit2] & 0b0000001111) << 4) | (pinmap[digit3] >> 6));
    part4 = ~(((pinmap[digit3] & 0b0000111111) << 2) | (pinmap[digit4] >> 8));
    part5 = ~(((pinmap[digit4] & 0b0011111111)));
    part6 = dots;
    // Transmit over SPI
    SPI.begin();
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(SPI_CS, LOW);
    SPI.transfer(part1);
    SPI.transfer(part2);
    SPI.transfer(part3);
    SPI.transfer(part4);
    SPI.transfer(part5);
    SPI.transfer(part6);
    digitalWrite(SPI_CS, HIGH);
    SPI.endTransaction();
}
void Nixie::write_time(time_t local, bool dot_state)
{
    Nixie::write(hour(local)/10, hour(local)%10, minute(local)/10, minute(local)%10, dot_state*0b1000);
}
void Nixie::write_date(time_t local, bool dot_state)
{
    Nixie::write(day(local)/10, day(local)%10, month(local)/10, month(local)%10, dot_state*0b1000);
}

Nixie nixieTap = Nixie();