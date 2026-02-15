/**
 *  @file main.cpp
 *  @brief Arduino Weather Station -- Using BME280 sensor & XIAO ESP32-C3
 *
 *  Author       : Ayush Chinmay
 *  Date Created : 11 August 2023
 *  Date Modified: 02 February 2026
 *
 * CHANGELOG:
 *    * 2 February 2026
 *            - Added WiFi connectivity
 *            - Added MQTT integration for Home Assistant
 *            - Added Home Assistant MQTT Auto-Discovery
 *    * 2 October 2023
 *            - Replace BME - 280 Sensor with BME280 Sensor
 *            - Updated Code to display pressure in Bars (Pa * E-5) instead of heatIndex
 *    * 11 August 2023
 *            - Get BME280 Sensor Data using ESP32-S2
 *            - Print Sensor data to OLED Display (SSD1306)
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// I2C OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// BME Sensor
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

/** ==========[ CONFIGURATION ]========== **/
// WiFi credentials
const char *WIFI_SSID = "YushNet Lux 2.4G";
const char *WIFI_PASSWORD = "1337x@Ayush6901";

// MQTT Broker settings
const char* MQTT_SERVER = "192.168.8.195";  // TODO: Replace with your Home Assistant IP address
const int MQTT_PORT = 1883;
const char* MQTT_USER = "ayush.chinmay";              // MQTT user (create in HA: Settings > People > Users)
const char *MQTT_PASSWORD = "1337x@#Ayush6901";   // MQTT user password

// Device identifiers
const char* DEVICE_NAME = "pc_xiaoc3_weather";
const char* DEVICE_FRIENDLY_NAME = "Desktop Station Weather";

/** ==========[ CONSTANTS ]========== **/
// I2C OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define LED_PIN 10  // Built-in LED on XIAO ESP32-C3
#define BOOT_BTN 9  // Boot button on XIAO ESP32-C3 (hold to reset MQTT)

// BME280
#define BME_ADDRESS 0x76

// MQTT Topics
#define MQTT_DISCOVERY_PREFIX "homeassistant"
#define MQTT_STATE_TOPIC "pc_xiaoc3_weather/state"
#define MQTT_AVAILABILITY_TOPIC "pc_xiaoc3_weather/availability"

/** ==========[ OBJECTS ]========== **/
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

/** ==========[ VARIABLES ]========== **/
float humid, tempC, pressure;  // Humidity (%), Temperature (°C), Pressure (hPa)

// Timing variables
unsigned long oledTime = 0;
unsigned long oledDelay = 2000;
unsigned long bmeTime = 0;
unsigned long bmeDelay = 41;
unsigned long mqttTime = 0;
unsigned long mqttDelay = 10000;  // Publish to MQTT every 10 seconds
unsigned long wifiReconnectTime = 0;
unsigned long wifiReconnectDelay = 30000;

bool mqttDiscoverySent = false;
bool mqttResetRequested = false;

/** ==========[ WIFI FUNCTIONS ]========== **/
void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed - will retry later");
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiReconnectTime >= wifiReconnectDelay) {
      wifiReconnectTime = millis();
      Serial.println("WiFi disconnected, attempting reconnection...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

/** ==========[ MQTT FUNCTIONS ]========== **/
void resetMQTT() {
  // Complete MQTT reset - clears ALL retained messages for this device
  char topic[128];
  Serial.println("Resetting MQTT - clearing all retained messages...");

  // Clear current discovery topics
  snprintf(topic, sizeof(topic), "%s/sensor/%s/temperature/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, "", true);

  snprintf(topic, sizeof(topic), "%s/sensor/%s/humidity/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, "", true);

  snprintf(topic, sizeof(topic), "%s/sensor/%s/pressure/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, "", true);

  // Clear old/legacy discovery topics
  snprintf(topic, sizeof(topic), "%s/sensor/%s/temperature_f/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, "", true);

  snprintf(topic, sizeof(topic), "%s/sensor/%s/pressure_bar/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, "", true);

  snprintf(topic, sizeof(topic), "%s/sensor/%s/altitude/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, "", true);

  // Clear state and availability topics
  mqttClient.publish(MQTT_STATE_TOPIC, "", true);
  mqttClient.publish(MQTT_AVAILABILITY_TOPIC, "", true);

  delay(500);  // Allow messages to be sent
  Serial.println("MQTT reset complete - device removed from Home Assistant");
}

void publishDiscovery() {
  if (mqttDiscoverySent) return;

  JsonDocument doc;
  char buffer[512];
  char topic[128];

  // Temperature sensor (°C)
  doc.clear();
  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"][0] = DEVICE_NAME;
  device["name"] = DEVICE_FRIENDLY_NAME;
  device["model"] = "XIAO ESP32-C3 + BME280";
  device["manufacturer"] = "Seeed Studio";
  doc["name"] = "Temperature";
  doc["unique_id"] = "xiao_weather_temperature";
  doc["state_topic"] = MQTT_STATE_TOPIC;
  doc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  doc["value_template"] = "{{ value_json.temperature }}";
  doc["unit_of_measurement"] = "°C";
  doc["device_class"] = "temperature";
  doc["state_class"] = "measurement";
  serializeJson(doc, buffer);
  snprintf(topic, sizeof(topic), "%s/sensor/%s/temperature/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, buffer, true);

  // Humidity sensor (%)
  doc.clear();
  device = doc["device"].to<JsonObject>();
  device["identifiers"][0] = DEVICE_NAME;
  device["name"] = DEVICE_FRIENDLY_NAME;
  device["model"] = "XIAO ESP32-C3 + BME280";
  device["manufacturer"] = "Seeed Studio";
  doc["name"] = "Humidity";
  doc["unique_id"] = "xiao_weather_humidity";
  doc["state_topic"] = MQTT_STATE_TOPIC;
  doc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  doc["value_template"] = "{{ value_json.humidity }}";
  doc["unit_of_measurement"] = "%";
  doc["device_class"] = "humidity";
  doc["state_class"] = "measurement";
  serializeJson(doc, buffer);
  snprintf(topic, sizeof(topic), "%s/sensor/%s/humidity/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, buffer, true);

  // Pressure sensor (ATM)
  doc.clear();
  device = doc["device"].to<JsonObject>();
  device["identifiers"][0] = DEVICE_NAME;
  device["name"] = DEVICE_FRIENDLY_NAME;
  device["model"] = "XIAO ESP32-C3 + BME280";
  device["manufacturer"] = "Seeed Studio";
  doc["name"] = "Pressure";
  doc["unique_id"] = "xiao_weather_pressure";
  doc["state_topic"] = MQTT_STATE_TOPIC;
  doc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  doc["value_template"] = "{{ value_json.pressure }}";
  doc["unit_of_measurement"] = "hPa";
  doc["device_class"] = "pressure";
  doc["state_class"] = "measurement";
  serializeJson(doc, buffer);
  snprintf(topic, sizeof(topic), "%s/sensor/%s/pressure/config", MQTT_DISCOVERY_PREFIX, DEVICE_NAME);
  mqttClient.publish(topic, buffer, true);

  mqttDiscoverySent = true;
  Serial.println("MQTT Discovery messages sent for temperature, humidity, and pressure sensors");
}

void mqttReconnect() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  Serial.print("Connecting to MQTT at ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" as ");
  Serial.print(MQTT_USER);
  Serial.print("...");

  if (mqttClient.connect(DEVICE_NAME, MQTT_USER, MQTT_PASSWORD,
                         MQTT_AVAILABILITY_TOPIC, 0, true, "offline")) {
    Serial.println("connected!");
    mqttClient.publish(MQTT_AVAILABILITY_TOPIC, "online", true);
    mqttDiscoverySent = false;  // Re-send discovery on reconnect
    publishDiscovery();
  } else {
    Serial.print("FAILED! Error code: ");
    Serial.print(mqttClient.state());
    Serial.println();
    // Error codes: -4=timeout, -3=connection lost, -2=connect failed
    //              -1=disconnected, 0=connected, 1=bad protocol
    //              2=bad client ID, 3=unavailable, 4=bad credentials, 5=unauthorized
    switch(mqttClient.state()) {
      case -4: Serial.println("  -> Connection timeout"); break;
      case -2: Serial.println("  -> Connection refused (check IP/port)"); break;
      case 4:  Serial.println("  -> Bad credentials (check user/password)"); break;
      case 5:  Serial.println("  -> Not authorized (check MQTT ACL)"); break;
    }
  }
}

void publishSensorData() {
  if (!mqttClient.connected()) return;

  JsonDocument doc;
  char buffer[128];

  doc["temperature"] = round(tempC * 10) / 10.0;
  doc["humidity"] = round(humid * 10) / 10.0;
  doc["pressure"] = round(pressure * 10) / 10.0;  // hPa with 1 decimal

  serializeJson(doc, buffer);
  mqttClient.publish(MQTT_STATE_TOPIC, buffer, true);  // Retained for HA

  Serial.println("Published sensor data to MQTT");
}

/** ==========[ INIT OLED ]========== **/
void initOled() {
  Serial.println("I2C OLED Test!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.setRotation(3);  // 90° counter-clockwise (left side = bottom)

  display.display();
  delay(1000);
  display.clearDisplay();
  display.display();
  delay(1000);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
  delay(1000);
}

/** ==========[ INIT BME ]========== **/
void initBME() {
  Serial.println(F("BME 280 test!"));
  if (!bme.begin(BME_ADDRESS)) {
    Serial.println("Could not find BME280 sensor!");
    while (1) delay(10);
  }

  Serial.println("-- Indoor Navigation Scenario --");
  bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                  Adafruit_BME280::SAMPLING_X2,
                  Adafruit_BME280::SAMPLING_X16,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::FILTER_X16,
                  Adafruit_BME280::STANDBY_MS_0_5);
}

/** ==========[ PRINT BME ]========== **/
void printBME() {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (isnan(humid) || isnan(tempC)) {
    Serial.println(F("[ERROR] Failed to read from BME Sensor!"));
    display.setTextSize(1);
    display.println("BME Error");
    display.display();
    delay(1000);
    return;
  }

  // Serial output
  Serial.print("Humid: "); Serial.print(humid); Serial.print(" %");
  Serial.print("\t|\tTemp: "); Serial.print(tempC, 1); Serial.print(" C");
  Serial.print("\t|\tPress: "); Serial.print(pressure, 1); Serial.println(" hPa\n");

  // Display: Labels above values (portrait layout)
  display.setTextSize(1);
  display.println("Humid:");
  display.setTextSize(2);
  display.print(humid, 1);
  display.println("%");

  display.println();  // spacing
  display.setTextSize(1);
  display.println("Temp:");
  display.setTextSize(2);
  display.print(tempC, 1);
  display.println("C");

  display.println();  // spacing
  display.setTextSize(1);
  display.println("Press:");
  display.setTextSize(2);
  display.print(pressure, 0);
  display.setTextSize(1);
  display.println("hPa");

  display.display();
}

/** ==========[ READ BME ]========== **/
void readBME() {
  humid = bme.readHumidity();
  tempC = bme.readTemperature();
  pressure = bme.readPressure() / 100.0F;  // hPa (1 hPa = 100 Pa)
}

/** ==========[ SETUP ]========== **/
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Xiao Weather Station ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  // Check if boot button is held for MQTT reset
  if (digitalRead(BOOT_BTN) == LOW) {
    Serial.println("Boot button held - MQTT reset requested");
    mqttResetRequested = true;
  }

  // Initialize hardware
  initOled();
  initBME();

  // Show status message on display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  if (mqttResetRequested) {
    display.println("MQTT RESET");
    display.println("MODE");
    display.println();
    display.println("Connecting");
    display.println("to WiFi...");
  } else {
    display.println("Connecting");
    display.println("to WiFi...");
  }
  display.display();

  // Setup WiFi and MQTT
  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setBufferSize(512);  // Increase buffer for discovery messages

  // Handle MQTT reset if requested
  if (mqttResetRequested && WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Resetting");
    display.println("MQTT...");
    display.display();

    // Connect to MQTT and reset
    if (mqttClient.connect(DEVICE_NAME, MQTT_USER, MQTT_PASSWORD)) {
      resetMQTT();
      mqttClient.disconnect();

      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("MQTT Reset");
      display.println("Complete!");
      display.println();
      display.println("Restarting");
      display.println("in 3s...");
      display.display();
      delay(3000);
      ESP.restart();
    } else {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("MQTT Reset");
      display.println("FAILED!");
      display.println();
      display.println("Check MQTT");
      display.println("settings");
      display.display();
      delay(5000);
    }
    mqttResetRequested = false;
  }

  // Show IP on display briefly
  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi");
    display.println("Connected!");
    display.println();
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
  }
}

/** ==========[ LOOP ]========== **/
void loop() {
  // Maintain WiFi connection
  checkWiFi();

  // Maintain MQTT connection
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      mqttReconnect();
    }
    mqttClient.loop();
  }

  // Read BME sensor
  if (millis() - bmeTime >= bmeDelay || bmeTime == 0) {
    bmeTime = millis();
    readBME();
  }

  // Update OLED display
  if (millis() - oledTime >= oledDelay || oledTime == 0) {
    oledTime = millis();
    printBME();
  }

  // Publish to MQTT
  if (millis() - mqttTime >= mqttDelay || mqttTime == 0) {
    mqttTime = millis();
    publishSensorData();
  }
}
