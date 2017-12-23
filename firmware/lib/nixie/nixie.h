#ifndef NIXIE_H_   /* Include guard */
#define NIXIE_H_

#include "RTClib.h"

class Nixie
{
    static const uint8_t SPI_CS;
    static const uint16_t pinmap[11];

public:
    void init();
    void write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);
    void write_time(const DateTime& dt, uint8_t dot_state);
    void write_date(const DateTime& dt, uint8_t dot_state);

};

#endif // NIXIE_H_
