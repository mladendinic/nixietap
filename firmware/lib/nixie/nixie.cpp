#include <SPI.h>
#include <Arduino.h>
#include "Nixie.h"

const uint8_t Nixie::SPI_CS = D8;
const uint16_t Nixie::pinmap[11] =
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

/**
* Initialize the display
*
* This function configures pinModes based on .h file.
*/
void Nixie::init()
{
    // Set SPI chip select as output
    pinMode(SPI_CS, OUTPUT);

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

void Nixie::write_time(const DateTime& dt, uint8_t dot_state)
{
    Nixie::write(dt.hour()/10, dt.hour()%10, dt.minute()/10, dt.minute()%10, dot_state*0b1000);

}
void Nixie::write_date(const DateTime& dt, uint8_t dot_state)
{
    Nixie::write(dt.day()/10, dt.day()%10, dt.month()/10, dt.month()%10, dot_state*0b1000);

}
