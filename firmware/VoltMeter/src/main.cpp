#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NimBLEDevice.h>
#include "esp_wifi.h"
#include <math.h>

const char* DEVICE_ID_PREFIX = "PUMA-VoltMeter-";
const char* BLE_DEVICE_NAME_PREFIX = "PUMA-Voltmeter-";
const char* FIRMWARE_VERSION = "2026.07.05";

const char* CONFIG_AP_SSID_PREFIX = "BattMeter-Setup-";
const char* CONFIG_AP_PASSWORD = "12345678";

const char* PREF_NAMESPACE = "runtime";
const char* PREF_MODE_KEY = "mode";
const char* PREF_DIVIDER_RATIO_KEY = "divider";
const char* PREF_ADC_GAIN_KEY = "adc_gain";
const char* PREF_ADC_OFFSET_KEY = "adc_off";

const char* MEASUREMENT_SERVICE_UUID = "7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001";
const char* MEASUREMENT_CHARACTERISTIC_UUID = "7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001";
const char* CONTROL_CHARACTERISTIC_UUID = "7f9e0003-6a9d-4f7e-8d4d-32e7be6f1001";
const uint16_t MEASUREMENT_CHARACTERISTIC_MAX_LEN = 384;
const uint16_t CONTROL_CHARACTERISTIC_MAX_LEN = 96;

const int8_t WIFI_MAX_TX_POWER_QUARTER_DBM = 60;

#ifndef A0
#define A0 0
#endif

#ifndef A1
#define A1 1
#endif

#ifndef A2
#define A2 2
#endif

#ifndef A9
#define A9 9
#endif

const int VOLTAGE_PIN = A0;
const int LIFT_SW_PIN = A1;
const int RGB_PIN = A2;
const int BOOT_BUTTON_PIN = A9;
const int STATUS_LED_PIN = 8;
constexpr int NUM_RGB_LEDS = 1;
CRGB rgbLeds[NUM_RGB_LEDS];

#ifndef USE_LIFT_SWITCH
#define USE_LIFT_SWITCH 0
#endif

#ifndef USE_ONBOARD_LED
#define USE_ONBOARD_LED 1
#endif

#ifndef STATUS_LED_ACTIVE_LOW
#define STATUS_LED_ACTIVE_LOW 1
#endif

#ifndef USE_RGB_LED
#define USE_RGB_LED 1
#endif

#ifndef USE_MOCK_VOLTAGE
#define USE_MOCK_VOLTAGE 0
#endif

const float MOCK_VOLTAGE_BASE = 12.45f;
const float MOCK_VOLTAGE_SWING = 0.18f;
const unsigned long MOCK_VOLTAGE_PERIOD_MS = 2200;

const float ADC_REFERENCE_VOLTAGE = 3.3f;
const int ADC_MAX = 4095;
const float DEFAULT_DIVIDER_RATIO = 9.11f;
const float DEFAULT_ADC_GAIN = 1.0f;
const float DEFAULT_ADC_OFFSET = 0.0f;
const float VOLTAGE_OFFSET = 0.00f;

const unsigned long SAMPLE_INTERVAL_MS = 120;
const int SAMPLE_WINDOW = 8;
const float STABLE_DELTA = 0.12f;
const int STABLE_HITS_REQUIRED = 3;
const int DEFAULT_AUTO_CAL_SAMPLES = 24;
const unsigned long BLE_NOTIFY_INTERVAL_MS = 250;
const unsigned long MODE_LED_BLINK_ON_MS = 45;
const unsigned long MODE_LED_BLINK_OFF_MS = 170;
const unsigned long MODE_LED_PATTERN_GAP_MS = 1200;

enum RuntimeMode {
  RUNTIME_MODE_WIFI = 0,
  RUNTIME_MODE_BLE = 1,
};

float sampleBuffer[SAMPLE_WINDOW];
int sampleIndex = 0;
int sampleCount = 0;
float lastAverageVoltage = 0.0f;
int lastAdcRaw = 0;
int lastAdcMilliVolts = 0;
float lastAdcVoltageFromRaw = 0.0f;
float lastAdcVoltageFromMilliVolts = 0.0f;
float lastAdcVoltageCorrected = 0.0f;
float dividerRatio = DEFAULT_DIVIDER_RATIO;
float adcGain = DEFAULT_ADC_GAIN;
float adcOffset = DEFAULT_ADC_OFFSET;
int stableHits = 0;
bool isStable = false;
unsigned long lastSampleAt = 0;
unsigned long lastNotifyAt = 0;
unsigned long sampledAtMs = 0;

String measurementPayload = "";
String lastNotifiedPayload = "";
String deviceId = "";
String deviceSuffix = "";
String bleDeviceName = "";
String configApSsid = "";
String wifiStatusMessage = "booting";
String wifiFailureReason = "";
String bleStatusMessage = "inactive";
String serialInputBuffer = "";
String lastBleControlMessage = "ready";
bool serialDebugEnabled = false;
int pendingDebugSamples = 0;
bool useLiftSwitch = USE_LIFT_SWITCH;
bool useMockVoltage = USE_MOCK_VOLTAGE;
bool httpServerStarted = false;
bool configPortalActive = false;
bool hasEverConnected = false;
bool wifiManagerPortalStarted = false;
bool rgbManualEnabled = false;
bool rgbTestEnabled = false;
bool bleClientConnected = false;
RuntimeMode activeMode = RUNTIME_MODE_WIFI;
unsigned long lastWifiManagerProcessAt = 0;
bool rgbLedInitialized = false;
unsigned long lastRgbTestAt = 0;
uint8_t rgbTestIndex = 0;
bool systemReady = false;
bool rgbBootAnimationPending = false;
bool lastMeasurementStable = false;
unsigned long stableReadHoldUntil = 0;
unsigned long bootColorHoldUntil = 0;
unsigned long lastModeLedStepAt = 0;
uint8_t modeLedStep = 0;
bool modeLedPatternEnabled = true;

const unsigned long BOOT_COLOR_HOLD_MS = 1200;
const unsigned long STABLE_READ_COLOR_HOLD_MS = 1400;

Preferences preferences;
WiFiManager wifiManager;
WebServer server(80);
NimBLEServer* bleServer = nullptr;
NimBLECharacteristic* measurementCharacteristic = nullptr;
NimBLECharacteristic* controlCharacteristic = nullptr;

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
  configApSsid = String(CONFIG_AP_SSID_PREFIX) + deviceSuffix;
}

const char* runtimeModeName(RuntimeMode mode) {
  return mode == RUNTIME_MODE_BLE ? "ble" : "wifi";
}

RuntimeMode parseRuntimeMode(const String& rawMode) {
  String mode = rawMode;
  mode.trim();
  mode.toLowerCase();
  if (mode == "ble") {
    return RUNTIME_MODE_BLE;
  }
  return RUNTIME_MODE_WIFI;
}

void loadRuntimeMode() {
  preferences.begin(PREF_NAMESPACE, true);
  const String savedMode = preferences.getString(PREF_MODE_KEY, "wifi");
  preferences.end();
  activeMode = parseRuntimeMode(savedMode);
  Serial.print("[MODE] saved mode = ");
  Serial.println(savedMode);
}

void saveRuntimeMode(RuntimeMode mode) {
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putString(PREF_MODE_KEY, runtimeModeName(mode));
  preferences.end();
}

bool isDividerRatioValid(float value) {
  return !isnan(value) && value >= 0.1f && value <= 50.0f;
}

bool isAdcGainValid(float value) {
  return !isnan(value) && value >= 0.1f && value <= 10.0f;
}

bool isAdcOffsetValid(float value) {
  return !isnan(value) && value >= -2.0f && value <= 2.0f;
}

void loadDividerRatio() {
  preferences.begin(PREF_NAMESPACE, true);
  const float savedDividerRatio = preferences.getFloat(PREF_DIVIDER_RATIO_KEY, DEFAULT_DIVIDER_RATIO);
  preferences.end();

  dividerRatio = isDividerRatioValid(savedDividerRatio) ? savedDividerRatio : DEFAULT_DIVIDER_RATIO;
  Serial.print("[CAL] divider ratio = ");
  Serial.println(dividerRatio, 3);
}

void saveDividerRatio(float value) {
  if (!isDividerRatioValid(value)) {
    return;
  }

  dividerRatio = value;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putFloat(PREF_DIVIDER_RATIO_KEY, dividerRatio);
  preferences.end();
}

void loadAdcCalibration() {
  preferences.begin(PREF_NAMESPACE, true);
  const float savedAdcGain = preferences.getFloat(PREF_ADC_GAIN_KEY, DEFAULT_ADC_GAIN);
  const float savedAdcOffset = preferences.getFloat(PREF_ADC_OFFSET_KEY, DEFAULT_ADC_OFFSET);
  preferences.end();

  adcGain = isAdcGainValid(savedAdcGain) ? savedAdcGain : DEFAULT_ADC_GAIN;
  adcOffset = isAdcOffsetValid(savedAdcOffset) ? savedAdcOffset : DEFAULT_ADC_OFFSET;
  Serial.print("[CAL] adc gain = ");
  Serial.println(adcGain, 4);
  Serial.print("[CAL] adc offset = ");
  Serial.println(adcOffset, 4);
}

void saveAdcCalibration(float gain, float offset) {
  if (!isAdcGainValid(gain) || !isAdcOffsetValid(offset)) {
    return;
  }

  adcGain = gain;
  adcOffset = offset;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putFloat(PREF_ADC_GAIN_KEY, adcGain);
  preferences.putFloat(PREF_ADC_OFFSET_KEY, adcOffset);
  preferences.end();
}

float applyAdcCalibration(float voltage) {
  return (voltage * adcGain) + adcOffset;
}

int sanitizeAutoCalSamples(int samples) {
  if (samples < 4) {
    return 4;
  }

  if (samples > 128) {
    return 128;
  }

  return samples;
}

void sampleAdcAverages(int samples, int& rawAverage, int& milliVoltsAverage, float& rawVoltageAverage, float& milliVoltsVoltageAverage) {
  long rawSum = 0;
  long mvSum = 0;

  for (int i = 0; i < samples; i++) {
    const int raw = analogRead(VOLTAGE_PIN);
    const int milliVolts = analogReadMilliVolts(VOLTAGE_PIN);
    rawSum += raw;
    mvSum += milliVolts;
    delay(8);
  }

  rawAverage = static_cast<int>(rawSum / samples);
  milliVoltsAverage = static_cast<int>(mvSum / samples);
  rawVoltageAverage = (static_cast<float>(rawAverage) / ADC_MAX) * ADC_REFERENCE_VOLTAGE;
  milliVoltsVoltageAverage = static_cast<float>(milliVoltsAverage) / 1000.0f;
}

void applyWifiTxPower() {
  const esp_err_t err = esp_wifi_set_max_tx_power(WIFI_MAX_TX_POWER_QUARTER_DBM);
  Serial.print("[WIFI] set tx power result: ");
  Serial.println(static_cast<int>(err));
}

void ensureRgbLedInitialized() {
  if (!USE_RGB_LED || rgbLedInitialized) {
    return;
  }

  FastLED.addLeds<SK6812, RGB_PIN, GRB>(rgbLeds, NUM_RGB_LEDS);
  FastLED.setBrightness(48);
  rgbLeds[0] = CRGB::Black;
  FastLED.show();
  rgbLedInitialized = true;
}

void setRgbColor(const CRGB& color) {
  if (!USE_RGB_LED) {
    return;
  }

  ensureRgbLedInitialized();
  rgbLeds[0] = color;
  FastLED.show();
}

void applyLedState(bool enabled) {
  if (!USE_ONBOARD_LED) {
    return;
  }

  const uint8_t level = STATUS_LED_ACTIVE_LOW
    ? (enabled ? LOW : HIGH)
    : (enabled ? HIGH : LOW);
  digitalWrite(STATUS_LED_PIN, level);
}

void blinkLed(unsigned long intervalMs) {
  static bool state = false;
  static unsigned long lastToggleAt = 0;

  if (millis() - lastToggleAt >= intervalMs) {
    lastToggleAt = millis();
    state = !state;
    applyLedState(state);
  }
}

bool isTriggered() {
  if (!useLiftSwitch) {
    return true;
  }

  return digitalRead(LIFT_SW_PIN) == LOW;
}

bool isLiftSwitchPressedRaw() {
  return digitalRead(LIFT_SW_PIN) == LOW;
}

bool isBootButtonPressed() {
  return digitalRead(BOOT_BUTTON_PIN) == LOW;
}

float readBatteryVoltage() {
  if (useMockVoltage) {
    const float phase = static_cast<float>(millis() % MOCK_VOLTAGE_PERIOD_MS) / static_cast<float>(MOCK_VOLTAGE_PERIOD_MS);
    const float wave = sinf(phase * 2.0f * PI);
    lastAdcRaw = 0;
    lastAdcMilliVolts = 0;
    lastAdcVoltageFromRaw = 0.0f;
    lastAdcVoltageFromMilliVolts = 0.0f;
    lastAdcVoltageCorrected = 0.0f;
    return MOCK_VOLTAGE_BASE + (wave * MOCK_VOLTAGE_SWING);
  }

  lastAdcRaw = analogRead(VOLTAGE_PIN);
  lastAdcMilliVolts = analogReadMilliVolts(VOLTAGE_PIN);
  lastAdcVoltageFromRaw = (static_cast<float>(lastAdcRaw) / ADC_MAX) * ADC_REFERENCE_VOLTAGE;
  lastAdcVoltageFromMilliVolts = static_cast<float>(lastAdcMilliVolts) / 1000.0f;
  lastAdcVoltageCorrected = applyAdcCalibration(lastAdcVoltageFromMilliVolts);
  return (lastAdcVoltageCorrected * dividerRatio) + VOLTAGE_OFFSET;
}

void resetMeasurementState() {
  sampleIndex = 0;
  sampleCount = 0;
  lastAverageVoltage = 0.0f;
  stableHits = 0;
  isStable = false;
  lastMeasurementStable = false;
  sampledAtMs = 0;
}

bool isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String maskSsid(const String& ssid) {
  if (ssid.isEmpty()) {
    return "";
  }

  if (ssid.length() <= 4) {
    return ssid;
  }

  return ssid.substring(0, 2) + "***" + ssid.substring(ssid.length() - 2);
}

void updateMeasurementLedState(bool triggered, bool stable) {
  if (rgbTestEnabled) {
    return;
  }

  if (rgbManualEnabled) {
    setRgbColor(CRGB::White);
    applyLedState(true);
    return;
  }

  if (!systemReady || millis() < bootColorHoldUntil) {
    setRgbColor(CRGB::Purple);
    applyLedState(true);
    return;
  }

  if (millis() < stableReadHoldUntil) {
    setRgbColor(CRGB::Blue);
    applyLedState(true);
    return;
  }

  if (useLiftSwitch && isLiftSwitchPressedRaw()) {
    setRgbColor(CRGB::Red);
    applyLedState(true);
    return;
  }

  if (activeMode == RUNTIME_MODE_WIFI) {
    if (configPortalActive) {
      setRgbColor(CRGB::Purple);
      blinkLed(120);
      return;
    }

    if (!isWifiConnected()) {
      setRgbColor(CRGB::Purple);
      blinkLed(700);
      return;
    }
  } else {
    if (!bleClientConnected) {
      setRgbColor(CRGB::Purple);
      blinkLed(700);
      return;
    }
  }

  if (!triggered) {
    setRgbColor(CRGB::Green);
    applyLedState(true);
    return;
  }

  if (stable) {
    setRgbColor(CRGB::Green);
    applyLedState(true);
    return;
  }

  setRgbColor(CRGB::Orange);
  blinkLed(180);
}

String buildMeasurementPayload() {
  const bool triggered = isTriggered();

  JsonDocument doc;
  doc["id"] = deviceId;
  doc["fw"] = FIRMWARE_VERSION;
  doc["t"] = sampledAtMs;
  doc["tr"] = triggered ? 1 : 0;
  doc["raw"] = lastAdcRaw;
  doc["adc_mv"] = lastAdcMilliVolts;
  doc["adc_from_raw"] = lastAdcVoltageFromRaw;
  doc["adc"] = lastAdcVoltageFromMilliVolts;
  doc["adc_corrected"] = lastAdcVoltageCorrected;

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
  if (isStable && !lastMeasurementStable) {
    stableReadHoldUntil = millis() + STABLE_READ_COLOR_HOLD_MS;
  }
  lastMeasurementStable = isStable;
  updateMeasurementLedState(true, isStable);
  measurementPayload = buildMeasurementPayload();
}

void rebootToMode(RuntimeMode mode, const char* source) {
  saveRuntimeMode(mode);
  Serial.print("[MODE] switching to ");
  Serial.print(runtimeModeName(mode));
  Serial.print(" via ");
  Serial.println(source);
  delay(200);
  ESP.restart();
}

void checkBootButtonModeOverride() {
  if (!isBootButtonPressed()) {
    Serial.println("[MODE] boot button not pressed");
    return;
  }

  const RuntimeMode nextMode = activeMode == RUNTIME_MODE_WIFI ? RUNTIME_MODE_BLE : RUNTIME_MODE_WIFI;
  saveRuntimeMode(nextMode);
  activeMode = nextMode;
  Serial.println("[MODE] boot button pressed");
  Serial.print("[MODE] boot button override -> ");
  Serial.println(runtimeModeName(activeMode));
}

void updateModeLedPattern() {
  if (!USE_ONBOARD_LED || !modeLedPatternEnabled) {
    return;
  }

  if (activeMode == RUNTIME_MODE_BLE) {
    applyLedState(true);
    return;
  }

  const uint8_t blinkCount = 1;
  const uint8_t totalSteps = (blinkCount * 2) + 1;
  const unsigned long now = millis();
  const unsigned long interval = (modeLedStep == blinkCount * 2) ? MODE_LED_PATTERN_GAP_MS : ((modeLedStep % 2 == 0) ? MODE_LED_BLINK_ON_MS : MODE_LED_BLINK_OFF_MS);

  if (now - lastModeLedStepAt < interval) {
    return;
  }

  lastModeLedStepAt = now;
  modeLedStep = (modeLedStep + 1) % totalSteps;

  if (modeLedStep >= blinkCount * 2) {
    applyLedState(false);
    return;
  }

  applyLedState((modeLedStep % 2) == 0);
}

String buildStatusPayload() {
  JsonDocument doc;
  doc["id"] = deviceId;
  doc["fw"] = FIRMWARE_VERSION;
  doc["active_mode"] = runtimeModeName(activeMode);
  doc["wifi_status"] = wifiStatusMessage;
  doc["wifi_failure_reason"] = wifiFailureReason;
  doc["ble_status"] = bleStatusMessage;
  doc["sta_connected"] = isWifiConnected() ? 1 : 0;
  doc["sta_ssid"] = isWifiConnected() ? maskSsid(WiFi.SSID()) : "";
  doc["sta_ip"] = isWifiConnected() ? WiFi.localIP().toString() : "";
  doc["config_ap_active"] = configPortalActive ? 1 : 0;
  doc["config_ap_ssid"] = configPortalActive ? configApSsid : "";
  doc["config_ap_ip"] = configPortalActive ? WiFi.softAPIP().toString() : "";
  doc["ble_connected"] = bleClientConnected ? 1 : 0;
  doc["ble_name"] = bleDeviceName;
  doc["tx_power_qdbm"] = WIFI_MAX_TX_POWER_QUARTER_DBM;
  doc["lift_switch"] = useLiftSwitch ? 1 : 0;
  doc["mock_voltage"] = useMockVoltage ? 1 : 0;
  doc["rgb_led"] = USE_RGB_LED ? 1 : 0;
  doc["rgb_manual"] = rgbManualEnabled ? 1 : 0;
  doc["rgb_test"] = rgbTestEnabled ? 1 : 0;
  doc["divider_ratio"] = dividerRatio;
  doc["adc_gain"] = adcGain;
  doc["adc_offset"] = adcOffset;
  doc["voltage_offset"] = VOLTAGE_OFFSET;
  doc["adc_raw"] = lastAdcRaw;
  doc["adc_mv"] = lastAdcMilliVolts;
  doc["adc_voltage_from_raw"] = lastAdcVoltageFromRaw;
  doc["adc_voltage"] = lastAdcVoltageFromMilliVolts;
  doc["adc_voltage_corrected"] = lastAdcVoltageCorrected;
  doc["measurement"] = serialized(measurementPayload.isEmpty() ? buildMeasurementPayload() : measurementPayload);

  String payload;
  serializeJson(doc, payload);
  return payload;
}

void handleMeasurementApi() {
  if (measurementPayload.isEmpty()) {
    measurementPayload = buildMeasurementPayload();
  }

  server.send(200, "application/json", measurementPayload);
}

void handleStatusApi() {
  server.send(200, "application/json", buildStatusPayload());
}

void handleClearWifi() {
  if (activeMode != RUNTIME_MODE_WIFI) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"wifi_mode_required\"}");
    return;
  }

  wifiManager.resetSettings();
  WiFi.disconnect(true, true);
  wifiStatusMessage = "credentials_cleared";
  wifiFailureReason = "";
  server.send(200, "application/json", "{\"ok\":true,\"restarting\":1}");
  delay(250);
  ESP.restart();
}

void handleModeChange() {
  const String modeArg = server.arg("mode");
  if (modeArg.isEmpty()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"mode_required\"}");
    return;
  }

  const RuntimeMode nextMode = parseRuntimeMode(modeArg);
  if (nextMode == activeMode) {
    server.send(200, "application/json", "{\"ok\":true,\"changed\":0}");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true,\"changed\":1,\"restarting\":1}");
  delay(250);
  rebootToMode(nextMode, "http");
}

void handleCalibrationUpdate() {
  const String ratioArg = server.arg("ratio");
  const String gainArg = server.arg("adc_gain");
  const String offsetArg = server.arg("adc_offset");

  if (ratioArg.isEmpty() && gainArg.isEmpty() && offsetArg.isEmpty()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"calibration_required\"}");
    return;
  }

  float nextRatio = dividerRatio;
  float nextGain = adcGain;
  float nextOffset = adcOffset;

  if (!ratioArg.isEmpty()) {
    nextRatio = ratioArg.toFloat();
  }

  if (!gainArg.isEmpty()) {
    nextGain = gainArg.toFloat();
  }

  if (!offsetArg.isEmpty()) {
    nextOffset = offsetArg.toFloat();
  }

  if (!isDividerRatioValid(nextRatio) || !isAdcGainValid(nextGain) || !isAdcOffsetValid(nextOffset)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_calibration\"}");
    return;
  }

  saveDividerRatio(nextRatio);
  saveAdcCalibration(nextGain, nextOffset);
  resetMeasurementState();
  measurementPayload = buildMeasurementPayload();

  JsonDocument doc;
  doc["ok"] = true;
  doc["divider_ratio"] = dividerRatio;
  doc["adc_gain"] = adcGain;
  doc["adc_offset"] = adcOffset;

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);

  Serial.print("[CAL] divider ratio updated via http: ");
  Serial.println(dividerRatio, 3);
  Serial.print("[CAL] adc gain updated via http: ");
  Serial.println(adcGain, 4);
  Serial.print("[CAL] adc offset updated via http: ");
  Serial.println(adcOffset, 4);
}

void handleAutoCalibration() {
  const String actualAdcArg = server.arg("actual_adc");
  const String actualBatteryArg = server.arg("actual_battery");
  const String samplesArg = server.arg("samples");

  if (actualAdcArg.isEmpty() && actualBatteryArg.isEmpty()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"reference_required\"}");
    return;
  }

  const bool hasActualAdc = !actualAdcArg.isEmpty();
  const bool hasActualBattery = !actualBatteryArg.isEmpty();
  const float actualAdc = hasActualAdc ? actualAdcArg.toFloat() : 0.0f;
  const float actualBattery = hasActualBattery ? actualBatteryArg.toFloat() : 0.0f;
  const int sampleCount = sanitizeAutoCalSamples(samplesArg.isEmpty() ? DEFAULT_AUTO_CAL_SAMPLES : samplesArg.toInt());

  if ((hasActualAdc && actualAdc <= 0.0f) || (hasActualBattery && actualBattery <= 0.0f)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_reference\"}");
    return;
  }

  int rawAverage = 0;
  int milliVoltsAverage = 0;
  float rawVoltageAverage = 0.0f;
  float adcVoltageAverage = 0.0f;
  sampleAdcAverages(sampleCount, rawAverage, milliVoltsAverage, rawVoltageAverage, adcVoltageAverage);

  if (adcVoltageAverage <= 0.0f) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"adc_sample_failed\"}");
    return;
  }

  float nextGain = adcGain;
  float nextOffset = adcOffset;
  float nextDividerRatio = dividerRatio;

  if (hasActualAdc) {
    nextGain = (actualAdc - adcOffset) / adcVoltageAverage;
  }

  const float correctedAdcVoltage = (adcVoltageAverage * nextGain) + nextOffset;
  if (correctedAdcVoltage <= 0.0f) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_corrected_adc\"}");
    return;
  }

  if (hasActualBattery) {
    nextDividerRatio = (actualBattery - VOLTAGE_OFFSET) / correctedAdcVoltage;
  }

  if (!isAdcGainValid(nextGain) || !isAdcOffsetValid(nextOffset) || !isDividerRatioValid(nextDividerRatio)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"auto_calculation_out_of_range\"}");
    return;
  }

  saveAdcCalibration(nextGain, nextOffset);
  saveDividerRatio(nextDividerRatio);
  resetMeasurementState();
  measurementPayload = buildMeasurementPayload();

  JsonDocument doc;
  doc["ok"] = true;
  doc["samples"] = sampleCount;
  doc["sample_raw"] = rawAverage;
  doc["sample_adc_mv"] = milliVoltsAverage;
  doc["sample_adc_voltage_from_raw"] = rawVoltageAverage;
  doc["sample_adc_voltage"] = adcVoltageAverage;
  doc["sample_adc_corrected"] = correctedAdcVoltage;
  doc["divider_ratio"] = dividerRatio;
  doc["adc_gain"] = adcGain;
  doc["adc_offset"] = adcOffset;

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);

  Serial.println("[AUTO-CAL] completed");
  Serial.print("  samples: ");
  Serial.println(sampleCount);
  Serial.print("  sample raw avg: ");
  Serial.println(rawAverage);
  Serial.print("  sample adc mv avg: ");
  Serial.println(milliVoltsAverage);
  Serial.print("  sample adc voltage avg: ");
  Serial.println(adcVoltageAverage, 4);
  Serial.print("  sample adc corrected: ");
  Serial.println(correctedAdcVoltage, 4);
  if (hasActualAdc) {
    Serial.print("  actual adc ref: ");
    Serial.println(actualAdc, 4);
  }
  if (hasActualBattery) {
    Serial.print("  actual battery ref: ");
    Serial.println(actualBattery, 4);
  }
  Serial.print("  saved adc gain: ");
  Serial.println(adcGain, 4);
  Serial.print("  saved adc offset: ");
  Serial.println(adcOffset, 4);
  Serial.print("  saved divider ratio: ");
  Serial.println(dividerRatio, 4);
}

void handleRoot() {
  String html;
  html.reserve(3200);
  html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>BattMeter API</title></head><body>";
  html += "<h1>BattMeter API</h1>";
  html += "<p>Device: " + deviceId + "</p>";
  html += "<p>Firmware: " + String(FIRMWARE_VERSION) + "</p>";
  html += "<p>Active mode: " + String(runtimeModeName(activeMode)) + "</p>";
  html += "<p>WiFi status: " + wifiStatusMessage + "</p>";
  html += "<p>SSID: " + String(isWifiConnected() ? maskSsid(WiFi.SSID()) : "-") + "</p>";
  html += "<p>IP: " + String(isWifiConnected() ? WiFi.localIP().toString() : "-") + "</p>";
  html += "<p>Divider ratio: " + String(dividerRatio, 3) + "</p>";
  html += "<p>ADC gain: " + String(adcGain, 4) + "</p>";
  html += "<p>ADC offset: " + String(adcOffset, 4) + "</p>";
  html += "<p><a href='/api/measurement'>/api/measurement</a></p>";
  html += "<p><a href='/api/status'>/api/status</a></p>";
  html += "<form method='POST' action='/api/calibration'>";
  html += "<label>Divider ratio <input type='number' name='ratio' step='0.001' min='0.1' max='50' value='" + String(dividerRatio, 3) + "'></label>";
  html += "<br><label>ADC gain <input type='number' name='adc_gain' step='0.0001' min='0.1' max='10' value='" + String(adcGain, 4) + "'></label>";
  html += "<br><label>ADC offset <input type='number' name='adc_offset' step='0.0001' min='-2' max='2' value='" + String(adcOffset, 4) + "'></label>";
  html += "<button type='submit'>Save Calibration</button></form>";
  html += "<hr><form method='POST' action='/api/calibration/auto'>";
  html += "<p>Guided Auto Calibration</p>";
  html += "<label>Actual A0 voltage <input type='number' name='actual_adc' step='0.0001' min='0' placeholder='1.1800'></label>";
  html += "<br><label>Actual battery voltage <input type='number' name='actual_battery' step='0.0001' min='0' placeholder='11.9500'></label>";
  html += "<br><label>Samples <input type='number' name='samples' step='1' min='4' max='128' value='" + String(DEFAULT_AUTO_CAL_SAMPLES) + "'></label>";
  html += "<br><button type='submit'>Run Auto Calibration</button></form>";
  html += "<form method='POST' action='/mode'><input type='hidden' name='mode' value='ble'><button type='submit'>Switch To BLE</button></form>";
  html += "<form method='POST' action='/clearwifi'><button type='submit'>Clear Wi-Fi</button></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  JsonDocument doc;
  doc["error"] = "not_found";
  doc["path"] = server.uri();

  String payload;
  serializeJson(doc, payload);
  server.send(404, "application/json", payload);
}

void startHttpServer() {
  if (httpServerStarted) {
    return;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/clearwifi", HTTP_POST, handleClearWifi);
  server.on("/mode", HTTP_POST, handleModeChange);
  server.on("/api/calibration", HTTP_POST, handleCalibrationUpdate);
  server.on("/api/calibration/auto", HTTP_POST, handleAutoCalibration);
  server.on("/api/measurement", HTTP_GET, handleMeasurementApi);
  server.on("/api/status", HTTP_GET, handleStatusApi);
  server.onNotFound(handleNotFound);
  server.begin();
  httpServerStarted = true;

  Serial.println("[HTTP] server started on port 80");
}

void wifiEventHandler(arduino_event_t* sysEvent) {
  switch (sysEvent->event_id) {
    case ARDUINO_EVENT_WIFI_STA_START:
      if (wifiStatusMessage == "booting") {
        wifiStatusMessage = "sta_start";
      }
      wifiFailureReason = "";
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      wifiStatusMessage = "sta_connected";
      wifiFailureReason = "";
      hasEverConnected = true;
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifiStatusMessage = "connected";
      wifiFailureReason = "";
      hasEverConnected = true;
      configPortalActive = false;
      applyWifiTxPower();
      Serial.print("[WIFI] connected. IP: ");
      Serial.println(IPAddress(sysEvent->event_info.got_ip.ip_info.ip.addr));
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifiStatusMessage = "disconnected";
      wifiFailureReason = "sta_disconnected";
      if (hasEverConnected) {
        Serial.println("[WIFI] disconnected");
      }
      break;

    default:
      break;
  }
}

void configModeCallback(WiFiManager* wm) {
  configPortalActive = true;
  wifiStatusMessage = "waiting_for_wifi_credentials";
  wifiFailureReason = "";
  Serial.println("[WM] config portal started");
  Serial.print("[WM] AP SSID: ");
  Serial.println(wm->getConfigPortalSSID());
  Serial.print("[WM] AP IP: ");
  Serial.println(WiFi.softAPIP());
}

class BleServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
    (void)server;
    bleClientConnected = true;
    bleStatusMessage = "connected";
    Serial.print("[BLE] client connected: ");
    if (desc) {
      NimBLEAddress address(desc->peer_ota_addr);
      Serial.println(address.toString().c_str());
    } else {
      Serial.println("unknown");
    }
    updateMeasurementLedState(isTriggered(), isStable);
  }

  void onDisconnect(NimBLEServer* server, ble_gap_conn_desc* desc) override {
    (void)server;
    bleClientConnected = false;
    bleStatusMessage = "advertising";
    Serial.print("[BLE] client disconnected: ");
    if (desc) {
      NimBLEAddress address(desc->peer_ota_addr);
      Serial.println(address.toString().c_str());
    } else {
      Serial.println("unknown");
    }
    NimBLEDevice::startAdvertising();
    updateMeasurementLedState(isTriggered(), isStable);
  }
};

class BleControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic) override {
    std::string value = characteristic->getValue();
    String command = String(value.c_str());
    command.trim();
    command.toLowerCase();

    if (command == "mode:wifi") {
      lastBleControlMessage = "switching_to_wifi";
      characteristic->setValue(lastBleControlMessage.c_str());
      rebootToMode(RUNTIME_MODE_WIFI, "ble");
      return;
    }

    if (command == "mode:ble") {
      lastBleControlMessage = "already_in_ble";
      characteristic->setValue(lastBleControlMessage.c_str());
      return;
    }

    if (command == "reboot") {
      lastBleControlMessage = "rebooting";
      characteristic->setValue(lastBleControlMessage.c_str());
      Serial.println("[BLE] reboot requested");
      delay(200);
      ESP.restart();
      return;
    }

    if (command == "status") {
      lastBleControlMessage = buildStatusPayload();
      characteristic->setValue(lastBleControlMessage.c_str());
      return;
    }

    if (command.startsWith("ratio:")) {
      const float nextRatio = command.substring(6).toFloat();
      if (!isDividerRatioValid(nextRatio)) {
        lastBleControlMessage = "invalid_ratio";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      saveDividerRatio(nextRatio);
      resetMeasurementState();
      measurementPayload = buildMeasurementPayload();
      lastBleControlMessage = String("{\"ok\":true,\"divider_ratio\":") + String(dividerRatio, 3) + "}";
      characteristic->setValue(lastBleControlMessage.c_str());
      Serial.print("[CAL] divider ratio updated via ble: ");
      Serial.println(dividerRatio, 3);
      return;
    }

    if (command.startsWith("adc_gain:")) {
      const float nextGain = command.substring(9).toFloat();
      if (!isAdcGainValid(nextGain)) {
        lastBleControlMessage = "invalid_adc_gain";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      saveAdcCalibration(nextGain, adcOffset);
      resetMeasurementState();
      measurementPayload = buildMeasurementPayload();
      lastBleControlMessage = String("{\"ok\":true,\"adc_gain\":") + String(adcGain, 4) + "}";
      characteristic->setValue(lastBleControlMessage.c_str());
      Serial.print("[CAL] adc gain updated via ble: ");
      Serial.println(adcGain, 4);
      return;
    }

    if (command.startsWith("adc_offset:")) {
      const float nextOffset = command.substring(11).toFloat();
      if (!isAdcOffsetValid(nextOffset)) {
        lastBleControlMessage = "invalid_adc_offset";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      saveAdcCalibration(adcGain, nextOffset);
      resetMeasurementState();
      measurementPayload = buildMeasurementPayload();
      lastBleControlMessage = String("{\"ok\":true,\"adc_offset\":") + String(adcOffset, 4) + "}";
      characteristic->setValue(lastBleControlMessage.c_str());
      Serial.print("[CAL] adc offset updated via ble: ");
      Serial.println(adcOffset, 4);
      return;
    }

    if (command.startsWith("autocal:")) {
      String payload = command.substring(8);
      String adcText = "";
      String batteryText = "";
      String samplesText = "";

      const int firstComma = payload.indexOf(',');
      const int secondComma = firstComma >= 0 ? payload.indexOf(',', firstComma + 1) : -1;

      if (firstComma >= 0) {
        adcText = payload.substring(0, firstComma);
        if (secondComma >= 0) {
          batteryText = payload.substring(firstComma + 1, secondComma);
          samplesText = payload.substring(secondComma + 1);
        } else {
          batteryText = payload.substring(firstComma + 1);
        }
      } else {
        batteryText = payload;
      }

      adcText.trim();
      batteryText.trim();
      samplesText.trim();

      const bool hasActualAdc = !adcText.isEmpty();
      const bool hasActualBattery = !batteryText.isEmpty();
      const float actualAdc = hasActualAdc ? adcText.toFloat() : 0.0f;
      const float actualBattery = hasActualBattery ? batteryText.toFloat() : 0.0f;
      const int sampleCount = sanitizeAutoCalSamples(samplesText.isEmpty() ? DEFAULT_AUTO_CAL_SAMPLES : samplesText.toInt());

      if ((!hasActualAdc && !hasActualBattery) || (hasActualAdc && actualAdc <= 0.0f) || (hasActualBattery && actualBattery <= 0.0f)) {
        lastBleControlMessage = "invalid_autocal_reference";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      int rawAverage = 0;
      int milliVoltsAverage = 0;
      float rawVoltageAverage = 0.0f;
      float adcVoltageAverage = 0.0f;
      sampleAdcAverages(sampleCount, rawAverage, milliVoltsAverage, rawVoltageAverage, adcVoltageAverage);

      if (adcVoltageAverage <= 0.0f) {
        lastBleControlMessage = "adc_sample_failed";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      float nextGain = adcGain;
      float nextOffset = adcOffset;
      float nextDividerRatio = dividerRatio;

      if (hasActualAdc) {
        nextGain = (actualAdc - adcOffset) / adcVoltageAverage;
      }

      const float correctedAdcVoltage = (adcVoltageAverage * nextGain) + nextOffset;
      if (correctedAdcVoltage <= 0.0f) {
        lastBleControlMessage = "invalid_corrected_adc";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      if (hasActualBattery) {
        nextDividerRatio = (actualBattery - VOLTAGE_OFFSET) / correctedAdcVoltage;
      }

      if (!isAdcGainValid(nextGain) || !isAdcOffsetValid(nextOffset) || !isDividerRatioValid(nextDividerRatio)) {
        lastBleControlMessage = "auto_calculation_out_of_range";
        characteristic->setValue(lastBleControlMessage.c_str());
        return;
      }

      saveAdcCalibration(nextGain, nextOffset);
      saveDividerRatio(nextDividerRatio);
      resetMeasurementState();
      measurementPayload = buildMeasurementPayload();

      lastBleControlMessage =
        String("{\"ok\":true,\"samples\":") + String(sampleCount) +
        ",\"adc_gain\":" + String(adcGain, 4) +
        ",\"adc_offset\":" + String(adcOffset, 4) +
        ",\"divider_ratio\":" + String(dividerRatio, 4) + "}";
      characteristic->setValue(lastBleControlMessage.c_str());

      Serial.println("[AUTO-CAL] completed via ble");
      Serial.print("  samples: ");
      Serial.println(sampleCount);
      Serial.print("  sample raw avg: ");
      Serial.println(rawAverage);
      Serial.print("  sample adc mv avg: ");
      Serial.println(milliVoltsAverage);
      Serial.print("  sample adc voltage avg: ");
      Serial.println(adcVoltageAverage, 4);
      Serial.print("  sample adc corrected: ");
      Serial.println(correctedAdcVoltage, 4);
      if (hasActualAdc) {
        Serial.print("  actual adc ref: ");
        Serial.println(actualAdc, 4);
      }
      if (hasActualBattery) {
        Serial.print("  actual battery ref: ");
        Serial.println(actualBattery, 4);
      }
      Serial.print("  saved adc gain: ");
      Serial.println(adcGain, 4);
      Serial.print("  saved adc offset: ");
      Serial.println(adcOffset, 4);
      Serial.print("  saved divider ratio: ");
      Serial.println(dividerRatio, 4);
      return;
    }

    lastBleControlMessage = "unknown_command";
    characteristic->setValue(lastBleControlMessage.c_str());
  }
};

void setupBle() {
  bleStatusMessage = "starting";
  NimBLEDevice::init(bleDeviceName.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new BleServerCallbacks());

  NimBLEService* service = bleServer->createService(MEASUREMENT_SERVICE_UUID);
  measurementCharacteristic = service->createCharacteristic(
    MEASUREMENT_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
    MEASUREMENT_CHARACTERISTIC_MAX_LEN
  );
  controlCharacteristic = service->createCharacteristic(
    CONTROL_CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
    CONTROL_CHARACTERISTIC_MAX_LEN
  );
  controlCharacteristic->setCallbacks(new BleControlCallbacks());

  measurementPayload = buildMeasurementPayload();
  measurementCharacteristic->setValue(measurementPayload);
  controlCharacteristic->setValue(lastBleControlMessage.c_str());

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advertisingData;
  NimBLEAdvertisementData scanResponseData;

  advertisingData.setFlags(0x06);
  advertisingData.setCompleteServices(NimBLEUUID(MEASUREMENT_SERVICE_UUID));
  scanResponseData.setName(bleDeviceName.c_str());

  advertising->setAdvertisementData(advertisingData);
  advertising->setScanResponseData(scanResponseData);
  advertising->start();

  bleStatusMessage = "advertising";
  Serial.println("[BLE] advertising started");
  Serial.print("[BLE] Device name: ");
  Serial.println(bleDeviceName);
  Serial.print("[BLE] Device id: ");
  Serial.println(deviceId);
  Serial.print("[BLE] Service UUID: ");
  Serial.println(MEASUREMENT_SERVICE_UUID);
  Serial.print("[BLE] Measurement UUID: ");
  Serial.println(MEASUREMENT_CHARACTERISTIC_UUID);
  Serial.print("[BLE] Control UUID: ");
  Serial.println(CONTROL_CHARACTERISTIC_UUID);
}

void updateBleCharacteristic() {
  if (activeMode != RUNTIME_MODE_BLE || !measurementCharacteristic) {
    return;
  }

  if (measurementPayload.isEmpty()) {
    measurementPayload = buildMeasurementPayload();
  }

  measurementCharacteristic->setValue(measurementPayload);

  if (!controlCharacteristic) {
    return;
  }

  controlCharacteristic->setValue(lastBleControlMessage.c_str());

  if (!bleClientConnected) {
    return;
  }

  const bool changed = measurementPayload != lastNotifiedPayload;
  const bool intervalElapsed = millis() - lastNotifyAt >= BLE_NOTIFY_INTERVAL_MS;
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

void setupStatusIndicators() {
  if (USE_ONBOARD_LED) {
    pinMode(STATUS_LED_PIN, OUTPUT);
    applyLedState(false);
  }
}

void printBootInfo() {
  Serial.println();
  Serial.println("ESP32-C3 BattMeter Multi-Mode");
  Serial.println("========================================");
  Serial.print("[BOOT] DEVICE_ID = ");
  Serial.println(deviceId);
  Serial.print("[BOOT] DEVICE_SUFFIX = ");
  Serial.println(deviceSuffix);
  Serial.print("[BOOT] FIRMWARE_VERSION = ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("[BOOT] ACTIVE_MODE = ");
  Serial.println(runtimeModeName(activeMode));
  Serial.print("[BOOT] BLE_DEVICE_NAME = ");
  Serial.println(bleDeviceName);
  Serial.print("[BOOT] CONFIG_AP_SSID = ");
  Serial.println(configApSsid);
  Serial.print("[BOOT] TX_POWER_QDBM = ");
  Serial.println(WIFI_MAX_TX_POWER_QUARTER_DBM);
  Serial.println("[BOOT] API_MEASUREMENT = /api/measurement");
  Serial.println("[BOOT] API_STATUS = /api/status");
  Serial.println("========================================");
  Serial.print("[BOOT] VOLTAGE_PIN = ");
  Serial.println(VOLTAGE_PIN);
  Serial.print("[BOOT] STATUS_LED_PIN = ");
  Serial.println(STATUS_LED_PIN);
  Serial.print("[BOOT] BOOT_BUTTON_PIN = ");
  Serial.println(BOOT_BUTTON_PIN);
  Serial.print("[BOOT] LIFT_SW_PIN = ");
  Serial.println(LIFT_SW_PIN);
  Serial.print("[BOOT] USE_LIFT_SWITCH = ");
  Serial.println(useLiftSwitch ? "true" : "false");
  Serial.print("[BOOT] USE_ONBOARD_LED = ");
  Serial.println(USE_ONBOARD_LED ? "true" : "false");
  Serial.print("[BOOT] USE_RGB_LED = ");
  Serial.println(USE_RGB_LED ? "true" : "false");
  Serial.print("[BOOT] RGB_PIN = ");
  Serial.println(RGB_PIN);
  Serial.print("[BOOT] USE_MOCK_VOLTAGE = ");
  Serial.println(useMockVoltage ? "true" : "false");
  Serial.print("[BOOT] DIVIDER_RATIO = ");
  Serial.println(dividerRatio, 3);
  Serial.print("[BOOT] ADC_GAIN = ");
  Serial.println(adcGain, 4);
  Serial.print("[BOOT] ADC_OFFSET = ");
  Serial.println(adcOffset, 4);
  Serial.println("[BOOT] SERIAL_COMMANDS = help,info,payload,status,wifi,ble,mode,mode wifi,mode ble,clearwifi,mock on/off,lift on/off,rgb on/off/test,reboot");
  Serial.println("[BOOT] SERIAL_LINE_ENDING = newline or CRLF");
}

void logMeasurementDebug() {
  static unsigned long lastLogAt = 0;
  if ((!serialDebugEnabled && pendingDebugSamples <= 0) || millis() - lastLogAt < 1000) {
    return;
  }

  lastLogAt = millis();

  if (useMockVoltage) {
    Serial.println("[ADC] mock mode");
    Serial.print("  calculated battery voltage: ");
    Serial.print(readBatteryVoltage(), 3);
    Serial.println(" V");
  } else {
    const int raw = analogRead(VOLTAGE_PIN);
    const int milliVolts = analogReadMilliVolts(VOLTAGE_PIN);
    const float adcVoltageFromRaw = (static_cast<float>(raw) / ADC_MAX) * ADC_REFERENCE_VOLTAGE;
    const float adcVoltageFromMilliVolts = static_cast<float>(milliVolts) / 1000.0f;
    const float adcVoltageCorrected = applyAdcCalibration(adcVoltageFromMilliVolts);
    const float batteryVoltage = (adcVoltageCorrected * dividerRatio) + VOLTAGE_OFFSET;

    Serial.println("[ADC] sample");
    Serial.println("  raw input");
    Serial.print("    adcRaw: ");
    Serial.println(raw);
    Serial.print("    adcVoltageFromRaw: ");
    Serial.print(adcVoltageFromRaw, 3);
    Serial.println(" V");
    Serial.println("  analogReadMilliVolts");
    Serial.print("    adcMv: ");
    Serial.println(milliVolts);
    Serial.print("    adcVoltage: ");
    Serial.print(adcVoltageFromMilliVolts, 3);
    Serial.println(" V");
    Serial.println("  adc calibration");
    Serial.print("    adcGain: ");
    Serial.println(adcGain, 4);
    Serial.print("    adcOffset: ");
    Serial.println(adcOffset, 4);
    Serial.print("    adcCorrected: ");
    Serial.print(adcVoltageCorrected, 3);
    Serial.println(" V");
    Serial.println("  calculated battery");
    Serial.print("    dividerRatio: ");
    Serial.println(dividerRatio, 3);
    Serial.print("    voltageOffset: ");
    Serial.println(VOLTAGE_OFFSET, 3);
    Serial.print("    batteryVoltage: ");
    Serial.print(batteryVoltage, 3);
    Serial.println(" V");
  }

  Serial.println("  filtered result");
  Serial.print("    voltageAvg: ");
  Serial.print(lastAverageVoltage, 3);
  Serial.println(" V");
  Serial.print("    stableHits: ");
  Serial.print(stableHits);
  Serial.print("    stable: ");
  Serial.print(isStable ? "true" : "false");
  Serial.print("    triggered: ");
  Serial.println(isTriggered() ? "true" : "false");

  if (pendingDebugSamples > 0) {
    pendingDebugSamples -= 1;
  }

  if (pendingDebugSamples == 0) {
    serialDebugEnabled = false;
  }
}

void printMeasurementStatus() {
  Serial.println("[INFO] Measurement");
  Serial.println("  state");
  Serial.print("    state: ");
  Serial.println(isTriggered() ? (isStable ? "ready" : "measuring") : "idle");
  Serial.println("  raw input");
  Serial.print("    adcRaw: ");
  Serial.println(lastAdcRaw);
  Serial.print("    adcVoltageFromRaw: ");
  Serial.print(lastAdcVoltageFromRaw, 3);
  Serial.println(" V");
  Serial.println("  analogReadMilliVolts");
  Serial.print("    adcMv: ");
  Serial.println(lastAdcMilliVolts);
  Serial.print("    adcVoltage: ");
  Serial.print(lastAdcVoltageFromMilliVolts, 3);
  Serial.println(" V");
  Serial.println("  adc calibration");
  Serial.print("    adcGain: ");
  Serial.println(adcGain, 4);
  Serial.print("    adcOffset: ");
  Serial.println(adcOffset, 4);
  Serial.print("    adcCorrected: ");
  Serial.print(lastAdcVoltageCorrected, 3);
  Serial.println(" V");
  Serial.println("  calculated battery");
  Serial.print("    dividerRatio: ");
  Serial.println(dividerRatio, 3);
  Serial.print("    voltageOffset: ");
  Serial.println(VOLTAGE_OFFSET, 3);
  Serial.print("    voltageAvg: ");
  Serial.print(lastAverageVoltage, 3);
  Serial.println(" V");
  Serial.println("  filter / status");
  Serial.print("    stableHits: ");
  Serial.println(stableHits);
  Serial.print("    sampledAtMs: ");
  Serial.println(sampledAtMs);
  Serial.print("    liftSwitchEnabled: ");
  Serial.println(useLiftSwitch ? "true" : "false");
  Serial.print("    mockVoltageEnabled: ");
  Serial.println(useMockVoltage ? "true" : "false");
}

void printPayloadInfo() {
  if (measurementPayload.isEmpty()) {
    measurementPayload = buildMeasurementPayload();
  }

  Serial.print("[PAYLOAD] ");
  Serial.println(measurementPayload);
}

void printWifiInfo() {
  Serial.println("[INFO] WiFi");
  Serial.print("  activeMode: ");
  Serial.println(runtimeModeName(activeMode));
  Serial.print("  status: ");
  Serial.println(wifiStatusMessage);
  Serial.print("  failureReason: ");
  Serial.println(wifiFailureReason);
  Serial.print("  connected: ");
  Serial.println(isWifiConnected() ? "true" : "false");
  Serial.print("  ssid: ");
  Serial.println(isWifiConnected() ? maskSsid(WiFi.SSID()) : "");
  Serial.print("  ip: ");
  Serial.println(isWifiConnected() ? WiFi.localIP().toString() : "");
  Serial.print("  configPortalActive: ");
  Serial.println(configPortalActive ? "true" : "false");
  Serial.print("  configPortalSsid: ");
  Serial.println(configPortalActive ? configApSsid : "");
}

void printBleInfo() {
  Serial.println("[INFO] BLE");
  Serial.print("  activeMode: ");
  Serial.println(runtimeModeName(activeMode));
  Serial.print("  status: ");
  Serial.println(bleStatusMessage);
  Serial.print("  connected: ");
  Serial.println(bleClientConnected ? "true" : "false");
  Serial.print("  name: ");
  Serial.println(bleDeviceName);
  Serial.print("  serviceUuid: ");
  Serial.println(MEASUREMENT_SERVICE_UUID);
  Serial.print("  measurementUuid: ");
  Serial.println(MEASUREMENT_CHARACTERISTIC_UUID);
  Serial.print("  controlUuid: ");
  Serial.println(CONTROL_CHARACTERISTIC_UUID);
}

void printHelp() {
  Serial.println("[HELP] Commands");
  Serial.println("  help");
  Serial.println("  info");
  Serial.println("  payload");
  Serial.println("  status");
  Serial.println("  wifi");
  Serial.println("  ble");
  Serial.println("  mode");
  Serial.println("  mode wifi");
  Serial.println("  mode ble");
  Serial.println("  clearwifi");
  Serial.println("  debug once");
  Serial.println("  debug <n>");
  Serial.println("  rgb on");
  Serial.println("  rgb off");
  Serial.println("  rgb test");
  Serial.println("  mock on");
  Serial.println("  mock off");
  Serial.println("  lift on");
  Serial.println("  lift off");
  Serial.println("  reboot");
  Serial.println("[OK] help");
}

void setRgbManualEnabled(bool enabled) {
  rgbManualEnabled = enabled;
  rgbTestEnabled = false;

  if (!enabled) {
    setRgbColor(CRGB::Black);
    updateMeasurementLedState(isTriggered(), isStable);
    return;
  }

  setRgbColor(CRGB::White);
  applyLedState(true);
}

void startRgbTest() {
  rgbManualEnabled = false;
  rgbTestEnabled = true;
  rgbTestIndex = 0;
  lastRgbTestAt = 0;
}

void updateRgbTest() {
  if (!rgbTestEnabled) {
    return;
  }

  if (millis() - lastRgbTestAt < 450) {
    return;
  }

  lastRgbTestAt = millis();
  switch (rgbTestIndex % 3) {
    case 0:
      setRgbColor(CRGB::Red);
      break;
    case 1:
      setRgbColor(CRGB::Green);
      break;
    default:
      setRgbColor(CRGB::Blue);
      break;
  }
  rgbTestIndex += 1;
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

void clearWifiAndRestart() {
  Serial.println("[WIFI] clearing stored credentials");
  wifiManager.resetSettings();
  WiFi.disconnect(true, true);
  delay(200);
  ESP.restart();
}

void printModeInfo() {
  Serial.println("[INFO] Mode");
  Serial.print("  active: ");
  Serial.println(runtimeModeName(activeMode));
  Serial.println("  switch with: mode wifi | mode ble");
  Serial.println("[OK] mode");
}

void handleSerialCommand(const String& rawCommand) {
  Serial.print("[CMD] ");
  Serial.println(rawCommand);

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
    printModeInfo();
    printWifiInfo();
    printBleInfo();
    printMeasurementStatus();
    printPayloadInfo();
    Serial.println("[OK] info");
    return;
  }

  if (normalizedCommand == "payload") {
    printPayloadInfo();
    Serial.println("[OK] payload");
    return;
  }

  if (normalizedCommand == "status") {
    printMeasurementStatus();
    Serial.println("[OK] status");
    return;
  }

  if (normalizedCommand == "wifi") {
    printWifiInfo();
    Serial.println("[OK] wifi");
    return;
  }

  if (normalizedCommand == "ble") {
    printBleInfo();
    Serial.println("[OK] ble");
    return;
  }

  if (normalizedCommand == "mode") {
    printModeInfo();
    return;
  }

  if (normalizedCommand == "mode wifi") {
    if (activeMode == RUNTIME_MODE_WIFI) {
      Serial.println("[MODE] already in wifi");
      return;
    }
    rebootToMode(RUNTIME_MODE_WIFI, "serial");
    return;
  }

  if (normalizedCommand == "mode ble") {
    if (activeMode == RUNTIME_MODE_BLE) {
      Serial.println("[MODE] already in ble");
      return;
    }
    rebootToMode(RUNTIME_MODE_BLE, "serial");
    return;
  }

  if (normalizedCommand == "clearwifi") {
    clearWifiAndRestart();
    return;
  }

  if (normalizedCommand == "debug once") {
    pendingDebugSamples = 1;
    serialDebugEnabled = false;
    Serial.println("[DEBUG] queued 1 debug sample");
    Serial.println("[OK] debug");
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
      Serial.println("[OK] debug");
      return;
    }
  }

  if (normalizedCommand == "rgb on") {
    setRgbManualEnabled(true);
    Serial.println("[CONFIG] rgb manual white");
    Serial.println("[OK] rgb");
    return;
  }

  if (normalizedCommand == "rgb off") {
    setRgbManualEnabled(false);
    Serial.println("[CONFIG] rgb off");
    Serial.println("[OK] rgb");
    return;
  }

  if (normalizedCommand == "rgb test") {
    startRgbTest();
    Serial.println("[CONFIG] rgb test red->green->blue");
    Serial.println("[OK] rgb");
    return;
  }

  if (normalizedCommand == "mock on") {
    setMockVoltageEnabled(true);
    Serial.println("[CONFIG] mock voltage enabled");
    Serial.println("[OK] mock");
    return;
  }

  if (normalizedCommand == "mock off") {
    setMockVoltageEnabled(false);
    Serial.println("[CONFIG] mock voltage disabled");
    Serial.println("[OK] mock");
    return;
  }

  if (normalizedCommand == "lift on") {
    setLiftSwitchEnabled(true);
    Serial.println("[CONFIG] lift switch enabled");
    Serial.println("[OK] lift");
    return;
  }

  if (normalizedCommand == "lift off") {
    setLiftSwitchEnabled(false);
    Serial.println("[CONFIG] lift switch disabled");
    Serial.println("[OK] lift");
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
  auto readFromStream = [&](Stream& stream) {
    while (stream.available() > 0) {
      const char nextChar = static_cast<char>(stream.read());

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
  };

  readFromStream(Serial);
}

void startWifiManagerPortal() {
  configPortalActive = false;
  wifiStatusMessage = "connecting_or_waiting_for_credentials";
  wifiFailureReason = "";

  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setConfigPortalTimeout(0);
  wifiManager.setConnectTimeout(15);
  wifiManager.setWiFiAutoReconnect(true);
  wifiManager.setShowStaticFields(false);
  wifiManager.setShowDnsFields(false);
  wifiManager.setConfigPortalBlocking(false);

  Serial.println("[WM] starting WiFiManager");
  Serial.print("[WM] portal SSID: ");
  Serial.println(configApSsid);

  const bool connected = wifiManager.autoConnect(configApSsid.c_str(), CONFIG_AP_PASSWORD);
  if (connected) {
    configPortalActive = false;
    wifiManagerPortalStarted = false;
    wifiStatusMessage = "connected";
    wifiFailureReason = "";
    hasEverConnected = true;
    applyWifiTxPower();
    Serial.print("[WIFI] connected. IP: ");
    Serial.println(WiFi.localIP());
    return;
  }

  wifiManagerPortalStarted = true;
  wifiStatusMessage = "waiting_for_wifi_credentials";
  wifiFailureReason = "";
  Serial.println("[WM] config portal running in background");
}

void setupWifiMode() {
  wifiStatusMessage = "starting";
  bleStatusMessage = "inactive";
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.onEvent(wifiEventHandler);
  applyWifiTxPower();
  startWifiManagerPortal();
}

void setupBleMode() {
  wifiStatusMessage = "inactive";
  wifiFailureReason = "";
  configPortalActive = false;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  setupBle();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.setTimeout(20);

  loadRuntimeMode();
  loadDividerRatio();
  loadAdcCalibration();
  setupDeviceIdentity();
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  checkBootButtonModeOverride();
  printBootInfo();
  pinMode(LIFT_SW_PIN, INPUT_PULLUP);

  setupAdc();
  setupStatusIndicators();
  measurementPayload = buildMeasurementPayload();

  if (activeMode == RUNTIME_MODE_WIFI) {
    setupWifiMode();
    if (isWifiConnected()) {
      startHttpServer();
    }
  } else {
    setupBleMode();
  }

  systemReady = true;
  rgbBootAnimationPending = true;
  Serial.println("[READY] serial command input active");
}

void loop() {
  handleSerialInput();

  if (rgbBootAnimationPending) {
    rgbBootAnimationPending = false;
    bootColorHoldUntil = millis() + BOOT_COLOR_HOLD_MS;
    updateMeasurementLedState(isTriggered(), isStable);
  }

  if (activeMode == RUNTIME_MODE_WIFI) {
    if (wifiManagerPortalStarted && !isWifiConnected() && millis() - lastWifiManagerProcessAt >= 20) {
      lastWifiManagerProcessAt = millis();
      wifiManager.process();
    }

    if (isWifiConnected() && wifiManagerPortalStarted) {
      wifiManagerPortalStarted = false;
      configPortalActive = false;
      wifiStatusMessage = "connected";
      wifiFailureReason = "";
      applyWifiTxPower();
      Serial.print("[WIFI] connected. IP: ");
      Serial.println(WiFi.localIP());
    }
  }

  updateModeLedPattern();
  handleSerialInput();

  updateRgbTest();
  updateMeasurement();

  if (activeMode == RUNTIME_MODE_WIFI && isWifiConnected() && !httpServerStarted) {
    startHttpServer();
  }

  if (activeMode == RUNTIME_MODE_WIFI && httpServerStarted) {
    server.handleClient();
  }

  if (activeMode == RUNTIME_MODE_BLE) {
    updateBleCharacteristic();
  }

  logMeasurementDebug();
}
