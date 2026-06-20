#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>

const char* DEVICE_ID = "ESP32-C3-LIFTER-01";
const char* AP_SSID = "ESP32-LIFTER-SETUP";
const char* AP_PASSWORD = "12345678";

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
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long FORCE_AP_HOLD_MS = 2500;

WebServer server(80);
Preferences preferences;

float sampleBuffer[SAMPLE_WINDOW];
int sampleIndex = 0;
int sampleCount = 0;
float lastAverageVoltage = 0.0f;
int stableHits = 0;
bool isStable = false;
unsigned long lastSampleAt = 0;
String sampledAt = "";

bool wifiConnected = false;
bool provisioningMode = false;
String wifiSsid = "";
String wifiPassword = "";

String isoTimestampNow() {
  const time_t now = time(nullptr);
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

void loadWifiCredentials() {
  preferences.begin("wifi", true);
  wifiSsid = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  preferences.end();
}

void saveWifiCredentials(const String& ssid, const String& password) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  wifiSsid = ssid;
  wifiPassword = password;
}

void clearWifiCredentials() {
  preferences.begin("wifi", false);
  preferences.clear();
  preferences.end();

  wifiSsid = "";
  wifiPassword = "";
}

bool shouldForceProvisioningMode() {
  Serial.println("Hold Lift_SW during boot to force setup AP...");
  const unsigned long startedAt = millis();

  while (millis() - startedAt < FORCE_AP_HOLD_MS) {
    if (digitalRead(LIFT_SW_PIN) != LOW) {
      Serial.println("Normal boot mode.");
      return false;
    }

    blinkLed(CRGB::White, CRGB::Black, 120);
    delay(20);
  }

  Serial.println("Force setup AP requested from Lift_SW.");
  return true;
}

bool connectToOfficeWifi() {
  if (wifiSsid.isEmpty()) {
    Serial.println("No saved WiFi credentials.");
    return false;
  }

  Serial.print("Connecting to office WiFi: ");
  Serial.println(wifiSsid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startedAt) < WIFI_CONNECT_TIMEOUT_MS) {
    blinkLed(CRGB::Blue, CRGB::Black, 250);
    delay(50);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Office WiFi connection failed.");
    return false;
  }

  wifiConnected = true;
  provisioningMode = false;
  setLed(CRGB::Green);

  Serial.print("Office WiFi connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  return value;
}

String setupPageHtml(const String& message, bool isError) {
  const String escapedMessage = htmlEscape(message);
  const String escapedSsid = htmlEscape(wifiSsid);

  String html = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 WiFi Setup</title>
  <style>
    :root { color-scheme: light; }
    body {
      margin: 0;
      font-family: Arial, sans-serif;
      background: linear-gradient(180deg, #eef6ff 0%, #f7fbff 100%);
      color: #10233a;
    }
    .wrap {
      max-width: 520px;
      margin: 0 auto;
      padding: 32px 20px 48px;
    }
    .card {
      background: #ffffff;
      border: 1px solid #d7e4f3;
      border-radius: 18px;
      box-shadow: 0 16px 40px rgba(16, 35, 58, 0.08);
      padding: 24px;
    }
    h1 {
      margin: 0 0 8px;
      font-size: 28px;
    }
    p {
      margin: 0 0 16px;
      line-height: 1.5;
      color: #46617f;
    }
    label {
      display: block;
      margin: 14px 0 6px;
      font-weight: 700;
      font-size: 14px;
    }
    input {
      width: 100%;
      box-sizing: border-box;
      border: 1px solid #b7cae0;
      border-radius: 12px;
      padding: 12px 14px;
      font-size: 16px;
    }
    button {
      width: 100%;
      margin-top: 18px;
      border: 0;
      border-radius: 12px;
      padding: 13px 16px;
      font-size: 16px;
      font-weight: 700;
      background: #1d4ed8;
      color: #fff;
    }
    .hint {
      margin-top: 16px;
      font-size: 14px;
    }
    .msg {
      margin-bottom: 16px;
      padding: 12px 14px;
      border-radius: 12px;
      font-size: 14px;
      font-weight: 700;
    }
    .ok {
      background: #e7f8ed;
      color: #166534;
      border: 1px solid #b7ebc5;
    }
    .err {
      background: #fff1f2;
      color: #be123c;
      border: 1px solid #fecdd3;
    }
    code {
      background: #eef4fb;
      padding: 2px 6px;
      border-radius: 8px;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>ESP32 WiFi Setup</h1>
      <p>Connect this device to the office WiFi. After saving, the ESP32 will restart and join the office network.</p>
)HTML";

  if (!escapedMessage.isEmpty()) {
    html += "<div class=\"msg ";
    html += isError ? "err" : "ok";
    html += "\">";
    html += escapedMessage;
    html += "</div>";
  }

  html += R"HTML(
      <form method="post" action="/wifi/save">
        <label for="ssid">Office WiFi SSID</label>
        <input id="ssid" name="ssid" value=")HTML";
  html += escapedSsid;
  html += R"HTML(" placeholder="Your office WiFi name" required>

        <label for="password">Password</label>
        <input id="password" name="password" type="password" placeholder="WiFi password">

        <button type="submit">Save and Connect</button>
      </form>
      <p class="hint">Setup AP: <code>)HTML";
  html += AP_SSID;
  html += R"HTML(</code> | Password: <code>)HTML";
  html += AP_PASSWORD;
  html += R"HTML(</code></p>
    </div>
  </div>
</body>
</html>
)HTML";

  return html;
}

void startProvisioningPortal() {
  provisioningMode = true;
  wifiConnected = false;

  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_AP);
  const bool started = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (!started) {
    Serial.println("SoftAP start failed.");
    setLed(CRGB::Red);
    return;
  }

  delay(300);
  const IPAddress apIp = WiFi.softAPIP();
  Serial.println("SoftAP provisioning mode started.");
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(apIp);
  Serial.print("AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  setLed(CRGB::Purple);
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

void updateMeasurementLedState(bool triggered, bool stable) {
  if (provisioningMode) {
    blinkLed(CRGB::Purple, CRGB::Black, 300);
    return;
  }

  if (!wifiConnected) {
    blinkLed(CRGB::Red, CRGB::Black, 300);
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
  if (provisioningMode) {
    server.send(200, "text/html", setupPageHtml("", false));
    return;
  }

  server.send(200, "text/plain", "ESP32-C3 Battery Voltage Server");
}

void handleVoltage() {
  JsonDocument doc;
  doc["deviceId"] = DEVICE_ID;
  doc["sampledAt"] = sampledAt;
  doc["wifiConnected"] = wifiConnected;
  doc["provisioningMode"] = provisioningMode;

  if (provisioningMode || !wifiConnected) {
    doc["status"] = "offline";
    doc["state"] = "offline";
    doc["stable"] = false;
    doc["triggered"] = false;
    doc["switchPressed"] = false;
    doc["contact"] = false;
    doc["probeContact"] = false;
    doc["measureEnabled"] = false;
    doc["voltage"] = 0.0;
  } else {
    const bool triggered = isTriggered();
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
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleWifiSave() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/html", setupPageHtml("SSID is required.", true));
    return;
  }

  const String newSsid = server.arg("ssid");
  const String newPassword = server.arg("password");

  if (newSsid.isEmpty()) {
    server.send(400, "text/html", setupPageHtml("SSID is required.", true));
    return;
  }

  saveWifiCredentials(newSsid, newPassword);

  String html = setupPageHtml("WiFi saved. Device will restart and connect to office WiFi.", false);
  server.send(200, "text/html", html);
  delay(1200);
  ESP.restart();
}

void handleWifiClear() {
  clearWifiCredentials();
  server.send(200, "application/json", "{\"ok\":true,\"message\":\"WiFi credentials cleared. Rebooting.\"}");
  delay(600);
  ESP.restart();
}

void handleWifiStatus() {
  JsonDocument doc;
  doc["deviceId"] = DEVICE_ID;
  doc["provisioningMode"] = provisioningMode;
  doc["wifiConnected"] = wifiConnected;
  doc["ssid"] = wifiSsid;
  doc["localIp"] = wifiConnected ? WiFi.localIP().toString() : "";
  doc["apIp"] = provisioningMode ? WiFi.softAPIP().toString() : "";

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/voltage", HTTP_GET, handleVoltage);
  server.on("/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/wifi/clear", HTTP_POST, handleWifiClear);
  server.on("/wifi/status", HTTP_GET, handleWifiStatus);
  server.begin();

  Serial.println("HTTP server started.");
  Serial.println("GET  /voltage");
  Serial.println("GET  /wifi/status");
  Serial.println("POST /wifi/save");
  Serial.println("POST /wifi/clear");
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
  Serial.println("ESP32-C3 Battery Voltage Server");
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
  loadWifiCredentials();

  const bool forceProvisioning = shouldForceProvisioningMode();

  if (!forceProvisioning && connectToOfficeWifi()) {
    setupTime();
  } else {
    startProvisioningPortal();
  }

  setupServer();
}

void loop() {
  if (!provisioningMode && !wifiConnected && !wifiSsid.isEmpty()) {
    connectToOfficeWifi();
  }

  updateMeasurement();
  server.handleClient();
}
