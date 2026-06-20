#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <math.h>

const char* DEVICE_ID = "ESP32-C3-LIFTER-01";
const char* BLE_DEVICE_NAME = "ESP32-LIFTER-01";

const char* MEASUREMENT_SERVICE_UUID = "7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001";
const char* MEASUREMENT_CHARACTERISTIC_UUID = "7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001";

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
const unsigned long NOTIFY_INTERVAL_MS = 250;

float sampleBuffer[SAMPLE_WINDOW];
int sampleIndex = 0;
int sampleCount = 0;
float lastAverageVoltage = 0.0f;
int stableHits = 0;
bool isStable = false;
unsigned long lastSampleAt = 0;
unsigned long lastNotifyAt = 0;
unsigned long sampledAtMs = 0;

String measurementPayload = "";
String lastNotifiedPayload = "";
bool bleClientConnected = false;

NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* measurementCharacteristic = nullptr;

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
  sampledAtMs = 0;
}

void updateMeasurementLedState(bool triggered, bool stable) {
  if (!bleClientConnected) {
    blinkLed(CRGB::Purple, CRGB::Black, 700);
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

String buildMeasurementPayload() {
  const bool triggered = isTriggered();

  JsonDocument doc;
  doc["deviceId"] = DEVICE_ID;
  doc["transport"] = "ble";
  doc["sampledAt"] = "";
  doc["sampledAtMs"] = sampledAtMs;
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

  String payload;
  serializeJson(doc, payload);
  return payload;
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
    measurementPayload = buildMeasurementPayload();
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
  sampledAtMs = millis();
  updateMeasurementLedState(true, isStable);
  measurementPayload = buildMeasurementPayload();
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    bleClientConnected = true;
    Serial.print("BLE client connected: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    updateMeasurementLedState(isTriggered(), isStable);
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    bleClientConnected = false;
    Serial.print("BLE client disconnected, reason: ");
    Serial.println(reason);
    NimBLEDevice::startAdvertising();
    updateMeasurementLedState(isTriggered(), isStable);
  }
};

void setupBle() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService* service = bleServer->createService(MEASUREMENT_SERVICE_UUID);
  measurementCharacteristic = service->createCharacteristic(
    MEASUREMENT_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  measurementPayload = buildMeasurementPayload();
  measurementCharacteristic->setValue(measurementPayload);

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(MEASUREMENT_SERVICE_UUID);
  advertising->setName(BLE_DEVICE_NAME);
  advertising->start();

  Serial.println("BLE advertising started.");
  Serial.print("Device name: ");
  Serial.println(BLE_DEVICE_NAME);
  Serial.print("Service UUID: ");
  Serial.println(MEASUREMENT_SERVICE_UUID);
  Serial.print("Characteristic UUID: ");
  Serial.println(MEASUREMENT_CHARACTERISTIC_UUID);
}

void updateBleCharacteristic() {
  if (!measurementCharacteristic) {
    return;
  }

  if (measurementPayload.isEmpty()) {
    measurementPayload = buildMeasurementPayload();
  }

  measurementCharacteristic->setValue(measurementPayload);

  if (!bleClientConnected) {
    return;
  }

  const bool changed = measurementPayload != lastNotifiedPayload;
  const bool intervalElapsed = millis() - lastNotifyAt >= NOTIFY_INTERVAL_MS;

  if (!changed && !intervalElapsed) {
    return;
  }

  measurementCharacteristic->notify();
  lastNotifiedPayload = measurementPayload;
  lastNotifyAt = millis();
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

void printBootInfo() {
  Serial.println();
  Serial.println("ESP32-C3 BLE Voltage Sensor");
  Serial.print("VOLTAGE_PIN = ");
  Serial.println(VOLTAGE_PIN);
  Serial.print("LIFT_SW_PIN = ");
  Serial.println(LIFT_SW_PIN);
  Serial.print("RGB_PIN = ");
  Serial.println(RGB_PIN);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  printBootInfo();

  pinMode(LIFT_SW_PIN, INPUT_PULLUP);
  setupAdc();
  setupRgb();
  setupBle();
}

void loop() {
  updateMeasurement();
  updateBleCharacteristic();
}
