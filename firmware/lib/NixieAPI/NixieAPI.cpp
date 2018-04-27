#include "NixieAPI.h"

NixieAPI::NixieAPI() {
}

String NixieAPI::UrlEncode(const String url) {
  String e;
  for (int i = 0; i < url.length(); i++) {
    char c = url.charAt(i);
    if (c == 0x20) {
      e += "%20";            // "+" only valid for some uses
    } else if (isalnum(c)) {
      e += c;
    } else {
      e += "%";
      if (c < 0x10) e += "0";
      e += String(c, HEX);
    }
  }
  return e;
}
/*                                            *
 * Get IP location from Freegeoip API server. *
 *                                            */
String NixieAPI::getLocFromFreegeo() { // Using freegeoip.net to map public IP's location
    HTTPClient http;
    String URL = "http://freegeoip.net/json/"; // no host or IP specified returns client's public IP info
    String payload;
    String loc;
    http.setUserAgent(UserAgent);
    if(!http.begin(URL)) {
        Serial.println(F("getLocFromFreegeo: [HTTP] connect failed!"));
    } else {
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonBuffer jsonBuffer;
                JsonObject& root = jsonBuffer.parseObject(payload);
                if(root.success()) {
                    String region = root["region_name"];
                    String country = root["country_code"];
                    String lat = root["latitude"];
                    String lng = root["longitude"];
                    loc = lat + "," + lng;
                    Serial.println("Your IP location is: " + region + ", " + country + ". With cordintates: " + loc);
                } else {
                    Serial.println(F("getLocFromFreegeo: JSON parse failed!"));
                    Serial.println(payload);
                }
            } else {
                Serial.printf("getLocFromFreegeo: [HTTP] GET reply %d\r\n", stat);
            }
        } else {
            Serial.printf("getLocFromFreegeo: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
        }
    }
    http.end();
    return loc;
}
/*                                         *
 * Get IP location from Google API server. *
 *                                         */
String NixieAPI::getLocFromGoogle() { // using google maps API, return location for provided Postal Code
    HTTPClient http;
    String location = "";   // set to postal code to bypass geoIP lookup
    String URL = "https://maps.googleapis.com/maps/api/geocode/json?address=" + UrlEncode(location) + "&key=" + String(gMapsKey);
    String payload;
    String loc;
    http.setIgnoreTLSVerifyFailure(true);   // https://github.com/esp8266/Arduino/pull/2821
    http.setUserAgent(UserAgent);
    if(!http.begin(URL, gMapsCrt)) {
        Serial.println(F("getLocFromGoogle: [HTTP] connect failed!"));
    } else {
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonBuffer jsonBuffer;
                JsonObject& root = jsonBuffer.parseObject(payload);
                if(root.success()) {
                    JsonObject& results = root["results"][0];
                    JsonObject& results_geometry = results["geometry"];
                    String address = results["formatted_address"];
                    String lat = results_geometry["location"]["lat"];
                    String lng = results_geometry["location"]["lng"];
                    loc = lat + "," + lng;
                    Serial.print(F("Your IP location is: "));
                    Serial.println(address);
                } else {
                    Serial.println(F("getLocFromGoogle: JSON parse failed!"));
                    Serial.println(payload);
                }
            } else {
                Serial.printf("getLocFromGoogle: [HTTP] GET reply %d\r\n", stat);
            }
        } else {
            Serial.printf("getLocFromGoogle: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
        }
    }
    http.end();
    return loc;
}
/*                                *
 * Get Time Zone from Google API. *
 *                                */
int NixieAPI::getTimeZoneOffset(time_t now, String loc, uint8_t *dst) { // using google maps API, return TimeZone for provided timestamp
    HTTPClient http;
    int tz = false;
    String URL = "https://maps.googleapis.com/maps/api/timezone/json?location="
                + UrlEncode(loc) + "&timestamp=" + String(now) + "&key=" + String(gTimeZoneKey);
    String payload;
    http.setIgnoreTLSVerifyFailure(true);   // https://github.com/esp8266/Arduino/pull/2821
    http.setUserAgent(UserAgent);
    if(!http.begin(URL, gMapsCrt)) {
        Serial.println(F("getTimeZoneOffset: [HTTP] connect failed!"));
    } else {
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonBuffer jsonBuffer;
                JsonObject& root = jsonBuffer.parseObject(payload);
                if(root.success()) {
                    tz = int (root["rawOffset"]) / 60;  // Time Zone offset in minutes.
                    *dst = int (root["dstOffset"]) / 3600; // DST ih hours.
                    const char* tzname = root["timeZoneName"];
                    Serial.printf("Your Time Zone name is: %s (Offset from UTC: %d)\r\n", tzname, tz);
                    Serial.printf("Is DST(Daylight saving time) active at your location: %s", *dst == 1 ? "Yes (+1 hour)" : "No (+0 hour)");
                } else {
                    Serial.println(F("getTimeZoneOffset: JSON parse failed!"));
                    Serial.println(payload);
                }
            } else {
                Serial.printf("getTimeZoneOffset: [HTTP] GET reply %d\r\n", stat);
            }
        } else {
            Serial.printf("getTimeZoneOffset: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
        }
    }
    http.end();
    return tz;
}

NixieAPI nixieTapAPI = NixieAPI();