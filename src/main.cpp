/*
 * CARDBIRD Web Controller — ESP32, 5 servos
 * Control the cardboard robotic arm from any browser on the same WiFi.
 * One slider per servo, live (no page reload).
 *
 * REQUIRED LIBRARY (Arduino IDE -> Tools -> Manage Libraries):
 *   "ESP32Servo" by Kevin Harrington / John K. Bennett
 *   (WiFi.h and WebServer.h ship with the ESP32 board package, so no extra install)
 *
 * BOARD: any ESP32 dev board (e.g. "ESP32 Dev Module" / WROOM-32).
 *
 * USAGE:
 *   1. Set WIFI_SSID / WIFI_PASS below.
 *   2. Wire each servo signal wire to the pins in servoPin[] (defaults: 13,14,25,26,27).
 *   3. Upload, open Serial Monitor @ 115200 baud, read the IP it prints.
 *   4. Open that IP in a browser (http://192.168.x.x) — phone or laptop on the same WiFi.
 *
 * POWER (read this — it's the #1 cause of resets/fried boards):
 *   Servos must run off an EXTERNAL 5V supply (a 5V/2A+ wall adapter or 4xAA), NOT the
 *   ESP32's 3.3V pin or USB. Tie the external supply's GND to the ESP32 GND (common ground).
 *   Signal wires go to the GPIO pins; only the ground is shared.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ---------------- USER CONFIG ----------------
const char* WIFI_SSID = "CottonwoodATT";
const char* WIFI_PASS = "t8zd7kqakfz#";

#define NUM_SERVOS 4
#define CR  0
#define POS 1
 
// Base & Elbow moved to fresh pins (were 13 & 25) to test the dead channels.
const int   servoPin[NUM_SERVOS]  = {  18,        14,        19,        26     };
const char* servoName[NUM_SERVOS] = { "Base", "Shoulder", "Elbow",  "Gripper" };
 
// Which joints are positional (MG995) vs continuous-rotation.
// Default: only the Shoulder (index 1) is positional. Change CR<->POS to move it.
int type[NUM_SERVOS] = { CR, POS, CR, CR };
 
// --- Positional (MG995) settings ---
const int POS_MIN_US = 500;       // pulse range for the positional servo
const int POS_MAX_US = 2500;
int posHome[NUM_SERVOS] = { 90, 90, 90, 90 };   // boot angle (positional joints hold here)
int posMin[NUM_SERVOS]  = {  0,  0,  0,  0 };    // tighten to protect the joint
int posMax[NUM_SERVOS]  = {180,180,180,180 };
 
// --- Continuous-rotation settings ---
int stopUs[NUM_SERVOS]  = {1500,1500,1500,1500}; // tweak if a CR servo creeps when stopped
int dir[NUM_SERVOS]     = {  +1,  +1,  +1,  +1 }; // flip a CR joint that goes the wrong way
const int SPEED_RANGE   = 350;    // us from stop at full jog
const int NUDGE_SPEED   = 55;     // jog speed (%) during a nudge burst
const int DEF_STEP_MS   = 150;    // default nudge duration
// ---------------------------------------------
 
Servo     servo[NUM_SERVOS];
int       curAngle[NUM_SERVOS];
WebServer server(80);
 
void writeStop(int i) { servo[i].writeMicroseconds(stopUs[i]); }
 
void setSpeed(int i, int v) {                 // CR: v = -100..100, 0 = stop
  if (v < -100) v = -100;
  if (v >  100) v =  100;
  servo[i].writeMicroseconds(stopUs[i] + dir[i] * (v * SPEED_RANGE / 100));
}
 
void setAngle(int i, int a) {                 // POS: a = degrees
  if (a < posMin[i]) a = posMin[i];
  if (a > posMax[i]) a = posMax[i];
  curAngle[i] = a;
  servo[i].write(a);
}
 
String buildPage() {
  String h = F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CARDBIRD Control</title><style>"
    "body{font-family:system-ui,Arial,sans-serif;margin:0;background:#11141a;color:#e8edf2}"
    ".wrap{max-width:520px;margin:0 auto;padding:20px}"
    "h1{font-size:1.4rem;margin:.2rem 0 .3rem}"
    ".note{color:#8a93a3;font-size:.85rem;margin:0 0 1rem}"
    ".row{background:#1b1f29;border-radius:12px;padding:14px 16px;margin:12px 0}"
    ".tag{font-size:.7rem;color:#8a93a3;font-weight:600}"
    ".lbl{display:flex;justify-content:space-between;font-weight:600;margin-bottom:8px}"
    ".val{font-variant-numeric:tabular-nums;color:#5cc8ff}"
    "input[type=range]{width:100%;height:34px;accent-color:#5cc8ff}"
    ".nudge{display:flex;gap:10px;margin-top:10px}"
    ".nudge button{flex:1;padding:14px;border:0;border-radius:10px;background:#2a3340;"
    "color:#e8edf2;font-size:1.2rem;font-weight:800}"
    ".stop{width:100%;padding:16px;margin-top:16px;border:0;border-radius:12px;"
    "background:#ff5c5c;color:#160606;font-size:1.1rem;font-weight:800}"
    "</style></head><body><div class='wrap'><h1>CARDBIRD Control</h1>"
    "<p class='note'>Positional joint holds its angle. CR joints: tap &lt;/&gt; to step, slider to spin.</p>"
    "<div class='row'><div class='lbl'><span>Step size</span>"
    "<span class='val'><span id='stepv'>150</span> ms</span></div>"
    "<input type='range' id='step' min='40' max='500' value='150' "
    "oninput='document.getElementById(\"stepv\").innerHTML=this.value'></div>");
 
  for (int i = 0; i < NUM_SERVOS; i++) {
    h += "<div class='row'><div class='lbl'><span>";
    h += servoName[i];
    if (type[i] == POS) {
      h += " <span class='tag'>HOLDS</span></span><span class='val' id='v"; h += i; h += "'>";
      h += curAngle[i]; h += "&deg;</span></div>"
           "<input type='range' min='"; h += posMin[i]; h += "' max='"; h += posMax[i];
      h += "' value='"; h += curAngle[i];
      h += "' oninput='pos("; h += i; h += ",this.value)' onchange='posf("; h += i; h += ",this.value)'>";
    } else {
      h += " <span class='tag'>SPIN</span></span><span class='val' id='v"; h += i; h += "'>0</span></div>"
           "<input type='range' class='cr' data-i='"; h += i;
      h += "' min='-100' max='100' value='0' "
           "oninput='mv("; h += i; h += ",this.value)' onchange='rel("; h += i; h += ",this)'>"
           "<div class='nudge'>"
           "<button onclick='nudge("; h += i; h += ",-1)'>&#9664;</button>"
           "<button onclick='nudge("; h += i; h += ",1)'>&#9654;</button></div>";
    }
    h += "</div>";
  }
 
  h += F("<button class='stop' onclick='stopAll()'>STOP ALL (spin)</button>"
    "<script>"
    "let t=0;"
    "function lbl(i,x){document.getElementById('v'+i).innerHTML=x;}"
    "function snd(i,v){fetch('/set?s='+i+'&v='+v);}"
    "function pos(i,a){lbl(i,a+'\\u00B0');var n=Date.now();if(n-t<40)return;t=n;snd(i,a);}"
    "function posf(i,a){lbl(i,a+'\\u00B0');snd(i,a);}"
    "function mv(i,v){lbl(i,v);var n=Date.now();if(n-t<40)return;t=n;snd(i,v);}"
    "function rel(i,el){el.value=0;lbl(i,0);snd(i,0);}"
    "function nudge(i,d){fetch('/nudge?s='+i+'&d='+d+'&t='+document.getElementById('step').value);}"
    "function stopAll(){var s=document.querySelectorAll('.cr');"
    "for(var k=0;k<s.length;k++){var i=s[k].getAttribute('data-i');s[k].value=0;lbl(i,0);}fetch('/stop');}"
    "</script></body></html>");
  return h;
}
 
void handleRoot() { server.send(200, "text/html", buildPage()); }
 
void handleSet() {
  if (server.hasArg("s") && server.hasArg("v")) {
    int i = server.arg("s").toInt();
    int v = server.arg("v").toInt();
    if (i >= 0 && i < NUM_SERVOS) {
      if (type[i] == POS) setAngle(i, v);
      else                setSpeed(i, v);
    }
  }
  server.send(200, "text/plain", "ok");
}
 
void handleNudge() {
  if (server.hasArg("s") && server.hasArg("d")) {
    int i = server.arg("s").toInt();
    int d = server.arg("d").toInt();
    int ms = server.hasArg("t") ? server.arg("t").toInt() : DEF_STEP_MS;
    if (ms < 20)  ms = 20;
    if (ms > 800) ms = 800;
    if (i >= 0 && i < NUM_SERVOS && type[i] == CR) {
      setSpeed(i, (d >= 0 ? 1 : -1) * NUDGE_SPEED);
      delay(ms);
      writeStop(i);
    }
  }
  server.send(200, "text/plain", "ok");
}
 
void handleStop() {                            // stops CR servos; positional joints keep holding
  for (int i = 0; i < NUM_SERVOS; i++)
    if (type[i] == CR) writeStop(i);
  server.send(200, "text/plain", "ok");
}
 
void setup() {
  Serial.begin(115200);
 
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
 
  for (int i = 0; i < NUM_SERVOS; i++) {
    servo[i].setPeriodHertz(50);
    if (type[i] == POS) {
      servo[i].attach(servoPin[i], POS_MIN_US, POS_MAX_US);
      curAngle[i] = posHome[i];
      servo[i].write(posHome[i]);             // hold at home
    } else {
      servo[i].attach(servoPin[i], 500, 2500);
      writeStop(i);                           // start stopped
    }
  }
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.print("\nReady!  Open  http://");
  Serial.println(WiFi.localIP());
 
  server.on("/",      handleRoot);
  server.on("/set",   handleSet);
  server.on("/nudge", handleNudge);
  server.on("/stop",  handleStop);
  server.begin();
}
 
void loop() {
  server.handleClient();
}