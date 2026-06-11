#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <time.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* DEVICE_ID = "ESP32-LIFTER-01";

const int VOLTAGE_PIN = 34;
const int SWITCH_PIN = 27;

// Update these for your divider and ADC calibration.
const float ADC_REFERENCE_VOLTAGE = 3.3f;
const int ADC_MAX = 4095;
const float DIVIDER_RATIO = 5.70f;
const float VOLTAGE_OFFSET = 0.00f;

const unsigned long SAMPLE_INTERVAL_MS = 120;
const int SAMPLE_WINDOW = 8;
const float STABLE_DELTA = 0.12f;
const int STABLE_HITS_REQUIRED = 3;

WebServer server(80);

float sampleBuffer[SAMPLE_WINDOW];
int sampleIndex = 0;
int sampleCount = 0;
float lastAverageVoltage = 0.0f;
int stableHits = 0;
bool isStable = false;
unsigned long lastSampleAt = 0;
String sampledAt = "";

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

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  for (int i = 0; i < 20; i++) {
    if (time(nullptr) > 100000) {
      return;
    }

    delay(250);
  }
}

float readBatteryVoltage() {
  int raw = analogRead(VOLTAGE_PIN);
  float adcVoltage = (static_cast<float>(raw) / ADC_MAX) * ADC_REFERENCE_VOLTAGE;
  float batteryVoltage = (adcVoltage * DIVIDER_RATIO) + VOLTAGE_OFFSET;
  return batteryVoltage;
}

bool isTriggered() {
  return digitalRead(SWITCH_PIN) == LOW;
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

  if (!isTriggered()) {
    resetMeasurementState();
    return;
  }

  float reading = readBatteryVoltage();
  sampleBuffer[sampleIndex] = reading;
  sampleIndex = (sampleIndex + 1) % SAMPLE_WINDOW;

  if (sampleCount < SAMPLE_WINDOW) {
    sampleCount += 1;
  }

  float sum = 0.0f;
  for (int i = 0; i < sampleCount; i++) {
    sum += sampleBuffer[i];
  }

  float averageVoltage = sum / sampleCount;

  if (sampleCount >= 2 && fabs(averageVoltage - lastAverageVoltage) <= STABLE_DELTA) {
    stableHits += 1;
  } else {
    stableHits = 1;
  }

  lastAverageVoltage = averageVoltage;
  isStable = (sampleCount >= SAMPLE_WINDOW && stableHits >= STABLE_HITS_REQUIRED);
  sampledAt = isoTimestampNow();
}

void handleVoltage() {
  const bool triggered = isTriggered();

  DynamicJsonDocument doc(512);
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
    doc["voltage"] = 0.0f;
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

void handleRoot() {
  server.send(200, "text/plain", "ESP32 Battery Voltage Server");
}

void setup() {
  Serial.begin(115200);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);

  connectWifi();
  setupTime();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/voltage", HTTP_GET, handleVoltage);
  server.begin();

  Serial.println(WiFi.localIP());
}

void loop() {
  updateMeasurement();
  server.handleClient();
}
