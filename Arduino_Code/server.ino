#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_BME280.h> // BME280 Library

// Create BLE Server instance
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Timer
unsigned long previousMillis = 0;
const long interval = 2000;  // Send data every 2 seconds

// BME280 Initialization
Adafruit_BME280 bme;

// BLE UUIDs
#define SERVICE_UUID        "c36bda34-c052-4045-9635-7f42a81b0deb"
#define CHARACTERISTIC_UUID "97a128ba-e071-4602-8d34-03c67a925c72"

class MyServerCallbacks : public BLEServerCallbacks {
   void onConnect(BLEServer* pServer) {
       deviceConnected = true;
   };

   void onDisconnect(BLEServer* pServer) {
       deviceConnected = false;
   }
};

void setup() {
   Serial.begin(115200);
   Serial.println("Starting BLE Server...");

   if (!bme.begin(0x76)) { // BME280 default I2C address is 0x76
       Serial.println("Could not find a valid BME280 sensor, check wiring!");
       while (1);  // Halt the program until the issue is resolved
   }

   BLEDevice::init("BME280_Server");
   pServer = BLEDevice::createServer();
   pServer->setCallbacks(new MyServerCallbacks());

   BLEService *pService = pServer->createService(SERVICE_UUID);

   pCharacteristic = pService->createCharacteristic(
       CHARACTERISTIC_UUID,
       BLECharacteristic::PROPERTY_READ |
       BLECharacteristic::PROPERTY_NOTIFY
   );
   pCharacteristic->addDescriptor(new BLE2902());

   pService->start();

   BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
   pAdvertising->addServiceUUID(SERVICE_UUID);
   pAdvertising->setScanResponse(true);
   pAdvertising->setMinPreferred(0x06);  
   pAdvertising->setMinPreferred(0x12);  
   BLEDevice::startAdvertising();

   Serial.println("BLE Server Started! Now advertising...");
}

void loop() {
   if (deviceConnected) {
       unsigned long currentMillis = millis();
       if (currentMillis - previousMillis >= interval) {
           previousMillis = currentMillis;

           float temperature = bme.readTemperature();

           String tempStr = String(temperature, 2) + " C"; // Keep two decimal points
           pCharacteristic->setValue(tempStr.c_str());
           pCharacteristic->notify();

           Serial.print("Notify value: ");
           Serial.println(tempStr);
       }
   }

   if (!deviceConnected && oldDeviceConnected) {
       delay(500);  // Wait briefly
       pServer->startAdvertising(); 
       Serial.println("Re-advertising BLE...");
       oldDeviceConnected = deviceConnected;
   }

   if (deviceConnected && !oldDeviceConnected) {
       oldDeviceConnected = deviceConnected;
   }

   delay(1000); 
}

