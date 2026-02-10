#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <ESPmDNS.h>      //used for changing DHCP I  P to static name
#include <Preferences.h>  //Used for storing data in flash


//Confugration
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
String staSSID, staPASS;
bool dataPending = false;

//Function Declaration
void saveData();
void loadAllSettings();
void handleRoot();
void handleAdmin();
void mqttCallback(char* topic, byte* payload, unsigned int length);

//Setup Function
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("      GATEWAY SYSTEM START    ");

  loadAllSettings();

  // 1. Hotspot Setup
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Shelf-Gateway", "12345678");
  Serial.print("[INFO] Hotspot IP: "); Serial.println(WiFi.softAPIP());

  // 2. Home WiFi Setup
  if (staSSID != "" && staSSID != "NULL") {
    Serial.print("[INFO] Connecting to Home WiFi: "); Serial.println(staSSID);
    WiFi.begin(staSSID.c_str(), staPASS.c_str());
  }

  // 3. mDNS Setup
  if (MDNS.begin("shelf")) {
    Serial.println("[INFO] mDNS Started: http://shelf.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[ERROR] mDNS Failed!");
  }

  // 4. Server Routes
  server.on("/", handleRoot);
  server.on("/admin", handleAdmin);
  server.on("/set", [](){
    partProd = server.arg("p"); partQty = server.arg("q");
    partPrice = server.arg("pr"); partDisc = server.arg("d");
    saveData();
    server.send(200, "text/html", "Stored! <script>setTimeout(()=>location.href='/',1000);</script>");
  });
  server.on("/save_wifi", [](){
    if (!server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) return server.requestAuthentication();
    pref.begin("wifi-creds", false);
    pref.putString("ssid", server.arg("s"));
    pref.putString("pass", server.arg("p"));
    pref.end();
    server.send(200, "text/html", "Rebooting...");
    Serial.println("[ADMIN] WiFi Settings Updated. Rebooting...");
    delay(2000); ESP.restart();
  });
  
  server.begin();
  mqtt.setServer(AIO_SERVER, 1883);
  mqtt.setCallback(mqttCallback);
  BLEDevice::init("GATEWAY");
  Serial.println("[INFO] Server and BLE Initialized.");
}

//Loop Function
void loop() {
  server.handleClient();
  
  // Every 10 seconds, print status for debugging
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[STATUS] WiFi Connected (IP: "); Serial.print(WiFi.localIP()); Serial.println(")");
    } else {
      Serial.println("[STATUS] WiFi Disconnected from Home Network.");
    }
    lastStatus = millis();
  }

  // MQTT Connection Logic
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      Serial.print("[MQTT] Attempting connection...");
      if (mqtt.connect("ESP32_Gate_Main", IO_USERNAME, IO_KEY)) {
        Serial.println("Success!");
        mqtt.subscribe(IO_USERNAME "/feeds/" FEED_PROD);
        mqtt.subscribe(IO_USERNAME "/feeds/" FEED_QTY);
        mqtt.subscribe(IO_USERNAME "/feeds/" FEED_PRICE);
        mqtt.subscribe(IO_USERNAME "/feeds/" FEED_DISC);
        mqtt.subscribe(IO_USERNAME "/feeds/" FEED_SEND);
      } else {
        Serial.print("Failed, state: "); Serial.println(mqtt.state());
      }
    }
    mqtt.loop();
  }

  // BLE Scan and Connect Logic
  if (dataPending) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 5000) { // Scan every 5 seconds
      Serial.println("[BLE] Scanning for Shelf Label...");
      BLEScan* pScan = BLEDevice::getScan();
      pScan->setActiveScan(true);
      BLEScanResults* results = pScan->start(1, false);
      
      for (int i = 0; i < results->getCount(); i++) {
        BLEAdvertisedDevice device = results->getDevice(i);
        if (device.getName() == TARGET_NAME) {
          Serial.println("[BLE] Target Found! Connecting...");
          BLEClient* pClient = BLEDevice::createClient();
          if (pClient->connect(device.getAddress())) {
            BLERemoteService* pSvc = pClient->getService(SERVICE_UUID);
            if (pSvc) {
              BLERemoteCharacteristic* pChr = pSvc->getCharacteristic(CHAR_UUID);
              if (pChr) {
                String payload = partProd + "|" + partQty + "|" + partPrice + "|" + partDisc;
                pChr->writeValue(payload.c_str(), payload.length());
                Serial.println("[BLE] DATA SENT: " + payload);
                dataPending = false;
                pref.begin("shelf-data", false); pref.putBool("pending", false); pref.end();
              }
            }
            pClient->disconnect();
          } else {
            Serial.println("[BLE] Connection Failed.");
          }
        }
      }
      pScan->clearResults();
      lastScan = millis();
    }
  }
}

//Function Definations
//Storage Logic
void saveData() {
  pref.begin("shelf-data", false);
  pref.putString("p", partProd); pref.putString("q", partQty);
  pref.putString("pr", partPrice); pref.putString("d", partDisc);
  pref.putBool("pending", true);
  pref.end();
  dataPending = true;
  Serial.println("\n[DEBUG] Data saved to Flash Memory.");
}

void loadAllSettings() {
  pref.begin("shelf-data", true);
  partProd = pref.getString("p", "Empty");
  partQty  = pref.getString("q", "0");
  partPrice = pref.getString("pr", "0");
  partDisc  = pref.getString("d", "0%");
  dataPending = pref.getBool("pending", false);
  pref.end();

  pref.begin("wifi-creds", true);
  staSSID = pref.getString("ssid", "");
  staPASS = pref.getString("pass", "");
  pref.end();
  Serial.println("[DEBUG] Settings & WiFi Creds loaded from Flash.");
}

//Webpages
void handleRoot() {
  Serial.println("[DEBUG] Web: Sales Panel accessed.");
  String h = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>body{font-family:sans-serif; text-align:center; background:#f4f7f6; padding:20px;}.card{background:white; padding:15px; max-width:400px; margin:10px auto; border-radius:12px; box-shadow:0 4px 10px rgba(0,0,0,0.1);}input{margin:5px 0; padding:10px; width:90%; border-radius:5px; border:1px solid #ccc;}button{padding:12px; width:90%; background:#3498db; color:white; border:none; border-radius:8px; font-weight:bold; cursor:pointer;}</style></head><body>";
  h += "<h2>Sales Update Panel</h2>";
  h += "<div class='card'><h3>Update Price/Qty</h3><form action='/set'>";
  h += "Product: <input name='p' value='"+partProd+"'>";
  h += "Qty: <input name='q' value='"+partQty+"'>";
  h += "Price: <input name='pr' value='"+partPrice+"'>";
  h += "Discount: <input name='d' value='"+partDisc+"'>";
  h += "<button type='submit'>PUSH TO SHELF</button></form></div>";
  h += "<p style='font-size:0.8em; color:#999;'>Go to /admin to change WiFi settings.</p></body></html>";
  server.send(200, "text/html", h);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USERNAME, ADMIN_PASSWORD)) {
    Serial.println("[DEBUG] Web: Unauthorized Admin Access Attempt.");
    return server.requestAuthentication();
  }
  Serial.println("[DEBUG] Web: Admin Panel accessed.");
  String h = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>body{font-family:sans-serif; text-align:center; background:#fee; padding:20px;}.card{background:white; padding:15px; max-width:400px; margin:10px auto; border-radius:12px; border:2px solid #e74c3c;}input{margin:5px 0; padding:10px; width:90%;}button{padding:12px; width:90%; background:#e74c3c; color:white; border:none; border-radius:8px; font-weight:bold; cursor:pointer;}</style></head><body>";
  h += "<h2>⚠️ Admin System Settings</h2><div class='card'><h3>WiFi Configuration</h3><form action='/save_wifi'>";
  h += "SSID: <input name='s' value='"+staSSID+"'><br>";
  h += "Pass: <input name='p' type='password' value='"+staPASS+"'><br>";
  h += "<button type='submit'>SAVE & REBOOT</button></form></div>";
  h += "<br><a href='/'>Back to Sales</a></body></html>";
  server.send(200, "text/html", h);
}

//MQTT Callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = ""; for (int i=0; i<length; i++) msg += (char)payload[i];
  String t = String(topic);
  Serial.print("\n[MQTT] Message Arrived ["); Serial.print(t); Serial.print("]: "); Serial.println(msg);

  if (t.endsWith(FEED_PROD))      partProd = msg;
  else if (t.endsWith(FEED_QTY))  partQty = msg;
  else if (t.endsWith(FEED_PRICE)) partPrice = msg;
  else if (t.endsWith(FEED_DISC))  partDisc = msg;
  else if (t.endsWith(FEED_SEND)) {
    if (msg == "1" || msg == "ON") {
      Serial.println("[MQTT] Trigger received! Saving data.");
      saveData();
    }
  }
}




