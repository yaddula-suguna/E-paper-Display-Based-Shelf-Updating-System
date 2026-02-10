#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include "esp_sleep.h"

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

//BLECONFIG
#define DEVICE_NAME "ShelfLabel_01"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_UUID    "abcd1234-5678-90ab-cdef-1234567890ab"

#define BLE_ACTIVE_TIME_MS   30000  // 30s BLE window
#define POST_UPDATE_DELAY_MS 2000
#define RTC_WAKE_SEC          60    // wake every 1 min
#define RTC_WAKE_US (RTC_WAKE_SEC * 1000000ULL)

//DISPLAY PINS
#define CS   5
#define DC   22
#define RST  21
#define BUSY 4
#define CLK  18
#define DIN  23

//DISPLAY DRIVER
GxEPD2_BW<GxEPD2_290_T94, GxEPD2_290_T94::HEIGHT> display(
  GxEPD2_290_T94(CS, DC, RST, BUSY)
);

//NVS (This will store data in Flash)
Preferences prefs;

//RECIEVER BUFFER =================
char rxBuffer[128];
uint8_t rxIndex = 0;
volatile bool packetReady = false;

// PRODUCT STRUCTURE
typedef struct {
  char name[32];
  char price[16];
  char qty[16];
  char offer[8];
} ProductInfo_t;

ProductInfo_t currentProduct;
ProductInfo_t lastProduct;

//POWER STATE
unsigned long bleStartTime = 0;
bool updateHappened = false;

//FUNCTION DECLARATIONS
void drawCenteredText(const char* text, int y, const GFXfont* font);
bool parseData(const char* input, ProductInfo_t &product);
void fullRefresh();
void partialRefresh();
void updateDisplay(const char* input, bool save);
void goToSleep();

// BLE WRITE CALLBACK 
class WriteCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String val = pChar->getValue();	
    for (int i = 0; i < val.length(); i++) {
      if (rxIndex < sizeof(rxBuffer) - 1) rxBuffer[rxIndex++] = val[i];
    }
    rxBuffer[rxIndex] = '\0';

    // Packet complete when 3 pipes are present
    uint8_t pipes = 0;
    for (uint8_t i = 0; i < rxIndex; i++) if (rxBuffer[i] == '|') pipes++;
    if (pipes >= 3) {
      packetReady = true;
      rxIndex = 0;
    }
  }
};

//SETUP FUNCTION
void setup() {
  Serial.begin(115200);
  delay(1000);

  prefs.begin("esl", false);

  display.init(115200);
  display.setRotation(1);

  String last = prefs.getString("last", "");
  updateDisplay(last.length() ? last.c_str() : "WELCOME|--|--|--", false);

  BLEDevice::init(DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *characteristic = service->createCharacteristic(
    CHAR_UUID, BLECharacteristic::PROPERTY_WRITE
  );
  characteristic->setCallbacks(new WriteCallback());
  service->start();

  BLEDevice::getAdvertising()->start();
  bleStartTime = millis();
  updateHappened = false;

  Serial.println("BLE ESL awake and advertising");
}

//LOOP FUNCTION
void loop() {
  if (packetReady) {
    packetReady = false;
    updateHappened = true;

    updateDisplay(rxBuffer, true);
    delay(POST_UPDATE_DELAY_MS);
    goToSleep();
  }

  if (!updateHappened && millis() - bleStartTime > BLE_ACTIVE_TIME_MS) {
    Serial.println("No update received, sleeping...");
    goToSleep();
  }
}

// FUNCTIONS DEFINATIONS
void drawCenteredText(const char* text, int y, const GFXfont* font) {
  int16_t x1, y1; uint16_t w, h;
  display.setFont(font);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((display.width() - w) / 2, y);
  display.print(text);
}

bool parseData(const char* input, ProductInfo_t &product) {
  int fields = sscanf(input,
    "%31[^|]|%15[^|]|%15[^|]|%7[^|]",
    product.name,
    product.price,
    product.qty,
    product.offer
  );
  return fields == 4;
}

void fullRefresh() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.drawRect(2, 2, 292, 124, GxEPD_BLACK);
    drawCenteredText(currentProduct.name, 35, &FreeMonoBold18pt7b);
    display.drawLine(10, 45, 286, 45, GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(15, 75); display.print("Price : \xE2\x82\xB9"); display.print(currentProduct.price);
    display.setCursor(15, 95); display.print("Qty   : "); display.print(currentProduct.qty);

    display.drawRect(185, 65, 95, 35, GxEPD_BLACK);
    display.setCursor(195, 88); display.print(currentProduct.offer); display.print("% OFF");
  } while (display.nextPage());
}

void partialRefresh() {
  display.setPartialWindow(0, 60, display.width(), 60);
  display.firstPage();
  do {
    display.fillRect(15, 60, 160, 40, GxEPD_WHITE);
    display.fillRect(185, 65, 95, 35, GxEPD_WHITE);

    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(15, 75); display.print("Price : \xE2\x82\xB9"); display.print(currentProduct.price);
    display.setCursor(15, 95); display.print("Qty   : "); display.print(currentProduct.qty);

    display.drawRect(185, 65, 95, 35, GxEPD_BLACK);
    display.setCursor(195, 88); display.print(currentProduct.offer); display.print("% OFF");
  } while (display.nextPage());
}

void updateDisplay(const char* input, bool save) {
  if (!parseData(input, currentProduct)) {
    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); drawCenteredText("WELCOME", 60, &FreeMonoBold18pt7b); }
    while (display.nextPage());
    return;
  }

  bool productChanged = strcmp(currentProduct.name, lastProduct.name) != 0;
  if (productChanged) { fullRefresh(); lastProduct = currentProduct; }
  else partialRefresh();

  if (save) prefs.putString("last", input);
}

void goToSleep() {
  Serial.println("Going to deep sleep...");
  BLEDevice::deinit(true);
  delay(100);
  esp_sleep_enable_timer_wakeup(RTC_WAKE_US);
  esp_deep_sleep_start();
} 

