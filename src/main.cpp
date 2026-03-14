#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>

// ---- Firmware Version ----
#define FIRMWARE_VERSION "2.0.4"
#define DEVICE_NAME "Face_Control"
#define HARDWARE_VERSION "2.0"

// ---- Preferences ----
Preferences prefs;

// ---- WiFi Credentials ----
String ssid;
String password;
uint8_t globalBrightness = 128; // Default brightness

// ---- Static IP (optional) ----
IPAddress local_IP(192, 168, 11, 123);
IPAddress gateway(192, 168, 11, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// ---- Server Setup ----
WebServer server(80);

// ---- Pin mapping ----
#define LEFT_EYE_PIN   4
#define RIGHT_EYE_PIN  12
#define MOUTH_PIN      5
#define SHOULDER_PIN   2

// ---- LED counts ----
#define NUM_EYE_LEDS   8
#define NUM_MOUTH_LEDS 4
#define NUM_SHOULDER_LEDS 6

// ---- Arrays ----
CRGB leftEye[NUM_EYE_LEDS];
CRGB rightEye[NUM_EYE_LEDS];
CRGB mouth[NUM_MOUTH_LEDS];
CRGB shoulder[NUM_SHOULDER_LEDS];

// ---- State control ----
bool mouthTalkingEnabled = false;
bool smileEnabled = false;
bool emoEnabled = false;
bool surprisedEnabled = false;
bool angryEnabled = false;
bool winkEnabled = false;
bool sleepEnabled = false;
bool rainbowEnabled = false;
unsigned long shoulderStartTime = 0;
const unsigned long SHOULDER_EYE_DURATION = 5000UL; // 5 seconds for eye effect on shoulder

// ---- WiFi Connection State ----
bool wifiConnecting = false;
unsigned long wifiStartTime = 0;
const unsigned long WIFI_TIMEOUT = 90000UL; // 1.5 minutes timeout

// ---- Timers ----
unsigned long prevMouth = 0, prevEmo = 0, prevEyes = 0, prevRainbow = 0;

// ---- OTA Status ----
String otaStatus = "Idle";
size_t otaTotalSize = 0;
size_t otaUploadedSize = 0;

// ---- WiFi Event Handler ----
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print(F("\nConnected! IP: "));
      WiFi.localIP().printTo(Serial);
      Serial.println();
      wifiConnecting = false;
      shoulderStartTime = millis();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println(F("WiFi disconnected"));
      if (WiFi.getMode() == WIFI_MODE_AP) return;
      if (wifiConnecting) {
        // Already attempting connection, let timeout handle failure
        return;
      }
      // New disconnection, start reconnection attempt
      wifiConnecting = true;
      wifiStartTime = millis();
      WiFi.reconnect();
      break;
    default:
      break;
  }
}

// ---- Eye and shoulder helpers ----
void setEyeNormal(CRGB *leds, bool topHalf, bool bottomHalf, CRGB color) {
  for (int i = 0; i < 4; i++) leds[i] = topHalf ? color : CRGB::Black;
  for (int i = 4; i < 8; i++) leds[i] = bottomHalf ? color : CRGB::Black;
}

void setEyeRotated(CRGB *leds, bool topHalf, bool bottomHalf, CRGB color) {
  for (int i = 0; i < 4; i++) leds[i] = bottomHalf ? color : CRGB::Black;
  for (int i = 4; i < 8; i++) leds[i] = topHalf ? color : CRGB::Black;
}

void setShoulderEyeEffect() {
  for (int i = 0; i < min(NUM_SHOULDER_LEDS, NUM_EYE_LEDS); i++) {
    shoulder[i] = leftEye[i];
  }
  if (NUM_SHOULDER_LEDS > NUM_EYE_LEDS) {
    for (int i = NUM_EYE_LEDS; i < NUM_SHOULDER_LEDS; i++) {
      shoulder[i] = CRGB::Black;
    }
  }
}

void setShoulderBlue() {
  fill_solid(shoulder, NUM_SHOULDER_LEDS, CRGB::Blue);
}

// ---- Breathing white effect ----
void breathingWhite() {
  static uint8_t breath = 0;
  static int8_t dir = 1;
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 20UL) return;
  lastUpdate = millis();
  breath += dir * 5;
  if (breath >= 255) {
    breath = 255;
    dir = -1;
  } else if (breath <= 0) {
    breath = 0;
    dir = 1;
  }
  CRGB w = CRGB::White;
  fill_solid(leftEye, NUM_EYE_LEDS, w);
  fill_solid(rightEye, NUM_EYE_LEDS, w);
  fill_solid(mouth, NUM_MOUTH_LEDS, w);
  fill_solid(shoulder, NUM_SHOULDER_LEDS, w);
  nscale8(leftEye, NUM_EYE_LEDS, breath);
  nscale8(rightEye, NUM_EYE_LEDS, breath);
  nscale8(mouth, NUM_MOUTH_LEDS, breath);
  nscale8(shoulder, NUM_SHOULDER_LEDS, breath);
}

// ---- Mouth animations ----
void animateMouthTalking() {
  if (millis() - prevMouth < 80UL) return;
  prevMouth = millis();
  static uint8_t brightness = 0; // Changed from float to uint8_t
  static int8_t dir = 1;
  brightness += dir * 2;
  if (brightness >= 100) dir = -1;
  else if (brightness <= 10) dir = 1;
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
  for (int i = 0; i < NUM_MOUTH_LEDS; i++) {
    if (random8() < 80) {
      mouth[i] = CRGB::White;
      mouth[i].fadeLightBy(255 - brightness);
    }
  }
}

void showSmile() {
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::White);
}

void animateEmo() {
  if (millis() - prevEmo < 150UL) return;
  prevEmo = millis();
  static int pos = 0;
  static int dir = 1;
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
  mouth[pos] = CRGB::White;
  pos += dir;
  if (pos >= NUM_MOUTH_LEDS - 1 || pos <= 0) dir = -dir;
}

void animateSurprised() {
  setEyeNormal(leftEye, true, true, CRGB::White);
  setEyeRotated(rightEye, true, true, CRGB::White);
  static uint8_t b = 0;
  static int8_t dir = 5;
  b += dir;
  if (b == 0 || b == 255) dir = -dir;
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Blue);
  fadeLightBy(mouth, NUM_MOUTH_LEDS, 255 - b);
}

void animateAngry() {
  setEyeNormal(leftEye, true, true, CRGB::Red);
  setEyeRotated(rightEye, true, true, CRGB::Red);
  static bool on = false;
  static unsigned long t = 0;
  if (millis() - t > 150UL) {
    t = millis();
    on = !on;
  }
  fill_solid(mouth, NUM_MOUTH_LEDS, on ? CRGB::Red : CRGB::Black);
}

void animateWink() {
  static unsigned long t = 0;
  static bool leftClosed = false;
  if (millis() - t > 3000UL) {
    t = millis();
    leftClosed = !leftClosed;
  }
  if (leftClosed) {
    setEyeNormal(leftEye, false, false, CRGB::Black);
  } else {
    setEyeNormal(leftEye, true, true, CRGB::White);
  }
  setEyeRotated(rightEye, true, true, CRGB::White);
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
}

void animateSleep() {
  static int8_t dir = -2;
  static uint8_t b = 255;
  b += dir;
  if (b <= 10) b = 10;
  fill_solid(leftEye, NUM_EYE_LEDS, CRGB::Blue);
  fill_solid(rightEye, NUM_EYE_LEDS, CRGB::Blue);
  fadeLightBy(leftEye, NUM_EYE_LEDS, 255 - b);
  fadeLightBy(rightEye, NUM_EYE_LEDS, 255 - b);
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
}

// ---- Rainbow mode ----
void animateRainbow() {
  if (millis() - prevRainbow < 50UL) return;
  prevRainbow = millis();
  static uint8_t startHue = 0;
  fill_rainbow(leftEye, NUM_EYE_LEDS, startHue++, 7);
  fill_rainbow(rightEye, NUM_EYE_LEDS, startHue++, 7);
  fill_rainbow(mouth, NUM_MOUTH_LEDS, startHue++, 7);
  fill_rainbow(shoulder, NUM_SHOULDER_LEDS, startHue++, 7);
}

// ---- Web handlers for brightness ----
void handleBrightnessUp() {
  if (globalBrightness <= 245) globalBrightness += 10;
  else globalBrightness = 255;
  server.send(200, "text/plain", "Brightness increased");
}

void handleBrightnessDown() {
  if (globalBrightness >= 10) globalBrightness -= 10;
  else globalBrightness = 0;
  server.send(200, "text/plain", "Brightness decreased");
}

// ---- Eye blinking ----
void animateEyesBlinking() {
  static uint8_t state = 0;
  const unsigned long intervals[] = {2000UL, 200UL, 200UL, 200UL};
  if (millis() - prevEyes < intervals[state]) return;
  prevEyes = millis();
  switch (state) {
    case 0:
      setEyeNormal(leftEye, true, true, CRGB::White);
      setEyeRotated(rightEye, true, true, CRGB::White);
      break;
    case 1:
      setEyeNormal(leftEye, true, false, CRGB::White);
      setEyeRotated(rightEye, true, false, CRGB::White);
      break;
    case 2:
      setEyeNormal(leftEye, false, false, CRGB::White);
      setEyeRotated(rightEye, false, false, CRGB::White);
      break;
    case 3:
      setEyeNormal(leftEye, true, true, CRGB::White);
      setEyeRotated(rightEye, true, true, CRGB::White);
      break;
  }
  state = (state + 1) % 4;
}

// ---- Get WiFi Status String ----
String getWiFiStatus() {
  if (WiFi.getMode() == WIFI_MODE_AP) {
    String status = F("Access Point Mode<br>AP SSID: FACE Setup<br>AP IP: ");
    status += WiFi.softAPIP().toString();
    return status;
  } else if (WiFi.status() == WL_CONNECTED) {
    String status = F("Connected to ");
    status += WiFi.SSID();
    status += F("<br>IP: ");
    status += WiFi.localIP().toString();
    status += F("<br>RSSI: ");
    status += String(WiFi.RSSI());
    status += F(" dBm");
    return status;
  } else {
    return F("Not Connected<br>Mode: Station");
  }
}

// ---- Web handlers ----
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
    <head>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; padding: 10px; margin: 0; }
        h2 { color: #2c3e50; font-size: 18px; margin: 10px 0; }
        .highlight { color: #e74c3c; font-weight: bold; }
        .button-group { margin: 5px 0; display: flex; flex-wrap: wrap; justify-content: center; }
        button { background-color: #3498db; color: white; border: none; padding: 6px 12px; margin: 3px; font-size: 14px; border-radius: 4px; cursor: pointer; }
        button:hover { background-color: #2980b9; }
        input[type=text], input[type=password], select { padding: 4px; margin: 3px; width: 150px; font-size: 14px; }
        input[type=submit] { padding: 6px 12px; margin: 5px; font-size: 14px; border-radius: 4px; cursor: pointer; }
        form { margin: 10px 0; }
        h3 { font-size: 16px; margin: 10px 0; }
        #otaStatus { font-size: 14px; margin: 5px 0; font-weight: bold; }
        #otaProgress { width: 90%; margin: 5px auto; }
        .status { font-size: 14px; margin: 10px 0; padding: 10px; background-color: #ecf0f1; border-radius: 4px; }
      </style>
      <script>
        function updateOTAStatus() {
          fetch('/otaStatus')
            .then(response => response.json())
            .then(data => {
              document.getElementById('otaStatus').innerText = 'OTA Status: ' + data.status;
              if (data.totalSize > 0) {
                let progress = (data.uploadedSize / data.totalSize) * 100;
                document.getElementById('otaProgress').value = progress;
              }
            });
        }
        setInterval(updateOTAStatus, 1000);
      </script>
    </head>
    <body onload="updateOTAStatus()">
      <h2>Face Control <span class="highlight">by Embedded Team</span></h2>

      <h3>Device Information</h3>
      <div class="status">
        Device Name: )rawliteral";
  html += DEVICE_NAME;
  html += R"rawliteral(<br>
        Firmware Version: )rawliteral";
  html += FIRMWARE_VERSION;
  html += R"rawliteral(<br>
        Hardware Version: )rawliteral";
  html += HARDWARE_VERSION;
  html += R"rawliteral(<br>
        MAC Address: )rawliteral";
  html += WiFi.macAddress();
  html += R"rawliteral(<br>
        Free Heap: )rawliteral";
  html += String(ESP.getFreeHeap());
  html += R"rawliteral( bytes
      </div>

      <h3>System Status</h3>
      <div class="status">
        )rawliteral";
  html += getWiFiStatus();
  html += R"rawliteral(
      </div>

      <div class="button-group">
        <button onclick="fetch('/start')">Talk</button>
        <button onclick="fetch('/stop')">Stop</button>
        <button onclick="fetch('/smile')">Smile</button>
        <button onclick="fetch('/smileoff')">Smile Off</button>
      </div>

      <div class="button-group">
        <button onclick="fetch('/emo')">Emo</button>
        <button onclick="fetch('/emooff')">Emo Off</button>
        <button onclick="fetch('/surprised')">Surprised</button>
        <button onclick="fetch('/surprisedoff')">Surp. Off</button>
      </div>

      <div class="button-group">
        <button onclick="fetch('/angry')">Angry</button>
        <button onclick="fetch('/angryoff')">Angry Off</button>
        <button onclick="fetch('/wink')">Wink</button>
        <button onclick="fetch('/winkoff')">Wink Off</button>
      </div>

      <div class="button-group">
        <button onclick="fetch('/sleep')">Sleep</button>
        <button onclick="fetch('/sleepoff')">Sleep Off</button>
        <button onclick="fetch('/rainbow')">Rainbow</button>
        <button onclick="fetch('/rainbowoff')">Rain. Off</button>
      </div>

      <div class="button-group">
        <button onclick="fetch('/brightup')">Bright +</button>
        <button onclick="fetch('/brightdown')">Bright -</button>
      </div>

      <h3>WiFi Setup</h3>
      <form method='POST' action='/setwifi'>)rawliteral";

  // Non-blocking approach: offer saved SSID as default
  prefs.begin("wifi", true); // read-only
  String savedSsid = prefs.getString("ssid", "");
  prefs.end();
  if (savedSsid.length() > 0) {
    html += F("Saved SSID: <b>");
    html += savedSsid;
    html += F("</b><br>");
  }
  html += F("SSID: <input name='ssid' value='");
  html += savedSsid;
  html += R"rawliteral("'><br>
        Pass: <input name='pass' type='password'><br>
        <input type='submit' value='Save & Connect'>
      </form>
      <form method='GET' action='/'><input type='submit' value='Refresh Status'></form>

      <h3>OTA Update</h3>
      <form method='POST' action='/update' enctype='multipart/form-data'>
        <input type='file' name='update'>
        <input type='submit' value='Upload'>
      </form>
      <div id='otaStatus'>OTA Status: Idle</div>
      <progress id='otaProgress' value='0' max='100'></progress>
    </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// ---- OTA Status Handler ----
void handleOTAStatus() {
  String json = "{\"status\":\"";
  json += otaStatus;
  json += "\",\"uploadedSize\":";
  json += String(otaUploadedSize);
  json += ",\"totalSize\":";
  json += String(otaTotalSize);
  json += "}";
  server.send(200, "application/json", json);
}

// ---- Control Handlers ----
void handleStart() { mouthTalkingEnabled = true; server.send(200, "text/plain", "Mouth animation started"); }
void handleStop() {
  mouthTalkingEnabled = false;
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
  FastLED.show();
  server.send(200, "text/plain", "Mouth animation stopped");
}
void handleSmile() { smileEnabled = true; server.send(200, "text/plain", "Smile enabled"); }
void handleSmileOff() {
  smileEnabled = false;
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
  FastLED.show();
  server.send(200, "text/plain", "Smile disabled");
}
void handleEmo() { emoEnabled = true; server.send(200, "text/plain", "Emo animation started"); }
void handleEmoOff() {
  emoEnabled = false;
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
  FastLED.show();
  server.send(200, "text/plain", "Emo animation stopped");
}
void handleSurprised() { surprisedEnabled = true; server.send(200, "text/plain", "Surprised ON"); }
void handleSurprisedOff() { surprisedEnabled = false; server.send(200, "text/plain", "Surprised OFF"); }
void handleAngry() { angryEnabled = true; server.send(200, "text/plain", "Angry ON"); }
void handleAngryOff() { angryEnabled = false; server.send(200, "text/plain", "Angry OFF"); }
void handleWink() { winkEnabled = true; server.send(200, "text/plain", "Wink ON"); }
void handleWinkOff() { winkEnabled = false; server.send(200, "text/plain", "Wink OFF"); }
void handleSleep() { sleepEnabled = true; server.send(200, "text/plain", "Sleep ON"); }
void handleSleepOff() { sleepEnabled = false; server.send(200, "text/plain", "Sleep OFF"); }
void handleRainbow() { rainbowEnabled = true; server.send(200, "text/plain", "Rainbow ON"); }
void handleRainbowOff() {
  rainbowEnabled = false;
  fill_solid(leftEye, NUM_EYE_LEDS, CRGB::Black);
  fill_solid(rightEye, NUM_EYE_LEDS, CRGB::Black);
  fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
  fill_solid(shoulder, NUM_SHOULDER_LEDS, CRGB::Black);
  FastLED.show();
  server.send(200, "text/plain", "Rainbow OFF");
}

// ---- WiFi setup ----
void connectToWiFi() {
  prefs.begin("wifi", false);
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");

  if (ssid.length() == 0 || password.length() == 0) {
    Serial.println(F("No saved WiFi credentials. Starting AP mode."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FACE Setup");
    Serial.print(F("AP IP: "));
    WiFi.softAPIP().printTo(Serial);
    Serial.println();
    wifiConnecting = false;
    shoulderStartTime = millis();
    prefs.end();
    return;
  }
  prefs.end();

  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println(F("Failed to configure static IP - continuing with DHCP"));
  }
  WiFi.begin(ssid.c_str(), password.c_str());
  wifiConnecting = true;
  wifiStartTime = millis();
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(ssid.c_str());
}

// ---- Attempt a reconnect without restart after saving credentials ----
void tryReconnectWithSavedCreds() {
  prefs.begin("wifi", false);
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");
  if (ssid.length() == 0) {
    Serial.println(F("No saved credentials, starting AP mode."));
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FACE Setup");
    Serial.print(F("AP IP: "));
    WiFi.softAPIP().printTo(Serial);
    Serial.println();
    wifiConnecting = false;
    shoulderStartTime = millis();
    prefs.end();
    return;
  }
  prefs.end();
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println(F("Failed to configure static IP - continuing with DHCP"));
  }
  WiFi.begin(ssid.c_str(), password.c_str());
  wifiConnecting = true;
  wifiStartTime = millis();
  Serial.print(F("Attempting connection to saved SSID: "));
  Serial.println(ssid.c_str());
}

// ---- Check WiFi Connection Status ----
void checkWiFiStatus() {
  if (!wifiConnecting) return;

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnecting = false;
    shoulderStartTime = millis();
  } else if (millis() - wifiStartTime >= WIFI_TIMEOUT) {
    Serial.println(F("\nConnection failed. Starting AP mode."));
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("FACE Setup");
    Serial.print(F("AP IP: "));
    WiFi.softAPIP().printTo(Serial);
    Serial.println();
    wifiConnecting = false;
    shoulderStartTime = millis();
  }
}

// ---- SET WIFI (POST) ----
void handleSetWiFi() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String newSsid = server.arg("ssid");
    String newPass = server.arg("pass");
    prefs.begin("wifi", false);
    prefs.putString("ssid", newSsid);
    prefs.putString("pass", newPass);
    prefs.end();
    server.send(200, "text/plain", "WiFi saved. Attempting to connect...");
    Serial.println(F("Saved new WiFi credentials, attempting reconnect..."));
    tryReconnectWithSavedCreds();
  } else {
    server.send(400, "text/plain", "Missing SSID or Password");
  }
}

// ---- OTA Upload Handler ----
void handleUpdate() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaStatus = "Uploading";
    otaTotalSize = upload.totalSize;
    otaUploadedSize = 0;
    Serial.printf("OTA Update Started: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      otaStatus = "Error: Begin Failed";
      Update.printError(Serial);
      Serial.println(F("Update.begin failed"));
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    otaUploadedSize += upload.currentSize;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      otaStatus = "Error: Write Failed";
      Update.printError(Serial);
      Serial.println(F("Update.write failed"));
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaStatus = "Success: Update written and restarting";
      Serial.println(F("OTA Update Successful. Restarting ESP..."));
      server.send(200, "text/plain", "Update Successful. Device restarting...");
      delay(1000);
      ESP.restart();
    } else {
      otaStatus = "Error: End Failed";
      Update.printError(Serial);
      Serial.println(F("Update.end failed"));
      server.send(500, "text/plain", "Update Failed during finalize");
    }
    otaUploadedSize = otaTotalSize;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    otaStatus = "Error: Upload Aborted";
    Update.abort();
    Serial.println(F("OTA Update Aborted"));
    server.send(400, "text/plain", "Upload Aborted");
  }
  yield();
}

// ---- Setup ----
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.print(F("Device: "));
  Serial.println(DEVICE_NAME);
  Serial.print(F("Firmware Version: "));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("Hardware Version: "));
  Serial.println(HARDWARE_VERSION);

  FastLED.addLeds<WS2812, LEFT_EYE_PIN, GRB>(leftEye, NUM_EYE_LEDS);
  FastLED.addLeds<WS2812, RIGHT_EYE_PIN, GRB>(rightEye, NUM_EYE_LEDS);
  FastLED.addLeds<WS2812, MOUTH_PIN, GRB>(mouth, NUM_MOUTH_LEDS);
  FastLED.addLeds<WS2812, SHOULDER_PIN, GRB>(shoulder, NUM_SHOULDER_LEDS);
  FastLED.clear();
  FastLED.show();
  delay(100);

  setEyeNormal(leftEye, true, true, CRGB::White);
  setEyeRotated(rightEye, true, true, CRGB::White);
  setShoulderEyeEffect();
  FastLED.setBrightness(globalBrightness);
  FastLED.show();

  connectToWiFi();

  WiFi.onEvent(WiFiEvent);

  server.on("/", handleRoot);
  server.on("/otaStatus", handleOTAStatus);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/smile", handleSmile);
  server.on("/smileoff", handleSmileOff);
  server.on("/emo", handleEmo);
  server.on("/emooff", handleEmoOff);
  server.on("/surprised", handleSurprised);
  server.on("/surprisedoff", handleSurprisedOff);
  server.on("/angry", handleAngry);
  server.on("/angryoff", handleAngryOff);
  server.on("/wink", handleWink);
  server.on("/winkoff", handleWinkOff);
  server.on("/sleep", handleSleep);
  server.on("/sleepoff", handleSleepOff);
  server.on("/rainbow", handleRainbow);
  server.on("/rainbowoff", handleRainbowOff);
  server.on("/setwifi", HTTP_POST, handleSetWiFi);
  server.on("/update", HTTP_POST, []() { server.send(200); }, handleUpdate);
  server.on("/brightup", handleBrightnessUp);
  server.on("/brightdown", handleBrightnessDown);

  server.begin();
  Serial.println(F("HTTP server started"));
}

// ---- Loop ----
void loop() {
  server.handleClient();
  checkWiFiStatus();

  if (wifiConnecting) {
    breathingWhite();
    FastLED.setBrightness(globalBrightness);
    FastLED.show();
    return; // Early return to reduce load during connection
  }

  // Core animation selection
  animateEyesBlinking();

  if (rainbowEnabled) {
    animateRainbow();
  } else if (surprisedEnabled) {
    animateSurprised();
    setShoulderBlue();
  } else if (angryEnabled) {
    animateAngry();
    setShoulderBlue();
  } else if (winkEnabled) {
    animateWink();
    setShoulderBlue();
  } else if (sleepEnabled) {
    animateSleep();
    setShoulderBlue();
  } else if (emoEnabled) {
    animateEmo();
    setShoulderBlue();
  } else if (smileEnabled) {
    showSmile();
    setShoulderBlue();
  } else if (mouthTalkingEnabled) {
    animateMouthTalking();
    setShoulderBlue();
  } else {
    fill_solid(mouth, NUM_MOUTH_LEDS, CRGB::Black);
    if (millis() - shoulderStartTime < SHOULDER_EYE_DURATION) {
      setShoulderEyeEffect();
    } else {
      setShoulderBlue();
    }
  }
  FastLED.setBrightness(globalBrightness);
  FastLED.show();
}