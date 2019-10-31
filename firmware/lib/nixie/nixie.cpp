#include "nixie.h"

Nixie::Nixie() {
    begin();
}

void Nixie::begin()
{
    // Fire up the serial if DEBUG is defined.
    #ifdef DEBUG
        Serial.begin(115200);
    #endif // DEBUG
    // Turn off the Nixie tubes. If this is not called nixies might show some random stuff on startup.
    write(11, 11, 11, 11, 0);
    // Set SPI chip select as output
    pinMode(SPI_CS, OUTPUT);
    // Configure the ESP to receive interrupts from a RTC. 
    pinMode(RTC_IRQ_PIN, INPUT);
    // Initialise the integrated button in a NixieTap as a input. 
    pinMode(TOUCH_BUTTON, INPUT_PULLUP);
    pinMode(CONFIG_BUTTON, INPUT_PULLDOWN_16);
    // fire up the RTC
    RTC.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    RTC.setCharger(2);
    setSyncProvider(RTC.get);   // Tells the Time.h library from where to sink the time.
    setSyncInterval(60);        // Sync interval is in seconds.
}

uint8_t Nixie::checkDate(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm) {
    if(y >= 1971 && y <= 9999) { // Check year.
        if(m >= 1 && m <= 12) {  // Check month.
            // Check days.
            if((d >= 1 && d <= 31) && (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12) && (h >= 0 && h <= 23) && ( mm >= 0 && mm <= 59))
                return 1;
            else if((d >= 1 && d <= 30) && (m == 4 || m == 6 || m == 9 || m == 11) && (h >= 0 && h <= 23) && ( mm >= 0 && mm <= 59))
                return 1;
            else if((d >= 1 && d <= 28) && (m == 2))
                return 1;
            else if(d == 29 && m == 2 && (y%400 == 0 ||(y%4 == 0 && y%100 != 0)) && (h >= 0 && h <= 23) && ( mm >= 0 && mm <= 59))
                return 1;
            else
                return 0;
        } else
            return 0;
    } else
        return 0;
}
/*                                                                          *
 *  Change the state of the nixie Display                                   *
 *                                                                          *
 *  This function takes four input digits and one dot (encoded in uint8_t). *
 *  Digits are updated via SPI, dots are updated via GPIO.                  *
 *  Digits are MSB to LSB (digit1 = H1), dots take binary values:           *
 *  H1 dot is 0b1000, H0 dot is 0b100, etc.                                 *
 *  @param digit1 H1 digit value, 0-9, 10 is off                            *
 *  @param digit2 H0 digit value, 0-9, 10 is off                            *
 *  @param digit3 M1 digit value, 0-9, 10 is off                            *
 *  @param digit4 M0 digit value, 0-9, 10 is off                            *
 *  @param dots dot values, encoded in binary;                              *
 *  (H1, H0, M1, M0) = (0b10, 0b100, 0b1000, 0b10000)                       *
 *                                                                          */



void Nixie::writeLowLevel(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots)
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




/*                                                         *
 * With this function, time is displayed on a nixie tubes. *
 *                                                         */
void Nixie::writeTime(time_t local, bool dot_state, bool timeFormat) {   
	antiPoison(local, timeFormat);
    if(timeFormat) {
        write(hour(local)/10, hour(local)%10, minute(local)/10, minute(local)%10, dot_state*0b1000);
    } else {
        write(hourFormat12(local)/10, hourFormat12(local)%10, minute(local)/10, minute(local)%10, dot_state*0b1000);
    }
    k = 0; // Reset the number position in the writeNumber function.
}
/*                                                         *
 * With this function, date is displayed on a nixie tubes. *
 *                                                         */
void Nixie::writeDate(time_t local, bool dot_state) {
    write(day(local)/10, day(local)%10, month(local)/10, month(local)%10, dot_state*0b1000);
    k = 0; // Reset the number position in the writeNumber function.
}
/*                                                                                                                                   *
 * With this function you can display random numbers(int or float, negative or positive) longer or shorter then four digits          *
 * and also set their scrolling speed(if the speed is zero number is stationed).                                                     *
 * Function accepts number in string form and then transfers it in to the int array with memorised position of a dot(if it exists).  *
 * Max size number, including integer and decimal part, is 100 digits. If you need to display longer number,                         *
 * you can easily modify number Array size in Nixie.h file.                                                                          *
 *                                                                                                                                   */
void Nixie::writeNumber(String newNumber, unsigned int movingSpeed) {
    if(newNumber != oldNumber) {
        k = 0; // Reset the number position.
        oldNumber = newNumber;
        String number = newNumber;
        #ifdef DEBUG
            Serial.println("---------------------------------------------------------------------------------------------");
            Serial.println("Number to display is: " + number);
        #endif // DEBUG
        number.trim(); // Get a version of the string with any leading and trailing whitespace removed.
        if(number.startsWith("-")) {
            numIsNeg = 1;
            number.remove(0, 1); // Remove minus from string.
            #ifdef DEBUG
                Serial.println("Number is negative!");
            #endif // DEBUG
        } else numIsNeg = 0;
        numberSize = number.length() + 8; // For a simplicity of showing numbers on Nixies, we add four NULL(number 10 in this case) numbers before and after the real number.
        dotPos = number.indexOf('.');
        if(dotPos != -1) { // If the number is float type, we will replace the dot with the following number. So the whole size of the number will be reduced by one. Example: 1.23 -> 123
            numberSize = numberSize - 1;
            dotPos = dotPos + 4; // But we will remember the exact position where the point was.
        }
        #ifdef DEBUG
            Serial.println("Number after trimming: " + number);
            Serial.printf("Size of a number(including dot(if exists) and 8 added numbers) is: %d", numberSize);
            Serial.printf("\nDot position is(-1 = dot does not exists): %d\n", dotPos);
        #endif // DEBUG
        for(int i = 0; i < numberSize; i++) {
            if(i >= 0 && i < 4) {
                numberArray[i] = 10;
            } else if(i >= 4 && i < numberSize - 4) {
                if((int(number.charAt(i - 4)) >= 48 && int(number.charAt(i - 4)) <= 57) || int(number.charAt(i - 4)) == 46) {
                    if((i < dotPos) || dotPos == -1) {
                        numberArray[i] = int(number.charAt(i - 4)) - 48;
                    } else if(i >= dotPos) {
                        numberArray[i] = int(number.charAt(i - 3)) - 48; // this way we skip the dot place and replace it with the next number.
                    }
                } else {
                    #ifdef DEBUG
                        Serial.println("Error in the function writeNumber! Reason: Given string is not a number.");
                    #endif // DEBUG
                    break;
                }
            } else {
                numberArray[i] = 10;
            }  
        }
        #ifdef DEBUG
            Serial.println("An array of numbers is created from a string:");
            for(int i = 0; i < numberSize; i++) {
                Serial.printf("%d: %d\t", i + 1, numberArray[i]);
            }
            Serial.println();
        #endif // DEBUG
    }
    if(k < (numberSize - 4)) { // Since we, in the function write(), display four digits at the same time, we have to make up for it by reducing nuber k.
        if(movingSpeed > 0) {
            if(millis() - previousMillis >= movingSpeed) { // Determining how fast the number will scroll.
                previousMillis = millis();
                #ifdef DEBUG
                    // Serial.printf("\nNow writting this digits on Nixies: \n1. -> %d, 2. -> %d, 3. -> %d, 4. -> %d \n", numberArray[k], numberArray[k + 1], numberArray[k + 2], numberArray[k + 3]);
                #endif // DEBUG
                if((dotPos - k >= 0)  && (dotPos - k <= 3)) { //If the number is decimal, the decimal point will be displayed when these factors are met.
                    write(numberArray[k], numberArray[k + 1], numberArray[k + 2], numberArray[k + 3], (0b1 << (dotPos - k + 1)) | (((0b1 & numIsNeg) * ((k + 4 > 4 )&&(k + 4 < 9))) << (5 - k)));
                } else {
                    write(numberArray[k], numberArray[k + 1], numberArray[k + 2], numberArray[k + 3], 0 | (((0b1 & numIsNeg) * ((k + 4 > 4 )&&(k + 4 < 9))) << (5 - k)));
                }
                k++;
            }
        } else if(movingSpeed == 0) {
            if(numberSize > 12) {
                #ifdef DEBUG
                    Serial.println("Number is longer than 4 digits! It can not be completely displayed on the nixie screen.");
                #endif // DEBUG
            } else
                write(numberArray[4], numberArray[5], numberArray[6], numberArray[7], (0b1 << (dotPos - 3)) | (0b10 * numIsNeg));
        } else {
            #ifdef DEBUG
                Serial.println("Wrong value of movingSpeed. Speed of movement is not recognized.");
            #endif // DEBUG
        }
    }
    if(k >= (numberSize - 4)) k = 0;
}

void Nixie::antiPoison(time_t local, bool timeFormat) {

	uint8_t stopH1=0, stopH0=0, stopM1=0, stopM0=0;
	uint8_t H1=0, H0=0, M1=0, M0=0;
	bool foundH1=false, foundH0=false, foundM1=false, foundM0=false;
	uint8_t indexH1=0, indexH0=0, indexM1=0, indexM0=0;
	stopM1 = minute(local)/10;
	stopM0 = minute(local)%10;

	if (timeFormat) {
		stopH1 = hour(local)/10;
		stopH0 = hour(local)%10;
	} else {
		stopH1 = hourFormat12(local)/10;
		stopH0 = hourFormat12(local)%10;
	}

	for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == stopM1) indexM1 = i;
	for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == stopM0) indexM0 = i;
	for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == stopH1) indexH1 = i;
	for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == stopH0) indexH0 = i;

	if(stopM0 != autoPoisonDoneOnMinute) {
	autoPoisonDoneOnMinute = stopM0;			

//		for(uint8_t slotCount=0; slotCount<4; slotCount++) {
//
//			for(uint8_t j=0; j<10; j++) {
//				
//				if (!foundM0) {
//					if((j+indexM0) > 9) M0 = orderedDigits[j+indexM0-10];
//					else M0 = orderedDigits[j+indexM0];
//				}
//				if (!foundM1) {
//					if((j+indexM1) > 9) M1 = orderedDigits[j+indexM1-10];
//					else M1 = orderedDigits[j+indexM1];
//				}
//				if (!foundH0) {
//					if((j+indexH0) > 9) H0 = orderedDigits[j+indexH0-10];
//					else H0 = orderedDigits[j+indexH0];
//				}
//				if (!foundH1) {
//					if((j+indexH1) > 9) H1 = orderedDigits[j+indexH1-10];
//					else H1 = orderedDigits[j+indexH1];
//				}
//
//				write(H1,H0,M1,M0,0b11110);
//				delay(25);
//			}
//
//			for(uint8_t j=10; j>0; j--) {
//				if (!foundM0) M0 = orderedDigits[j-1];
//				if((slotCount==0) && (M0 == stopM0)) foundM0 = true;
//
//				if (!foundM1) M1 = orderedDigits[j-1];
//				if((slotCount==1) && (M1 == stopM1)) foundM1 = true;
//
//				if (!foundH0) H0 = orderedDigits[j-1];
//				if((slotCount==2) && (H0 == stopH0)) foundH0 = true;
//
//				if (!foundH1) H1 = orderedDigits[j-1];
//				if((slotCount==3) && (H1 == stopH1)) foundH1 = true;
//
//				write(H1,H0,M1,M0,0b11110);
//				delay(25);
//			}

//		}

		for(uint8_t slotCount=0; slotCount<4; slotCount++) {

			for(uint8_t j=0; j<10; j++) {
				
				if (!foundM0) {
					if((j+indexM0) > 9) M0 = orderedDigits[j+indexM0-10];
					else M0 = orderedDigits[j+indexM0];
				}
				if (!foundM1) {
					if((j+indexM1) > 9) M1 = orderedDigits[j+indexM1-10];
					else M1 = orderedDigits[j+indexM1];
				}
				if (!foundH0) {
					if((j+indexH0) > 9) H0 = orderedDigits[j+indexH0-10];
					else H0 = orderedDigits[j+indexH0];
				}
				if (!foundH1) {
					if((j+indexH1) > 9) H1 = orderedDigits[j+indexH1-10];
					else H1 = orderedDigits[j+indexH1];
				}

				delay(25);

				if (slotCount==0) write(stopH1,stopH0,stopM1,M0,0b10000);
				if (slotCount==1) write(stopH1,stopH0,M1,stopM0,0b01000);
				if (slotCount==2) write(stopH1,H0,stopM1,stopM0,0b00100);
				if (slotCount==3) write(H1,stopH0,stopM1,stopM0,0b00010);
			}

			for(uint8_t j=10; j>0; j--) {
				if (!foundM0) M0 = orderedDigits[j-1];
				if((slotCount==0) && (M0 == stopM0)) foundM0 = true;

				if (!foundM1) M1 = orderedDigits[j-1];
				if((slotCount==1) && (M1 == stopM1)) foundM1 = true;

				if (!foundH0) H0 = orderedDigits[j-1];
				if((slotCount==2) && (H0 == stopH0)) foundH0 = true;

				if (!foundH1) H1 = orderedDigits[j-1];
				if((slotCount==3) && (H1 == stopH1)) foundH1 = true;

				if (slotCount==0) write(stopH1,stopH0,stopM1,M0,0b10000);
				if (slotCount==1) write(stopH1,stopH0,M1,stopM0,0b01000);
				if (slotCount==2) write(stopH1,H0,stopM1,stopM0,0b00100);
				if (slotCount==3) write(H1,stopH0,stopM1,stopM0,0b00010);
				delay(25);
			}

		}
	}
}

void Nixie::setAnimation(bool animate) {
	animate = animate;
}

void Nixie::write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots)
{
	uint8_t H1=0, H0=0, M1=0, M0=0;
	bool foundH1=false, foundH0=false, foundM1=false, foundM0=false;
	uint8_t indexH1=0, indexH0=0, indexM1=0, indexM0=0;

	if(animate) {
		animate=false;
		Serial.println("animiramo");
		for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == oldDigit4) indexM1 = i;
		for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == oldDigit3) indexM0 = i;
		for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == oldDigit2) indexH1 = i;
		for(uint8_t i=0; i<10; i++) if(orderedDigits[i] == oldDigit1) indexH0 = i;

		for(uint8_t j=0; j<10; j++) {
			
			if (!foundM0) {
				if((j+indexM0) > 9) M0 = orderedDigits[j+indexM0-10];
				else M0 = orderedDigits[j+indexM0];
				if (M0 == digit4) {foundM0=true; continue;}
			}
			if (!foundM1) {
				if((j+indexM1) > 9) M1 = orderedDigits[j+indexM1-10];
				else M1 = orderedDigits[j+indexM1];
				if (M1 == digit3) {foundM1=true; continue;}
			}
			if (!foundH0) {
				if((j+indexH0) > 9) H0 = orderedDigits[j+indexH0-10];
				else H0 = orderedDigits[j+indexH0];
				if (H0 == digit2) {foundH0=true; continue;}
			}
			if (!foundH1) {
				if((j+indexH1) > 9) H1 = orderedDigits[j+indexH1-10];
				else H1 = orderedDigits[j+indexH1];
				if (H1 == digit1) {foundH1=true; continue;}
			}

			write(H1,H0,M1,M0,0b11110);
			delay(25);
			}

		oldDigit1 = digit1;
		oldDigit2 = digit2;
		oldDigit3 = digit3;
		oldDigit4 = digit4;
	}
	else writeLowLevel(digit1, digit2, digit3, digit4, dots);

}

Nixie nixieTap = Nixie();
