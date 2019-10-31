#ifndef _NIXIEAPI_h   /* Include guard */
#define _NIXIEAPI_h

#include <Arduino.h>
#include <ESP8266HTTPClient.h>  // This request has been included in this library: https://github.com/esp8266/Arduino/pull/2821
                                // If it is not included, Google Maps API would not work properly.
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>

#define MAX_CONNECTION_TIMEOUT 5000

#ifndef DEBUG
#define DEBUG
#endif // DEBUG

class NixieAPI {
    String UserAgent = "NixieTap";
    String timezonedbKey = "0"; // You can get your key here: https://timezonedb.com
    String ipStackKey = "0"; // You can get your key here: https://ipstack.com/
    String googleLocKey = "0"; // You can get your key here: https://developers.google.com/maps/documentation/geolocation/get-api-key
    String googleTimeZoneKey = "0"; // You can get your key here: https://developers.google.com/maps/documentation/timezone/get-api-key
    String openWeaterMapKey = "0"; // // You can get your key here: https://openweathermap.org/api
    const uint8_t googleTimeZoneCrt[20] = {0x5A, 0xCF, 0xFE, 0xF0, 0xF1, 0xA6, 0xF4, 0x5F, 0xD2, 0x11, 0x11, 0xC6, 0x1D, 0x2F, 0x0E, 0xBC, 0x39, 0x8D, 0x50, 0xE0}; // Https fingerprint certification. Details at: https://github.com/esp8266/Arduino/blob/master/doc/esp8266wifi/client-secure-examples.rst
    const uint8_t cryptoPriceCrt[20] =  {0x5A, 0xCF, 0xFE, 0xF0, 0xF1, 0xA6, 0xF4, 0x5F, 0xD2, 0x11, 0x11, 0xC6, 0x1D, 0x2F, 0x0E, 0xBC, 0x39, 0x8D, 0x50, 0xE0};  // Use web browser to view and copy, SHA1 fingerprint of the certificate
    const char * crypto_cert = "89:4A:D4:77:06:AE:58:86:EC:81:FE:C3:76:42:FB:8C:E4:96:75:5D";
    String location; // This allows us to save our location so that we can reuse it in the code, without the need to requesting it again from the server.
    String ip;
    unsigned long int prevObtainedIpTime;
public:

    NixieAPI();
    void applyKey(String key, uint8_t selectAPI);
    String getSurroundingWiFiJson();

    String getPublicIP();
    
    String getCryptoPrice(char * crypto_key, char * currencyID);
    String getTempAtMyLocation(String location, uint8_t format);
    
    String getLocFromIpstack(String publicIP);
    String getLocFromGoogle();
    String getLocFromIpapi(String publicIP);
    String getLocation();

    int getTimeZoneOffsetFromGoogle(time_t now, String location, uint8_t *dst);
    int getTimeZoneOffsetFromIpstack(time_t now, String publicIP, uint8_t *dst);    // This service must be paid. Which is the reason why I am not able test the code.
    int getTimeZoneOffsetFromTimezonedb(time_t now, String location, String ip, uint8_t *dst);
    int getTimezoneOffset(time_t now, uint8_t *dst);
    
protected: 
    String MACtoString(uint8_t* macAddress);
};

extern NixieAPI nixieTapAPI;

#endif // _NIXIEAPI_h