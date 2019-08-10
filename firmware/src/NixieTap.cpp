#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <nixie.h>

int i = 0;



void setup() {
	WiFi.mode(WIFI_STA);
}

void loop() {
    nixieTap.write(i, i, i, i,0b11110);
    delay(200);
    i++;
    if(i==10) i=0;
}

