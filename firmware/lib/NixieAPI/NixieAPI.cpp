#include "NixieAPI.h"
#include <WiFiClientSecure.h>

NixieAPI::NixieAPI() {
}
void NixieAPI::applyKey(String key, uint8_t selectAPI) {
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
    #endif // DEBUG
    switch(selectAPI) {
        case 0 : 
                timezonedbKey = key;
                #ifdef DEBUG
                    Serial.println("applyKey successful, timezonedb key is: " + timezonedbKey);
                #endif // DEBUG
                break;
        case 1 : 
                ipStackKey = key;
                #ifdef DEBUG
                    Serial.println("applyKey successful, ipstack key is:  " + ipStackKey);
                #endif // DEBUG
                break;
        case 2 : 
                googleLocKey = key;
                #ifdef DEBUG
                    Serial.println("applyKey successful, Google location key is:  " + googleLocKey);
                #endif // DEBUG
                break;
        case 3 : 
                googleTimeZoneKey = key;
                #ifdef DEBUG
                    Serial.println("applyKey successful, Google timezone key is:  " + googleTimeZoneKey);
                #endif // DEBUG
                break;
        case 4 : 
                openWeaterMapKey = key;
                #ifdef DEBUG
                    Serial.println("applyKey successful, OpenWeaterMap key is:  " + openWeaterMapKey);
                #endif // DEBUG
                break;
        default: 
                Serial.println("Unknown value of selectAPI!");
                break;
    }
}
String NixieAPI::MACtoString(uint8_t* macAddress) {
    char macStr[18] = { 0 };
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    return String(macStr);
}
/*                                                          *
 *  Function to get a list of surrounding WiFi signals in   *
 *  JSON format to get location via Google Location API.    *
 *                                                          */
String NixieAPI::getSurroundingWiFiJson() {
    String wifiArray = "[\n";
    int8_t numWifi = WiFi.scanNetworks();
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
        Serial.println(String(numWifi) + " WiFi networks found.");
    #endif // DEBUG
    for(uint8_t i = 0; i < numWifi; i++) {
        // Serial.print("WiFi.BSSID(i) = ");
        // Serial.println((char *)WiFi.BSSID(i));
        wifiArray += "{\"macAddress\":\"" + MACtoString(WiFi.BSSID(i)) + "\",";
        wifiArray += "\"signalStrength\":" + String(WiFi.RSSI(i)) + ",";
        wifiArray += "\"channel\":" + String(WiFi.channel(i)) + "}";
        if (i < (numWifi - 1)) {
            wifiArray += ",\n";
        }
    }
    WiFi.scanDelete();
    wifiArray += "]";
    #ifdef DEBUG
        Serial.println("WiFi list :\n" + wifiArray);
    #endif // DEBUG
    return wifiArray;
}
/*                                                        *
 *  Calls the ipify API to get a public IP address.       *
 *  If that fails, it tries the same with the seeip API.  * 
 *  https://www.ipify.org/   https://seeip.org/           *
 *                                                        */
String NixieAPI::getPublicIP() {
    // If the IP does not exist or more than an hour has passed since the last request, ip will be re-requested from the API service.
    if(ip == "" || ip == "0" || ((millis() - prevObtainedIpTime) >= 3600000)) {
        prevObtainedIpTime = millis();
        WiFiClient client;
        String headers = "", body = "";
        bool finishedHeaders = false, currentLineIsBlank = false, gotResponse = false, bodyStarts = false, bodyEnds = false;
        long timeout;
        #ifdef DEBUG
            Serial.println("---------------------------------------------------------------------------------------------");
        #endif // DEBUG
        if(client.connect("api.ipify.org", 80)) {
            #ifdef DEBUG
                Serial.println("Connected to ipify.org!");
            #endif // DEBUG
            client.print("GET /?format=json HTTP/1.1\r\nHost: api.ipify.org\r\n\r\n");
            timeout = millis() + MAX_CONNECTION_TIMEOUT;
            // checking the timeout
            while(client.available() == 0) {
                if(timeout - millis() < 0) {
                    #ifdef DEBUG
                        Serial.println("Client Timeout!");
                    #endif // DEBUG
                    client.stop();
                    break;
                }
            }
            if(client.available()) {
                //marking we got a response
                gotResponse = true;
            }
        } 
        if(!gotResponse && client.connect("ip.seeip.org", 80)) {
            #ifdef DEBUG
                Serial.println("Failed to fetch public IP address from ipify.org! Now trying with seeip.org.");
                Serial.println("Connected to seeip.org!");
            #endif // DEBUG
            client.print("GET /json HTTP/1.1\r\nHost: ip.seeip.org\r\n\r\n");
            timeout = millis() + MAX_CONNECTION_TIMEOUT;
            // checking the timeout
            while(client.available() == 0) {
                if(timeout - millis() < 0) {
                    #ifdef DEBUG
                        Serial.println("Client Timeout!");
                    #endif // DEBUG
                    client.stop();
                    return "0";
                }
            }
            if(client.available()) {
                //marking we got a response
                gotResponse = true;
            }
        } 
        if(gotResponse) {
            while(client.available()) {
                char c = client.read();
                if(finishedHeaders) {   // Separate json file from rest of the received response.
                    if(c == '{') {      // If this additional filtering is not performed. seeip.org json response will not be accepted by ArduinoJson lib.
                        bodyStarts = true;
                    }
                    if(bodyStarts && !bodyEnds) {
                        body = body + c;
                    }
                    if(c == '}') {
                        bodyEnds = true;
                    }
                } else {
                    if(currentLineIsBlank && c == '\n') {
                        finishedHeaders = true;
                    }
                    else {
                        headers = headers + c;
                    }
                }
                if(c == '\n') {
                    currentLineIsBlank = true;
                } else if(c != '\r') {
                    currentLineIsBlank = false;
                }
            }
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, body);
            if(!error) {
                ip = doc["ip"].as<String>();
                #ifdef DEBUG
                    Serial.println("Your Public IP location is: " + ip);
                #endif // DEBUG
                return ip;
            } else {
                #ifdef DEBUG
                    Serial.print("Failed to deserialize JSON, error code: ");
                    Serial.println(error.c_str());
                #endif // DEBUG
                return "0";
            }
        } else {
            #ifdef DEBUG
                Serial.println("Failed to fetch public IP address from ipify.org!");
                Serial.println("Failed to obtain a public IP address from any API!");
            #endif // DEBUG
            return "0";
        }
    }
    return ip;
}
/*                                                    *
 *  Calls IPStack API to get latitude and longitude   *
 *  coordinates relative to your public IP location.  *
 *  The API can automatically detect your public IP   *
 *  address without the need to send it.              *
 *  Free up to 10.000 request per month.              *
 *  https://ipstack.com/                              *
 *                                                    */
String NixieAPI::getLocFromIpstack(String publicIP) {
    WiFiClient client;
    HTTPClient http;
    String payload = "";
    if(publicIP == "") {
        publicIP = "check";
    }
    String URL = "http://api.ipstack.com/" + publicIP + "?access_key=" + ipStackKey + "&output=json&fields=country_name,region_name,city,latitude,longitude";
    http.setUserAgent(UserAgent);
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
    #endif // DEBUG
    if(!http.begin(client, URL)) {
        #ifdef DEBUG
            Serial.println(F("getLocFromIpstack: Connection failed!"));
        #endif // DEBUG
        return "0";
    } else {
        #ifdef DEBUG
            Serial.println("Connected to api.ipstack.com!");
        #endif // DEBUG
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                if(!error) {
                    String country = doc["country_name"];
                    String region = doc["region_name"];
                    String city = doc["city"];
                    String lat = doc["latitude"];
                    String lng = doc["longitude"];
                    location = lat + "," + lng;
                    #ifdef DEBUG
                        Serial.print("Your IP location is: " + country + ", " + region + ", " + city + ". ");
                        Serial.println("With coordinates: latitude: " + lat + ", " + "longitude: " + lng);
                    #endif // DEBUG
                } else {
                    #ifdef DEBUG
                        Serial.println(F("getLocFromIpstack: JSON deserialization failed, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG
                    return "0";
                }
            } else {
                #ifdef DEBUG
                    Serial.printf("getLocFromIpstack: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
                return "0";
            }
        } else {
            #ifdef DEBUG
                Serial.printf("getLocFromIpstack: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
            #endif // DEBUG
            return "0";
        }
    }
    http.end();

    return location;
}
/*                                                                     *
 *  Calls Google Location API to get current location using            *
 *  surrounding WiFi signals information.                              *
 *  You get $200 free usage every month for Maps, Routes, or Places.   *
 *  https://developers.google.com/maps/documentation/geolocation/intro *
 *                                                                     */
String NixieAPI::getLocFromGoogle() {
    WiFiClientSecure client;
    String lat = "", lng = "", accuracy = "";
    String headers = "", hull = "", response = "";
    bool finishedHeaders = false, currentLineIsBlank = false, gotResponse = false;
    const char* googleLocApiHost = "www.googleapis.com";
    const char* googleLocApiUrl = "/geolocation/v1/geolocate";
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
    #endif // DEBUG
    if(client.connect(googleLocApiHost, 443)) {
        #ifdef DEBUG
            Serial.println("Connected to Google Location API endpoint!");
        #endif // DEBUG
    } else {
        #ifdef DEBUG
            Serial.println("getLocFromGoogle: HTTPS error!");
        #endif // DEBUG
        return "0";
    }
    String body = "{\"wifiAccessPoints\":" + getSurroundingWiFiJson() + "}";
    #ifdef DEBUG
        Serial.println("requesting URL: " + String(googleLocApiUrl) + "?key=" + googleLocKey);
    #endif // DEBUG
    String request = String("POST ") + String(googleLocApiUrl);
    if(googleLocKey != "") 
        request += "?key=" + googleLocKey;
    request += " HTTP/1.1\r\n";
    request += "Host: " + String(googleLocApiHost) + "\r\n";
    request += "User-Agent: " + String(UserAgent) + "\r\n";
    request += "Content-Type:application/json\r\n";
    request += "Content-Length:" + String(body.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += body;
    #ifdef DEBUG
        Serial.println("Request: \n" + request);
    #endif // DEBUG
    client.println(request);
    #ifdef DEBUG
        Serial.println("getLocFromGoogle: Request sent!");
    #endif // DEBUG
    // Wait for response
    long timeout = millis() + MAX_CONNECTION_TIMEOUT;
    // checking the timeout
    while(client.available() == 0) {
        if(timeout - millis() < 0) {
            #ifdef DEBUG
                Serial.println("getLocFromGoogle: Client Timeout!");
            #endif // DEBUG
            client.stop();
            break;
        }
    }
    while(client.available()) {
        char c = client.read();
        if(finishedHeaders) {
            hull = hull + c;
        } else {
            if(currentLineIsBlank && c == '\n') {
                finishedHeaders = true;
            }
            else {
                headers = headers + c;
            }
        }
        if(c == '\n') {
            currentLineIsBlank = true;
        } else if(c != '\r') {
            currentLineIsBlank = false;
        }
        gotResponse = true;
    }
    if(gotResponse) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, hull);
        if(!error) {
            accuracy = doc["accuracy"].as<String>();
            lat = doc["location"]["lat"].as<String>(); 
            lng = doc["location"]["lng"].as<String>();
            location = lat + "," + lng;
            #ifdef DEBUG
                Serial.println("Your location is: " + location + " \nThe accuracy of the estimated location is: " + accuracy + "m");
            #endif // DEBUG
        } else {
            #ifdef DEBUG
                Serial.println("getLocFromGoogle: Failed to deserialize JSON, error code: ");
                Serial.println(error.c_str());
            #endif // DEBUG
            return "0";
        }
    }
    return location;
}
/*                                                     *
 *  Calls ip-api API to get latitude and longitude     *
 *  coordinates relative to your public IP location.   *
 *  Free up to 150 request per minute.                 *
 *  http://ip-api.com/                                 *
 *                                                     */
String NixieAPI::getLocFromIpapi(String publicIP) {
    WiFiClient client;
    HTTPClient http;
    String payload = "";
    String URL = "http://ip-api.com/json/" + publicIP;
    http.setUserAgent(UserAgent);
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
    #endif // DEBUG
    if(!http.begin(client, URL)) {
        #ifdef DEBUG
            Serial.println(F("getLocFromIpapi: Connection failed!"));
        #endif // DEBUG
        return "0";
    } else {
        #ifdef DEBUG
            Serial.println("Connected to ip-api.com!");
        #endif // DEBUG
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                if(!error) {
                    String country = doc["country"];
                    String region = doc["region"];
                    String city = doc["city"];
                    String lat = doc["lat"];
                    String lng = doc["lon"];
                    location = lat + "," + lng;
                    #ifdef DEBUG
                        Serial.print("Your IP location is: " + country + ", " + region + ", " + city + ". ");
                        Serial.println("With coordinates: latitude: " + lat + ", " + "longitude: " + lng);
                    #endif // DEBUG
                } else {
                    #ifdef DEBUG
                        Serial.println(F("getLocFromIpapi: JSON deserialization failed, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG 
                    return "0";
                }
            } else {
                #ifdef DEBUG
                    Serial.printf("getLocFromIpapi: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
                return "0";
            }
        } else {
            #ifdef DEBUG
                Serial.printf("getLocFromIpapi: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
            #endif // DEBUG
            return "0";
        }
    }
    http.end();

    return location;
}
/*                                                                         *
 *   This function combines all API services to get location parameters.   *
 *   After it receives the location, it will no longer call API services   *
 *   untill NixieTap is restarted.                                         *
 *                                                                         */
String NixieAPI::getLocation() {
    if(googleLocKey != "" && googleLocKey != "0" && (location == "" || location == "0")) {
        getLocFromGoogle();
    }
    if(ipStackKey != "" && ipStackKey != "0" && (location == "" || location == "0")) {
        getLocFromIpstack(getPublicIP());
    }
    if(location == "" || location == "0") {
        getLocFromIpapi(getPublicIP());
    }
    if(location == "" || location == "0") {
        #ifdef DEBUG
            Serial.println("getLocation: Failed to get any location from the API servers.");
        #endif // DEBUG
        return "0";
    }
    return location;
}

/*                                                            *
 *  Calls IPStack API to get time zone parameters according   *
 *  to your public IP location. The API can automatically     *
 *  detect your public IP address without the need to send it.*
 *  To get the time zone parameters from this API,you need    *
 *  to buy their services.  https://ipstack.com/              *
 *                                                            */
int NixieAPI::getTimeZoneOffsetFromIpstack(time_t now, String publicIP, uint8_t *dst) {
    WiFiClient client;
    HTTPClient http;
    int tz = 0;
    if(publicIP == "") {
        publicIP = "check";
    }
    String URL = "http://api.ipstack.com/" + publicIP + "?access_key=" + ipStackKey + "&output=json&fields=time_zone.id,time_zone.gmt_offset,time_zone.is_daylight_saving";
    String payload, tzname;
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
    #endif // DEBUG
    http.setUserAgent(UserAgent);
    if(!http.begin(client, URL)) {
        tz = 22;    // 22 is set as a time zone error
        #ifdef DEBUG
            Serial.println(F("getIpstackTimeZoneOffset: Connection failed!"));
        #endif // DEBUG
    } else {
        #ifdef DEBUG
            Serial.println("Connected to api.ipstack.com!");
        #endif // DEBUG
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                if(!error) {
                    tz = ((doc["gmt_offset"].as<int>()) / 60);  // Time Zone offset in minutes.
                    *dst = doc["is_daylight_saving"].as<int>(); // DST ih hours.
                    tzname = doc["id"].as<String>();
                    #ifdef DEBUG
                        Serial.println("Your Time Zone name is:" + tzname + " (Offset from UTC: " + String(tz) + ")");
                        Serial.printf("Is DST(Daylight saving time) active at your location: %s\n", *dst == 1 ? "Yes (+1 hour)" : "No (+0 hour)");
                    #endif // DEBUG
                } else {
                    tz = 22;
                    #ifdef DEBUG
                        Serial.println(F("getTimeZoneOffset: JSON deserialization failed!, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG
                }
            } else {
                tz = 22;
                #ifdef DEBUG
                    Serial.printf("getTimeZoneOffset: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
            }
        } else {
            tz = 22;
            #ifdef DEBUG
                Serial.printf("getTimeZoneOffset: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
            #endif // DEBUG
        }
    }
    http.end();

    return tz;
}
/*                                                                     *
 *  Calls Google Timezone API to get current timezone offset and dst.  *
 *  You get $200 free usage every month for Maps, Routes, or Places.   *
 *  https://developers.google.com/maps/documentation/timezone/start    *
 *                                                                     */
int NixieAPI::getTimeZoneOffsetFromGoogle(time_t now, String location, uint8_t *dst) {
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(googleTimeZoneCrt);
    HTTPClient https;
    int tz = 0;
    String URL = "https://maps.googleapis.com/maps/api/timezone/json?location=" + location + "&timestamp=" + String(now) + "&key=" + googleTimeZoneKey;
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
        Serial.println("Requesting URL: " + URL);
    #endif // DEBUG
    String payload, tzName, tzId;
    // http.setInsecure();   // https://github.com/esp8266/Arduino/pull/2821
    https.setUserAgent(UserAgent);
    if(!https.begin(*client, URL)) {
        tz = 22;    // 22 is set as a time zone error
        #ifdef DEBUG
            Serial.println(F("getTimeZoneOffset: [HTTP] connect failed!"));
        #endif // DEBUG
    } else {
        int stat = https.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = https.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                if(!error) {
                    tz = ((doc["rawOffset"].as<int>()) / 60);  // Time Zone offset in minutes.
                    *dst = ((doc["dstOffset"].as<int>()) / 3600); // DST ih hours.
                    tzName = doc["timeZoneName"].as<String>();
                    tzId = doc["timeZoneId"].as<String>();
                    #ifdef DEBUG
                        Serial.println("Your Time Zone name is: " + tzName + " (Offset from UTC: " + String(tz) + ") at location: " + tzId);
                        Serial.printf("Is DST(Daylight saving time) active at your location: %s\n", *dst == 1 ? "Yes (+1 hour)" : "No (+0 hour)");     
                    #endif // DEBUG
                } else {
                    tz = 22;
                    #ifdef DEBUG
                        Serial.println(F("getTimeZoneOffset: JSON deserialization failed, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG
                }
            } else {
                tz = 22;
                #ifdef DEBUG
                    Serial.printf("getTimeZoneOffset: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
            }
        } else {
            tz = 22;
            #ifdef DEBUG
            Serial.printf("getTimeZoneOffset: [HTTP] GET failed: %s\r\n", https.errorToString(stat).c_str());
            #endif // DEBUG
        }
    }
    https.end();

    return tz;
}
/*                                                                      *
 *  Calls timezonedb.com API to get time zone parameters for            *
 *  your latitude and longitude location.                               *
 *  API is free for personal and non-commercial usage.                  *
 *  There is a rate limit where you can only send request to            *
 *  the server once per second. Additional queries will get blocked.    *
 *  With premium packet you can get time zone parameters by IP address. *
 *  https://timezonedb.com/                                             *
 *                                                                      */
int NixieAPI::getTimeZoneOffsetFromTimezonedb(time_t now, String location, String ip, uint8_t *dst) {
    WiFiClient client;
    HTTPClient http;
    int tz = 0;
    String URL, payload, tzname;
    location.replace(",", "&lng="); // This API request this format of coordinates: &lat=45.0&lng=19.0
    if(ip == "")
        URL = "http://api.timezonedb.com/v2/get-time-zone?key=" + timezonedbKey + "&format=json&by=position&position&lat=" + location + "&time=" + String(now) + "&fields=zoneName,gmtOffset,dst";
    else 
        URL = "http://api.timezonedb.com/v2/get-time-zone?key=" + timezonedbKey + "&format=json&by=ip&ip=" + ip + "&time=" + String(now) + "&fields=zoneName,gmtOffset,dst";
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
    #endif // DEBUG
    http.setUserAgent(UserAgent);
    if(!http.begin(client, URL)) {
        tz = 22;    // 22 is set as a time zone error
        #ifdef DEBUG
            Serial.println(F("getTimeZoneOffsetFromTimezonedb: Connection failed!"));
        #endif // DEBUG
    } else {
        #ifdef DEBUG
            Serial.println("Connected to api.timezonedb.com!");
        #endif // DEBUG
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                if(!error) {
                    tz = ((doc["gmtOffset"].as<int>()) / 60);  // Time Zone offset in minutes.
                    *dst = doc["dst"].as<int>(); // DST ih hours.
                    if(*dst) {
                        tz -= 60;
                    }
                    tzname = doc["zoneName"].as<String>();
                    #ifdef DEBUG
                        Serial.println("Your Time Zone name is: " + tzname + " (Offset from UTC: " + String(tz) + ")");
                        Serial.printf("Is DST(Daylight saving time) active at your location: %s\n", *dst == 1 ? "Yes (+1 hour)" : "No (+0 hour)");
                    #endif // DEBUG
                } else {
                    tz = 22;
                    #ifdef DEBUG
                        Serial.println(F("getTimeZoneOffsetFromTimezonedb: JSON deserialization failed, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG
                }
            } else {
                tz = 22;
                #ifdef DEBUG
                    Serial.printf("getTimeZoneOffsetFromTimezonedb: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
            }
        } else {
            tz = 22;
            #ifdef DEBUG
                Serial.printf("getTimeZoneOffsetFromTimezonedb: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
            #endif // DEBUG
        }
    }
    http.end();

    return tz;
}
/*                                                                          *
 *   This function combines all API services to get time zone parameters.   *
 *                                                                          */
int NixieAPI::getTimezoneOffset(time_t now, uint8_t *dst) {
    int tz = 22;
    String loc = getLocation();
    if(googleTimeZoneKey != "" && googleTimeZoneKey != "0" && loc != "" && loc != "0") {
        tz = getTimeZoneOffsetFromGoogle(now, loc, dst);
    }
    if(timezonedbKey != "" && timezonedbKey != "0" && tz == 22) {   // 22 is set as a time zone error
        if (loc != "" && loc != "0") {
            tz = getTimeZoneOffsetFromTimezonedb(now, loc, "", dst);
        } else {
            tz = getTimeZoneOffsetFromTimezonedb(now, "", getPublicIP(), dst);
        }
    }
    if(ipStackKey != "" && ipStackKey != "0" && tz == 22) {
        tz = getTimeZoneOffsetFromIpstack(now, getPublicIP(), dst);
    } 
    if(tz == 22) {
        tz = 0;
        *dst = 0;
        #ifdef DEBUG
            Serial.println("The time zone could not be detected. There are no keys for any of API service.");
        #endif // DEBUG
    }
    return tz;
}
/*                                                                  *
 *  Calls Coinmarketcap API, to get crypto price                    *
 *  Should be limited to 30 requests per minute.                    *
 *  https://coinmarketcap.com/api/#endpoint_listings                *
 *                                                                  */
String NixieAPI::getCryptoPrice(char * crypto_key, char * currencyID) {
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(crypto_cert);
    HTTPClient https;
    String URL = "http://pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest?CMC_PRO_API_KEY="+(String)crypto_key+"&id="+(String)currencyID;
    String payload, price, cryptoName;
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
        Serial.println("Requesting price of a selected currency from: " + URL);
    #endif // DEBUG
    //http.setInsecure();   // https://github.com/esp8266/Arduino/pull/2821
    if (!https.begin(*client, URL))
    {
        #ifdef DEBUG
            Serial.println(F("CMC failed to connect!"));
        #endif // DEBUG
        return "0";
    } else {
        int stat = https.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = https.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                if(!error) {
                    price = doc["data"]["quotes"]["USD"]["price"].as<String>();
                    cryptoName = doc["data"]["name"].as<String>();
                    #ifdef DEBUG
                        Serial.println("The current price of " + cryptoName + " is: " + price);
                    #endif // DEBUG
                } else {
                    #ifdef DEBUG
                        Serial.println(F("CMC deserialization failed, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG
                    return "0";
                }
            } else {
                #ifdef DEBUG
                    Serial.printf("CMC: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
                return "0";
            }
        } else {
            #ifdef DEBUG
            Serial.printf("CMC: [HTTP] GET failed: %s\r\n", https.errorToString(stat).c_str());
            #endif // DEBUG
            return "0";
        }
    }
    https.end();

    return price;
}
/*                                                                            *
 *  Calls OpenWeatherMap API, to get the temperature for the given location.  *
 *  Available for Free. To access the API you need to sign up for an API key. * 
 *  Should be limited to 60 requests per minute.                              *
 *  https://openweathermap.org/api                                            *
 *                                                                            */
String NixieAPI::getTempAtMyLocation(String location, uint8_t format) {
    WiFiClient client;
    HTTPClient http;
    String payload, temperature;
    String formatType = (format == 1) ? "metric" : "imperial";
    String URL = "http://api.openweathermap.org/data/2.5/weather?id=" + location + "&units=" + formatType + "&APPID=" + openWeaterMapKey;
    #ifdef DEBUG
        Serial.println("---------------------------------------------------------------------------------------------");
        Serial.println("Requesting temperature for my location from: " + URL);
    #endif // DEBUG
    http.setUserAgent(UserAgent);
    if(!http.begin(client, URL)) {
        #ifdef DEBUG
            Serial.println(F("getTempAtMyLocation: Connection failed!"));
        #endif // DEBUG
    } else {
        #ifdef DEBUG
            Serial.println("Connected to api.openweathermap.org!");
        #endif // DEBUG
        int stat = http.GET();
        if(stat > 0) {
            if(stat == HTTP_CODE_OK) {
                payload = http.getString();
                DynamicJsonDocument doc(1024);
                DeserializationError error = deserializeJson(doc, payload);
                JsonObject main = doc["main"];
                if(!error) {
                    temperature = main["temp"].as<String>();
                    int8_t dotPos = temperature.indexOf('.');
                    if(dotPos != -1) {
                        temperature.remove(dotPos);
                    }
                    #ifdef DEBUG
                        String degreeType = (format == 1) ? "celsius" : "fahrenheit";
                        Serial.printf("Temperature at your location is %s degrees %s.\n", temperature.c_str(), degreeType.c_str());
                    #endif // DEBUG
                } else {
                    #ifdef DEBUG
                        Serial.println(F("getTempAtMyLocation: JSON deserialization failed, error code: "));
                        Serial.println(error.c_str());
                        Serial.println(payload);
                    #endif // DEBUG
                }
            } else {
                #ifdef DEBUG
                    Serial.printf("getTempAtMyLocation: [HTTP] GET reply %d\r\n", stat);
                #endif // DEBUG
            }
        } else {
            #ifdef DEBUG
                Serial.printf("getTempAtMyLocation: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
            #endif // DEBUG
        }
    }
    http.end();

    return temperature;
}

NixieAPI nixieTapAPI = NixieAPI();