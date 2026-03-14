#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <Preferences.h>

/* ================= DEVICE INFO ================= */

#define DEVICE_NAME "Face_Control"
#define HARDWARE_VERSION "2.0"
#define FIRMWARE_VERSION "2.2.0"

#define GIT_FIRMWARE_URL "https://raw.githubusercontent.com/adarshpk5491-ops/Thara_Eye_V.1.0/main/firmware.bin"

/* ================= LED CONFIG ================= */

#define LEFT_EYE_PIN   4
#define RIGHT_EYE_PIN  12
#define MOUTH_PIN      5
#define SHOULDER_PIN   2

#define NUM_EYE_LEDS 8
#define NUM_MOUTH_LEDS 4
#define NUM_SHOULDER_LEDS 6

CRGB leftEye[NUM_EYE_LEDS];
CRGB rightEye[NUM_EYE_LEDS];
CRGB mouth[NUM_MOUTH_LEDS];
CRGB shoulder[NUM_SHOULDER_LEDS];

uint8_t brightness = 120;

/* ================= WIFI ================= */

Preferences prefs;
WebServer server(80);

String ssid;
String password;

/* ================= ANIMATION STATE ================= */

enum Mode{
  IDLE,
  TALK,
  SMILE,
  EMO,
  SLEEP,
  RAINBOW,
  HAPPY,
  SCAN,
  HEARTBEAT,
  ERROR_MODE
};

Mode currentMode = IDLE;

/* ================= HELPERS ================= */

void clearFace(){
  fill_solid(leftEye,NUM_EYE_LEDS,CRGB::Black);
  fill_solid(rightEye,NUM_EYE_LEDS,CRGB::Black);
  fill_solid(mouth,NUM_MOUTH_LEDS,CRGB::Black);
}

void mirrorShoulder(){
  for(int i=0;i<NUM_SHOULDER_LEDS;i++)
    shoulder[i]=leftEye[i%NUM_EYE_LEDS];
}

/* ================= ANIMATIONS ================= */

void animateTalk(){
  static uint8_t b=0;
  static int dir=1;

  b+=dir*4;

  if(b>120)dir=-1;
  if(b<10)dir=1;

  fill_solid(mouth,NUM_MOUTH_LEDS,CRGB::White);
  fadeLightBy(mouth,NUM_MOUTH_LEDS,255-b);
}

void animateSmile(){
  fill_solid(mouth,NUM_MOUTH_LEDS,CRGB::White);
}

void animateEmo(){

  static int pos=0;
  static int dir=1;

  fill_solid(mouth,NUM_MOUTH_LEDS,CRGB::Black);

  mouth[pos]=CRGB::White;

  pos+=dir;

  if(pos>=NUM_MOUTH_LEDS-1 || pos<=0)
    dir=-dir;
}

void animateSleep(){

  static uint8_t b=200;
  static int dir=-1;

  b+=dir;

  if(b<20)dir=1;
  if(b>200)dir=-1;

  fill_solid(leftEye,NUM_EYE_LEDS,CRGB::Blue);
  fill_solid(rightEye,NUM_EYE_LEDS,CRGB::Blue);

  fadeLightBy(leftEye,NUM_EYE_LEDS,255-b);
  fadeLightBy(rightEye,NUM_EYE_LEDS,255-b);
}

void animateRainbow(){

  static uint8_t hue=0;

  fill_rainbow(leftEye,NUM_EYE_LEDS,hue,8);
  fill_rainbow(rightEye,NUM_EYE_LEDS,hue,8);
  fill_rainbow(mouth,NUM_MOUTH_LEDS,hue,8);
  fill_rainbow(shoulder,NUM_SHOULDER_LEDS,hue,8);

  hue+=2;
}

void animateHappy(){

  static int pos=0;
  static int dir=1;

  clearFace();

  leftEye[pos]=CRGB::Yellow;
  rightEye[NUM_EYE_LEDS-pos-1]=CRGB::Yellow;

  pos+=dir;

  if(pos==NUM_EYE_LEDS-1 || pos==0)
    dir=-dir;

  fill_solid(mouth,NUM_MOUTH_LEDS,CRGB::Yellow);
}

void animateScan(){

  static int pos=0;
  static int dir=1;

  clearFace();

  leftEye[pos]=CRGB::Green;
  rightEye[pos]=CRGB::Green;

  pos+=dir;

  if(pos>=NUM_EYE_LEDS-1 || pos<=0)
    dir=-dir;
}

void animateHeartbeat(){

  static uint8_t b=0;
  static int dir=8;

  b+=dir;

  if(b>200)dir=-20;
  if(b<20)dir=8;

  CRGB col=CRGB::Red;
  col.fadeToBlackBy(255-b);

  fill_solid(leftEye,NUM_EYE_LEDS,col);
  fill_solid(rightEye,NUM_EYE_LEDS,col);
  fill_solid(mouth,NUM_MOUTH_LEDS,col);
}

void animateError(){

  static bool on=false;
  static unsigned long t=0;

  if(millis()-t>300){
    t=millis();
    on=!on;
  }

  fill_solid(leftEye,NUM_EYE_LEDS,on?CRGB::Red:CRGB::Black);
  fill_solid(rightEye,NUM_EYE_LEDS,on?CRGB::Red:CRGB::Black);
  fill_solid(mouth,NUM_MOUTH_LEDS,CRGB::Red);
}

/* ================= WIFI ================= */

void connectWiFi(){

  prefs.begin("wifi",true);

  ssid=prefs.getString("ssid","");
  password=prefs.getString("pass","");

  prefs.end();

  if(ssid==""){
    WiFi.softAP("FACE_SETUP");
    return;
  }

  WiFi.begin(ssid.c_str(),password.c_str());
}

/* ================= OTA ================= */

void updateFromGit(){

WiFiClientSecure client;
client.setInsecure();

HTTPClient https;

if(https.begin(client,GIT_FIRMWARE_URL)){

int httpCode=https.GET();

if(httpCode==HTTP_CODE_OK){

int len=https.getSize();

if(Update.begin(len)){

WiFiClient *stream=https.getStreamPtr();

size_t written=Update.writeStream(*stream);

if(Update.end(true)){
ESP.restart();
}

}

}

https.end();

}

}

/* ================= WEB UI ================= */

void handleRoot(){

String html = R"rawliteral(

<!DOCTYPE html>
<html>
<head>

<meta name="viewport" content="width=device-width,initial-scale=1">

<style>

body{
font-family:Arial;
background:#1e1e1e;
color:white;
text-align:center;
}

button{

width:140px;
height:40px;
margin:5px;
border-radius:8px;
border:none;
background:#3498db;
color:white;
font-size:14px;

}

button:hover{
background:#2980b9;
}

.card{
background:#2c2c2c;
padding:15px;
margin:10px;
border-radius:10px;
}

</style>

</head>

<body>

<h2>Thara Eye Face Controller</h2>

<div class="card">

Firmware: )rawliteral" + String(FIRMWARE_VERSION) + R"rawliteral(<br>
Device: )rawliteral" + String(DEVICE_NAME) + R"rawliteral(<br>
IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(

</div>

<div class="card">

<button onclick="fetch('/talk')">Talk</button>
<button onclick="fetch('/smile')">Smile</button>
<button onclick="fetch('/emo')">Emo</button>
<button onclick="fetch('/sleep')">Sleep</button>

<button onclick="fetch('/rainbow')">Rainbow</button>
<button onclick="fetch('/happy')">Happy</button>
<button onclick="fetch('/scan')">Scan</button>
<button onclick="fetch('/heartbeat')">Heartbeat</button>

<button onclick="fetch('/error')">Error</button>

</div>

<div class="card">

<button onclick="fetch('/gitupdate')">Git OTA Update</button>

</div>

</body>
</html>

)rawliteral";

server.send(200,"text/html",html);

}

/* ================= SETUP ================= */

void setup(){

Serial.begin(115200);

FastLED.addLeds<WS2812,LEFT_EYE_PIN,GRB>(leftEye,NUM_EYE_LEDS);
FastLED.addLeds<WS2812,RIGHT_EYE_PIN,GRB>(rightEye,NUM_EYE_LEDS);
FastLED.addLeds<WS2812,MOUTH_PIN,GRB>(mouth,NUM_MOUTH_LEDS);
FastLED.addLeds<WS2812,SHOULDER_PIN,GRB>(shoulder,NUM_SHOULDER_LEDS);

FastLED.setBrightness(brightness);

connectWiFi();

server.on("/",handleRoot);

server.on("/talk",[]{currentMode=TALK;});
server.on("/smile",[]{currentMode=SMILE;});
server.on("/emo",[]{currentMode=EMO;});
server.on("/sleep",[]{currentMode=SLEEP;});
server.on("/rainbow",[]{currentMode=RAINBOW;});
server.on("/happy",[]{currentMode=HAPPY;});
server.on("/scan",[]{currentMode=SCAN;});
server.on("/heartbeat",[]{currentMode=HEARTBEAT;});
server.on("/error",[]{currentMode=ERROR_MODE;});
server.on("/gitupdate",updateFromGit);

server.begin();

}

/* ================= LOOP ================= */

void loop(){

server.handleClient();

switch(currentMode){

case TALK: animateTalk(); break;
case SMILE: animateSmile(); break;
case EMO: animateEmo(); break;
case SLEEP: animateSleep(); break;
case RAINBOW: animateRainbow(); break;
case HAPPY: animateHappy(); break;
case SCAN: animateScan(); break;
case HEARTBEAT: animateHeartbeat(); break;
case ERROR_MODE: animateError(); break;

default: clearFace();

}

mirrorShoulder();

FastLED.show();

}