#ifndef _NIXIEAPI_h   /* Include guard */
#define _NIXIEAPI_h


#include <Arduino.h>
#include <ESP8266HTTPClient.h>  // This request has been included in this library: https://github.com/esp8266/Arduino/pull/2821
                                // If it is not included, Google Maps API would not work properly.
#include <ArduinoJson.h>

class NixieAPI {
    
    const char *UserAgent = "NixieTap";
    // openssl s_client -connect maps.googleapis.com:443 | openssl x509 -fingerprint -noout
    const char *gMapsCrt = "‎‎67:7B:99:A4:E5:A7:AE:E4:F0:92:01:EF:F5:58:B8:0B:49:CF:53:D4";
    const char *gMapsKey = "AIzaSyCuaNaMISYldK2xqIsBfkI3UaHvUonNlTs"; // You can get your key here: https://developers.google.com/maps/documentation/geolocation/get-api-key
    const char *gTimeZoneKey = "AIzaSyA33HiwHg1ejenWHW2SW-UAy0BGOuEzHHU"; // You can get your key here: https://developers.google.com/maps/documentation/timezone/get-api-key

public:
    NixieAPI();
    int getGoogleTimeZoneOffset(time_t now, String loc,  uint8_t *dst);
    String getLocFromFreegeo();
    String getLocFromGoogle();
    String UrlEncode(const String url);
};

#ifdef nixieTapAPI
#undef nixieTapAPI // workaround for Arduino Due, which defines "nixieTap"...
#endif

extern NixieAPI nixieTapAPI;

#endif // _NIXIEAPI_h