#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

//BLE CONFIG
#define DEVICE_NAME "ShelfLabel_01"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_UUID    "abcd1234-5678-90ab-cdef-1234567890ab"

//DISPLAY PINS
#define CS   5
#define DC   22
#define RST  21
#define BUSY 4

//DISPLAY DRIVER
GxEPD2_BW<GxEPD2_290_T94, GxEPD2_290_T94::HEIGHT> display(
  GxEPD2_290_T94(CS,DC,RST,BUSY)
);

//RECIEVER BUFFER
char rxBuffer[128];        // Stores incoming BLE data
uint8_t rxIndex = 0;       // Current position in buffer
volatile bool packetReady = false; 

// PRODUCT STRUCTURE
typedef struct {
  char name[32];
  char price[16];
  char qty[16];
  char offer[8];
} ProductInfo_t;

ProductInfo_t currentProduct;
ProductInfo_t lastProduct;   // To detect changes

//FUNCTION DECLARATIONS
void drawCenteredText(const char* text, int y, const GFXfont* font);
bool parseData(const char* input, ProductInfo_t &product);
void fullRefresh();
void partialRefresh();
void updateDisplay(const char* input);


// BLE WRITE CALLBACK 
class WriteCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String value = pChar->getValue();

   //Copy Incoming data
    for (int i = 0; i < value.length(); i++) {
      if (rxIndex < sizeof(rxBuffer) - 1) {
        rxBuffer[rxIndex++] = value[i];
      }
    }
    rxBuffer[rxIndex] = '\0'; // Null terminate

    //creating pipes for dividing data
    uint8_t pipeCount = 0;
    for (uint8_t i = 0; i < rxIndex; i++) {
      if (rxBuffer[i] == '|') pipeCount++;
    }

    if (pipeCount >= 3) {
      packetReady = true;
      rxIndex = 0; // Ready for next packet
    }
  }
};

//SETUP FUNCTION
void setup() {
  Serial.begin(115200);
  delay(1500);

  // Initialize display
  display.init(115200);
  display.setRotation(1);
  updateDisplay("WELCOME|--|--|--");

  // Initialize BLE
  BLEDevice::init(DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *characteristic = service->createCharacteristic(
    CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  characteristic->setCallbacks(new WriteCallback());
  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();

  Serial.println("BLE ESL ready ");
}

//LOOP FUNCTION
void loop() {
  if (packetReady) {
    packetReady = false;

    Serial.print("Received: ");
    Serial.println(rxBuffer);

    updateDisplay(rxBuffer);
  }
}

//FUNCTION DEFINATIONS

// CENTER TEXT 
void drawCenteredText(const char* text, int y, const GFXfont* font) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setFont(font);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int x = (display.width() - w) / 2;

  display.setCursor(x, y);
  display.print(text);
}

// PARSE BLE DATA 
bool parseData(const char* input, ProductInfo_t &product) {
  // Expected format: name|price|qty|offer
  int fields = sscanf(input,
    "%31[^|]|%15[^|]|%15[^|]|%7[^|]",
    product.name,
    product.price,
    product.qty,
    product.offer
  );

  return fields == 4;
}

//FULL DISPLAY REFRESH 
void fullRefresh() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // Draw border
    display.drawRect(2, 2, 292, 124, GxEPD_BLACK);

    // Product name
    drawCenteredText(currentProduct.name, 35, &FreeMonoBold18pt7b);

    // Divider line
    display.drawLine(10, 45, 286, 45, GxEPD_BLACK);

    // Price & Quantity
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(15, 75);
    display.print("Price : \xE2\x82\xB9");
    display.print(currentProduct.price);

    display.setCursor(15, 95);
    display.print("Qty   : ");
    display.print(currentProduct.qty);

    // Offer box
    display.drawRect(185, 65, 95, 35, GxEPD_BLACK);
    display.setCursor(195, 88);
    display.print(currentProduct.offer);
    display.print("% OFF");

  } while (display.nextPage());
}

// PARTIAL REFRESH 
void partialRefresh() {
  display.setPartialWindow(0, 60, display.width(), 60);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(15, 75);
    display.print("Price : \xE2\x82\xB9");
    display.print(currentProduct.price);

    display.setCursor(15, 95);
    display.print("Qty   : ");
    display.print(currentProduct.qty);

    display.drawRect(185, 65, 95, 35, GxEPD_BLACK);
    display.setCursor(195, 88);
    display.print(currentProduct.offer);
    display.print("% OFF");

  } while (display.nextPage());
}

// UPDATE DISPLAY 
void updateDisplay(const char* input) {
  if (!parseData(input, currentProduct)) {
    Serial.println("Invalid packet ignored");
    return;
  }

  bool productChanged = strcmp(currentProduct.name, lastProduct.name) != 0;

  if (productChanged) {
    fullRefresh();
    lastProduct = currentProduct; 
  } else {
    partialRefresh();
  }
}