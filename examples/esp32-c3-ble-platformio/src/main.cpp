#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <math.h>

const char* DEVICE_ID = "ESP32C3-BATTMETER-01";
const char* BLE_DEVICE_NAME = "ESP32C3_BLE-BATTMETER-01";

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

#ifndef LED_BUILTIN
#define LED_BUILTIN 8
#endif

const int STATUS_LED_PIN = LED_BUILTIN;
constexpr int NUM_RGB_LEDS = 1;
CRGB rgbLeds[NUM_RGB_LEDS];

const bool USE_LIFT_SWITCH = false;
const bool USE_RGB_LED = false;
const bool USE_ONBOARD_LED = true;
const bool USE_MOCK_VOLTAGE = true;
const float MOCK_VOLTAGE_BASE = 12.45f;
const float MOCK_VOLTAGE_SWING = 0.18f;
const unsigned long MOCK_VOLTAGE_PERIOD_MS = 2200;

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

void applyLedState(bool enabled) {
  if (USE_ONBOARD_LED) {
    digitalWrite(STATUS_LED_PIN, enabled ? HIGH : LOW);
  }

  if (USE_RGB_LED) {
    rgbLeds[0] = enabled ? CRGB::White : CRGB::Black;
    FastLED.show();
  }
}

void setLedColor(const CRGB& color) {
  if (USE_RGB_LED) {
    rgbLeds[0] = color;
    FastLED.show();
  }

  if (USE_ONBOARD_LED) {
    digitalWrite(STATUS_LED_PIN, color == CRGB::Black ? LOW : HIGH);
  }
}

void blinkLed(unsigned long intervalMs) {
  static bool state = false;
  static unsigned long lastToggleAt = 0;

  if (millis() - lastToggleAt >= intervalMs) {
    lastToggleAt = millis();
    state = !state;
    if (USE_RGB_LED) {
      setLedColor(state ? CRGB::White : CRGB::Black);
    } else {
      applyLedState(state);
    }
  }
}

bool isTriggered() {
  if (!USE_LIFT_SWITCH) {
    return true;
  }

  return digitalRead(A1) == LOW;
}

float readBatteryVoltage() {
  if (USE_MOCK_VOLTAGE) {
    const float phase = static_cast<float>(millis() % MOCK_VOLTAGE_PERIOD_MS) / static_cast<float>(MOCK_VOLTAGE_PERIOD_MS);
    const float wave = sinf(phase * 2.0f * PI);
    return MOCK_VOLTAGE_BASE + (wave * MOCK_VOLTAGE_SWING);
  }

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
    if (USE_RGB_LED) {
      static bool state = false;
      static unsigned long lastToggleAt = 0;
      if (millis() - lastToggleAt >= 700) {
        lastToggleAt = millis();
        state = !state;
        setLedColor(state ? CRGB::Purple : CRGB::Black);
      }
    } else {
      blinkLed(700);
    }
    return;
  }

  if (!triggered) {
    if (USE_RGB_LED) {
      setLedColor(CRGB::Blue);
    } else {
      applyLedState(false);
    }
    return;
  }

  if (stable) {
    if (USE_RGB_LED) {
      setLedColor(CRGB::Green);
    } else {
      applyLedState(true);
    }
    return;
  }

  if (USE_RGB_LED) {
    static bool state = false;
    static unsigned long lastToggleAt = 0;
    if (millis() - lastToggleAt >= 180) {
      lastToggleAt = millis();
      state = !state;
      setLedColor(state ? CRGB::Orange : CRGB::Black);
    }
  } else {
    blinkLed(180);
  }
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
  doc["liftSwitchUsed"] = USE_LIFT_SWITCH;
  doc["mockVoltageUsed"] = USE_MOCK_VOLTAGE;
  doc["rgbLedUsed"] = USE_RGB_LED;

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
    Serial.print("[BLE] client connected: ");
    Serial.println(connInfo.getAddress().toString().c_str());
    updateMeasurementLedState(isTriggered(), isStable);
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    bleClientConnected = false;
    Serial.print("[BLE] client disconnected, reason: ");
    Serial.println(reason);
    NimBLEDevice::startAdvertising();
    updateMeasurementLedState(isTriggered(), isStable);
  }
};

void setupBle() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setPower(9);

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

  Serial.println("[BLE] advertising started.");
  Serial.print("[BLE] Device name: ");
  Serial.println(BLE_DEVICE_NAME);
  Serial.print("[BLE] Service UUID: ");
  Serial.println(MEASUREMENT_SERVICE_UUID);
  Serial.print("[BLE] Characteristic UUID: ");
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

  static unsigned long lastNotifyLogAt = 0;
  if (millis() - lastNotifyLogAt >= 1000) {
    lastNotifyLogAt = millis();
    Serial.print("[BLE] notify payload: ");
    Serial.println(measurementPayload);
  }
}

void setupAdc() {
  if (USE_MOCK_VOLTAGE) {
    return;
  }

  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
}

void setupStatusIndicators() {
  if (USE_ONBOARD_LED) {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
  }

  if (USE_RGB_LED) {
    FastLED.addLeds<WS2812, RGB_PIN, GRB>(rgbLeds, NUM_RGB_LEDS);
    FastLED.setBrightness(40);
    setLedColor(CRGB::Black);
  }
}

void printBootInfo() {
  Serial.println();
  Serial.println("ESP32-C3 BLE Voltage Sensor");
  Serial.print("[BOOT] VOLTAGE_PIN = ");
  Serial.println(VOLTAGE_PIN);
  Serial.print("[BOOT] STATUS_LED_PIN = ");
  Serial.println(STATUS_LED_PIN);
  Serial.print("[BOOT] LIFT_SW_PIN = ");
  Serial.println(LIFT_SW_PIN);
  Serial.print("[BOOT] RGB_PIN = ");
  Serial.println(RGB_PIN);
  Serial.print("[BOOT] USE_LIFT_SWITCH = ");
  Serial.println(USE_LIFT_SWITCH ? "true" : "false");
  Serial.print("[BOOT] USE_RGB_LED = ");
  Serial.println(USE_RGB_LED ? "true" : "false");
  Serial.print("[BOOT] USE_ONBOARD_LED = ");
  Serial.println(USE_ONBOARD_LED ? "true" : "false");
  Serial.print("[BOOT] USE_MOCK_VOLTAGE = ");
  Serial.println(USE_MOCK_VOLTAGE ? "true" : "false");
  Serial.print("[BOOT] DIVIDER_RATIO = ");
  Serial.println(DIVIDER_RATIO, 3);
  if (USE_MOCK_VOLTAGE) {
    Serial.print("[BOOT] MOCK_VOLTAGE_BASE = ");
    Serial.println(MOCK_VOLTAGE_BASE, 3);
  }
}

void logMeasurementDebug() {
  static unsigned long lastLogAt = 0;
  if (millis() - lastLogAt < 1000) {
    return;
  }

  lastLogAt = millis();

  if (USE_MOCK_VOLTAGE) {
    Serial.print("[ADC] mock battery=");
    Serial.print(readBatteryVoltage(), 3);
  } else {
    const int raw = analogRead(VOLTAGE_PIN);
    const float adcVoltage = (static_cast<float>(raw) / ADC_MAX) * ADC_REFERENCE_VOLTAGE;
    const float batteryVoltage = (adcVoltage * DIVIDER_RATIO) + VOLTAGE_OFFSET;

    Serial.print("[ADC] raw=");
    Serial.print(raw);
    Serial.print(" adc=");
    Serial.print(adcVoltage, 3);
    Serial.print("V battery=");
    Serial.print(batteryVoltage, 3);
  }

  Serial.print("V avg=");
  Serial.print(lastAverageVoltage, 3);
  Serial.print("V stableHits=");
  Serial.print(stableHits);
  Serial.print(" stable=");
  Serial.print(isStable ? "true" : "false");
  Serial.print(" triggered=");
  Serial.println(isTriggered() ? "true" : "false");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  printBootInfo();

  if (USE_LIFT_SWITCH) {
    pinMode(LIFT_SW_PIN, INPUT_PULLUP);
  }

  setupAdc();
  setupStatusIndicators();
  setupBle();
}

void loop() {
  updateMeasurement();
  updateBleCharacteristic();
  logMeasurementDebug();
}
