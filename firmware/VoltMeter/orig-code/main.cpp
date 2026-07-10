#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <math.h>

const char* DEVICE_ID_PREFIX = "PUMA-VoltMeter-";
const char* BLE_DEVICE_NAME_PREFIX = "PUMA-Voltmeter-";
const char* FIRMWARE_VERSION = "2026.06.20";

const char* MEASUREMENT_SERVICE_UUID = "7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001";
const char* MEASUREMENT_CHARACTERISTIC_UUID = "7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001";
const uint16_t MEASUREMENT_CHARACTERISTIC_MAX_LEN = 384;

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

#ifndef USE_LIFT_SWITCH
#define USE_LIFT_SWITCH 0
#endif

#ifndef USE_RGB_LED
#define USE_RGB_LED 0
#endif

#ifndef USE_ONBOARD_LED
#define USE_ONBOARD_LED 1
#endif

#ifndef USE_MOCK_VOLTAGE
#define USE_MOCK_VOLTAGE 0
#endif

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
String deviceId = "";
String bleDeviceName = "";
String deviceSuffix = "";
bool bleClientConnected = false;
bool serialDebugEnabled = false;
String serialInputBuffer = "";
int pendingDebugSamples = 0;
bool useLiftSwitch = USE_LIFT_SWITCH;
bool useRgbLed = USE_RGB_LED;
bool useMockVoltage = USE_MOCK_VOLTAGE;
bool rgbLedInitialized = false;

NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* measurementCharacteristic = nullptr;

String buildDeviceSuffix() {
  const uint64_t chipId = ESP.getEfuseMac();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06llX", chipId & 0xFFFFFFULL);
  return String(suffix);
}

void setupDeviceIdentity() {
  deviceSuffix = buildDeviceSuffix();
  deviceId = String(DEVICE_ID_PREFIX) + deviceSuffix;
  bleDeviceName = String(BLE_DEVICE_NAME_PREFIX) + deviceSuffix;
}

void ensureRgbLedInitialized() {
  if (rgbLedInitialized) {
    return;
  }

  FastLED.addLeds<WS2812, RGB_PIN, GRB>(rgbLeds, NUM_RGB_LEDS);
  FastLED.setBrightness(40);
  rgbLeds[0] = CRGB::Black;
  FastLED.show();
  rgbLedInitialized = true;
}

void applyLedState(bool enabled) {
  if (USE_ONBOARD_LED) {
    digitalWrite(STATUS_LED_PIN, enabled ? HIGH : LOW);
  }

  if (useRgbLed) {
    ensureRgbLedInitialized();
    rgbLeds[0] = enabled ? CRGB::White : CRGB::Black;
    FastLED.show();
  }
}

void setLedColor(const CRGB& color) {
  if (useRgbLed) {
    ensureRgbLedInitialized();
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
    if (useRgbLed) {
      setLedColor(state ? CRGB::White : CRGB::Black);
    } else {
      applyLedState(state);
    }
  }
}

bool isTriggered() {
  if (!useLiftSwitch) {
    return true;
  }

  return digitalRead(A1) == LOW;
}

float readBatteryVoltage() {
  if (useMockVoltage) {
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
    if (useRgbLed) {
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
    if (useRgbLed) {
      setLedColor(CRGB::Blue);
    } else {
      applyLedState(false);
    }
    return;
  }

  if (stable) {
    if (useRgbLed) {
      setLedColor(CRGB::Green);
    } else {
      applyLedState(true);
    }
    return;
  }

  if (useRgbLed) {
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
  doc["id"] = deviceId;
  doc["fw"] = FIRMWARE_VERSION;
  doc["t"] = sampledAtMs;
  doc["tr"] = triggered ? 1 : 0;

  if (!triggered) {
    doc["s"] = "idle";
    doc["st"] = 0;
    doc["v"] = 0.0;
  } else {
    doc["s"] = isStable ? "ready" : "measuring";
    doc["st"] = isStable ? 1 : 0;
    doc["v"] = lastAverageVoltage;
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
  NimBLEDevice::init(bleDeviceName.c_str());
  NimBLEDevice::setPower(9);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService* service = bleServer->createService(MEASUREMENT_SERVICE_UUID);
  measurementCharacteristic = service->createCharacteristic(
    MEASUREMENT_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
    MEASUREMENT_CHARACTERISTIC_MAX_LEN
  );

  measurementPayload = buildMeasurementPayload();
  measurementCharacteristic->setValue(measurementPayload);

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advertisingData;
  NimBLEAdvertisementData scanResponseData;

  advertisingData.setFlags(0x06);
  advertisingData.setCompleteServices(NimBLEUUID(MEASUREMENT_SERVICE_UUID));
  scanResponseData.setName(bleDeviceName.c_str());

  advertising->setAdvertisementData(advertisingData);
  advertising->setScanResponseData(scanResponseData);
  advertising->start();

  Serial.println("[BLE] advertising started.");
  Serial.print("[BLE] Device name: ");
  Serial.println(bleDeviceName);
  Serial.print("[BLE] Device id: ");
  Serial.println(deviceId);
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
  if (serialDebugEnabled && millis() - lastNotifyLogAt >= 1000) {
    lastNotifyLogAt = millis();
    Serial.print("[BLE] notify len=");
    Serial.print(measurementPayload.length());
    Serial.print(" payload: ");
    Serial.println(measurementPayload);
  }
}

void setupAdc() {
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
}

void setupStatusIndicators() {
  if (USE_ONBOARD_LED) {
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
  }
}

void printBootInfo() {
  Serial.println();
  Serial.println("ESP32-C3 BLE Voltage Sensor");
  Serial.println("========================================");
  Serial.println("[IDENTITY] Copy/Paste Values");
  Serial.print("[BOOT] DEVICE_ID = ");
  Serial.println(deviceId);
  Serial.print("[BOOT] BLE_DEVICE_NAME = ");
  Serial.println(bleDeviceName);
  Serial.print("[BOOT] DEVICE_SUFFIX = ");
  Serial.println(deviceSuffix);
  Serial.print("[BOOT] FIRMWARE_VERSION = ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("[BOOT] BLE_SCAN_PREFIX = ");
  Serial.println(BLE_DEVICE_NAME_PREFIX);
  Serial.print("[BOOT] PAYLOAD_KEYS = ");
  Serial.println("id,fw,v,st,tr,s,t");
  Serial.print("[BOOT] EXAMPLE_PAYLOAD = ");
  Serial.println(buildMeasurementPayload());
  Serial.println("========================================");
  Serial.print("[BOOT] VOLTAGE_PIN = ");
  Serial.println(VOLTAGE_PIN);
  Serial.print("[BOOT] STATUS_LED_PIN = ");
  Serial.println(STATUS_LED_PIN);
  Serial.print("[BOOT] LIFT_SW_PIN = ");
  Serial.println(LIFT_SW_PIN);
  Serial.print("[BOOT] RGB_PIN = ");
  Serial.println(RGB_PIN);
  Serial.print("[BOOT] USE_LIFT_SWITCH = ");
  Serial.println(useLiftSwitch ? "true" : "false");
  Serial.print("[BOOT] USE_RGB_LED = ");
  Serial.println(useRgbLed ? "true" : "false");
  Serial.print("[BOOT] USE_ONBOARD_LED = ");
  Serial.println(USE_ONBOARD_LED ? "true" : "false");
  Serial.print("[BOOT] USE_MOCK_VOLTAGE = ");
  Serial.println(useMockVoltage ? "true" : "false");
  Serial.print("[BOOT] DIVIDER_RATIO = ");
  Serial.println(DIVIDER_RATIO, 3);
  if (useMockVoltage) {
    Serial.print("[BOOT] MOCK_VOLTAGE_BASE = ");
    Serial.println(MOCK_VOLTAGE_BASE, 3);
  }
  Serial.println("[BOOT] SERIAL_COMMANDS = help,info,id,ble,payload,status,read,debug once,debug <n>,rgb on/off,lift on/off,mock on/off");
}

void logMeasurementDebug() {
  static unsigned long lastLogAt = 0;
  if ((!serialDebugEnabled && pendingDebugSamples <= 0) || millis() - lastLogAt < 1000) {
    return;
  }

  lastLogAt = millis();

  if (useMockVoltage) {
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

  if (pendingDebugSamples > 0) {
    pendingDebugSamples -= 1;
  }

  if (pendingDebugSamples == 0) {
    serialDebugEnabled = false;
  }
}

void printIdentityInfo() {
  Serial.println("[INFO] Identity");
  Serial.print("  deviceId: ");
  Serial.println(deviceId);
  Serial.print("  bleName: ");
  Serial.println(bleDeviceName);
  Serial.print("  suffix: ");
  Serial.println(deviceSuffix);
  Serial.print("  firmwareVersion: ");
  Serial.println(FIRMWARE_VERSION);
}

void printBleInfo() {
  Serial.println("[INFO] BLE");
  Serial.print("  connected: ");
  Serial.println(bleClientConnected ? "true" : "false");
  Serial.print("  serviceUuid: ");
  Serial.println(MEASUREMENT_SERVICE_UUID);
  Serial.print("  characteristicUuid: ");
  Serial.println(MEASUREMENT_CHARACTERISTIC_UUID);
}

void printMeasurementStatus() {
  Serial.println("[INFO] Measurement");
  Serial.print("  state: ");
  Serial.println(isTriggered() ? (isStable ? "ready" : "measuring") : "idle");
  Serial.print("  voltageAvg: ");
  Serial.println(lastAverageVoltage, 3);
  Serial.print("  stableHits: ");
  Serial.println(stableHits);
  Serial.print("  sampledAtMs: ");
  Serial.println(sampledAtMs);
  Serial.print("  liftSwitchEnabled: ");
  Serial.println(useLiftSwitch ? "true" : "false");
  Serial.print("  rgbLedEnabled: ");
  Serial.println(useRgbLed ? "true" : "false");
  Serial.print("  mockVoltageEnabled: ");
  Serial.println(useMockVoltage ? "true" : "false");
}

void printReadOnce() {
  const float reading = readBatteryVoltage();
  Serial.print("[READ] voltageNow=");
  Serial.println(reading, 3);
}

void printPayloadInfo() {
  if (measurementPayload.isEmpty()) {
    measurementPayload = buildMeasurementPayload();
  }

  Serial.print("[PAYLOAD] ");
  Serial.println(measurementPayload);
}

void printHelp() {
  Serial.println("[HELP] Commands");
  Serial.println("  help");
  Serial.println("  info");
  Serial.println("  id");
  Serial.println("  ble");
  Serial.println("  payload");
  Serial.println("  status");
  Serial.println("  read");
  Serial.println("  debug once");
  Serial.println("  debug <n>");
  Serial.println("  rgb on");
  Serial.println("  rgb off");
  Serial.println("  lift on");
  Serial.println("  lift off");
  Serial.println("  mock on");
  Serial.println("  mock off");
  Serial.println("  reboot");
}

void setRgbLedEnabled(bool enabled) {
  useRgbLed = enabled;
  if (enabled) {
    ensureRgbLedInitialized();
  } else if (rgbLedInitialized) {
    rgbLeds[0] = CRGB::Black;
    FastLED.show();
  }
  updateMeasurementLedState(isTriggered(), isStable);
}

void setLiftSwitchEnabled(bool enabled) {
  useLiftSwitch = enabled;
  resetMeasurementState();
  measurementPayload = buildMeasurementPayload();
  updateMeasurementLedState(isTriggered(), isStable);
}

void setMockVoltageEnabled(bool enabled) {
  useMockVoltage = enabled;
  resetMeasurementState();
  measurementPayload = buildMeasurementPayload();
}

void handleSerialCommand(const String& rawCommand) {
  String command = rawCommand;
  command.trim();
  String normalizedCommand = command;
  normalizedCommand.toLowerCase();

  if (normalizedCommand.isEmpty()) {
    return;
  }

  if (normalizedCommand == "help") {
    printHelp();
    return;
  }

  if (normalizedCommand == "info") {
    printIdentityInfo();
    printBleInfo();
    printMeasurementStatus();
    printPayloadInfo();
    return;
  }

  if (normalizedCommand == "id") {
    printIdentityInfo();
    return;
  }

  if (normalizedCommand == "ble") {
    printBleInfo();
    return;
  }

  if (normalizedCommand == "payload") {
    printPayloadInfo();
    return;
  }

  if (normalizedCommand == "status") {
    printMeasurementStatus();
    return;
  }

  if (normalizedCommand == "read") {
    printReadOnce();
    return;
  }

  if (normalizedCommand == "debug once") {
    pendingDebugSamples = 1;
    serialDebugEnabled = false;
    Serial.println("[DEBUG] queued 1 debug sample");
    return;
  }

  if (normalizedCommand.startsWith("debug ")) {
    const String countText = normalizedCommand.substring(6);
    const int count = countText.toInt();

    if (count > 0) {
      pendingDebugSamples = count;
      serialDebugEnabled = false;
      Serial.print("[DEBUG] queued ");
      Serial.print(count);
      Serial.println(" debug samples");
      return;
    }
  }

  if (normalizedCommand == "debug off") {
    pendingDebugSamples = 0;
    serialDebugEnabled = false;
    Serial.println("[DEBUG] cleared debug queue");
    return;
  }

  if (normalizedCommand == "rgb on") {
    setRgbLedEnabled(true);
    Serial.println("[CONFIG] rgb led enabled");
    return;
  }

  if (normalizedCommand == "rgb off") {
    setRgbLedEnabled(false);
    Serial.println("[CONFIG] rgb led disabled");
    return;
  }

  if (normalizedCommand == "lift on") {
    setLiftSwitchEnabled(true);
    Serial.println("[CONFIG] lift switch enabled");
    return;
  }

  if (normalizedCommand == "lift off") {
    setLiftSwitchEnabled(false);
    Serial.println("[CONFIG] lift switch disabled");
    return;
  }

  if (normalizedCommand == "mock on") {
    setMockVoltageEnabled(true);
    Serial.println("[CONFIG] mock voltage enabled");
    return;
  }

  if (normalizedCommand == "mock off") {
    setMockVoltageEnabled(false);
    Serial.println("[CONFIG] mock voltage disabled");
    return;
  }

  if (normalizedCommand == "reboot") {
    Serial.println("[SYSTEM] rebooting...");
    delay(150);
    ESP.restart();
    return;
  }

  Serial.print("[WARN] Unknown command: ");
  Serial.println(rawCommand);
  printHelp();
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    const char nextChar = static_cast<char>(Serial.read());

    if (nextChar == '\r' || nextChar == '\n') {
      if (serialInputBuffer.isEmpty()) {
        continue;
      }
      handleSerialCommand(serialInputBuffer);
      serialInputBuffer = "";
      continue;
    }

    serialInputBuffer += nextChar;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  setupDeviceIdentity();
  printBootInfo();

  pinMode(LIFT_SW_PIN, INPUT_PULLUP);

  setupAdc();
  setupStatusIndicators();
  setupBle();
}

void loop() {
  handleSerialInput();
  updateMeasurement();
  updateBleCharacteristic();
  logMeasurementDebug();
}
