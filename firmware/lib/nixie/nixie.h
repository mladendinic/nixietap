#ifndef NIXIE_H_   /* Include guard */
#define NIXIE_H_

class nixie
{
    static const uint8_t H1_DOT;
    static const uint8_t H0_DOT;
    static const uint8_t M1_DOT;   // CHECK IN HARDWARE!!!!
    static const uint8_t M0_DOT;    // CHECK IN HARDWARE!!!
    static const uint8_t PSU_EN;
    static const uint8_t SPI_CS;
    static const uint16_t pinmap[11];

    public:
        nixie(void);
        void init(void);
        void write(
            uint8_t digit1,
            uint8_t digit2,
            uint8_t digit3,
            uint8_t digit4,
            uint8_t dots);

};

#endif // NIXIE_H_
