/**
 *  @file WeatherStation.ino
 *  @brief Arduino Weather Station -- Using BME280 sensor & ESP32-S2
 *
 *  Author       : Ayush Chinmay
 *  Date Created : 11 August 2023
 *  Date Modified: 15 August 2023
 *
 * CHANGELOG:
 *    * 2 October 2023
 *            - Replace BME - 280 Sensor with BME280 Sensor
 *            - Updated Code to display pressure in Bars (Pa * E-5) instead of heatIndex
 *    * 15 August 2023
 *            - Connected data to Firebase
 *    * 11 August 2023
 *            - Get BME280 Sensor Data using ESP32-S2
 *            - Print Sensor data to OLED Display (SSD1306)
 *
 * TODO:
 *    - [-]   
 */

/** ==========[ INCLUDES ]========== **/
#include "config.h"
#include <Wire.h>
// I2C OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// BME Sensor
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
// Wifi
#include <WiFi.h>
// Firebase database
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" // Provide the token generation process info.
#include "addons/RTDBHelper.h" // Provide the RTDB payload printing info and other helper functions.

/** ==========[ CONSTANTS ]========== **/
// I2C OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Wifi Credentials
const String ssid = WIFI_SSID;                  // Replace with WiFi SSID
const String pass = WIFI_PASS;                  // Replace WiFi Password

// Firebase Credentials
const String api_key = API_KEY;                 // Replace with API Key
const String firebase_host = FIREBASE_HOST;     // Replace with DataBase URL
const String user_mail = AUTH_MAIL;             // Replace with User email
const String user_pass = AUTH_PASS;             // Replace with User password

/** ==========[ VARIABLES ]========== **/
// I2C OLED Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// BME 280 sensor
Adafruit_BME280 bme;
#define SEALEVELPRESSURE_HPA (1013.25)

// Firebase database
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Measurement variables
float humid, tempC, tempF, pressure, altitude;
bool unitFlg = false;
unsigned long fbTime = 0;
unsigned long fbDelay = 10*60000;
unsigned long oledTime = 0;
unsigned long oledDelay = 2000;
unsigned long bmeTime = 0;
unsigned long bmeDelay = 41;
unsigned long count = 0;


/*  ==========[ SETUP ]========== */
void setup() {
    // Initialize Serial Monitor
    Serial.begin(115200);  // Start serial communication
    // Initialize OLED
    initOled();
    // Initialize Wifi
        //initWifi();
    // Initialize Firebase
        //initFirebase();
    // Initialize BME
    initBME();
}


/*  ==========[ LOOP ]========== */
void loop() {
    // Get temperature event and print its value.
    if (millis() - bmeTime >= bmeDelay || bmeTime == 0) {
        bmeTime = millis();
        readBME();
    }

    if (millis() - oledTime >= oledDelay || oledTime == 0) {
        oledTime = millis();
        printBME(unitFlg);
        unitFlg = !unitFlg;
    }
}


/** ==========[ INIT OLED ]========== **
 *  Initialize OLED Display
 */
void initOled() {
    // I2C OLED SETUP
    Serial.println("I2C OLED Test!");
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;  // Don't proceed, loop forever
    }

    display.display();
    delay(1000);
    display.clearDisplay();
    display.display();
    delay(1000);

    // DISPLAY TEXT
    display.setTextSize(1);               // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);  // Draw white text
    display.setCursor(0, 0);              // Start at top-left corner
    display.cp437(true);                  // Use full 256 char 'Code Page 437' font
    delay(1000);
}


/** ==========[ INIT WIFI ]========== **
 *  Initialize WiFi
 */
void initBME() {
    // BME280 SETUP
    Serial.println(F("BME 280 test!"));  // Print text to serial port
    bme.begin(0x76);                     // Initialize BME sensor

    // indoor navigation
    Serial.println("-- Indoor Navigation Scenario --");
    Serial.println("normal mode, 16x pressure / 2x temperature / 1x humidity oversampling,");
    Serial.println("0.5ms standby period, filter 16x");
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X2,  // temperature
                    Adafruit_BME280::SAMPLING_X16, // pressure
                    Adafruit_BME280::SAMPLING_X1,  // humidity
                    Adafruit_BME280::FILTER_X16,
                    Adafruit_BME280::STANDBY_MS_0_5 );
}


/** ==========[ INIT WIFI ]========== **
 *  Initialize WiFi
 */
void initWifi() {
    // WIFI SETUP
    WiFi.begin(ssid, pass);  // Connect to Wifi
    Serial.print("[INFO] Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {

        display.clearDisplay();
        display.setTextSize(1);
        display.print("Connecting to WiFi...");
        display.display();

        Serial.print(".");
        delay(1000);
    }
    Serial.print("[INFO] Connected with IP: "); Serial.println(WiFi.localIP());
    display.clearDisplay();
    display.print("WiFi Connected!");
    display.display();
}


/** ==========[ INIT FIREBASE ]========== **
 *  Initialize Firebase Database
 */
void initFirebase() {
    // Firebase Setup
    Serial.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    config.host = firebase_host;
    config.api_key = api_key;
    auth.user.email = user_mail;
    auth.user.password = user_pass;

    config.token_status_callback = tokenStatusCallback;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Firebase.setDoubleDigits(5);
}


/** ==========[ PRINT BME ]========== **
 *  Print BME-280 Sensor Data to OLED Display
 *  @param Fahren : Print Temperature in Fahrenheit
 */
void printBME(bool Fahren) {
    display.clearDisplay();
    display.setCursor(0, 0);

    if (isnan(humid) || isnan(tempC)) {
        Serial.println(F("[ERROR] Failed to read from BME Sensor!"));
        display.write("Failed to read BME");
        display.display();
        delay(1000);
        return;
    }

     display.clearDisplay();
     //  display.display();

    Serial.print("Humid: "); Serial.print(humid); Serial.print(" %");

    display.print("Humid: ");
    display.setTextSize(2);
    display.print(humid, 1);
    display.setTextSize(1);

    display.print(" ");
    display.setTextSize(2);
    display.print(" %");
    display.setTextSize(1);
    display.print("\n\n\nTemp:  ");

    if (Fahren == true) {
        Serial.print("\t|\tTemp: "); Serial.print(tempF, 1); Serial.print(" F");
        Serial.print("\t|\tPress: "); Serial.print(pressure, 1); Serial.println(" hPa\n");
    
        display.setTextSize(2);
        display.print(tempF, 1);
        display.setTextSize(1);
        display.print("  o");
        display.setTextSize(2);
        display.print("F");

        display.setTextSize(1);
        display.print("\n\n\nPress: ");
        display.setTextSize(2);
        display.print(pressure, 2);
        display.setTextSize(1);
        // display.print("h");
        // display.setTextSize(2);
        display.print("  Bar \n");
        display.setTextSize(1);
    } else {
        Serial.print("\t|\tTemp: "); Serial.print(tempC, 1); Serial.print(" C");
        Serial.print("\t|\tPress: "); Serial.print(pressure, 2); Serial.println(" C\n");

        display.setTextSize(2);
        display.print(tempC, 1);
        display.setTextSize(1);
        display.print("  o");
        display.setTextSize(2);
        display.print("C");

        display.setTextSize(1);
        display.print("\n\n\nPress: ");
        display.setTextSize(2);
        display.print(pressure, 2);
        display.setTextSize(1);
        // display.print("h");
        // display.setTextSize(2);
        display.print("  Bar \n");
        display.setTextSize(1);
    }
    display.display();
    // delay(500);
}

/** ==========[ READ BME ]========== **
 *  Read BME-280 Sensor Data
 */
void readBME() {
    humid = bme.readHumidity();                         // [%]
    tempC = bme.readTemperature();                      // [C]
    tempF = (tempC * 9/5.0) + 32.0;                     // [F]
    pressure = bme.readPressure() / 100000.0F;          // [Bar]
    altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);  // [m]
    // updateFB();
}


/** ==========[ UPDATE FIREBASE ]========== **
 *  Send data to firebase
 */
void updateFB() {
    if (Firebase.ready() && (millis() - fbTime > fbDelay || fbTime == 0)) {
        fbTime = millis();

        Firebase.RTDB.setFloat(&fbdo, F("/BME280/humidity"), humid);
        Firebase.RTDB.setFloat(&fbdo, F("/BME280/temperature/C"), tempC);
        Firebase.RTDB.setFloat(&fbdo, F("/BME280/temperature/F"), tempF);
        Firebase.RTDB.setFloat(&fbdo, F("/BME280/Pressure"), pressure);
        Firebase.RTDB.setFloat(&fbdo, F("/BME280/Altitude"), altitude);

        count++;
    }
}
