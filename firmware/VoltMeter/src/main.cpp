#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Preferences.h>
#include <TM1637Display.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include <math.h>

const char* DEVICE_ID_PREFIX = "PUMA-VoltMeter-";
const char* BLE_DEVICE_NAME_PREFIX = "PUMA-Voltmeter-";
const char* FIRMWARE_VERSION = "2026.07.12";
const char* BLE_VOLTMETER_SERVICE_UUID = "7f9e0001-6a9d-4f7e-8d4d-32e7be6f1001";
const char* BLE_VOLTMETER_STATUS_CHARACTERISTIC_UUID = "7f9e0002-6a9d-4f7e-8d4d-32e7be6f1001";
const char* BLE_VOLTMETER_CALIBRATION_CHARACTERISTIC_UUID = "7f9e0003-6a9d-4f7e-8d4d-32e7be6f1001";

const char* CONFIG_AP_SSID_PREFIX = "BattMeter-Setup-";
const char* CONFIG_AP_PASSWORD = "12345678";

const char* PREF_NAMESPACE = "runtime";
const char* PREF_MODE_KEY = "mode";
const char* PREF_DIVIDER_RATIO_KEY = "divider";
const char* PREF_ADC_GAIN_KEY = "adc_gain";
const char* PREF_ADC_OFFSET_KEY = "adc_off";
const char* PREF_INA_GAIN_KEY = "ina_gain";
const char* PREF_INA_OFFSET_KEY = "ina_off";
const char* PREF_DISPLAY_BRIGHTNESS_KEY = "disp_br";
const char* PREF_BOOT_SELFTEST_KEY = "boot_st";
const char* PREF_ZERO_CLAMP_KEY = "zero_cl";
const char* PREF_ACTION_BUTTON_PIN_KEY = "btn_pin";

const uint8_t KEYBOARD_REPORT_ID = 0x01;
const uint8_t KEY_MOD_NONE = 0x00;
const uint8_t KEY_CODE_ENTER = 0x28;
const uint8_t KEY_CODE_MINUS = 0x2D;
const uint8_t KEY_CODE_DOT = 0x37;

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
const int DEFAULT_ACTION_BUTTON_PIN = 1;
const int RGB_PIN = A2;
const int BUZZER_PIN = 3;
const int BOOT_BUTTON_PIN = A9;
const int STATUS_LED_PIN = 8;
const int TM1637_CLK_PIN = 20;
const int TM1637_DIO_PIN = 21;

#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 8
#endif

#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 9
#endif

const uint8_t INA226_I2C_ADDRESS = 0x40;
const uint8_t INA226_REG_CONFIG = 0x00;
const uint8_t INA226_REG_BUS_VOLTAGE = 0x02;
const uint8_t INA226_REG_MANUFACTURER_ID = 0xFE;
const uint8_t INA226_REG_DIE_ID = 0xFF;
const uint16_t INA226_EXPECTED_MANUFACTURER_ID = 0x5449;
const uint16_t INA226_EXPECTED_DIE_ID = 0x2260;
const uint16_t INA226_CONFIG_DEFAULT = 0x4527;

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
const float DEFAULT_INA_GAIN = 0.98277f;
const float DEFAULT_INA_OFFSET = 0.00743f;
const float VOLTAGE_OFFSET = 0.00f;
const float DEFAULT_ZERO_CLAMP_THRESHOLD = 0.02f;
const float MIN_ACTIVE_MEASUREMENT_VOLTAGE = 0.05f;

const unsigned long SAMPLE_INTERVAL_MS = 120;
const int SAMPLE_WINDOW = 8;
const float STABLE_DELTA = 0.12f;
const int STABLE_HITS_REQUIRED = 3;
const int DEFAULT_AUTO_CAL_SAMPLES = 24;
const unsigned long HID_KEY_PRESS_MS = 12;
const unsigned long HID_KEY_RELEASE_MS = 10;
const unsigned long HID_TYPE_GUARD_MS = 32;
const unsigned long SEND_ARM_DEBOUNCE_MS = 30;
const unsigned long ACTION_BUTTON_DEBOUNCE_MS = 25;
const unsigned long BLE_HOST_SETTLE_MS = 4000;
const unsigned long MODE_LED_BLINK_ON_MS = 45;
const unsigned long MODE_LED_BLINK_OFF_MS = 170;
const unsigned long MODE_LED_PATTERN_GAP_MS = 1200;
const unsigned long DISPLAY_REFRESH_MS = 160;
const unsigned long DISPLAY_VALUE_HOLD_MS = 2000;
const unsigned long STANDBY_TIMEOUT_MS = 60000;
const unsigned long STANDBY_ANIMATION_INTERVAL_MS = 140;
const uint8_t DEFAULT_DISPLAY_BRIGHTNESS = 5;
const bool DEFAULT_BOOT_SELFTEST_ENABLED = true;
const uint8_t STANDBY_DISPLAY_BRIGHTNESS = 1;

enum RuntimeMode {
  RUNTIME_MODE_WIFI = 0,
  RUNTIME_MODE_BLE = 1,
  RUNTIME_MODE_CALIBRATION = 2,
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
float lastDisplayVoltage = NAN;
float heldDisplayVoltage = NAN;
float pendingHidVoltage = NAN;
float dividerRatio = DEFAULT_DIVIDER_RATIO;
float adcGain = DEFAULT_ADC_GAIN;
float adcOffset = DEFAULT_ADC_OFFSET;
float inaGain = DEFAULT_INA_GAIN;
float inaOffset = DEFAULT_INA_OFFSET;
int stableHits = 0;
bool isStable = false;
unsigned long lastSampleAt = 0;
unsigned long lastNotifyAt = 0;
unsigned long sampledAtMs = 0;

String measurementPayload = "";
String deviceId = "";
String deviceSuffix = "";
String bleDeviceName = "";
String configApSsid = "";
String wifiStatusMessage = "booting";
String wifiFailureReason = "";
String bleStatusMessage = "inactive";
String serialInputBuffer = "";
String lastKeyboardTypedValue = "";
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
bool displayAvailable = false;
bool hidSendArmed = false;
bool hidTypedDuringCurrentCycle = false;
bool displayHoldActive = false;
bool waitForNewMeasurementCycleAfterSend = false;
bool stableReadyForSend = false;
bool lastBootButtonReading = false;
bool lastActionButtonRawState = true;
bool actionButtonPressedState = false;
bool actionButtonDebounceCandidateState = false;
bool actionButtonDebounceActive = false;
unsigned long bootButtonPressedAt = 0;
unsigned long actionButtonDebounceStartedAt = 0;
unsigned long stableReadHoldUntil = 0;
unsigned long bootColorHoldUntil = 0;
unsigned long lastModeLedStepAt = 0;
unsigned long bleConnectedAt = 0;
unsigned long lastDisplayRefreshAt = 0;
unsigned long displayValueHoldUntil = 0;
unsigned long lastStandbyAnimationAt = 0;
unsigned long lastMeasurementSeenAt = 0;
uint8_t modeLedStep = 0;
uint8_t standbyAnimationIndex = 0;
uint8_t lastAppliedDisplayBrightness = 0xFF;
bool modeLedPatternEnabled = true;
volatile bool actionButtonInterruptPressedState = false;
volatile bool actionButtonInterruptPending = false;

const unsigned long BOOT_COLOR_HOLD_MS = 1200;
const unsigned long STABLE_READ_COLOR_HOLD_MS = 1400;
const unsigned long SELF_TEST_COLOR_MS = 180;
const unsigned long SELF_TEST_RESULT_MS = 700;
const unsigned long SELF_TEST_BUZZ_MS = 90;
const unsigned long SELF_TEST_STEP_GAP_MS = 140;
const unsigned long SELF_TEST_SWITCH_TIMEOUT_MS = 5000;
const unsigned long SELF_TEST_SWITCH_HOLD_MS = 50;

Preferences preferences;
WiFiManager wifiManager;
WebServer server(80);
TM1637Display voltageDisplay(TM1637_CLK_PIN, TM1637_DIO_PIN);
BLEServer* bleServer = nullptr;
BLEHIDDevice* hidDevice = nullptr;
BLECharacteristic* inputKeyboard = nullptr;
BLECharacteristic* outputKeyboard = nullptr;
BLEService* voltMeterService = nullptr;
BLECharacteristic* voltMeterStatusCharacteristic = nullptr;
BLECharacteristic* voltMeterCalibrationCharacteristic = nullptr;

bool ina226Available = false;
bool bootSelfTestPassed = false;
bool bootSelfTestEnabled = DEFAULT_BOOT_SELFTEST_ENABLED;
float lastInaBusVoltage = 0.0f;
uint16_t ina226ManufacturerId = 0;
uint16_t ina226DieId = 0;
uint8_t displayBrightness = DEFAULT_DISPLAY_BRIGHTNESS;
float zeroClampThreshold = DEFAULT_ZERO_CLAMP_THRESHOLD;
int actionButtonPin = DEFAULT_ACTION_BUTTON_PIN;

struct BootSelfTestResult {
  bool rgbOk = false;
  bool buzzerOk = false;
  bool switchOk = false;
  bool inaOk = false;
  bool switchPressed = false;
  float busVoltage = 0.0f;
};

BootSelfTestResult bootSelfTestResult;

void rebootToMode(RuntimeMode mode, const char* source);
void IRAM_ATTR handleActionButtonInterrupt();
void syncActionButtonStateFromInterrupt();
void setupSevenSegmentDisplay();
void applyDisplayBrightness();
void applyDisplayBrightness(uint8_t brightnessLevel);
void loadDisplayBrightness();
void saveDisplayBrightness(uint8_t brightness);
void loadBootSelfTestEnabled();
void saveBootSelfTestEnabled(bool enabled);
void loadZeroClampThreshold();
void saveZeroClampThreshold(float threshold);
bool isActionButtonPinAllowed(int pin);
void loadActionButtonPin();
void saveActionButtonPin(int pin);
void configureActionButtonPin();
void displayStandbyAnimation();
void updateSevenSegmentDisplay();
void displayBootMessage();
void displaySelfTestResult(bool passed);
void displayModeMessage();
void displayVoltageValue(float voltage, bool stable);
void displayTextValue(const String& text);
String formatVoltageForDisplay(float voltage, bool stable);
uint8_t encodeDisplayCharacter(char value);
void writeDisplayString(const String& text);
bool readActionButtonPinPressed();
bool hasLiveMeasurement();
void updateMeasurementLedState(bool triggered, bool stable);

static const uint8_t KEYBOARD_REPORT_MAP[] = {
  USAGE_PAGE(1), 0x01,
  USAGE(1), 0x06,
  COLLECTION(1), 0x01,
  REPORT_ID(1), KEYBOARD_REPORT_ID,
  USAGE_PAGE(1), 0x07,
  USAGE_MINIMUM(1), 0xE0,
  USAGE_MAXIMUM(1), 0xE7,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1), 0x01,
  REPORT_COUNT(1), 0x08,
  HIDINPUT(1), 0x02,
  REPORT_COUNT(1), 0x01,
  REPORT_SIZE(1), 0x08,
  HIDINPUT(1), 0x01,
  REPORT_COUNT(1), 0x05,
  REPORT_SIZE(1), 0x01,
  USAGE_PAGE(1), 0x08,
  USAGE_MINIMUM(1), 0x01,
  USAGE_MAXIMUM(1), 0x05,
  HIDOUTPUT(1), 0x02,
  REPORT_COUNT(1), 0x01,
  REPORT_SIZE(1), 0x03,
  HIDOUTPUT(1), 0x01,
  REPORT_COUNT(1), 0x06,
  REPORT_SIZE(1), 0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x65,
  USAGE_PAGE(1), 0x07,
  USAGE_MINIMUM(1), 0x00,
  USAGE_MAXIMUM(1), 0x65,
  HIDINPUT(1), 0x00,
  END_COLLECTION(0)
};

String buildMeasurementPayload();
void resetMeasurementState();
bool isActionButtonPressedRaw();
bool isTriggered();
bool isWifiConnected();
void runBootSelfTest();
bool readIna226Register(uint8_t reg, uint16_t& value);
bool writeIna226Register(uint8_t reg, uint16_t value);
bool initIna226();
float readIna226BusVoltage();
void buzz(unsigned long durationMs);
void selfTestStep(const __FlashStringHelper* label, const CRGB& color, unsigned long holdMs = SELF_TEST_STEP_GAP_MS);
void selfTestPassTone();
void selfTestFailTone();
bool waitForActionButtonPress(unsigned long timeoutMs);
void setRgbColor(const CRGB& color);
void applyLedState(bool enabled);
bool isInaGainValid(float value);
bool isInaOffsetValid(float value);
void loadInaCalibration();
void saveInaCalibration(float gain, float offset);
float applyInaCalibration(float voltage);
float applyZeroClamp(float voltage);
String buildBleVoltMeterStatusPayload();
void updateBleVoltMeterStatusCharacteristic(bool notify = false);

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

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

const char* runtimeModeName(RuntimeMode mode) {
  if (mode == RUNTIME_MODE_BLE) {
    return "ble";
  }

  if (mode == RUNTIME_MODE_CALIBRATION) {
    return "calibration";
  }

  return "wifi";
}

RuntimeMode parseRuntimeMode(const String& rawMode) {
  String mode = rawMode;
  mode.trim();
  mode.toLowerCase();
  if (mode == "ble") {
    return RUNTIME_MODE_BLE;
  }
  if (mode == "calibration" || mode == "cal") {
    return RUNTIME_MODE_CALIBRATION;
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

bool isInaGainValid(float value) {
  return !isnan(value) && value >= 0.8f && value <= 1.2f;
}

bool isInaOffsetValid(float value) {
  return !isnan(value) && value >= -1.0f && value <= 1.0f;
}

void setupBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzz(unsigned long durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

void selfTestStep(const __FlashStringHelper* label, const CRGB& color, unsigned long holdMs) {
  Serial.print(F("[SELFTEST] "));
  Serial.println(label);
  setRgbColor(color);
  applyLedState(true);
  displayTextValue(String(label));
  delay(holdMs);
}

void selfTestPassTone() {
  buzz(60);
  delay(70);
  buzz(90);
}

void selfTestFailTone() {
  buzz(180);
  delay(120);
  buzz(180);
}

bool readActionButtonPinPressed() {
  return digitalRead(actionButtonPin) == LOW;
}

void IRAM_ATTR handleActionButtonInterrupt() {
  actionButtonInterruptPressedState = digitalRead(actionButtonPin) == LOW;
  actionButtonInterruptPending = true;
}

void syncActionButtonStateFromInterrupt() {
  bool pending = false;
  bool pressed = false;

  noInterrupts();
  pending = actionButtonInterruptPending;
  pressed = actionButtonInterruptPressedState;
  if (pending) {
    actionButtonInterruptPending = false;
  }
  interrupts();

  if (pending) {
    actionButtonDebounceCandidateState = pressed;
    actionButtonDebounceStartedAt = millis();
    actionButtonDebounceActive = true;
  }

  if (!actionButtonDebounceActive) {
    return;
  }

  if (millis() - actionButtonDebounceStartedAt < ACTION_BUTTON_DEBOUNCE_MS) {
    return;
  }

  const bool nextPressedState = actionButtonDebounceCandidateState;
  if (nextPressedState != actionButtonPressedState) {
    Serial.print("[BUTTON] debounced ");
    Serial.println(nextPressedState ? "PRESS" : "RELEASE");
  }

  actionButtonPressedState = nextPressedState;
  actionButtonDebounceActive = false;
}

bool waitForActionButtonPress(unsigned long timeoutMs) {
  const unsigned long startedAt = millis();

  while (millis() - startedAt < timeoutMs) {
    syncActionButtonStateFromInterrupt();
    if (isActionButtonPressedRaw()) {
      delay(SELF_TEST_SWITCH_HOLD_MS);
      syncActionButtonStateFromInterrupt();
      if (isActionButtonPressedRaw()) {
        while (isActionButtonPressedRaw()) {
          syncActionButtonStateFromInterrupt();
          delay(10);
        }
        return true;
      }
    }

    delay(10);
  }

  return false;
}

bool waitForActionButtonRelease(unsigned long timeoutMs) {
  const unsigned long startedAt = millis();

  while (millis() - startedAt < timeoutMs) {
    syncActionButtonStateFromInterrupt();
    if (!isActionButtonPressedRaw()) {
      delay(20);
      syncActionButtonStateFromInterrupt();
      return !isActionButtonPressedRaw();
    }

    delay(10);
  }

  return !isActionButtonPressedRaw();
}

void applyDisplayBrightness() {
  applyDisplayBrightness(displayBrightness);
}

void applyDisplayBrightness(uint8_t brightnessLevel) {
  const uint8_t normalizedBrightness = brightnessLevel <= 7 ? brightnessLevel : DEFAULT_DISPLAY_BRIGHTNESS;
  if (lastAppliedDisplayBrightness == normalizedBrightness) {
    return;
  }

  voltageDisplay.setBrightness(normalizedBrightness, true);
  lastAppliedDisplayBrightness = normalizedBrightness;
}

void loadDisplayBrightness() {
  preferences.begin(PREF_NAMESPACE, true);
  const uint8_t storedBrightness = preferences.getUChar(PREF_DISPLAY_BRIGHTNESS_KEY, DEFAULT_DISPLAY_BRIGHTNESS);
  preferences.end();
  displayBrightness = storedBrightness <= 7 ? storedBrightness : DEFAULT_DISPLAY_BRIGHTNESS;
}

void saveDisplayBrightness(uint8_t brightness) {
  displayBrightness = brightness <= 7 ? brightness : DEFAULT_DISPLAY_BRIGHTNESS;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUChar(PREF_DISPLAY_BRIGHTNESS_KEY, displayBrightness);
  preferences.end();
  applyDisplayBrightness();
}

void loadBootSelfTestEnabled() {
  preferences.begin(PREF_NAMESPACE, true);
  bootSelfTestEnabled = preferences.getBool(PREF_BOOT_SELFTEST_KEY, DEFAULT_BOOT_SELFTEST_ENABLED);
  preferences.end();
}

void saveBootSelfTestEnabled(bool enabled) {
  bootSelfTestEnabled = enabled;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putBool(PREF_BOOT_SELFTEST_KEY, bootSelfTestEnabled);
  preferences.end();
}

void loadZeroClampThreshold() {
  preferences.begin(PREF_NAMESPACE, true);
  const float storedThreshold = preferences.getFloat(PREF_ZERO_CLAMP_KEY, DEFAULT_ZERO_CLAMP_THRESHOLD);
  preferences.end();
  zeroClampThreshold = (!isnan(storedThreshold) && storedThreshold >= 0.0f && storedThreshold <= 1.0f)
    ? storedThreshold
    : DEFAULT_ZERO_CLAMP_THRESHOLD;
}

void saveZeroClampThreshold(float threshold) {
  zeroClampThreshold = (!isnan(threshold) && threshold >= 0.0f && threshold <= 1.0f)
    ? threshold
    : DEFAULT_ZERO_CLAMP_THRESHOLD;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putFloat(PREF_ZERO_CLAMP_KEY, zeroClampThreshold);
  preferences.end();
}

bool isActionButtonPinAllowed(int pin) {
  if (pin < 0 || pin > 21) {
    return false;
  }

  if (pin == VOLTAGE_PIN || pin == RGB_PIN || pin == BUZZER_PIN || pin == BOOT_BUTTON_PIN
    || pin == STATUS_LED_PIN || pin == TM1637_CLK_PIN || pin == TM1637_DIO_PIN
    || pin == I2C_SDA_PIN || pin == I2C_SCL_PIN) {
    return false;
  }

  return true;
}

void loadActionButtonPin() {
  preferences.begin(PREF_NAMESPACE, true);
  const int storedPin = preferences.getInt(PREF_ACTION_BUTTON_PIN_KEY, DEFAULT_ACTION_BUTTON_PIN);
  preferences.end();
  actionButtonPin = isActionButtonPinAllowed(storedPin) ? storedPin : DEFAULT_ACTION_BUTTON_PIN;
}

void saveActionButtonPin(int pin) {
  actionButtonPin = isActionButtonPinAllowed(pin) ? pin : DEFAULT_ACTION_BUTTON_PIN;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putInt(PREF_ACTION_BUTTON_PIN_KEY, actionButtonPin);
  preferences.end();
}

void configureActionButtonPin() {
  pinMode(actionButtonPin, INPUT_PULLUP);
  actionButtonPressedState = readActionButtonPinPressed();
  actionButtonDebounceCandidateState = actionButtonPressedState;
  lastActionButtonRawState = actionButtonPressedState;
  actionButtonInterruptPressedState = actionButtonPressedState;
  attachInterrupt(digitalPinToInterrupt(actionButtonPin), handleActionButtonInterrupt, CHANGE);
}

void setupSevenSegmentDisplay() {
  applyDisplayBrightness();
  voltageDisplay.clear();
  displayAvailable = true;
}

uint8_t encodeDisplayCharacter(char value) {
  switch (value) {
    case '0': return 0x3f;
    case '1': return 0x06;
    case '2': return 0x5b;
    case '3': return 0x4f;
    case '4': return 0x66;
    case '5': return 0x6d;
    case '6': return 0x7d;
    case '7': return 0x07;
    case '8': return 0x7f;
    case '9': return 0x6f;
    case 'A':
    case 'a': return 0x77;
    case 'B':
    case 'b': return 0x7c;
    case 'C':
    case 'c': return 0x39;
    case 'D':
    case 'd': return 0x5e;
    case 'E':
    case 'e': return 0x79;
    case 'F':
    case 'f': return 0x71;
    case 'G':
    case 'g': return 0x3d;
    case 'H':
    case 'h': return 0x76;
    case 'I':
    case 'i': return 0x06;
    case 'J':
    case 'j': return 0x1e;
    case 'L':
    case 'l': return 0x38;
    case 'N':
    case 'n': return 0x54;
    case 'O':
    case 'o': return 0x3f;
    case 'P':
    case 'p': return 0x73;
    case 'R':
    case 'r': return 0x50;
    case 'S':
    case 's': return 0x6d;
    case 'T':
    case 't': return 0x78;
    case 'U':
    case 'u': return 0x3e;
    case 'Y':
    case 'y': return 0x6e;
    case '-': return 0x40;
    case '_': return 0x08;
    case ' ': return 0x00;
    default: return 0x00;
  }
}

void writeDisplayString(const String& text) {
  uint8_t segments[4] = { 0, 0, 0, 0 };
  uint8_t digitIndex = 0;

  for (size_t charIndex = 0; charIndex < text.length() && digitIndex < 4; charIndex++) {
    const char currentChar = text[charIndex];

    if (currentChar == '.') {
      if (digitIndex > 0) {
        segments[digitIndex - 1] |= 0x80;
      }
      continue;
    }

    segments[digitIndex] = encodeDisplayCharacter(currentChar);
    digitIndex += 1;
  }

  voltageDisplay.setSegments(segments);
}

void displayTextValue(const String& text) {
  if (!displayAvailable) {
    return;
  }

  writeDisplayString(text);
}

void displayBootMessage() {
  displayTextValue("boot");
}

void displaySelfTestResult(bool passed) {
  displayTextValue(passed ? "PASS" : "FAIL");
}

void displayModeMessage() {
  if (activeMode == RUNTIME_MODE_CALIBRATION) {
    displayTextValue("CAL ");
    return;
  }

  if (activeMode == RUNTIME_MODE_BLE) {
    displayTextValue(bleClientConnected ? "BLE " : "PAIR");
    return;
  }

  if (configPortalActive) {
    displayTextValue(" AP ");
    return;
  }

  displayTextValue(isWifiConnected() ? "NET " : "WAIT");
}

String formatVoltageForDisplay(float voltage, bool stable) {
  if (isnan(voltage) || voltage <= 0.0f) {
    return stable ? "0.00" : "----";
  }

  const float roundedVoltage = voltage >= 0.0f
    ? floorf((voltage * 100.0f) + 0.5f) / 100.0f
    : ceilf((voltage * 100.0f) - 0.5f) / 100.0f;

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%.2f", roundedVoltage);

  return String(buffer);
}

void displayVoltageValue(float voltage, bool stable) {
  if (!displayAvailable) {
    return;
  }

  if (isnan(voltage) || voltage <= 0.0f) {
    displayTextValue(stable ? "0:00" : "----");
    return;
  }

  long scaledVoltage = voltage >= 0.0f
    ? static_cast<long>(floorf((voltage * 100.0f) + 0.5f))
    : static_cast<long>(ceilf((voltage * 100.0f) - 0.5f));
  if (scaledVoltage < 0) {
    scaledVoltage = 0;
  }
  if (scaledVoltage > 9999) {
    scaledVoltage = 9999;
  }

  voltageDisplay.showNumberDecEx(static_cast<int>(scaledVoltage), 0b01000000, true, 4, 0);
}

float roundVoltageToTwoDecimals(float voltage) {
  if (isnan(voltage)) {
    return voltage;
  }

  if (voltage >= 0.0f) {
    return floorf((voltage * 100.0f) + 0.5f) / 100.0f;
  }

  return ceilf((voltage * 100.0f) - 0.5f) / 100.0f;
}

void printRoundedVoltage(float voltage) {
  Serial.print(roundVoltageToTwoDecimals(voltage), 2);
}

void logVoltageRead(float rawVoltage, float calibratedVoltage, float averageVoltage, bool stable) {
  Serial.print("[READ] raw=");
  Serial.print(rawVoltage, 3);
  Serial.print(" V cal=");
  Serial.print(calibratedVoltage, 3);
  Serial.print(" V round2=");
  printRoundedVoltage(calibratedVoltage);
  Serial.print(" V avg=");
  printRoundedVoltage(averageVoltage);
  Serial.print(" V stable=");
  Serial.println(stable ? "true" : "false");
}

void displayStandbyAnimation() {
  static const uint8_t standbyFrames[][4] = {
    { 0x40, 0x00, 0x00, 0x00 },
    { 0x00, 0x40, 0x00, 0x00 },
    { 0x00, 0x00, 0x40, 0x00 },
    { 0x00, 0x00, 0x00, 0x40 },
    { 0x00, 0x00, 0x40, 0x00 },
    { 0x00, 0x40, 0x00, 0x00 },
  };

  if (millis() - lastStandbyAnimationAt >= STANDBY_ANIMATION_INTERVAL_MS) {
    lastStandbyAnimationAt = millis();
    standbyAnimationIndex = (standbyAnimationIndex + 1) % (sizeof(standbyFrames) / sizeof(standbyFrames[0]));
  }

  applyDisplayBrightness(STANDBY_DISPLAY_BRIGHTNESS);
  voltageDisplay.setSegments(standbyFrames[standbyAnimationIndex]);
}

void updateSevenSegmentDisplay() {
  if (!displayAvailable) {
    return;
  }

  if (millis() - lastDisplayRefreshAt < DISPLAY_REFRESH_MS) {
    return;
  }

  lastDisplayRefreshAt = millis();

  if (!systemReady) {
    displayBootMessage();
    return;
  }

  if (displayHoldActive && millis() < displayValueHoldUntil && !isnan(heldDisplayVoltage) && heldDisplayVoltage > 0.0f) {
    applyDisplayBrightness();
    displayVoltageValue(heldDisplayVoltage, true);
    return;
  }

  if (displayHoldActive && millis() >= displayValueHoldUntil) {
    displayHoldActive = false;
  }

  if (!isnan(lastDisplayVoltage) && lastDisplayVoltage > 0.0f) {
    applyDisplayBrightness();
    displayVoltageValue(lastDisplayVoltage, isStable);
    return;
  }

  const bool standbyTimedOut = lastMeasurementSeenAt == 0
    ? millis() >= STANDBY_TIMEOUT_MS
    : (millis() - lastMeasurementSeenAt) >= STANDBY_TIMEOUT_MS;

  if (!standbyTimedOut) {
    applyDisplayBrightness();
    displayTextValue("----");
    return;
  }

  displayStandbyAnimation();
}

bool writeIna226Register(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(INA226_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>((value >> 8) & 0xFF));
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  return Wire.endTransmission() == 0;
}

bool readIna226Register(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(INA226_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(INA226_I2C_ADDRESS, static_cast<uint8_t>(2)) != 2) {
    return false;
  }

  value = (static_cast<uint16_t>(Wire.read()) << 8) | static_cast<uint16_t>(Wire.read());
  return true;
}

bool initIna226() {
  if (!writeIna226Register(INA226_REG_CONFIG, INA226_CONFIG_DEFAULT)) {
    return false;
  }

  if (!readIna226Register(INA226_REG_MANUFACTURER_ID, ina226ManufacturerId)) {
    return false;
  }

  if (!readIna226Register(INA226_REG_DIE_ID, ina226DieId)) {
    return false;
  }

  if (ina226ManufacturerId != INA226_EXPECTED_MANUFACTURER_ID || ina226DieId != INA226_EXPECTED_DIE_ID) {
    Serial.print("[INA226] unexpected ids mfg=0x");
    Serial.print(ina226ManufacturerId, HEX);
    Serial.print(" die=0x");
    Serial.println(ina226DieId, HEX);
  }

  lastInaBusVoltage = readIna226BusVoltage();
  return true;
}

float readIna226BusVoltage() {
  uint16_t rawBusVoltage = 0;
  if (!readIna226Register(INA226_REG_BUS_VOLTAGE, rawBusVoltage)) {
    return NAN;
  }

  return static_cast<float>(rawBusVoltage) * 0.00125f;
}

void loadInaCalibration() {
  preferences.begin(PREF_NAMESPACE, true);
  const float savedInaGain = preferences.getFloat(PREF_INA_GAIN_KEY, DEFAULT_INA_GAIN);
  const float savedInaOffset = preferences.getFloat(PREF_INA_OFFSET_KEY, DEFAULT_INA_OFFSET);
  preferences.end();

  inaGain = isInaGainValid(savedInaGain) ? savedInaGain : DEFAULT_INA_GAIN;
  inaOffset = isInaOffsetValid(savedInaOffset) ? savedInaOffset : DEFAULT_INA_OFFSET;
  Serial.print("[CAL] ina gain = ");
  Serial.println(inaGain, 5);
  Serial.print("[CAL] ina offset = ");
  Serial.println(inaOffset, 5);
}

void saveInaCalibration(float gain, float offset) {
  if (!isInaGainValid(gain) || !isInaOffsetValid(offset)) {
    return;
  }

  inaGain = gain;
  inaOffset = offset;
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putFloat(PREF_INA_GAIN_KEY, inaGain);
  preferences.putFloat(PREF_INA_OFFSET_KEY, inaOffset);
  preferences.end();
}

float applyInaCalibration(float voltage) {
  return (voltage * inaGain) + inaOffset;
}

float applyZeroClamp(float voltage) {
  if (isnan(voltage)) {
    return voltage;
  }

  return fabsf(voltage) <= zeroClampThreshold ? 0.0f : voltage;
}

String buildBleVoltMeterStatusPayload() {
  JsonDocument doc;
  doc["device_id"] = deviceId;
  doc["device_name"] = bleDeviceName;
  doc["fw"] = FIRMWARE_VERSION;
  doc["ina_gain"] = inaGain;
  doc["ina_offset"] = inaOffset;
  doc["voltage"] = lastAverageVoltage;
  doc["stable"] = isStable;
  doc["ble_connected"] = bleClientConnected;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

void updateBleVoltMeterStatusCharacteristic(bool notify) {
  if (!voltMeterStatusCharacteristic) {
    return;
  }

  const String payload = buildBleVoltMeterStatusPayload();
  voltMeterStatusCharacteristic->setValue(payload.c_str());

  if (notify && bleClientConnected) {
    voltMeterStatusCharacteristic->notify();
  }
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
  return isActionButtonPressedRaw();
}

bool isActionButtonPressedRaw() {
  return actionButtonPressedState;
}

bool hasLiveMeasurement() {
  return sampleCount > 0 && !isnan(lastAverageVoltage) && lastAverageVoltage > 0.0f;
}

void logActionButtonStateIfChanged() {
  const bool pressed = isActionButtonPressedRaw();
  if (pressed == lastActionButtonRawState) {
    return;
  }

  lastActionButtonRawState = pressed;
  Serial.print("[BUTTON] ACTION_BUTTON = ");
  Serial.println(pressed ? "LOW (pressed)" : "HIGH (released)");
}

bool isBootButtonPressed() {
  return digitalRead(BOOT_BUTTON_PIN) == LOW;
}

uint8_t keyCodeForCharacter(char value) {
  if (value >= '1' && value <= '9') {
    return static_cast<uint8_t>(0x1E + (value - '1'));
  }

  if (value == '0') {
    return 0x27;
  }

  if (value == '.') {
    return KEY_CODE_DOT;
  }

  if (value == '-') {
    return KEY_CODE_MINUS;
  }

  if (value == '\n' || value == '\r') {
    return KEY_CODE_ENTER;
  }

  return 0x00;
}

void sendKeyboardReport(uint8_t modifier, uint8_t keyCode) {
  if (!inputKeyboard || !bleClientConnected) {
    return;
  }

  uint8_t report[8] = {
    modifier,
    0x00,
    keyCode,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
  };

  inputKeyboard->setValue(report, sizeof(report));
  inputKeyboard->notify();
  delay(HID_KEY_PRESS_MS);

  uint8_t releaseReport[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  inputKeyboard->setValue(releaseReport, sizeof(releaseReport));
  inputKeyboard->notify();
  delay(HID_KEY_RELEASE_MS);
}

bool typeKeyboardText(const String& text, bool appendEnter) {
  if (!bleClientConnected || !inputKeyboard) {
    return false;
  }

  for (size_t index = 0; index < text.length(); index++) {
    const uint8_t keyCode = keyCodeForCharacter(text[index]);
    if (keyCode == 0x00) {
      continue;
    }

    sendKeyboardReport(KEY_MOD_NONE, keyCode);
  }

  if (appendEnter) {
    sendKeyboardReport(KEY_MOD_NONE, KEY_CODE_ENTER);
  }

  delay(HID_TYPE_GUARD_MS);
  return true;
}

void startKeyboardMeasurementCycle(const char* reason) {
  if (hidSendArmed) {
    Serial.println("[HID] measurement cycle already running");
    return;
  }

  if (isnan(lastDisplayVoltage) || lastDisplayVoltage <= 0.0f) {
    Serial.println("[HID] no live voltage available to send");
    return;
  }

  hidSendArmed = true;
  hidTypedDuringCurrentCycle = false;
  pendingHidVoltage = lastDisplayVoltage;
  heldDisplayVoltage = lastDisplayVoltage;
  displayHoldActive = true;
  displayValueHoldUntil = millis() + DISPLAY_VALUE_HOLD_MS;
  lastKeyboardTypedValue = "";
  measurementPayload = buildMeasurementPayload();
  bleStatusMessage = bleClientConnected ? "measuring_for_send" : "waiting_for_host";
  Serial.println("[HID] startKeyboardMeasurementCycle");
  updateMeasurementLedState(true, true);
  logVoltageRead(lastAdcVoltageFromMilliVolts, lastAdcVoltageCorrected, pendingHidVoltage, isStable);
  Serial.print("[HID] captured voltage ");
  printRoundedVoltage(pendingHidVoltage);
  Serial.print(" V by ");
  Serial.println(reason);
}

bool isBleHostReadyForAction() {
  return bleClientConnected && (millis() - bleConnectedAt >= BLE_HOST_SETTLE_MS);
}

void updateActionButton() {
  const bool pressed = isActionButtonPressedRaw();

  if (pressed && !lastBootButtonReading) {
    bootButtonPressedAt = millis();
    Serial.println("[BUTTON] press edge");
    if (activeMode == RUNTIME_MODE_BLE && !bleClientConnected) {
      Serial.println("[HID] short press ignored: no BLE HID host connected");
    }
  }

  if (!pressed) {
    const unsigned long pressedDuration = lastBootButtonReading ? (millis() - bootButtonPressedAt) : 0;
    if (lastBootButtonReading) {
      Serial.print("[BUTTON] release edge, durationMs=");
      Serial.println(pressedDuration);
    }

    if (activeMode == RUNTIME_MODE_BLE && lastBootButtonReading && pressedDuration >= SEND_ARM_DEBOUNCE_MS) {
      if (!bleClientConnected) {
        Serial.println("[HID] short press ignored: no BLE HID host connected");
      } else if (!isBleHostReadyForAction()) {
        Serial.println("[HID] short press ignored: waiting for BLE HID host settle");
      } else {
        startKeyboardMeasurementCycle("button_release");
      }
    }

    if (lastBootButtonReading && activeMode != RUNTIME_MODE_BLE && pressedDuration >= SEND_ARM_DEBOUNCE_MS
      && !isnan(lastDisplayVoltage) && lastDisplayVoltage > 0.0f) {
      heldDisplayVoltage = lastDisplayVoltage;
      displayHoldActive = true;
      displayValueHoldUntil = millis() + DISPLAY_VALUE_HOLD_MS;
    }
  }

  lastBootButtonReading = pressed;
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

  if (!ina226Available) {
    lastAdcRaw = 0;
    lastAdcMilliVolts = 0;
    lastAdcVoltageFromRaw = 0.0f;
    lastAdcVoltageFromMilliVolts = 0.0f;
    lastAdcVoltageCorrected = 0.0f;
    return NAN;
  }

  const float rawBusVoltage = readIna226BusVoltage();
  if (isnan(rawBusVoltage)) {
    return NAN;
  }

  const float calibratedBusVoltage = applyZeroClamp(applyInaCalibration(rawBusVoltage));
  const float activeMeasurementThreshold = zeroClampThreshold > MIN_ACTIVE_MEASUREMENT_VOLTAGE
    ? zeroClampThreshold
    : MIN_ACTIVE_MEASUREMENT_VOLTAGE;
  const float normalizedBusVoltage = fabsf(calibratedBusVoltage) < activeMeasurementThreshold
    ? 0.0f
    : calibratedBusVoltage;
  lastInaBusVoltage = rawBusVoltage;
  lastAdcRaw = 0;
  lastAdcMilliVolts = static_cast<int>(normalizedBusVoltage * 1000.0f);
  lastAdcVoltageFromRaw = 0.0f;
  lastAdcVoltageFromMilliVolts = rawBusVoltage;
  lastAdcVoltageCorrected = normalizedBusVoltage;
  return normalizedBusVoltage;
}

void resetMeasurementState() {
  sampleIndex = 0;
  sampleCount = 0;
  lastAverageVoltage = 0.0f;
  stableHits = 0;
  isStable = false;
  lastMeasurementStable = false;
  waitForNewMeasurementCycleAfterSend = false;
  stableReadyForSend = false;
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

  if (displayHoldActive && millis() < displayValueHoldUntil) {
    setRgbColor(CRGB::Blue);
    applyLedState(true);
    return;
  }

  if (activeMode == RUNTIME_MODE_CALIBRATION) {
    setRgbColor(CRGB::Purple);
    blinkLed(260);
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

  if (millis() < stableReadHoldUntil) {
    setRgbColor(CRGB::Green);
    applyLedState(true);
    return;
  }

  if (isActionButtonPressedRaw()) {
    setRgbColor(CRGB::Red);
    applyLedState(true);
    return;
  }

  if (!triggered) {
    setRgbColor(CRGB::Green);
    applyLedState(true);
    return;
  }

  if (waitForNewMeasurementCycleAfterSend) {
    setRgbColor(CRGB::Green);
    applyLedState(true);
    return;
  }

  if (stable && stableReadyForSend) {
    setRgbColor(CRGB::Blue);
    applyLedState(true);
    return;
  }

  setRgbColor(triggered ? CRGB::Green : CRGB::Orange);
  if (triggered) {
    applyLedState(true);
  } else {
    blinkLed(180);
  }
}

String buildMeasurementPayload() {
  const bool hasMeasurement = hasLiveMeasurement();

  JsonDocument doc;
  doc["id"] = deviceId;
  doc["fw"] = FIRMWARE_VERSION;
  doc["t"] = sampledAtMs;
  doc["tr"] = hasMeasurement ? 1 : 0;
  doc["raw"] = lastAdcRaw;
  doc["adc_mv"] = lastAdcMilliVolts;
  doc["adc_from_raw"] = lastAdcVoltageFromRaw;
  doc["adc"] = lastAdcVoltageFromMilliVolts;
  doc["adc_corrected"] = lastAdcVoltageCorrected;

  if (!hasMeasurement) {
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

  const float reading = readBatteryVoltage();
  if (isnan(reading) || reading <= 0.0f) {
    resetMeasurementState();
    lastDisplayVoltage = NAN;
    sampledAtMs = millis();
    updateMeasurementLedState(false, false);
    measurementPayload = buildMeasurementPayload();
    updateBleVoltMeterStatusCharacteristic(false);
    return;
  }

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
  lastDisplayVoltage = averageVoltage;
  lastMeasurementSeenAt = millis();

  if (sampleCount >= 2 && fabsf(averageVoltage - lastAverageVoltage) <= STABLE_DELTA) {
    stableHits += 1;
  } else {
    stableHits = 1;
  }

  const bool wasStable = lastMeasurementStable;
  lastAverageVoltage = averageVoltage;
  isStable = (sampleCount >= SAMPLE_WINDOW && stableHits >= STABLE_HITS_REQUIRED);
  if (wasStable && !isStable) {
    stableReadyForSend = true;
  }
  if (waitForNewMeasurementCycleAfterSend && !isStable) {
    waitForNewMeasurementCycleAfterSend = false;
  }
  sampledAtMs = millis();
  lastMeasurementStable = isStable;
  updateMeasurementLedState(sampleCount > 0, isStable);
  measurementPayload = buildMeasurementPayload();
  updateBleVoltMeterStatusCharacteristic(isStable);
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

  activeMode = RUNTIME_MODE_CALIBRATION;
  Serial.println("[MODE] boot button pressed");
  Serial.print("[MODE] boot button override (one-shot) -> ");
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
  doc["ina_gain"] = inaGain;
  doc["ina_offset"] = inaOffset;
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
  const String inaGainArg = server.arg("ina_gain");
  const String inaOffsetArg = server.arg("ina_offset");

  if (ratioArg.isEmpty() && gainArg.isEmpty() && offsetArg.isEmpty() && inaGainArg.isEmpty() && inaOffsetArg.isEmpty()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"calibration_required\"}");
    return;
  }

  float nextRatio = dividerRatio;
  float nextGain = adcGain;
  float nextOffset = adcOffset;
  float nextInaGain = inaGain;
  float nextInaOffset = inaOffset;

  if (!ratioArg.isEmpty()) {
    nextRatio = ratioArg.toFloat();
  }

  if (!gainArg.isEmpty()) {
    nextGain = gainArg.toFloat();
  }

  if (!offsetArg.isEmpty()) {
    nextOffset = offsetArg.toFloat();
  }

  if (!inaGainArg.isEmpty()) {
    nextInaGain = inaGainArg.toFloat();
  }

  if (!inaOffsetArg.isEmpty()) {
    nextInaOffset = inaOffsetArg.toFloat();
  }

  if (!isDividerRatioValid(nextRatio) || !isAdcGainValid(nextGain) || !isAdcOffsetValid(nextOffset) || !isInaGainValid(nextInaGain) || !isInaOffsetValid(nextInaOffset)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_calibration\"}");
    return;
  }

  saveDividerRatio(nextRatio);
  saveAdcCalibration(nextGain, nextOffset);
  saveInaCalibration(nextInaGain, nextInaOffset);
  resetMeasurementState();
  measurementPayload = buildMeasurementPayload();

  JsonDocument doc;
  doc["ok"] = true;
  doc["divider_ratio"] = dividerRatio;
  doc["adc_gain"] = adcGain;
  doc["adc_offset"] = adcOffset;
  doc["ina_gain"] = inaGain;
  doc["ina_offset"] = inaOffset;

  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);

  Serial.print("[CAL] divider ratio updated via http: ");
  Serial.println(dividerRatio, 3);
  Serial.print("[CAL] adc gain updated via http: ");
  Serial.println(adcGain, 4);
  Serial.print("[CAL] adc offset updated via http: ");
  Serial.println(adcOffset, 4);
  Serial.print("[CAL] ina gain updated via http: ");
  Serial.println(inaGain, 5);
  Serial.print("[CAL] ina offset updated via http: ");
  Serial.println(inaOffset, 5);
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
  doc["ina_gain"] = inaGain;
  doc["ina_offset"] = inaOffset;

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
  html += "<br><label>INA gain <input type='number' name='ina_gain' step='0.00001' min='0.8' max='1.2' value='" + String(inaGain, 5) + "'></label>";
  html += "<br><label>INA offset <input type='number' name='ina_offset' step='0.00001' min='-1' max='1' value='" + String(inaOffset, 5) + "'></label>";
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

class BleKeyboardServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    (void)server;
    bleClientConnected = true;
    bleConnectedAt = millis();
    bleStatusMessage = activeMode == RUNTIME_MODE_CALIBRATION ? "calibration_connected" : "keyboard_connected";
    Serial.println(activeMode == RUNTIME_MODE_CALIBRATION ? "[BLE CAL] host connected" : "[BLE HID] host connected");
    updateBleVoltMeterStatusCharacteristic(true);
    updateMeasurementLedState(hasLiveMeasurement(), isStable);
  }

  void onDisconnect(BLEServer* server) override {
    (void)server;
    bleClientConnected = false;
    bleConnectedAt = 0;
    bleStatusMessage = activeMode == RUNTIME_MODE_CALIBRATION ? "calibration_advertising" : "keyboard_advertising";
    hidSendArmed = false;
    hidTypedDuringCurrentCycle = false;
    lastKeyboardTypedValue = "";
    Serial.println(activeMode == RUNTIME_MODE_CALIBRATION ? "[BLE CAL] host disconnected" : "[BLE HID] host disconnected");
    BLEDevice::startAdvertising();
    updateBleVoltMeterStatusCharacteristic(false);
    updateMeasurementLedState(hasLiveMeasurement(), isStable);
  }
};

class BleCalibrationCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const std::string rawValue = characteristic->getValue();
    if (rawValue.empty()) {
      return;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, rawValue.c_str());
    if (error) {
      Serial.print("[BLE CAL] invalid payload: ");
      Serial.println(error.c_str());
      return;
    }

    const String command = String(doc["cmd"] | "");
    if (command != "set_calibration") {
      Serial.print("[BLE CAL] unsupported command: ");
      Serial.println(command);
      return;
    }

    const float nextInaGain = doc["ina_gain"] | NAN;
    const float nextInaOffset = doc["ina_offset"] | NAN;

    if (!isInaGainValid(nextInaGain) || !isInaOffsetValid(nextInaOffset)) {
      Serial.println("[BLE CAL] rejected invalid gain/offset");
      return;
    }

    saveInaCalibration(nextInaGain, nextInaOffset);
    resetMeasurementState();
    measurementPayload = buildMeasurementPayload();
    updateBleVoltMeterStatusCharacteristic(true);
    buzz(50);
    delay(60);
    buzz(50);
    Serial.print("[BLE CAL] calibration updated via BLE gain=");
    Serial.print(inaGain, 5);
    Serial.print(" offset=");
    Serial.println(inaOffset, 5);
  }
};

void setupBle(bool enableKeyboardHid = true) {
  bleStatusMessage = enableKeyboardHid ? "keyboard_starting" : "calibration_starting";
  BLEDevice::init(bleDeviceName.c_str());
  BLEDevice::setPower(ESP_PWR_LVL_P9);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BleKeyboardServerCallbacks());

  hidDevice = nullptr;
  inputKeyboard = nullptr;
  outputKeyboard = nullptr;

  if (enableKeyboardHid) {
    hidDevice = new BLEHIDDevice(bleServer);
    inputKeyboard = hidDevice->inputReport(KEYBOARD_REPORT_ID);
    outputKeyboard = hidDevice->outputReport(KEYBOARD_REPORT_ID);
    (void)outputKeyboard;

    hidDevice->manufacturer()->setValue("PUMA Battery");
    hidDevice->pnp(0x02, 0x1234, 0xB001, 0x0100);
    hidDevice->hidInfo(0x00, 0x01);
    hidDevice->reportMap(const_cast<uint8_t*>(KEYBOARD_REPORT_MAP), sizeof(KEYBOARD_REPORT_MAP));
    hidDevice->startServices();
    hidDevice->setBatteryLevel(100);
  }

  voltMeterService = bleServer->createService(BLEUUID(BLE_VOLTMETER_SERVICE_UUID));
  voltMeterStatusCharacteristic = voltMeterService->createCharacteristic(
    BLEUUID(BLE_VOLTMETER_STATUS_CHARACTERISTIC_UUID),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  voltMeterStatusCharacteristic->addDescriptor(new BLE2902());
  voltMeterCalibrationCharacteristic = voltMeterService->createCharacteristic(
    BLEUUID(BLE_VOLTMETER_CALIBRATION_CHARACTERISTIC_UUID),
    BLECharacteristic::PROPERTY_WRITE
  );
  voltMeterCalibrationCharacteristic->setCallbacks(new BleCalibrationCharacteristicCallbacks());
  updateBleVoltMeterStatusCharacteristic(false);
  voltMeterService->start();

  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);
  security->setCapability(ESP_IO_CAP_NONE);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setKeySize();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(enableKeyboardHid ? HID_KEYBOARD : 0x0000);
  if (enableKeyboardHid && hidDevice) {
    advertising->addServiceUUID(hidDevice->hidService()->getUUID());
  }
  advertising->addServiceUUID(BLEUUID(BLE_VOLTMETER_SERVICE_UUID));
  advertising->start();

  bleStatusMessage = enableKeyboardHid ? "keyboard_advertising" : "calibration_advertising";
  Serial.println(enableKeyboardHid ? "[BLE HID] keyboard advertising started" : "[BLE CAL] calibration advertising started");
  Serial.print("[BLE HID] Device name: ");
  Serial.println(bleDeviceName);
  Serial.print("[BLE HID] Device id: ");
  Serial.println(deviceId);
}

void updateBleKeyboard() {
  if (activeMode != RUNTIME_MODE_BLE) {
    return;
  }

  if (!hidSendArmed || !bleClientConnected || hidTypedDuringCurrentCycle || isnan(pendingHidVoltage) || pendingHidVoltage <= 0.0f) {
    return;
  }

  const String typedValue = String(roundVoltageToTwoDecimals(pendingHidVoltage), 2);
  if (!typeKeyboardText(typedValue, true)) {
    bleStatusMessage = "keyboard_send_failed";
    return;
  }

  hidTypedDuringCurrentCycle = true;
  hidSendArmed = false;
  pendingHidVoltage = NAN;
  displayHoldActive = false;
  waitForNewMeasurementCycleAfterSend = true;
  stableReadyForSend = false;
  lastKeyboardTypedValue = typedValue;
  bleStatusMessage = "keyboard_value_typed";
  stableReadHoldUntil = millis() + STABLE_READ_COLOR_HOLD_MS;
  buzz(40);
  updateMeasurementLedState(hasLiveMeasurement(), isStable);
  Serial.print("[BLE HID] typed voltage: ");
  Serial.println(typedValue);
}

void setupAdc() {
  analogReadResolution(12);
  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
}

void setupI2cPeripherals() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  ina226Available = initIna226();
  if (ina226Available) {
    Serial.print("[INA226] ready, bus voltage = ");
    printRoundedVoltage(lastInaBusVoltage);
    Serial.println(" V");
  } else {
    Serial.println("[INA226] not detected");
  }
}

void setupStatusIndicators() {
  if (USE_ONBOARD_LED) {
    pinMode(STATUS_LED_PIN, OUTPUT);
    applyLedState(false);
  }
}

void runBootSelfTest() {
  bootSelfTestResult = BootSelfTestResult{};

  Serial.println("[SELFTEST] start");
  selfTestStep(F("buzzer"), CRGB::White, SELF_TEST_BUZZ_MS);
  buzz(SELF_TEST_BUZZ_MS);
  bootSelfTestResult.buzzerOk = true;
  delay(SELF_TEST_STEP_GAP_MS);

  const CRGB colors[] = { CRGB::Red, CRGB::Green, CRGB::Blue };
  for (const CRGB& color : colors) {
    setRgbColor(color);
    delay(SELF_TEST_COLOR_MS);
  }
  bootSelfTestResult.rgbOk = true;
  Serial.println("[SELFTEST] rgb ok");

  selfTestStep(F("ina226"), CRGB::Orange);
  bootSelfTestResult.inaOk = ina226Available;
  if (ina226Available) {
    const float rawBootVoltage = readIna226BusVoltage();
    if (!isnan(rawBootVoltage)) {
      const float calibratedBootVoltage = applyInaCalibration(rawBootVoltage);
      bootSelfTestResult.busVoltage = calibratedBootVoltage;
      lastInaBusVoltage = rawBootVoltage;
      Serial.print("[SELFTEST] INA226 bus voltage raw = ");
      printRoundedVoltage(rawBootVoltage);
      Serial.print(" V, calibrated = ");
      printRoundedVoltage(calibratedBootVoltage);
      Serial.println(" V");
      delay(SELF_TEST_RESULT_MS);
    } else {
      bootSelfTestResult.inaOk = false;
      Serial.println("[SELFTEST] INA226 read failed");
    }
  } else {
    Serial.println("[SELFTEST] INA226 not detected");
  }

  selfTestStep(F("press action button"), CRGB::Yellow);
  bootSelfTestResult.switchPressed = waitForActionButtonPress(SELF_TEST_SWITCH_TIMEOUT_MS);
  bootSelfTestResult.switchOk = bootSelfTestResult.switchPressed;
  Serial.print("[SELFTEST] action button pressed = ");
  Serial.println(bootSelfTestResult.switchPressed ? "true" : "false");

  bootSelfTestPassed = bootSelfTestResult.rgbOk
    && bootSelfTestResult.buzzerOk
    && bootSelfTestResult.switchOk
    && bootSelfTestResult.inaOk;

  Serial.print("[SELFTEST] result = ");
  Serial.println(bootSelfTestPassed ? "PASS" : "FAIL");

  displaySelfTestResult(bootSelfTestPassed);
  setRgbColor(bootSelfTestPassed ? CRGB::Green : CRGB::Red);
  applyLedState(true);
  if (bootSelfTestPassed) {
    selfTestPassTone();
  } else {
    selfTestFailTone();
  }
  delay(SELF_TEST_RESULT_MS);
  setRgbColor(CRGB::Black);
  applyLedState(false);
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
  Serial.print("[BOOT] RESET_REASON = ");
  Serial.println(resetReasonName(esp_reset_reason()));
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
  Serial.print("[BOOT] ACTION_BUTTON_PIN = ");
  Serial.println(actionButtonPin);
  Serial.print("[BOOT] BUZZER_PIN = ");
  Serial.println(BUZZER_PIN);
  Serial.print("[BOOT] TM1637_CLK_PIN = ");
  Serial.println(TM1637_CLK_PIN);
  Serial.print("[BOOT] TM1637_DIO_PIN = ");
  Serial.println(TM1637_DIO_PIN);
  Serial.print("[BOOT] DISPLAY_BRIGHTNESS = ");
  Serial.println(displayBrightness);
  Serial.print("[BOOT] ZERO_CLAMP_THRESHOLD = ");
  Serial.println(zeroClampThreshold, 3);
  Serial.print("[BOOT] SELFTEST_AT_BOOT = ");
  Serial.println(bootSelfTestEnabled ? "true" : "false");
  Serial.print("[BOOT] USE_LIFT_SWITCH = ");
  Serial.println(useLiftSwitch ? "true" : "false");
  Serial.print("[BOOT] USE_ONBOARD_LED = ");
  Serial.println(USE_ONBOARD_LED ? "true" : "false");
  Serial.print("[BOOT] USE_RGB_LED = ");
  Serial.println(USE_RGB_LED ? "true" : "false");
  Serial.print("[BOOT] RGB_PIN = ");
  Serial.println(RGB_PIN);
  Serial.print("[BOOT] I2C_SDA_PIN = ");
  Serial.println(I2C_SDA_PIN);
  Serial.print("[BOOT] I2C_SCL_PIN = ");
  Serial.println(I2C_SCL_PIN);
  Serial.print("[BOOT] INA_GAIN = ");
  Serial.println(inaGain, 5);
  Serial.print("[BOOT] INA_OFFSET = ");
  Serial.println(inaOffset, 5);
  Serial.print("[BOOT] USE_MOCK_VOLTAGE = ");
  Serial.println(useMockVoltage ? "true" : "false");
  Serial.print("[BOOT] DIVIDER_RATIO = ");
  Serial.println(dividerRatio, 3);
  Serial.print("[BOOT] ADC_GAIN = ");
  Serial.println(adcGain, 4);
  Serial.print("[BOOT] ADC_OFFSET = ");
  Serial.println(adcOffset, 4);
  Serial.println("[BOOT] SERIAL_COMMANDS = help / mode / brightness / rgb / mock / lift / selftest / reboot");
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
    printRoundedVoltage(readBatteryVoltage());
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
    printRoundedVoltage(adcVoltageFromRaw);
    Serial.println(" V");
    Serial.println("  analogReadMilliVolts");
    Serial.print("    adcMv: ");
    Serial.println(milliVolts);
    Serial.print("    adcVoltage: ");
    printRoundedVoltage(adcVoltageFromMilliVolts);
    Serial.println(" V");
    Serial.println("  adc calibration");
    Serial.print("    adcGain: ");
    Serial.println(adcGain, 4);
    Serial.print("    adcOffset: ");
    Serial.println(adcOffset, 4);
    Serial.print("    adcCorrected: ");
    printRoundedVoltage(adcVoltageCorrected);
    Serial.println(" V");
    Serial.println("  calculated battery");
    Serial.print("    dividerRatio: ");
    Serial.println(dividerRatio, 3);
    Serial.print("    voltageOffset: ");
    Serial.println(VOLTAGE_OFFSET, 3);
    Serial.print("    batteryVoltage: ");
    printRoundedVoltage(batteryVoltage);
    Serial.println(" V");
  }

  Serial.println("  filtered result");
  Serial.print("    voltageAvg: ");
  printRoundedVoltage(lastAverageVoltage);
  Serial.println(" V");
  Serial.print("    stableHits: ");
  Serial.print(stableHits);
  Serial.print("    stable: ");
  Serial.print(isStable ? "true" : "false");
  Serial.print("    liveMeasurement: ");
  Serial.println(hasLiveMeasurement() ? "true" : "false");

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
  Serial.println(hasLiveMeasurement() ? (isStable ? "ready" : "measuring") : "idle");
  Serial.println("  raw input");
  Serial.print("    adcRaw: ");
  Serial.println(lastAdcRaw);
  Serial.print("    adcVoltageFromRaw: ");
  printRoundedVoltage(lastAdcVoltageFromRaw);
  Serial.println(" V");
  Serial.println("  analogReadMilliVolts");
  Serial.print("    adcMv: ");
  Serial.println(lastAdcMilliVolts);
  Serial.print("    adcVoltage: ");
  printRoundedVoltage(lastAdcVoltageFromMilliVolts);
  Serial.println(" V");
  Serial.println("  adc calibration");
  Serial.print("    adcGain: ");
  Serial.println(adcGain, 4);
  Serial.print("    adcOffset: ");
  Serial.println(adcOffset, 4);
  Serial.print("    adcCorrected: ");
  printRoundedVoltage(lastAdcVoltageCorrected);
  Serial.println(" V");
  Serial.println("  calculated battery");
  Serial.print("    dividerRatio: ");
  Serial.println(dividerRatio, 3);
  Serial.print("    voltageOffset: ");
  Serial.println(VOLTAGE_OFFSET, 3);
  Serial.print("    voltageAvg: ");
  printRoundedVoltage(lastAverageVoltage);
  Serial.println(" V");
  Serial.println("  filter / status");
  Serial.print("    stableHits: ");
  Serial.println(stableHits);
  Serial.print("    sampledAtMs: ");
  Serial.println(sampledAtMs);
  Serial.print("    liftSwitchEnabled: ");
  Serial.println(useLiftSwitch ? "true" : "false");
  Serial.print("    actionButtonPin: ");
  Serial.println(actionButtonPin);
  Serial.print("    actionButtonRaw: ");
  Serial.println(digitalRead(actionButtonPin));
  Serial.print("    actionButtonPressed: ");
  Serial.println(isActionButtonPressedRaw() ? "true" : "false");
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
  Serial.println(activeMode == RUNTIME_MODE_CALIBRATION ? "[INFO] BLE CALIBRATION" : "[INFO] BLE HID");
  Serial.print("  activeMode: ");
  Serial.println(runtimeModeName(activeMode));
  Serial.print("  status: ");
  Serial.println(bleStatusMessage);
  Serial.print("  connected: ");
  Serial.println(bleClientConnected ? "true" : "false");
  Serial.print("  settleReady: ");
  Serial.println(isBleHostReadyForAction() ? "true" : "false");
  Serial.print("  name: ");
  Serial.println(bleDeviceName);
  Serial.print("  profile: ");
  Serial.println(activeMode == RUNTIME_MODE_CALIBRATION ? "calibration_gatt" : "hid_keyboard");
  Serial.print("  armed: ");
  Serial.println(hidSendArmed ? "true" : "false");
  Serial.print("  typedThisCycle: ");
  Serial.println(hidTypedDuringCurrentCycle ? "true" : "false");
  Serial.print("  lastTypedValue: ");
  Serial.println(lastKeyboardTypedValue);
  if (activeMode == RUNTIME_MODE_CALIBRATION) {
    Serial.println("  action: connect from web calibration page, read status, and sync gain/offset");
    Serial.println("  exit: use serial command hid or mode hid");
  } else {
    Serial.println("  action: short press A1, then release to measure, type voltage, and send Enter");
    Serial.println("  calibration: use serial command calibrate or mode cal");
  }
}

void printHelp() {
  Serial.println("[HELP] Commands");
  Serial.println("  Basic");
  Serial.println("    help | info | status | payload");
  Serial.println("    wifi | ble");
  Serial.println("  Mode");
  Serial.println("    mode");
  Serial.println("    mode wifi");
  Serial.println("    mode ble   (alias: mode hid, hid)");
  Serial.println("    mode cal   (alias: calibrate, cal)");
  Serial.println("  Display");
  Serial.println("    brightness");
  Serial.println("    brightness <0-7>");
  Serial.println("    clamp");
  Serial.println("    clamp <0.000-1.000>");
  Serial.println("  Input");
  Serial.println("    buttonpin");
  Serial.println("    buttonpin <gpio>");
  Serial.println("  RGB");
  Serial.println("    rgb on | rgb off | rgb test");
  Serial.println("  Measure");
  Serial.println("    send   (capture current voltage and type via BLE HID)");
  Serial.println("    mock on | mock off");
  Serial.println("    lift on | lift off");
  Serial.println("  System");
  Serial.println("    selftest");
  Serial.println("    selftest boot");
  Serial.println("    selftest boot on | selftest boot off");
  Serial.println("    clearwifi | reboot");
  Serial.println("[OK] help");
}

void setRgbManualEnabled(bool enabled) {
  rgbManualEnabled = enabled;
  rgbTestEnabled = false;

  if (!enabled) {
    setRgbColor(CRGB::Black);
    updateMeasurementLedState(hasLiveMeasurement(), isStable);
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
  lastDisplayVoltage = NAN;
  displayHoldActive = false;
  measurementPayload = buildMeasurementPayload();
  updateMeasurementLedState(hasLiveMeasurement(), isStable);
}

void setMockVoltageEnabled(bool enabled) {
  useMockVoltage = enabled;
  resetMeasurementState();
  lastDisplayVoltage = NAN;
  displayHoldActive = false;
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
  Serial.println("  switch with: mode wifi | mode ble | mode hid | mode cal | calibrate | hid");
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

  if (normalizedCommand == "mode ble" || normalizedCommand == "mode hid" || normalizedCommand == "hid") {
    if (activeMode == RUNTIME_MODE_BLE) {
      Serial.println("[MODE] already in ble");
      return;
    }
    rebootToMode(RUNTIME_MODE_BLE, "serial");
    return;
  }

  if (normalizedCommand == "mode cal" || normalizedCommand == "mode calibration" || normalizedCommand == "calibrate" || normalizedCommand == "cal") {
    if (activeMode == RUNTIME_MODE_CALIBRATION) {
      Serial.println("[MODE] already in calibration");
      return;
    }
    rebootToMode(RUNTIME_MODE_CALIBRATION, "serial");
    return;
  }

  if (normalizedCommand == "brightness") {
    Serial.print("[DISPLAY] brightness = ");
    Serial.println(displayBrightness);
    Serial.println("[OK] brightness");
    return;
  }

  if (normalizedCommand.startsWith("brightness ")) {
    const String valueText = normalizedCommand.substring(11);
    const int brightnessValue = valueText.toInt();

    if (valueText.length() > 0 && brightnessValue >= 0 && brightnessValue <= 7) {
      saveDisplayBrightness(static_cast<uint8_t>(brightnessValue));
      Serial.print("[DISPLAY] brightness set to ");
      Serial.println(displayBrightness);
      Serial.println("[OK] brightness");
      return;
    }

    Serial.println("[DISPLAY] brightness must be 0-7");
    return;
  }

  if (normalizedCommand == "clamp") {
    Serial.print("[CLAMP] threshold = ");
    Serial.println(zeroClampThreshold, 3);
    Serial.println("[OK] clamp");
    return;
  }

  if (normalizedCommand.startsWith("clamp ")) {
    const String valueText = normalizedCommand.substring(6);
    const float clampValue = valueText.toFloat();

    if (valueText.length() > 0 && !isnan(clampValue) && clampValue >= 0.0f && clampValue <= 1.0f) {
      saveZeroClampThreshold(clampValue);
      Serial.print("[CLAMP] threshold set to ");
      Serial.println(zeroClampThreshold, 3);
      Serial.println("[OK] clamp");
      return;
    }

    Serial.println("[CLAMP] threshold must be 0.000-1.000");
    return;
  }

  if (normalizedCommand == "buttonpin") {
    Serial.print("[INPUT] action button pin = ");
    Serial.println(actionButtonPin);
    Serial.println("[OK] buttonpin");
    return;
  }

  if (normalizedCommand.startsWith("buttonpin ")) {
    const String valueText = normalizedCommand.substring(10);
    const int nextPin = valueText.toInt();

    if (valueText.length() > 0 && isActionButtonPinAllowed(nextPin)) {
      detachInterrupt(digitalPinToInterrupt(actionButtonPin));
      saveActionButtonPin(nextPin);
      configureActionButtonPin();
      Serial.print("[INPUT] action button pin set to ");
      Serial.println(actionButtonPin);
      Serial.println("[OK] buttonpin");
      return;
    }

    Serial.println("[INPUT] invalid button pin for this board");
    return;
  }

  if (normalizedCommand == "send" || normalizedCommand == "hid send") {
    if (activeMode != RUNTIME_MODE_BLE) {
      Serial.println("[HID] send is available only in BLE HID mode");
      return;
    }

    if (!bleClientConnected) {
      Serial.println("[HID] send ignored: no BLE HID host connected");
      return;
    }

    if (!isBleHostReadyForAction()) {
      Serial.println("[HID] send ignored: waiting for BLE HID host settle");
      return;
    }

    startKeyboardMeasurementCycle("serial_send");
    Serial.println("[OK] send");
    return;
  }

  if (normalizedCommand == "clearwifi") {
    clearWifiAndRestart();
    return;
  }

  if (normalizedCommand == "selftest boot") {
    Serial.print("[SELFTEST] boot = ");
    Serial.println(bootSelfTestEnabled ? "on" : "off");
    Serial.println("[OK] selftest");
    return;
  }

  if (normalizedCommand == "selftest boot on") {
    saveBootSelfTestEnabled(true);
    Serial.println("[SELFTEST] boot enabled");
    Serial.println("[OK] selftest");
    return;
  }

  if (normalizedCommand == "selftest boot off") {
    saveBootSelfTestEnabled(false);
    Serial.println("[SELFTEST] boot disabled");
    Serial.println("[OK] selftest");
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

  if (normalizedCommand == "selftest") {
    runBootSelfTest();
    Serial.println("[OK] selftest");
    return;
  }

  Serial.print("[WARN] Unknown command: ");
  Serial.println(rawCommand);
  printHelp();
}

void handleSerialInput() {
  auto readFromStream = [&](Stream& stream) {
    const bool shouldEcho = (&stream == &Serial);

    while (stream.available() > 0) {
      const char nextChar = static_cast<char>(stream.read());

      if (nextChar == '\r' || nextChar == '\n') {
        if (serialInputBuffer.isEmpty()) {
          continue;
        }
        if (shouldEcho) {
          Serial.println();
        }
        handleSerialCommand(serialInputBuffer);
        serialInputBuffer = "";
        continue;
      }

      if ((nextChar == '\b' || nextChar == 127) && !serialInputBuffer.isEmpty()) {
        serialInputBuffer.remove(serialInputBuffer.length() - 1);
        if (shouldEcho) {
          Serial.print("\b \b");
        }
        continue;
      }

      serialInputBuffer += nextChar;
      if (shouldEcho) {
        Serial.write(nextChar);
      }
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
  setupBle(true);
}

void setupCalibrationMode() {
  wifiStatusMessage = "inactive";
  wifiFailureReason = "";
  configPortalActive = false;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  setupBle(false);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.setTimeout(20);

  loadRuntimeMode();
  loadDisplayBrightness();
  loadBootSelfTestEnabled();
  loadZeroClampThreshold();
  loadActionButtonPin();
  loadDividerRatio();
  loadAdcCalibration();
  loadInaCalibration();
  setupDeviceIdentity();
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  checkBootButtonModeOverride();
  printBootInfo();
  configureActionButtonPin();

  setupAdc();
  setupBuzzer();
  setupStatusIndicators();
  setupSevenSegmentDisplay();
  displayBootMessage();
  setupI2cPeripherals();
  if (bootSelfTestEnabled) {
    runBootSelfTest();
  } else {
    Serial.println("[SELFTEST] skipped at boot");
  }
  measurementPayload = buildMeasurementPayload();

  if (activeMode == RUNTIME_MODE_WIFI) {
    setupWifiMode();
    if (isWifiConnected()) {
      startHttpServer();
    }
  } else if (activeMode == RUNTIME_MODE_BLE) {
    setupBleMode();
  } else {
    setupCalibrationMode();
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
    updateMeasurementLedState(hasLiveMeasurement(), isStable);
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
  syncActionButtonStateFromInterrupt();
  logActionButtonStateIfChanged();

  updateRgbTest();
  updateMeasurement();
  updateSevenSegmentDisplay();

  if (activeMode == RUNTIME_MODE_WIFI && isWifiConnected() && !httpServerStarted) {
    startHttpServer();
  }

  if (activeMode == RUNTIME_MODE_WIFI && httpServerStarted) {
    server.handleClient();
  }

  if (activeMode == RUNTIME_MODE_BLE || activeMode == RUNTIME_MODE_CALIBRATION) {
    updateActionButton();
  }

  if (activeMode == RUNTIME_MODE_BLE) {
    updateBleKeyboard();
  }

  logMeasurementDebug();
}
