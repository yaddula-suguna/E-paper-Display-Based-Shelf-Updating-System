#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>   //For MQTT
#include <BLEDevice.h>  
#include <ESPmDNS.h>        //For changing DHCP ip into static name
#include <Preferences.h>    //For storing data into flash 
#include <ArduinoOTA.h>     //For OTA

//Configurations & Version History
#define VERSION         "2.4.0"
#define BUILD_DATE      "2026-01-28"
#define ADMIN_USERNAME  "admin"
#define ADMIN_PASSWORD  "admin"

#define AIO_SERVER      "io.adafruit.com"
#define IO_USERNAME     ""
#define IO_KEY          ""

#define FEED_PROD       "shelf-prod"
#define FEED_QTY        "shelf-qty"
#define FEED_PRICE      "shelf-price"
#define FEED_DISC       "shelf-disc"
#define FEED_SEND       "shelf-send"

#define TARGET_NAME     "ShelfLabel_01"
#define SERVICE_UUID    "12345678-1234-1234-1234-1234567890ab"
#define CHAR_UUID       "abcd1234-5678-90ab-cdef-1234567890ab"

//Global Variables
WiFiClient espClient;
PubSubClient mqtt(espClient);
WebServer server(80);
Preferences pref;

String partProd, partQty, partPrice, partDisc;
String staSSID, staPASS, lastUpdateDate = "Initial Build";
String webLogs = ""; 
bool dataPending = false;

//Function Declaration
void logMsg(String msg);
void handleRoot();
void handleOTAInfo();
void handleLogs();
void handleAdmin();
void saveData();
void loadAllSettings();
void mqttCallback(char* topic, byte* payload, unsigned int length);


//Setup Function
void setup() {
  Serial.begin(115200);
  loadAllSettings();
  logMsg("--- GATEWAY START v" + String(VERSION) + " ---");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Shelf-Gateway", "12345678");

  if (staSSID != "") {
    logMsg("[WIFI] Connecting to " + staSSID + "...");
    WiFi.begin(staSSID.c_str(), staPASS.c_str());
    int r = 0; while (WiFi.status() != WL_CONNECTED && r < 20) { delay(500); r++; }
    if (WiFi.status() == WL_CONNECTED) logMsg("[WIFI] Connected. IP: " + WiFi.localIP().toString());
  }

  if (MDNS.begin("shelf")) {
    MDNS.addService("http", "tcp", 80);
    logMsg("[mDNS] Started http://shelf.local");
  }

  ArduinoOTA.setHostname("shelf");
  ArduinoOTA.onEnd([]() {
    Preferences p; p.begin("ota-info", false);
    p.putString("last_ota", String(BUILD_DATE)); p.end();
  });
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/ota", handleOTAInfo);
  server.on("/log", handleLogs);
  server.on("/admin", handleAdmin);
  server.on("/set", [](){
    partProd = server.arg("p"); partQty = server.arg("q");
    partPrice = server.arg("pr"); partDisc = server.arg("d");
    saveData();
    server.send(200, "text/html", "OK! <script>setTimeout(()=>location.href='/',800);</script>");
  });
  server.on("/save_wifi", [](){
    if (!server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) return server.requestAuthentication();
    pref.begin("wifi-creds", false);
    pref.putString("ssid", server.arg("s")); pref.putString("pass", server.arg("p")); pref.end();
    server.send(200, "text/html", "Restarting..."); delay(2000); ESP.restart();
  });

  server.begin();
  mqtt.setServer(AIO_SERVER, 1883);
  mqtt.setCallback(mqttCallback);
  BLEDevice::init("GATEWAY");
  logMsg("[INIT] Setup Complete.");
}

//Loop Function
void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  static unsigned long lastMqtt = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastMqtt > 10000) {
    if (!mqtt.connected()) {
      logMsg("[MQTT] Attempting connection...");
      if (mqtt.connect("ESP32_Gate_Main", IO_USERNAME, IO_KEY)) {
        mqtt.subscribe(IO_USERNAME "/feeds/#");
        logMsg("[MQTT] Subscribed to all feeds.");
      }
    }
    lastMqtt = millis();
  }
  mqtt.loop();

  if (dataPending) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 5000) {
      logMsg("[BLE] Scanning for label...");
      BLEScan* pScan = BLEDevice::getScan();
      BLEScanResults* results = pScan->start(1, false);
      for (int i = 0; i < results->getCount(); i++) {
        if (results->getDevice(i).getName() == TARGET_NAME) {
          logMsg("[BLE] Device found. Connecting...");
          BLEClient* pClient = BLEDevice::createClient();
          if (pClient->connect(results->getDevice(i).getAddress())) {
            BLERemoteService* pSvc = pClient->getService(SERVICE_UUID);
            if (pSvc) {
              BLERemoteCharacteristic* pChr = pSvc->getCharacteristic(CHAR_UUID);
              if (pChr) {
                pChr->writeValue((partProd+"|"+partQty+"|"+partPrice+"|"+partDisc).c_str());
                logMsg("[BLE] Data Sent Successfully.");
                dataPending = false;
                pref.begin("shelf-data", false); pref.putBool("pending", false); pref.end();
              }
            }
            pClient->disconnect();
          }
        }
      }
      pScan->clearResults();
      lastScan = millis();
    }
  }
}


//Function Definations
void logMsg(String msg) {
  Serial.println(msg);
  String timestamp = "[" + String(millis() / 1000) + "s] ";
  webLogs += "<div><span style='color:#888;'>" + timestamp + "</span> " + msg + "</div>";
  if (webLogs.length() > 4000) {
    webLogs = webLogs.substring(webLogs.length() - 4000);
  }
}

//Webpage UI
String getHeader(String title) {
  String s = "<html><head><title>" + title + "</title>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  s += "<style>body{font-family:sans-serif; background:#f0f2f5; margin:0; padding:20px;}";
  s += ".container{max-width:500px; margin:auto;} .card{background:white; padding:20px; border-radius:12px; box-shadow:0 2px 10px rgba(0,0,0,0.1); margin-bottom:20px;}";
  s += "h2{margin-top:0; color:#1877f2;} .btn{background:#1877f2; color:white; border:none; padding:12px; width:100%; border-radius:8px; font-weight:bold; cursor:pointer;}";
  s += "input{width:100%; padding:10px; margin:8px 0; border:1px solid #ddd; border-radius:6px; box-sizing:border-box;}";
  s += ".terminal{background:#1c1e21; color:#00ff00; padding:15px; border-radius:8px; font-family:monospace; font-size:12px; height:400px; overflow-y:auto; border:2px solid #333;}";
  s += "nav{margin-bottom:20px; text-align:center;} nav a{text-decoration:none; color:#1877f2; font-weight:bold; margin:0 10px;}</style>";
  s += "<script>void window.onload(){ var term=document.getElementById('term'); if(term) term.scrollTop=term.scrollHeight; }</script>";
  s += "</head><body><div class='container'>";
  s += "<nav><a href='/'>SALES</a> | <a href='/ota'>SYSTEM</a> | <a href='/log'>LOGS</a> | <a href='/admin'>ADMIN</a></nav>";
  return s;
}

void handleRoot() {
  String h = getHeader("Sales Dashboard");
  h += "<div class='card'><h2> Update Label</h2><form action='/set'>";
  h += "Product<input name='p' value='"+partProd+"'>";
  h += "Qty<input name='q' value='"+partQty+"'>";
  h += "Price<input name='pr' value='"+partPrice+"'>";
  h += "Discount<input name='d' value='"+partDisc+"'>";
  h += "<button class='btn'>PUSH TO SHELF</button></form></div></div></body></html>";
  server.send(200, "text/html", h);
}

void handleOTAInfo() {
  String h = getHeader("System Info");
  h += "<div class='card'><h2> System Status</h2>";
  h += "<p><b>Version:</b> v" + String(VERSION) + "</p>";
  h += "<p><b>Last OTA Update:</b> " + lastUpdateDate + "</p>";
  h += "<p><b>Uptime:</b> " + String(millis()/60000) + " minutes</p>";
  h += "</div></div></body></html>";
  server.send(200, "text/html", h);
}

void handleLogs() {
  String h = getHeader("System Logs");
  h += "<div class='card' style='max-width:100%'><h2>🖥️ Console Output</h2>";
  h += "<div id='term' class='terminal'>" + webLogs + "</div>";
  h += "<br><button class='btn' onclick='location.reload()'>REFRESH</button></div></div></body></html>";
  server.send(200, "text/html", h);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) return server.requestAuthentication();
  String h = getHeader("Admin");
  h += "<div class='card'><h2>⚙️ WiFi Config</h2><form action='/save_wifi'>";
  h += "SSID<input name='s' value='"+staSSID+"'>";
  h += "Password<input name='p' type='password'>";
  h += "<button class='btn' style='background:#d33'>REBOOT & APPLY</button></form></div></div></body></html>";
  server.send(200, "text/html", h);void setup() {
  // put your setup code here, to run once:

}


//Data Logic
void saveData() {
  pref.begin("shelf-data", false);
  pref.putString("p", partProd); pref.putString("q", partQty);
  pref.putString("pr", partPrice); pref.putString("d", partDisc);
  pref.putBool("pending", true); pref.end();
  dataPending = true;
  logMsg("[SYSTEM] Data saved to Flash. BLE Scan starting.");
}

void loadAllSettings() {
  pref.begin("shelf-data", true);
  partProd = pref.getString("p", "Empty"); partQty = pref.getString("q", "0");
  partPrice = pref.getString("pr", "0"); partDisc = pref.getString("d", "0%");
  dataPending = pref.getBool("pending", false); pref.end();
  pref.begin("wifi-creds", true); staSSID = pref.getString("ssid", ""); staPASS = pref.getString("pass", ""); pref.end();
  pref.begin("ota-info", true); lastUpdateDate = pref.getString("last_ota", "Never"); pref.end();
}

//MQTT Callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = ""; for (int i=0; i<length; i++) msg += (char)payload[i];
  String t = String(topic);
  logMsg("[MQTT] Received: " + t.substring(t.lastIndexOf("/")+1) + " = " + msg);
  if (t.endsWith(FEED_PROD)) partProd = msg;
  else if (t.endsWith(FEED_QTY)) partQty = msg;
  else if (t.endsWith(FEED_PRICE)) partPrice = msg;
  else if (t.endsWith(FEED_DISC)) partDisc = msg;
  else if (t.endsWith(FEED_SEND) && (msg == "1" || msg == "ON")) saveData();
}


