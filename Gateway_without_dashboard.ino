#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

//Confugration
const char* TARGET_DEVICE_NAME = "ShelfLabel_01";
const char* SERVICE_UUID       = "12345678-1234-1234-1234-1234567890ab";
const char* CHAR_UUID          = "abcd1234-5678-90ab-cdef-1234567890ab";

// State Management
String currentPayload = "WELCOME|0|0|0";  //this is in this format product|price|qty|discount
String lastSentPayload = "";              //this will hold last string so it will not send same string twice
bool isDeviceFound = false;               //for connectiom
bool isBusy        = false;               //for checking if busy or not
bool isScanning    = false;               //to start or stop scan

BLEAddress* targetAddress = nullptr;      //this will hold address so it will not search shelf address again again
BLEScan* pBLEScanner;                     //ble scanner instance

// BLE Scanner Logic

class DeviceFoundCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (isBusy)
    { return;
    }

    if (advertisedDevice.haveName() && advertisedDevice.getName() == TARGET_DEVICE_NAME)
     {
      Serial.printf("Found Target: %s \n", advertisedDevice.getAddress().toString().c_str());
      
      targetAddress = new BLEAddress(advertisedDevice.getAddress());
      isDeviceFound = true;
      pBLEScanner->stop(); 
    }
  }
};

// COMMUNICATION LOGIC 

bool transmitUpdate() {
  if (!targetAddress)
  {
      return false;
  } 

  // for checking if current data is not equal to previous
  if (currentPayload == lastSentPayload)
  { return false;
  }

  isBusy = true;
  BLEClient* pClient = BLEDevice::createClient();
  bool success = false;

  Serial.println("Attempting connection...");
  
  // Try to connect (3 Attempts)
  for (int i = 0; i < 3; i++) {
    if (pClient->connect(*targetAddress))
     {
      success = true;
      break;
    }
    delay(500);
  }

  if (success) {
    BLERemoteService* pService = pClient->getService(SERVICE_UUID);
    if (pService) {
      BLERemoteCharacteristic* pChar = pService->getCharacteristic(CHAR_UUID);
      if (pChar && pChar->canWrite()) {
        pChar->writeValue(currentPayload.c_str(), true);
        lastSentPayload = currentPayload;
        Serial.println("Data Synced Successfully.");
      }
    }
    pClient->disconnect();
  } else {
    Serial.println("Connection Failed.");
  }

  // Cleanup
  delete targetAddress;
  targetAddress = nullptr;
  isDeviceFound = false;
  isBusy = false;
  return success;
}

//SYSTEM LIFECYCLE

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Smart Shelf Gateway Initialized ---");

  BLEDevice::init("ESP32_GATEWAY");
  pBLEScanner = BLEDevice::getScan();
  pBLEScanner->setAdvertisedDeviceCallbacks(new DeviceFoundCallbacks());
  pBLEScanner->setActiveScan(true);
  
  // Start the first scan
  pBLEScanner->start(5, false); //this will scan for 5 sec
  isScanning = true;
}

void loop() {
  // Listen for manual updates via Serial Monitor
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      currentPayload = input;
      Serial.printf("New Data Queued: %s\n", currentPayload.c_str());
    }
  }

  // If a device was found during scan, talk to it
  if (isDeviceFound && !isBusy) {
    transmitUpdate();
    Serial.println(" Resuming search...");
    pBLEScanner->start(5, false);
  }

  // Keep scanning if we haven't found anything
  if (!isDeviceFound && !isBusy) {
    pBLEScanner->start(1, false); 
  }

  delay(100); 
}