#ifndef _NIXIEAPI_h   /* Include guard */
#define _NIXIEAPI_h

#include <Arduino.h>
#include <ESP8266HTTPClient.h>  // This request has been included in this library: https://github.com/esp8266/Arduino/pull/2821
                                // If it is not included, Google Maps API would not work properly.
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#define MAX_CONNECTION_TIMEOUT 5000

#ifndef DEBUG
#define DEBUG
#endif // DEBUG

class NixieAPI {
    String UserAgent = "NixieTap";
    String timezonedbKey = ""; // You can get your key here: https://timezonedb.com
    String ipStackKey = ""; // You can get your key here: https://ipstack.com/
    String googleLocKey = ""; // You can get your key here: https://developers.google.com/maps/documentation/geolocation/get-api-key
    String googleTimeZoneKey = ""; // You can get your key here: https://developers.google.com/maps/documentation/timezone/get-api-key
    String googleTimeZoneCrt = "3A 93 DD B0 E6 91 AE 99 56 D2 23 F3 21 55 2C 13 05 AC 82 B0"; // Https fingerprint certification. Details at: https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/client-secure-examples.rst

public:
    NixieAPI();
    void applyKey(String key, uint8_t selectAPI);
    String getSurroundingWiFiJson();

    String getPublicIP();

    String getLocFromIpstack(String publicIP);
    String getLocFromGoogle();
    String getLocFromIpapi(String publicIP);
    String getEthPrice();
    
    int getTimeZoneOffsetFromGoogle(time_t now, String location, uint8_t *dst);
    int getTimeZoneOffsetFromIpstack(time_t now, String publicIP, uint8_t *dst); // This service must be paid. Which is the reason why I am not able test the code.
    int getTimeZoneOffsetFromTimezonedb(time_t now, String location, String ip, uint8_t *dst);
    
protected: 
    String MACtoString(uint8_t* macAddress);
};

extern NixieAPI nixieTapAPI;

#endif // _NIXIEAPI_h