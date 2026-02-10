**🏷️ ESP32 BLE E-Paper Shelf Label System**
Low-Power Electronic Shelf Label (ESL) with Web + MQTT Gateway

**📌 Project Overview**
This project implements a smart electronic shelf label (ESL) system using ESP32, BLE, and a 2.9" E-Paper display.
It consists of two ESP32 firmwares:
1. Shelf Label (BLE + E-Paper + Deep Sleep)
2. Gateway Controller (Wi-Fi + Web UI + MQTT + BLE Central)
The system is designed for retail price tags, offering:
-Ultra-low power operation
-Wireless updates via BLE
-Web dashboard & MQTT integration
-OTA updates for the gateway

**🖥️ Part 1: ESP32 Shelf Label (BLE + E-Paper)**
  
✨ **Features**
-BLE-based data update
-2.9" E-Paper display (GxEPD2)
-Full & partial refresh logic
-Data stored in Flash (NVS)
-Ultra-low power deep sleep
-Auto wake-up via RTC timer

**🧾 Display Layout**
-Product Name
-Price (₹)
-Quantity
-Discount / Offer

**🔋 Power Optimization**
-BLE active only for specific window
-Goes to deep sleep after update or timeout
-RTC wake-up every pre-defined time

**📡 BLE Protocol**
-Device Name: ""
-Service UUID: ""
-Characteristic UUID: ""

**Payload Format** - ProductName|Price|Qty|Discount
**Example** - Milk|36||500ml|12

**🌐 Part 2: ESP32 Gateway (Wi-Fi + Web + MQTT + BLE)**

**✨ Features**
-Web-based dashboard (Mobile Friendly)
-MQTT integration (Adafruit IO)
-BLE Central (auto scan & connect)
-OTA firmware updates
-mDNS support (shelf.local)
-Persistent storage using NVS
-System logs via browser
-Admin Wi-Fi configuration

**📊 Web Dashboard Pages**
URL	Description
/	      -     Sales dashboard (push label data)
/ota	  -     System & version info
/log	  -     Live system logs
/admin  -	  Wi-Fi configuration (auth protected)

**Default Credentials**
Username: admin
Password: admin

**☁️ MQTT Integration (Adafruit IO)**
Feeds Used
shelf-prod   → Product Name
shelf-qty    → Quantity
shelf-price  → Price
shelf-disc   → Discount
shelf-send   → Trigger BLE push

When shelf-send = 1, the gateway:
1.Saves data to Flash
2.Scans for Shelf Label
3.Sends BLE packet
4.Confirms success

**🔄 OTA Updates**
-OTA enabled on Gateway ESP32
-Last update date stored in Flash
-Accessible via /ota page

**📦 Libraries Used**
**Shelf Label**
-BLEDevice
-Preferences
-GxEPD2
-Adafruit GFX
-ESP Deep Sleep

**Gateway**
-WiFi
-WebServer
-PubSubClient (MQTT)
-ArduinoOTA
-ESPmDNS
-BLEDevice
-Preferences

**⚙️ Hardware Requirements**
**Shelf Label**
-ESP32
-2.9" E-Paper Display (GDEH029A / T94)
-Battery (Li-Ion / Li-Po)
-Gateway
-ESP32
-Wi-Fi Access
-Internet (for MQTT / OTA)

**🚀 Getting Started**
-Flash Shelf Label firmware to ESP32
-Flash Gateway firmware to another ESP32
-Connect Gateway to Wi-Fi
-Open http://shelf.local
-Enter product details
-Push to shelf 🎉

**🔐 Security Notes**
-BLE is short-range & time-limited
-Admin pages require authentication
-Wi-Fi credentials stored securely in NVS

**Conclusion**
This ESL system demonstrates a scalable, low‑power, and cost‑effective IoT solution for smart retail. With BLE updates, MQTT integration, OTA support, and a web dashboard, it bridges embedded systems with cloud IoT. Future improvements will make it enterprise‑ready, secure, and analytics‑driven.
