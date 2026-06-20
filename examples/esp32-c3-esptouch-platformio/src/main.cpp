#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <esp_smartconfig.h>
#include <time.h>
#include <math.h>

const char* DEVICE_ID = "ESP32-C3-LIFTER-01";

#ifndef A0
#define A0 0
#endif

#ifndef A1
#define A1 1
#endif

#ifndef A2
#define A2 2
#endif

const int VOLTAGE_PIN = A0;
const int LIFT_SW_PIN = A1;
const int RGB_PIN = A2;

constexpr int NUM_LEDS = 1;
CRGB leds[NUM_LEDS];

const float ADC_REFERENCE_VOLTAGE = 3.3f;
const int ADC_MAX = 4095;
const float DIVIDER_RATIO = 5.70f;
const float VOLTAGE_OFFSET = 0.00f;

const unsigned long SAMPLE_INTERVAL_MS = 120;
const int SAMPLE_WINDOW = 8;
const float STABLE_DELTA = 0.12f;
const int STABLE_HITS_REQUIRED = 3;
const unsigned long WIFI_RETRY_TIMEOUT_MS = 10000;
const unsigned long SMARTCONFIG_TIMEOUT_MS = 180000;

WebServer server(80);

float sampleBuffer[SAMPLE_WINDOW];
int sampleIndex = 0;
int sampleCount = 0;
float lastAverageVoltage = 0.0f;
int stableHits = 0;
bool isStable = false;
unsigned long lastSampleAt = 0;
String sampledAt = "";
bool wifiConnected = false;

String isoTimestampNow() {
  time_t now = time(nullptr);
  if (now < 100000) {
    return "";
  }

  struct tm timeInfo;
  gmtime_r(&now, &timeInfo);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
  return String(buffer);
}

void setLed(const CRGB& color) {
  leds[0] = color;
  FastLED.show();
}

void blinkLed(const CRGB& colorA, const CRGB& colorB, unsigned long intervalMs) {
  static bool state = false;
  static unsigned long lastToggleAt = 0;

  if (millis() - lastToggleAt >= intervalMs) {
    lastToggleAt = millis();
    state = !state;
    setLed(state ? colorA : colorB);
  }
}

void showBootPinout() {
  Serial.println();
  Serial.println("ESP32-C3 Battery Voltage Server");
  Serial.print("VOLTAGE_PIN = ");
  Serial.println(VOLTAGE_PIN);
  Serial.print("LIFT_SW_PIN = ");
  Serial.println(LIFT_SW_PIN);
  Serial.print("RGB_PIN = ");
  Serial.println(RGB_PIN);
}

void updateMeasurementLedState(bool triggered, bool stable) {
  if (!wifiConnected) {
    return;
  }

  if (!triggered) {
    setLed(CRGB::Blue);
    return;
  }

  if (stable) {
    setLed(CRGB::Green);
    return;
  }

  blinkLed(CRGB::Orange, CRGB::Black, 180);
}

bool waitForWifiConnection(unsigned long timeoutMs, const CRGB& blinkColor) {
  const unsigned long startedAt = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startedAt) < timeoutMs) {
    blinkLed(blinkColor, CRGB::Black, 250);
    delay(50);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool connectUsingSavedWifi() {
  Serial.println("Trying saved WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);
  WiFi.begin();

  if (!waitForWifiConnection(WIFI_RETRY_TIMEOUT_MS, CRGB::Blue)) {
    Serial.println("Saved WiFi connect timeout.");
    return false;
  }

  Serial.print("Connected using saved WiFi. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool connectUsingESPTouch() {
  Serial.println("Entering ESPTouch mode.");
  Serial.println("Checklist:");
  Serial.println("1. Phone must be on 2.4GHz WiFi");
  Serial.println("2. Mobile data should be OFF");
  Serial.println("3. Location permission should be ON for ESPTouch app");
  Serial.println("4. Use ESPTouch v2 if the app supports it");

  WiFi.disconnect(true, true);
  delay(300);
  WiFi.mode(WIFI_AP_STA);
  WiFi.persistent(true);

  esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
  esp_smartconfig_fast_mode(true);
  WiFi.beginSmartConfig();

  const unsigned long startedAt = millis();
  while (!WiFi.smartConfigDone() && (millis() - startedAt) < SMARTCONFIG_TIMEOUT_MS) {
    blinkLed(CRGB::Purple, CRGB::Black, 250);
    static unsigned long lastLogAt = 0;
    if (millis() - lastLogAt >= 5000) {
      lastLogAt = millis();
      Serial.println("Waiting for ESPTouch packet...");
    }
    delay(50);
  }

  if (!WiFi.smartConfigDone()) {
    Serial.println("ESPTouch timed out.");
    return false;
  }

  Serial.println("ESPTouch received. Connecting...");

  if (!waitForWifiConnection(WIFI_RETRY_TIMEOUT_MS, CRGB::Yellow)) {
    Serial.println("WiFi connect after ESPTouch timeout.");
    Serial.print("Last WiFi status = ");
    Serial.println(static_cast<int>(WiFi.status()));
    return false;
  }

  WiFi.stopSmartConfig();
  WiFi.mode(WIFI_STA);
  Serial.print("Connected using ESPTouch. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void connectWifi() {
  wifiConnected = connectUsingSavedWifi();

  if (!wifiConnected) {
    wifiConnected = connectUsingESPTouch();
  }

  if (!wifiConnected) {
    Serial.println("WiFi not connected. Rebooting in 3 seconds...");
    setLed(CRGB::Red);
    delay(3000);
    ESP.restart();
  }

  setLed(CRGB::Green);
}

void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  for (int i = 0; i < 20; i++) {
    if (time(nullptr) > 100000) {
      Serial.println("Time synced.");
      return;
    }
    delay(250);
  }

  Serial.println("Time sync skipped or unavailable.");
}

bool isTriggered() {
  return digitalRead(LIFT_SW_PIN) == LOW;
}

float readBatteryVoltage() {
  const int raw = analogRead(VOLTAGE_PIN);
  const float adcVoltage = (static_cast<float>(raw) / ADC_MAX) * ADC_REFERENCE_VOLTAGE;
  return (adcVoltage * DIVIDER_RATIO) + VOLTAGE_OFFSET;
}

void resetMeasurementState() {
  sampleIndex = 0;
  sampleCount = 0;
  lastAverageVoltage = 0.0f;
  stableHits = 0;
  isStable = false;
  sampledAt = "";
}

void updateMeasurement() {
  if (millis() - lastSampleAt < SAMPLE_INTERVAL_MS) {
    return;
  }

  lastSampleAt = millis();
  const bool triggered = isTriggered();

  if (!triggered) {
    resetMeasurementState();
    updateMeasurementLedState(false, false);
    return;
  }

  const float reading = readBatteryVoltage();
  sampleBuffer[sampleIndex] = reading;
  sampleIndex = (sampleIndex + 1) % SAMPLE_WINDOW;

  if (sampleCount < SAMPLE_WINDOW) {
    sampleCount += 1;
  }

  float sum = 0.0f;
  for (int i = 0; i < sampleCount; i++) {
    sum += sampleBuffer[i];
  }

  const float averageVoltage = sum / sampleCount;

  if (sampleCount >= 2 && fabsf(averageVoltage - lastAverageVoltage) <= STABLE_DELTA) {
    stableHits += 1;
  } else {
    stableHits = 1;
  }

  lastAverageVoltage = averageVoltage;
  isStable = (sampleCount >= SAMPLE_WINDOW && stableHits >= STABLE_HITS_REQUIRED);
  sampledAt = isoTimestampNow();

  updateMeasurementLedState(true, isStable);
}

void handleRoot() {
  server.send(200, "text/plain", "ESP32-C3 Battery Voltage Server");
}

void handleVoltage() {
  const bool triggered = isTriggered();

  JsonDocument doc;
  doc["deviceId"] = DEVICE_ID;
  doc["sampledAt"] = sampledAt;
  doc["triggered"] = triggered;
  doc["switchPressed"] = triggered;
  doc["contact"] = triggered;
  doc["probeContact"] = triggered;
  doc["measureEnabled"] = triggered;

  if (!triggered) {
    doc["status"] = "idle";
    doc["state"] = "idle";
    doc["stable"] = false;
    doc["voltage"] = 0.0;
  } else {
    doc["status"] = isStable ? "ready" : "measuring";
    doc["state"] = isStable ? "ready" : "measuring";
    doc["stable"] = isStable;
    doc["voltage"] = lastAverageVoltage;
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/voltage", HTTP_GET, handleVoltage);
  server.begin();
  Serial.println("HTTP server started.");
  Serial.println("Use GET /voltage");
}

void setupAdc() {
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
}

void setupRgb() {
  FastLED.addLeds<WS2812, RGB_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  setLed(CRGB::Black);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  showBootPinout();

  pinMode(LIFT_SW_PIN, INPUT_PULLUP);
  setupAdc();
  setupRgb();

  connectWifi();
  setupTime();
  setupServer();
}

void loop() {
  updateMeasurement();
  server.handleClient();
}
