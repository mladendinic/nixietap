#ifndef _NIXIE_h   /* Include guard */
#define _NIXIE_h

#include <TimeLib.h>

class Nixie
{
    static const uint8_t SPI_CS;
    static const uint16_t pinmap[11];

public:
    static void init();
    static void write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots);
    static void write_time(time_t local, bool dot_state);
    static void write_date(time_t local, bool dot_state);

};

#endif // _NIXIE_h
