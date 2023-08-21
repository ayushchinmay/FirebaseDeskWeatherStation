/**
 *  @file WeatherStation.ino
 *  @brief Arduino Weather Station -- Using DHT-11 sensor & ESP32-S2
 *
 *  Author       : Ayush Chinmay
 *  Date Created : 11 August 2023
 *  Date Modified: 15 August 2023
 *
 * CHANGELOG:
 *    * 15 August 2023
 *            - Connected data to Firebase
 *    * 11 August 2023
 *            - Get DHT 11 Sensor Data using ESP32-S2
 *            - Print Sensor data to OLED Display (SSD1306)
 *
 * TODO:
 *    - [-]   Replace DHT-11 Sensor with BME280 Sensor
 */

/** ==========[ INCLUDES ]========== **/
#include "config.h"
#include <Wire.h>
// I2C OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// DHT Sensor
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
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

// DHT-11 Sensor
#define DHTTYPE DHT11  // DHT 11  (AM2302)
#define DHTPIN 14      // Digital pin connected to the DHT sensor

// Wifi Credentials
const String ssid = WIFI_SSID;                  // Replace with WiFi SSID
const String pass = WIFI_PASS;                  // Replace WiFi Password
// Firebase Credentials
const String api_key = API_KEY;                 // Replace with API Key
const String firebase_host = FIREBASE_HOST;     // Replace with DataBase URL
const String user_mail = AUTH_MAIL;             // Replace with User email
const String user_pass = AUTH_PASS;             // Replace with User password

// const String pathTemp = "/Temperature";
// const String pathHumid = "/Humidity";
// const String pathHeatIndex = "/Heat Index";

/** ==========[ VARIABLES ]========== **/
// I2C OLED Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT-11 sensor
DHT dht(DHTPIN, DHTTYPE);

// Firebase database
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Measurement variables
float humid, tempC, tempF, heatIndexC, heatIndexF;
bool unitFlg = false;
unsigned long fbTime = 0;
unsigned long fbDelay = 10000;
unsigned long oledTime = 0;
unsigned long oledDelay = 1500;
unsigned long count = 0;


/*  ==========[ SETUP ]========== */
void setup() {
    // Setup Pin Modes
    pinMode(DHTPIN, INPUT_PULLUP);  // Set pin to INPUT

    // Initialize Serial Monitor
    Serial.begin(115200);  // Start serial communication
    // Initialize OLED
    initOled();
    // Initialize Wifi
    initWifi();
    // Initialize Firebase
    initFirebase();

    // Initialize DHT
    Serial.println(F("DHT-11 test!"));  // Print text to serial port
    dht.begin();                        // Initialize DHT sensor
}


/*  ==========[ LOOP ]========== */
void loop() {
    // Get temperature event and print its value.
    readDHT();
    if (millis() - oledTime > oledDelay) {
        oledTime = millis();
        printDHT(unitFlg);
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


/** ==========[ PRINT DHT ]========== **
 *  Print DHT-11 Sensor Data to OLED Display
 *  @param Fahren : Print Temperature in Fahrenheit
 */
void printDHT(bool Fahren) {
    display.clearDisplay();
    display.setCursor(0, 0);

    if (isnan(humid) || isnan(tempC)) {
        Serial.println(F("[ERROR] Failed to read from DHT Sensor!"));
        display.write("Failed to read DHT");
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
    display.print("%");
    display.setTextSize(1);
    display.print("\n\n\nTemp:  ");

    if (Fahren == true) {
        Serial.print("\t|\tTemp: "); Serial.print(tempF, 1); Serial.print(" F");
        Serial.print("\t|\tHI: "); Serial.print(heatIndexF, 1); Serial.println(" F\n");
    
        display.setTextSize(2);
        display.print(tempF, 1);
        display.setTextSize(1);
        display.print("o");
        display.setTextSize(2);
        display.print("F");

        display.setTextSize(1);
        display.print("\n\n\nHI:    ");
        display.setTextSize(2);
        display.print(heatIndexF, 1);
        display.setTextSize(1);
        display.print("o");
        display.setTextSize(2);
        display.print("F \n");
        display.setTextSize(1);
    } else {
        Serial.print("\t|\tTemp: "); Serial.print(tempC, 1); Serial.print(" C");
        Serial.print("\t|\tHI: "); Serial.print(heatIndexC, 1); Serial.println(" C\n");

        display.setTextSize(2);
        display.print(tempC, 1);
        display.setTextSize(1);
        display.print("o");
        display.setTextSize(2);
        display.print("C");

        display.setTextSize(1);
        display.print("\n\n\nHI:    ");
        display.setTextSize(2);
        display.print(heatIndexC, 1);
        display.setTextSize(1);
        display.print("o");
        display.setTextSize(2);
        display.print("C \n");
        display.setTextSize(1);
    }
    display.display();
    delay(500);
}

/** ==========[ READ DHT ]========== **
 *  Read DHT-11 Sensor Data
 */
void readDHT() {
    humid = dht.readHumidity();
    tempC = dht.readTemperature();
    tempF = dht.readTemperature(true);
    heatIndexC = dht.computeHeatIndex(tempC, humid, false);
    heatIndexF = dht.computeHeatIndex(tempF, humid, true);
    updateFB();
}


/** ==========[ UPDATE FIREBASE ]========== **
 *  Send data to firebase
 */
void updateFB() {
    if (Firebase.ready() && (millis() - fbTime > fbDelay || fbTime == 0)) {
        fbTime = millis();

        Firebase.RTDB.setFloat(&fbdo, F("/DHT11/humidity"), humid);
        Firebase.RTDB.setFloat(&fbdo, F("/DHT11/temperature/C"), tempC);
        Firebase.RTDB.setFloat(&fbdo, F("/DHT11/temperature/F"), tempF);
        Firebase.RTDB.setFloat(&fbdo, F("/DHT11/heatIndex/C"), heatIndexC);
        Firebase.RTDB.setFloat(&fbdo, F("/DHT11/heatIndex/F"), heatIndexF);

        count++;
    }
}
