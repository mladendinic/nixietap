#include <Arduino.h>
#include <nixie.h>
#include <NixieAPI.h>
#include <BQ32000RTC.h>
#include <NtpClientLib.h> 
#include <TimeLib.h>
#include <WiFiManager.h> 
#include <EEPROM.h>
#include <Ticker.h>
#include <map>

ICACHE_RAM_ATTR void irq_1Hz_int();         // Interrupt function for changing the dot state every 1 second.
ICACHE_RAM_ATTR void touchButtonPressed();   // Interrupt function when button is pressed.
ICACHE_RAM_ATTR void scrollDots();          // Interrupt function for scrolling dots.
void processSyncEvent(NTPSyncEvent_t ntpEvent);
void enableSecDot();
void disableSecDot();
void startPortalManually();    
void updateParameters();
void readParameters();
void cryptoRefresh();
void weatherRefresh();
void updateTime();
void readAndParseSerial();
void resetEepromToDefault(); 
void readButton();
void firstRunInit();

uint8_t fwVersion = 1.1;
volatile bool dot_state = LOW;
bool stopDef = false, secDotDef = false;
bool wifiFirstConnected = true;
bool syncEventTriggered = false; // True if a time event has been triggered.

uint8_t configButton = 0;
volatile uint8_t state = 0, dotPosition = 0b10, weatherRefreshFlag = 1, cryptoRefreshFlag = 1;
char buttonCounter;
uint16_t buttonPressedCounter;
bool buttonPressed = false;
String cryptoCurrencyPrice = "", temperature = "", loc = "";
Ticker movingDot, priceRefresh, temperatureRefresh; // Initializing software timer interrupt called movingDot and priceRefresh.
NTPSyncEvent_t ntpEvent;    // Last triggered event.
WiFiManager wifiManager;
time_t t;
String serialCommand = "";

time_t last_temp;
time_t last_crypto;

uint8 timeRefreshFlag;
uint8 dateRefreshFlag;

char _time[6] = "00:00";
char date[11] = "1970-01-01";
char SSID[50] = "NixieTap";
char password[50] = "nixietap";
char target_SSID[50] = "none";
char target_pw[50] = "none";
uint8 enable_time = 1;
uint8 enable_date = 1;
uint8 enable_crypto = 0;
uint8 enable_temp = 0;
uint8 manual_time_flag = 1;
uint8 enable_DST = 0;
uint8 weather_format = 0;
uint8 enable_24h = 1;
int16_t offset = 0;
char weather_key[50];
char weather_id[50];
char crypto_key[50];
char crypto_id[30];

std::map<String, int> mem_map;


void setup() {
    mem_map["SSID"] = 0;
    mem_map["password"] = 50;
    mem_map["target_ssid"] = 100;
    mem_map["target_pw"] = 150;
    mem_map["weather_key"] = 200;
    mem_map["weather_format"] = 250;
    mem_map["weather_id"] = 251;
    mem_map["crypto_key"] = 302;
    mem_map["crypto_id"] = 351;
    mem_map["manual_time_flag"] = 381;
    mem_map["enable_date"] = 382;
    mem_map["enable_time"] = 383;
    mem_map["enable_temp"] = 384;
    mem_map["enable_crypto"] = 385;
    mem_map["enable_dst"] = 386;
    mem_map["enable_24h"] = 387;
    mem_map["offset"] = 388;
    mem_map["non_init"] = 500;
    // This line prevents the ESP from making spurious WiFi networks (ESP_XXXXX)
	WiFi.mode(WIFI_STA);
	nixieTap.write(10,10,10,10,0b10); // progress bar 25%

    // Touch button interrupt.
    attachInterrupt(digitalPinToInterrupt(TOUCH_BUTTON), touchButtonPressed, RISING);


    if(digitalRead(CONFIG_BUTTON)) {
		delay(5000);
		Serial.println("NIXIE TAP");
		Serial.print("Firmware version: ");
		Serial.println(fwVersion);
	}

	nixieTap.write(10,10,10,10,0b110); // progress bar 50%

	firstRunInit(); 
    readParameters();           // Read all stored parameters from EEPROM.

	nixieTap.write(10,10,10,10,0b1110); // progress bar 75%

    setSyncProvider(RTC.get);   // the function to get the time from the RTC
    enableSecDot();
	// Serial debug message
	if(timeStatus()!= timeSet)
		Serial.println("Unable to sync with the RTC!");
	else
		Serial.println("RTC has set the system time!");
	
	nixieTap.write(10,10,10,10,0b11110); // progress bar 100%
}
void loop() {
	// Polling functions
	readAndParseSerial();
	readButton();

	// Mandatory functions to be executed every cycle
    t = now(); // update date and time variable

    // If time is configured to be set semi-auto or auto and NixiTap is just started, the NTP request is created.
    if(manual_time_flag == 0 && wifiFirstConnected && WiFi.status() == WL_CONNECTED) {
        NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {ntpEvent = event; syncEventTriggered = true;});
        NTP.begin();
        wifiFirstConnected = false;
    }
    if(syncEventTriggered) {
        processSyncEvent(ntpEvent);
        syncEventTriggered = false;
    }
    
	// State machine
    if(state > 3) state = 0;

	// Slot 0 - time
    if(state == 0 && enable_time) {
        nixieTap.writeTime(t, dot_state, enable_24h);
    } 
	else if(!enable_time && state == 0) state++;

	// Slot 1 - date 
	if(state == 1 && enable_date) {
		nixieTap.writeDate(t, 1);
	} 
	else if(!enable_date && state == 1) state++;

	// Slot 2 - crypto price
	if(state == 2 && enable_crypto) {
        if(cryptoRefreshFlag) {
            cryptoRefreshFlag = 0;
            cryptoCurrencyPrice = nixieTapAPI.getCryptoPrice(crypto_key, crypto_id);
            last_crypto = now();
        }
        if (now() - last_crypto >= 60){
            cryptoRefreshFlag = 1;
        }
        nixieTap.writeNumber(cryptoCurrencyPrice, 350);
	}
	else if(!enable_crypto && state == 2) state++;
	
	// Slot 3 - temperature
	if(state == 3 && enable_temp) {
		if(weather_key[0] != '\0') {
			if(weatherRefreshFlag) {
				weatherRefreshFlag = 0;
                temperature = nixieTapAPI.getTempAtMyLocation(weather_id, weather_format);
                last_temp = now();
			}
            // Checking local temp every 5 minutes
            if (now() - last_temp >= 300){ 
                weatherRefreshFlag = 1;
            }
			nixieTap.writeNumber(temperature, 0);
		} else state++;
	}
	else if(!enable_temp && state == 3) state++;

    // Here you can add new functions for displaying numbers on NixieTap, just follow the basic writing principle from above.
}

void startPortalManually() {
    // By pressing the button on the back of the device you can manually start the WiFi Manager and access it's settings.
    nixieTap.write(10, 10, 10, 10, 0);
    disableSecDot(); // If the dots are not disabled, precisely the RTC_IRQ_PIN interrupt, ConfigPortal will chrach.
    movingDot.attach(0.2, scrollDots);
	Serial.println("---------------------------------------------------------------------------------------------");
    wifiManager.setConfigPortalTimeout(1800);
    // This will run a new config portal if the SSID and PW are valid.
    if(!wifiManager.startConfigPortal(SSID, password)) {
		Serial.println("Failed to connect and hit timeout!");
        // If the NixieTap is not connected to WiFi, it will collect the entered parameters and configure the RTC according to them.
    }
    updateParameters();
    updateTime();
    movingDot.detach();
    nixieTap.write(10, 10, 10, 10, 0); // Deletes remaining dot on display.
    enableSecDot();
}

void processSyncEvent(NTPSyncEvent_t ntpEvent) {
	Serial.println("---------------------------------------------------------------------------------------------");
	// When syncEventTriggered is triggered, through NTPClient, Nixie checks if NTP time is received.
    // If NTP time is received, Nixie starts synchronization of RTC time with received NTP time and stops NTPClinet from sending new requests.

        if(ntpEvent < 0) {
			Serial.print("Time Sync error: ");
			if(ntpEvent == noResponse) {
				Serial.println("NTP server not reachable.");
			} else if(ntpEvent == invalidAddress) {
				Serial.println("Invalid NTP server address.");
			} else if (ntpEvent == errorSending) {
				Serial.println ("Error sending request");
			} else if (ntpEvent == responseError) {
				Serial.println ("NTP response error");
			}
			Serial.println("Synchronization will be attempted again after 15 seconds.");
			Serial.println("If the time is not synced after 2 minutes, please restart Nixie Tap and try again!");
			Serial.println("If restart does not help. There might be a problem with the NTP server or your WiFi connection. You can set the time manually.");
        } else {
            if(NTP.getLastNTPSync() != 0) {
				Serial.print("NTP time is obtained: ");
				Serial.println(NTP.getLastNTPSync());
                if(!manual_time_flag) {
					Serial.println("Auto time adjustment started!");
                    // Collect NTP time, put it in RTC and stop NTP synchronization.
                    RTC.set(NTP.getLastNTPSync() + offset*60 + enable_DST*60*60);
                    NTP.stop();
                    setSyncProvider(RTC.get);
                    wifiFirstConnected = false;
                }
            }
        }
}
void readParameters() {
	Serial.println("Reading saved parameters from EEPROM.");
    int EEaddress = mem_map["SSID"];
    EEPROM.get(EEaddress, SSID);
    Serial.println("SSID IS:"+(String)SSID);
    EEaddress = mem_map["password"];
    EEPROM.get(EEaddress, password);
    Serial.println("PW IS:" + (String)password);
    EEaddress = mem_map["target_ssid"];
    EEPROM.get(EEaddress, target_SSID);
    Serial.println("Target SSID IS:" + (String)target_SSID);
    EEaddress = mem_map["target_pw"];
    EEPROM.get(EEaddress, target_pw);
    Serial.println("Target PW IS:" + (String)target_pw);
    EEaddress = mem_map["weather_key"];
    EEPROM.get(EEaddress, weather_key);
    Serial.println("WEATHER KEY IS:" + (String)weather_key);
    EEaddress = mem_map["weather_id"];
    EEPROM.get(EEaddress, weather_id);
    Serial.println("WEATHER ID IS:" + (String)weather_id);
    EEaddress = mem_map["weather_format"];
    EEPROM.get(EEaddress, weather_format);
    Serial.println("WEATHER FORMAT IS:" + (String)weather_format);
    EEaddress = mem_map["crypto_key"];
    EEPROM.get(EEaddress, crypto_key);
    Serial.println("CRYPTO KEY IS:" + (String)crypto_key);
    EEaddress = mem_map["crypto_id"];
    EEPROM.get(EEaddress, crypto_id);
    Serial.println("CRYPTO ID IS:" + (String)crypto_id);
    EEaddress = mem_map["manual_time_flag"];
    EEPROM.get(EEaddress, manual_time_flag);
    Serial.println("MANUAL TIME FLAG IS:" + (String)manual_time_flag);
    EEaddress = mem_map["enable_date"];
    EEPROM.get(EEaddress, enable_date);
    Serial.println("ENABLE DATE IS:" + (String)enable_date);
    EEaddress = mem_map["enable_time"];
    EEPROM.get(EEaddress, enable_time);
    Serial.println("ENABLE TIME IS:" + (String)enable_time);
    EEaddress = mem_map["enable_24h"];
    EEPROM.get(EEaddress, enable_24h);
    Serial.println("24H IS:" + (String)enable_24h);
    EEaddress = mem_map["enable_temp"];
    EEPROM.get(EEaddress, enable_temp);
    Serial.println("ENABLE TEMP IS:" + (String)enable_temp);
    EEaddress = mem_map["enable_crypto"];
    EEPROM.get(EEaddress, enable_crypto);
    Serial.println("ENABLE CRYPTO IS:" + (String)enable_crypto);
    EEaddress = mem_map["enable_dst"];
    EEPROM.get(EEaddress, enable_DST);
    Serial.println("ENABLE DST IS:" + (String)enable_DST);
    EEaddress = mem_map["offset"];
    EEPROM.get(EEaddress, offset);
    Serial.println("OFFSET IS:" + (String)offset);

    nixieTapAPI.applyKey(weather_key, 4);
}

void updateParameters() {
	Serial.println("---------------------------------------------------------------------------------------------");
	Serial.println("Synchronization of parameters started.");
    EEPROM.begin(512); // Number of bytes to allocate for parameters.
    int EEaddress;
    Serial.println("Comparing entered keys with the saved ones.");
    if (wifiManager.nixie_params.count("SSID") == 1){
        const char * nixie_ssid = wifiManager.nixie_params["SSID"].c_str();
        if (nixie_ssid != "\0" and SSID != nixie_ssid)
        {
            EEaddress = mem_map["SSID"];
            strcpy(SSID, nixie_ssid);
            EEPROM.put(EEaddress, SSID);   
            const char * nixie_pw = wifiManager.nixie_params["hotspot_password"].c_str();
            if (nixie_pw != "\0" and password != nixie_pw)
            {
                EEaddress = mem_map["password"];
                strcpy(password, nixie_pw);
                EEPROM.put(EEaddress, password);
            }
        }
    }
    if (wifiManager.nixie_params.count("target_ssid") == 1){
        const char * new_target_ssid = wifiManager.nixie_params["target_ssid"].c_str();
        if (new_target_ssid != "\0" and target_SSID != new_target_ssid)
        {
            EEaddress = mem_map["target_ssid"];
            strcpy(target_SSID, new_target_ssid);
            EEPROM.put(EEaddress, target_SSID);
            const char * new_target_pw = wifiManager.nixie_params["target_password"].c_str();
            if (new_target_pw != "\0" and new_target_pw != target_pw)
            {
                EEaddress = mem_map["target_pw"];
                strcpy(target_pw, new_target_pw);
                EEPROM.put(EEaddress, target_pw);
                wifiManager.connectWifi(target_SSID, target_pw);
            }
        }
    }
    if (wifiManager.nixie_params.count("weather_api") == 1)
    {
        const char *new_weather_key = wifiManager.nixie_params["weather_api"].c_str();
        if (new_weather_key != "\0" and new_weather_key != weather_key)
        {
            EEaddress = mem_map["weather_key"];
            strcpy(weather_key, new_weather_key);
            EEPROM.put(EEaddress, weather_key);
            nixieTapAPI.applyKey(weather_key, 4);
            weatherRefreshFlag = 1;
        }
    }
    if (wifiManager.nixie_params.count("weather_id") == 1)
    {
        const char *new_weather_id = wifiManager.nixie_params["weather_id"].c_str();
        if (new_weather_id != "\0" and new_weather_id != weather_id)
        {
            EEaddress = mem_map["weather_id"];
            strcpy(weather_id, new_weather_id);
            EEPROM.put(EEaddress, weather_id);
            weatherRefreshFlag = 1;
        }
    }
    if (wifiManager.nixie_params.count("weatherFormat") == 1)
    {
        uint8_t user_input_weather_format = atoi(wifiManager.nixie_params["weatherFormat"].c_str());
        if (user_input_weather_format != weather_format)
        {
            weather_format = user_input_weather_format;
            EEaddress = mem_map["weather_format"];
            EEPROM.put(EEaddress, weather_format);
            weatherRefreshFlag = 1;
        }
    }
   
    if (wifiManager.nixie_params.count("cryptoID") == 1)
    {
        const char *new_crypto_id = wifiManager.nixie_params["cryptoID"].c_str();
        if (new_crypto_id != "\0" and new_crypto_id != crypto_id)
        {
            EEaddress = mem_map["crypto_id"];
            strcpy(crypto_id, new_crypto_id);
            EEPROM.put(EEaddress, crypto_id);
        }
    }
    uint8_t new_enable_date = (uint8_t)wifiManager.nixie_params.count("enableDate");
    if (new_enable_date != enable_date){
        EEaddress = mem_map["enable_date"];
        enable_date = new_enable_date;
        EEPROM.put(EEaddress, enable_date);
    }
    uint8_t new_enable_time = (uint8_t)wifiManager.nixie_params.count("enableTime");
    if (new_enable_time != enable_time)
    {
        EEaddress = mem_map["enable_time"];
        enable_time = new_enable_time;
        EEPROM.put(EEaddress, new_enable_time);
    }
    uint8_t new_enable_24h = (uint8_t)wifiManager.nixie_params.count("enable24h");
    if (enable_24h != new_enable_24h)
    {
        EEaddress = mem_map["enable_24h"];
        enable_24h = new_enable_24h;
        EEPROM.put(EEaddress, enable_24h);
    }
    uint8_t new_enable_temp = (uint8_t)wifiManager.nixie_params.count("enableTemp");
    if (new_enable_temp != enable_temp)
    {
        EEaddress = mem_map["enable_temp"];
        enable_temp = new_enable_temp;
        EEPROM.put(EEaddress, enable_temp);
    }
    uint8_t new_enable_crypto = (uint8_t)wifiManager.nixie_params.count("enableCrypto");
    if (new_enable_crypto != enable_crypto)
    {
        EEaddress = mem_map["enable_crypto"];
        enable_crypto = new_enable_crypto;
        EEPROM.put(EEaddress, enable_crypto);
    }
    uint8_t new_enable_dst = (uint8_t)wifiManager.nixie_params.count("dst");
    if (new_enable_dst != enable_DST)
    {
        EEaddress = mem_map["enable_dst"];
        enable_DST = new_enable_dst;
        EEPROM.put(EEaddress, enable_DST);
    }
    if (wifiManager.nixie_params.count("setTimeManuallyFlag") == 1)
    {
        uint8_t new_manual_time_flag = atoi(wifiManager.nixie_params["setTimeManuallyFlag"].c_str());
        if (new_manual_time_flag != manual_time_flag){
            EEaddress = mem_map["manual_time_flag"];
            uint8_t temp_time_flag = new_manual_time_flag;
            EEPROM.put(EEaddress, temp_time_flag);
        }
        manual_time_flag = new_manual_time_flag;
        timeRefreshFlag = 1;
    }
    if (wifiManager.nixie_params.count("offset") == 1)
    {
        int16_t new_offset = atoi(wifiManager.nixie_params["offset"].c_str());
        if (new_offset != offset){
            EEaddress = mem_map["offset"];
            offset = new_offset;
            EEPROM.put(EEaddress, offset);
        }
    }
    if (wifiManager.nixie_params.count("time") == 1)
    {
        const char * new_time = wifiManager.nixie_params["time"].c_str();
        if (new_time != "\0" and new_time != _time)
        {
            strcpy(_time, new_time);
            timeRefreshFlag = 1;
        }
    }
    if (wifiManager.nixie_params.count("date") == 1)
    {
        const char * new_date = wifiManager.nixie_params["date"].c_str();
        if (new_date != "\0" and new_date != date)
        {
            strcpy(date, new_date);
            timeRefreshFlag = 1;
        }
    }

    if (wifiManager.nixie_params.count("stackKey") == 1)
    {
        const char *new_crypto_key = wifiManager.nixie_params["stackKey"].c_str();
        if (new_crypto_key != "\0" and new_crypto_key != crypto_key)
        {
            EEaddress = mem_map["crypto_key"];
            strcpy(crypto_key, new_crypto_key);
            EEPROM.put(EEaddress, crypto_key);
        }
    }
    // Setting the "non initialized" flag to 0
    EEaddress = mem_map["non_init"];
    EEPROM.put(EEaddress, 0);

    EEPROM.commit();
    if(enable_crypto) {
        priceRefresh.attach(300, cryptoRefresh); // This will refresh the cryptocurrency price every 5min.
    } else 
        priceRefresh.detach();
    if(enable_temp) {
        temperatureRefresh.attach(3600, weatherRefresh);
    } else
        temperatureRefresh.detach();
    wifiManager.nixie_params.clear();
    Serial.println("Synchronization of parameters completed!");
}

void updateTime() {
    if (timeRefreshFlag){
        if(manual_time_flag) { // I need feedback from the WiFiManager API that this option has been selected.
            NTP.stop();     // NTP sync is disableded to avoid sync errors.
            int hours = -1;
            int minutes = -1;
            char * time_token = strtok(_time, ":");
            while (time_token != NULL)
            {
                if(hours == -1){
                    hours = atoi(time_token);
                }else{
                    minutes = atoi(time_token);
                }
                time_token = strtok(NULL, " ");
            }
            int year = -1;
            int month = -1;
            int day = -1;
            char * date_token = strtok(date, "-");
            while (date_token != NULL)
            {
                if(year == -1){
                    year = atoi(date_token);
                }
                else if (month == -1){
                    month = atoi(date_token);
                }else{
                    day = atoi(date_token);
                }
                date_token = strtok(NULL, "-");
            }
            setTime(hours, minutes, 0, day, month, year);
            t = now();
            RTC.set(t);
            setSyncProvider(RTC.get);
            Serial.println("Manually entered date and time saved!");
        }else if (WiFi.status() == WL_CONNECTED){
            Serial.println("NixieTap is auto and connected, setting time to NTP!");
            NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {ntpEvent = event; syncEventTriggered = true;});
            NTP.begin();
            wifiFirstConnected = false;
        }else{
            Serial.println("NixieTap not connected to WiFi, cannot auto sync time via NTP!");
        }
        timeRefreshFlag = 0;
    }
}

/*                                                           *
 *  Enables the center dot to change its state every second. *
 *                                                           */
void enableSecDot() {
    if(secDotDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(1);  // Configures the 512Hz interrupt from RTC.
        attachInterrupt(digitalPinToInterrupt(RTC_IRQ_PIN), irq_1Hz_int, FALLING);
        secDotDef = true;
        stopDef = false;
    }
}
/*                                                *
 * Disaling the dots function on nixie display.   *
 *                                                */
void disableSecDot() {
    if(stopDef == false) {
        detachInterrupt(RTC_IRQ_PIN);
        RTC.setIRQ(0);              // Configures the interrupt from RTC.
        dotPosition = 0b10;         // Restast dot position.
        stopDef = true;
        secDotDef = false;
    }
}
/*                                                                                       *
 * An interrupt function that changes the state and position of the dots on the display. *
 *                                                                                       */
void scrollDots() {
    if(dotPosition == 0b100000) dotPosition = 0b10;
    nixieTap.write(11, 11, 11, 11, dotPosition);
    dotPosition = dotPosition << 1;
}
/*                                                                  *
 * An interrupt function for changing the dot state every 1 second. *
 *                                                                  */
void irq_1Hz_int() {
    dot_state = !dot_state;
}
/*                                                                *
 * An interrupt function for the touch sensor when it is touched. *
 *                                                                */
void touchButtonPressed() {
    state++;
	nixieTap.setAnimation(true);
}
void cryptoRefresh() {
    enable_crypto = 1;
}
void weatherRefresh() {
    enable_temp = 1;
}

void readAndParseSerial() {

	if(Serial.available()) {
		serialCommand = Serial.readStringUntil('\n');

		if(serialCommand.equals("init\r")) {
			Serial.println("Writing factory defaults to EEPROM...");
			Serial.println("Hotspot SSID: NixieTap");
			Serial.println("Hotspot password: NixieTap");
			Serial.println("Time format: 24h");
			Serial.println("Enabled display modes: time and date");
			Serial.println("Disabled display modes: other");
			Serial.println("Operation mode: semi-auto");
			Serial.println("DST: 0");
			Serial.println("Time zone offset: 120min");
			resetEepromToDefault();
		}	
		else {
			Serial.println("Unknown command.");
		}

	}
 
}


void resetEepromToDefault() {
	EEPROM.begin(512);
    int EEaddress = mem_map["SSID"];
    EEPROM.put(EEaddress, "NixieTap");
    EEaddress = mem_map["password"];
    EEPROM.put(EEaddress, "nixietap");
    EEaddress = mem_map["target_ssid"];
    EEPROM.put(EEaddress, "");
    EEaddress = mem_map["target_pw"];
    EEPROM.put(EEaddress, "");
    EEaddress = mem_map["weather_key"];
    EEPROM.put(EEaddress, "");
    EEaddress = mem_map["weather_id"];
    EEPROM.put(EEaddress, "");
    EEaddress = mem_map["weather_format"];
    EEPROM.put(EEaddress, 0);
    EEaddress = mem_map["crypto_key"];
    EEPROM.put(EEaddress, "");
    EEaddress = mem_map["crypto_id"];
    EEPROM.put(EEaddress, "");
    EEaddress = mem_map["manual_time_flag"];
    EEPROM.put(EEaddress, 1);
    EEaddress = mem_map["enable_date"];
    EEPROM.put(EEaddress, 1);
    EEaddress = mem_map["enable_time"];
    EEPROM.put(EEaddress, 1);
    EEaddress = mem_map["enable_temp"];
    EEPROM.put(EEaddress, 0);
    EEaddress = mem_map["enable_crypto"];
    EEPROM.put(EEaddress, 0);
    EEaddress = mem_map["enable_dst"];
    EEPROM.put(EEaddress, 0);
    EEaddress = mem_map["enable_24h"];
    EEPROM.put(EEaddress, 0);
    EEaddress = mem_map["offset"];
    EEPROM.put(EEaddress, 0);
    EEPROM.commit();
}

void readButton() {
    configButton = digitalRead(CONFIG_BUTTON);
    if(configButton) {
		buttonCounter++;
		if (buttonCounter==5) {
			buttonCounter=0;
			startPortalManually();
		}	    
    }
}


void firstRunInit() {
	bool notInitialized=1;
	EEPROM.begin(512);
    EEPROM.get(mem_map["non_init"], notInitialized);
	if(notInitialized) {
		Serial.println("------------------------------------------");
		Serial.println("Performing first run initialization...");
		Serial.println("------------------------------------------");
		resetEepromToDefault();
	}
}
