#include "Nixie.h"

// Initialize the display. This function configures pinModes based on .h file.
const uint16_t Nixie::pinmap[11] =
{
    0b0000010000, // 0
    0b0000100000, // 1
    0b0001000000, // 2
    0b0010000000, // 3
    0b0100000000, // 4
    0b1000000000, // 5
    0b0000000001, // 6
    0b0000000010, // 7
    0b0000000100, // 8
    0b0000001000, // 9
    0b0000000000  // digit off
};

const char *UserAgent = "NixieTap";
// openssl s_client -connect maps.googleapis.com:443 | openssl x509 -fingerprint -noout
const char *gMapsCrt = "‎‎67:7B:99:A4:E5:A7:AE:E4:F0:92:01:EF:F5:58:B8:0B:49:CF:53:D4";
const char *gMapsKey = "KEY"; // You can get your key here: https://developers.google.com/maps/documentation/geolocation/get-api-key
const char *gTimeZoneKey = "KEY"; // You can get your key here: https://developers.google.com/maps/documentation/timezone/get-api-key

Nixie::Nixie() {
    begin();
}

void Nixie::begin()
{
    // Set SPI chip select as output
    pinMode(SPI_CS, OUTPUT);
    // Configure the ESP to receive interrupts from a RTC. 
    pinMode(RTC_IRQ_PIN, INPUT);
    // Initialise the integrated button in a NixieTap as a input. 
    pinMode(BUTTON, INPUT_PULLUP);
    // fire up the RTC
    RTC.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    RTC.setCharger(2);
}
/**
* Change the state of the nixie Display
*
* This function takes four input digits and one dot (encoded in uint8_t).
* Digits are updated via SPI, dots are updated via GPIO.
* Digits are MSB to LSB (digit1 = H1), dots take binary values: H1 dot is 0b1000,
* H0 dot is 0b100, etc.
* @param digit1 H1 digit value, 0-9, 10 is off
* @param digit2 H0 digit value, 0-9, 10 is off
* @param digit3 M1 digit value, 0-9, 10 is off
* @param digit4 M0 digit value, 0-9, 10 is off
* @param dots dot values, encoded in binary;
* (H1, H0, M1, M0) = (0b1000, 0b100, 0b10, 0b1)
*/
void Nixie::write(uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t dots)
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
void Nixie::write_time(time_t local, bool dot_state)
{
    Nixie::write(hour(local)/10, hour(local)%10, minute(local)/10, minute(local)%10, dot_state*0b1000);
}
void Nixie::write_date(time_t local, bool dot_state)
{
    Nixie::write(day(local)/10, day(local)%10, month(local)/10, month(local)%10, dot_state*0b1000);
}

String Nixie::UrlEncode(const String url) {
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
// Get IPlocation from Freegeoip API server.
String Nixie::getIPlocation() { // Using freegeoip.net to map public IP's location
    HTTPClient http;
    String URL = "http://freegeoip.net/json/"; // no host or IP specified returns client's public IP info
    String payload;
    String loc;
    http.setUserAgent(UserAgent);
    if(!http.begin(URL)) {
        Serial.println(F("getIPlocation: [HTTP] connect failed!"));
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
                    Serial.println("getIPlocation: " + region + ", " + country);
                } else {
                    Serial.println(F("getIPlocation: JSON parse failed!"));
                    Serial.println(payload);
                }
            } else {
                Serial.printf("getIPlocation: [HTTP] GET reply %d\r\n", stat);
            }
        } else {
            Serial.printf("getIPlocation: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
        }
    }
    http.end();
    return loc;
}
// Get IPlocation from Google API server.
String Nixie::getLocation() { // using google maps API, return location for provided Postal Code
    HTTPClient http;
    String location = "";   // set to postal code to bypass geoIP lookup
    String URL = "https://maps.googleapis.com/maps/api/geocode/json?address=" + UrlEncode(location) + "&key=" + String(gMapsKey);
    String payload;
    String loc;
    http.setIgnoreTLSVerifyFailure(true);   // https://github.com/esp8266/Arduino/pull/2821
    http.setUserAgent(UserAgent);
    if(!http.begin(URL, gMapsCrt)) {
        Serial.println(F("getLocation: [HTTP] connect failed!"));
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
                    Serial.print(F("getLocation: "));
                    Serial.println(address);
                } else {
                    Serial.println(F("getLocation: JSON parse failed!"));
                    Serial.println(payload);
                }
            } else {
                Serial.printf("getLocation: [HTTP] GET reply %d\r\n", stat);
            }
        } else {
            Serial.printf("getLocation: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
        }
    }
    http.end();
    return loc;
}
// Get Time Zone from Google API.
int Nixie::getTimeZone(time_t now, String loc, uint8_t *dst) { // using google maps API, return TimeZone for provided timestamp
    HTTPClient http;
    int tz = false;
    String URL = "https://maps.googleapis.com/maps/api/timezone/json?location="
                + UrlEncode(loc) + "&timestamp=" + String(now) + "&key=" + String(gTimeZoneKey);
    String payload;
    http.setIgnoreTLSVerifyFailure(true);   // https://github.com/esp8266/Arduino/pull/2821
    http.setUserAgent(UserAgent);
    if(!http.begin(URL, gMapsCrt)) {
        Serial.println(F("getTimeZone: [HTTP] connect failed!"));
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
                    Serial.printf("getTimeZone: %s (%d)\r\n", tzname, tz);
                } else {
                    Serial.println(F("getTimeZone: JSON parse failed!"));
                    Serial.println(payload);
                }
            } else {
                Serial.printf("getTimeZone: [HTTP] GET reply %d\r\n", stat);
            }
        } else {
            Serial.printf("getTimeZone: [HTTP] GET failed: %s\r\n", http.errorToString(stat).c_str());
        }
    }
    http.end();
    return tz;
}

Nixie nixieTap = Nixie();